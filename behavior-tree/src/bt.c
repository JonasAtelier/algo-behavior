// SPDX-License-Identifier: MIT
/*
 * Table-driven behaviour tree. See include/bt.h for the API.
 */

#include "bt.h"

#include <stddef.h>

/* Widest value cursor[] can hold, which caps a repeat's count. */
#define BT_MAX_COUNT	255

/*
 * Validation walk: depth first from the root, marking nodes on the way in.
 * A node reached twice is a cycle or a shared child, and either would have
 * two parents writing one cursor byte, so both are refused here rather than
 * left to misbehave at tick time.
 */
static int bt_check(const struct bt_def *def, unsigned char *seen, int idx)
{
	const struct bt_node *n = &def->nodes[idx];
	int i;

	if (seen[idx])
		return BT_EARG;
	seen[idx] = 1;

	switch (n->kind) {
	case BT_ACTION:
	case BT_CONDITION:
		if (!n->tick || n->children)
			return BT_EARG;
		return BT_OK;
	case BT_INVERT:
	case BT_REPEAT:
		if (n->n_children != 1)
			return BT_EARG;
		break;
	case BT_SEQUENCE:
	case BT_SELECTOR:
		break;
	default:
		return BT_EARG;
	}

	if (n->tick || !n->children || n->n_children < 1)
		return BT_EARG;
	if (n->kind == BT_REPEAT && (n->count < 0 || n->count > BT_MAX_COUNT))
		return BT_EARG;

	for (i = 0; i < n->n_children; i++) {
		int c = n->children[i];

		if (c < 0 || c >= def->n_nodes)
			return BT_EARG;
		if (bt_check(def, seen, c) != BT_OK)
			return BT_EARG;
	}

	return BT_OK;
}

/*
 * Forget a subtree the tree walked away from, so it cannot resume half
 * finished the next time it wins a tick.
 */
static void bt_forget(struct bt *t, int idx, int depth)
{
	const struct bt_node *n = &t->def->nodes[idx];
	int i;

	if (depth > BT_MAX_NODES)
		return;

	t->cursor[idx] = 0;
	if (!n->children)
		return;

	for (i = 0; i < n->n_children; i++)
		bt_forget(t, n->children[i], depth + 1);
}

static enum bt_status bt_run(struct bt *t, int idx, int depth)
{
	const struct bt_node *n = &t->def->nodes[idx];
	enum bt_status st;
	int i, prev, done;

	/* bt_init proved the tree is a tree, so this only fires on a def
	 * that changed under us. Cheaper than trusting it. */
	if (depth > BT_MAX_NODES)
		return BT_FAILURE;

	switch (n->kind) {
	case BT_ACTION:
		st = n->tick(t->ctx);
		if (st == BT_RUNNING)
			t->running = idx;
		return st;

	case BT_CONDITION:
		return n->tick(t->ctx) == BT_SUCCESS ? BT_SUCCESS : BT_FAILURE;

	case BT_SEQUENCE:
		/* memory: pick up at the child that was still running */
		for (i = t->cursor[idx]; i < n->n_children; i++) {
			st = bt_run(t, n->children[i], depth + 1);
			if (st == BT_RUNNING) {
				t->cursor[idx] = (unsigned char)i;
				return BT_RUNNING;
			}
			if (st == BT_FAILURE) {
				t->cursor[idx] = 0;
				return BT_FAILURE;
			}
		}
		t->cursor[idx] = 0;
		return BT_SUCCESS;

	case BT_SELECTOR:
		/* reactive: always from the first child, so a condition ahead
		 * of a running branch can take the tick away from it. The
		 * cursor holds that branch, plus one, 0 meaning none. */
		prev = t->cursor[idx];
		for (i = 0; i < n->n_children; i++) {
			st = bt_run(t, n->children[i], depth + 1);
			if (st == BT_FAILURE)
				continue;
			if (prev && prev - 1 != i)
				bt_forget(t, n->children[prev - 1], depth + 1);
			t->cursor[idx] = st == BT_RUNNING ?
					 (unsigned char)(i + 1) : 0;
			return st;
		}
		t->cursor[idx] = 0;
		return BT_FAILURE;

	case BT_INVERT:
		st = bt_run(t, n->children[0], depth + 1);
		if (st == BT_SUCCESS)
			return BT_FAILURE;
		if (st == BT_FAILURE)
			return BT_SUCCESS;
		return BT_RUNNING;

	case BT_REPEAT:
		/* the cursor carries the iterations done, so a child that
		 * goes RUNNING does not cost the repeat its place */
		done = t->cursor[idx];
		for (;;) {
			st = bt_run(t, n->children[0], depth + 1);
			if (st == BT_RUNNING)
				return BT_RUNNING;
			if (st == BT_FAILURE) {
				t->cursor[idx] = 0;
				return BT_FAILURE;
			}
			done++;
			if (n->count == 0) {
				t->cursor[idx] = 0;
				return BT_RUNNING;	/* one a tick */
			}
			if (done >= n->count) {
				t->cursor[idx] = 0;
				return BT_SUCCESS;
			}
			t->cursor[idx] = (unsigned char)done;
		}
	}

	return BT_FAILURE;	/* unreachable: bt_check rejects other kinds */
}

/**
 * bt_init() - check a tree and start it
 * @t: instance to set up
 * @def: tree to run, must outlive @t
 * @ctx: caller data for the leaf functions, may be NULL
 *
 * Walks the table once: every child index in range, every node reached at
 * most once (no cycles, no shared children), leaves with a tick function and
 * no children, composites with children and no tick function, one child for
 * invert and repeat, a repeat count of 0 to 255. Nodes the root cannot reach
 * are left alone. Nothing is checked again at tick time.
 *
 * Return: %BT_OK, or %BT_EARG on a bad argument or a bad table.
 */
int bt_init(struct bt *t, const struct bt_def *def, void *ctx)
{
	unsigned char seen[BT_MAX_NODES] = { 0 };

	if (!t || !def || !def->nodes)
		return BT_EARG;
	if (def->n_nodes < 1 || def->n_nodes > BT_MAX_NODES)
		return BT_EARG;
	if (def->root < 0 || def->root >= def->n_nodes)
		return BT_EARG;
	if (bt_check(def, seen, def->root) != BT_OK)
		return BT_EARG;

	t->def = def;
	t->ctx = ctx;
	bt_reset(t);

	return BT_OK;
}

/**
 * bt_tick() - run one tick, from the root
 * @t: running instance
 *
 * A sequence remembers where it was and picks up at the child that was
 * running. A selector asks again from its first child every tick, and resets
 * the branch it walks away from.
 *
 * Return: %BT_SUCCESS, %BT_FAILURE, or %BT_RUNNING when a leaf is not done.
 * A broken argument reports %BT_FAILURE, since the status has no error value.
 */
enum bt_status bt_tick(struct bt *t)
{
	if (!t || !t->def)
		return BT_FAILURE;

	t->running = -1;

	return bt_run(t, t->def->root, 0);
}

/**
 * bt_reset() - forget every running child
 * @t: running instance, may be NULL
 *
 * The next tick starts from the top of the tree. Your own @ctx is untouched.
 */
void bt_reset(struct bt *t)
{
	int i;

	if (!t)
		return;

	for (i = 0; i < BT_MAX_NODES; i++)
		t->cursor[i] = 0;
	t->running = -1;
}

/**
 * bt_running() - which leaf is not finished
 * @t: running instance, may be NULL
 *
 * Return: the @name of the leaf that returned %BT_RUNNING on the last tick,
 * or NULL when the last tick finished or that leaf has no name.
 */
const char *bt_running(const struct bt *t)
{
	if (!t || !t->def || t->running < 0)
		return NULL;

	return t->def->nodes[t->running].name;
}
