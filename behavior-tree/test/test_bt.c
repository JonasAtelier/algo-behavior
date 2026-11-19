// SPDX-License-Identifier: GPL-3.0-only
/*
 * Self-check: assert-based, no framework. Build with `make check`.
 */

#include "bt.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#define ARRAY_SIZE(a)	((int)(sizeof(a) / sizeof((a)[0])))

enum { L0, L1, L2, L3, N_LEAF };

struct rec {
	int calls[N_LEAF];
	enum bt_status ret[N_LEAF];
};

static enum bt_status leaf(void *ctx, int id)
{
	struct rec *r = (struct rec *)ctx;

	r->calls[id]++;

	return r->ret[id];
}

static enum bt_status leaf0(void *ctx)
{
	return leaf(ctx, L0);
}

static enum bt_status leaf1(void *ctx)
{
	return leaf(ctx, L1);
}

static enum bt_status leaf2(void *ctx)
{
	return leaf(ctx, L2);
}

/*
 * SUCCESS, SUCCESS, RUNNING, then SUCCESS forever. Catches a repeat that
 * forgets how many iterations it had done when its child went RUNNING.
 */
static enum bt_status leaf_script(void *ctx)
{
	struct rec *r = (struct rec *)ctx;

	r->calls[L3]++;

	return r->calls[L3] == 3 ? BT_RUNNING : BT_SUCCESS;
}

static void rec_init(struct rec *r)
{
	int i;

	for (i = 0; i < N_LEAF; i++) {
		r->calls[i] = 0;
		r->ret[i] = BT_SUCCESS;
	}
}

/* Three actions under one composite: nodes 1, 2, 3 are leaf0, leaf1, leaf2. */
static const int kids3[] = { 1, 2, 3 };

static const struct bt_node seq_nodes[] = {
	{ BT_SEQUENCE, "seq", NULL,  kids3, 3, 0 },
	{ BT_ACTION,   "a0",  leaf0, NULL,  0, 0 },
	{ BT_ACTION,   "a1",  leaf1, NULL,  0, 0 },
	{ BT_ACTION,   "a2",  leaf2, NULL,  0, 0 },
};

static const struct bt_def seq_def = { seq_nodes, ARRAY_SIZE(seq_nodes), 0 };

static const struct bt_node sel_nodes[] = {
	{ BT_SELECTOR, "sel", NULL,  kids3, 3, 0 },
	{ BT_ACTION,   "a0",  leaf0, NULL,  0, 0 },
	{ BT_ACTION,   "a1",  leaf1, NULL,  0, 0 },
	{ BT_ACTION,   "a2",  leaf2, NULL,  0, 0 },
};

static const struct bt_def sel_def = { sel_nodes, ARRAY_SIZE(sel_nodes), 0 };

static void test_sequence(void)
{
	struct rec r;
	struct bt t;

	rec_init(&r);
	assert(bt_init(&t, &seq_def, &r) == BT_OK);
	assert(t.running == -1);

	/* all succeed -> SUCCESS, everyone ticked once */
	assert(bt_tick(&t) == BT_SUCCESS);
	assert(r.calls[L0] == 1 && r.calls[L1] == 1 && r.calls[L2] == 1);

	/* the first failure stops it, later children are not called */
	rec_init(&r);
	r.ret[L1] = BT_FAILURE;
	assert(bt_tick(&t) == BT_FAILURE);
	assert(r.calls[L0] == 1 && r.calls[L1] == 1 && r.calls[L2] == 0);

	/* and it starts over from the first child next tick */
	assert(bt_tick(&t) == BT_FAILURE);
	assert(r.calls[L0] == 2);
}

static void test_sequence_resumes(void)
{
	struct rec r;
	struct bt t;

	rec_init(&r);
	r.ret[L1] = BT_RUNNING;
	assert(bt_init(&t, &seq_def, &r) == BT_OK);

	assert(bt_tick(&t) == BT_RUNNING);
	assert(r.calls[L0] == 1 && r.calls[L1] == 1 && r.calls[L2] == 0);

	/* resumes at the running child, does not re-tick the first one */
	assert(bt_tick(&t) == BT_RUNNING);
	assert(r.calls[L0] == 1 && r.calls[L1] == 2);

	/* it succeeds: this tick finishes the sequence from where it was */
	r.ret[L1] = BT_SUCCESS;
	assert(bt_tick(&t) == BT_SUCCESS);
	assert(r.calls[L0] == 1 && r.calls[L1] == 3 && r.calls[L2] == 1);

	/* finished, so the next tick is back at the top */
	assert(bt_tick(&t) == BT_SUCCESS);
	assert(r.calls[L0] == 2);
}

static void test_selector(void)
{
	struct rec r;
	struct bt t;

	rec_init(&r);
	assert(bt_init(&t, &sel_def, &r) == BT_OK);

	/* the first success wins, later children are not called */
	assert(bt_tick(&t) == BT_SUCCESS);
	assert(r.calls[L0] == 1 && r.calls[L1] == 0 && r.calls[L2] == 0);

	/* all fail -> FAILURE, everyone tried */
	rec_init(&r);
	r.ret[L0] = BT_FAILURE;
	r.ret[L1] = BT_FAILURE;
	r.ret[L2] = BT_FAILURE;
	assert(bt_tick(&t) == BT_FAILURE);
	assert(r.calls[L0] == 1 && r.calls[L1] == 1 && r.calls[L2] == 1);
}

/*
 * A selector asks again every tick, so a condition ahead of a running branch
 * can take the tick away from it - and the branch it walks away from must not
 * resume half-finished later.
 */
static const int react_kids[] = { 1, 2 };
static const int react_branch[] = { 3, 4 };

static const struct bt_node react_nodes[] = {
	{ BT_SELECTOR,  "sel",    NULL,  react_kids,   2, 0 },
	{ BT_CONDITION, "cond",   leaf0, NULL,         0, 0 },
	{ BT_SEQUENCE,  "branch", NULL,  react_branch, 2, 0 },
	{ BT_ACTION,    "b0",     leaf1, NULL,         0, 0 },
	{ BT_ACTION,    "b1",     leaf2, NULL,         0, 0 },
};

static const struct bt_def react_def = {
	react_nodes, ARRAY_SIZE(react_nodes), 0
};

static void test_selector_is_reactive(void)
{
	struct rec r;
	struct bt t;

	rec_init(&r);
	r.ret[L0] = BT_FAILURE;		/* condition says no, for now */
	r.ret[L2] = BT_RUNNING;		/* the branch's second step runs */
	assert(bt_init(&t, &react_def, &r) == BT_OK);

	assert(bt_tick(&t) == BT_RUNNING);
	assert(r.calls[L1] == 1 && r.calls[L2] == 1);
	assert(t.cursor[2] == 1);	/* branch parked on its second child */

	/* next tick still asks the condition first */
	assert(bt_tick(&t) == BT_RUNNING);
	assert(r.calls[L0] == 2);
	assert(r.calls[L2] == 2);

	/* now it says yes and takes the tick away from the running branch */
	r.ret[L0] = BT_SUCCESS;
	assert(bt_tick(&t) == BT_SUCCESS);
	assert(r.calls[L2] == 2);	/* the branch was not ticked */
	assert(t.cursor[2] == 0);	/* and it was reset, not left parked */
	assert(t.cursor[0] == 0);

	/* so the branch starts over when it gets the tick back */
	r.ret[L0] = BT_FAILURE;
	assert(bt_tick(&t) == BT_RUNNING);
	assert(r.calls[L1] == 2);
}

static const int one_kid[] = { 1 };

static const struct bt_node inv_nodes[] = {
	{ BT_INVERT, "not", NULL,  one_kid, 1, 0 },
	{ BT_ACTION, "a0",  leaf0, NULL,    0, 0 },
};

static const struct bt_def inv_def = { inv_nodes, ARRAY_SIZE(inv_nodes), 0 };

static const struct bt_node cond_nodes[] = {
	{ BT_CONDITION, "cond", leaf0, NULL, 0, 0 },
};

static const struct bt_def cond_def = { cond_nodes, 1, 0 };

static void test_invert_and_condition(void)
{
	struct rec r;
	struct bt t;

	rec_init(&r);
	assert(bt_init(&t, &inv_def, &r) == BT_OK);

	assert(bt_tick(&t) == BT_FAILURE);	/* SUCCESS inverted */

	r.ret[L0] = BT_FAILURE;
	assert(bt_tick(&t) == BT_SUCCESS);	/* FAILURE inverted */

	r.ret[L0] = BT_RUNNING;
	assert(bt_tick(&t) == BT_RUNNING);	/* RUNNING passes through */
	assert(strcmp(bt_running(&t), "a0") == 0);

	/* a condition saying RUNNING counts as FAILURE, and is not running */
	rec_init(&r);
	r.ret[L0] = BT_RUNNING;
	assert(bt_init(&t, &cond_def, &r) == BT_OK);
	assert(bt_tick(&t) == BT_FAILURE);
	assert(bt_running(&t) == NULL);
}

static const struct bt_node rep3_nodes[] = {
	{ BT_REPEAT, "x3", NULL,  one_kid, 1, 3 },
	{ BT_ACTION, "a0", leaf0, NULL,    0, 0 },
};

static const struct bt_def rep3_def = { rep3_nodes, ARRAY_SIZE(rep3_nodes), 0 };

static const struct bt_node rep0_nodes[] = {
	{ BT_REPEAT, "forever", NULL,  one_kid, 1, 0 },
	{ BT_ACTION, "a0",      leaf0, NULL,    0, 0 },
};

static const struct bt_def rep0_def = { rep0_nodes, ARRAY_SIZE(rep0_nodes), 0 };

static const struct bt_node reps_nodes[] = {
	{ BT_REPEAT, "x3",     NULL,       one_kid, 1, 3 },
	{ BT_ACTION, "script", leaf_script, NULL,   0, 0 },
};

static const struct bt_def reps_def = { reps_nodes, ARRAY_SIZE(reps_nodes), 0 };

static void test_repeat(void)
{
	struct rec r;
	struct bt t;

	/* count 3: three child ticks inside one tick */
	rec_init(&r);
	assert(bt_init(&t, &rep3_def, &r) == BT_OK);
	assert(bt_tick(&t) == BT_SUCCESS);
	assert(r.calls[L0] == 3);

	/* a failure stops it there */
	rec_init(&r);
	r.ret[L0] = BT_FAILURE;
	assert(bt_tick(&t) == BT_FAILURE);
	assert(r.calls[L0] == 1);

	/* count 0: forever, one child tick per tick, never finishes */
	rec_init(&r);
	assert(bt_init(&t, &rep0_def, &r) == BT_OK);
	assert(bt_tick(&t) == BT_RUNNING);
	assert(r.calls[L0] == 1);
	assert(bt_tick(&t) == BT_RUNNING);
	assert(r.calls[L0] == 2);

	/* the count survives a child that goes RUNNING mid-repeat */
	rec_init(&r);
	assert(bt_init(&t, &reps_def, &r) == BT_OK);
	assert(bt_tick(&t) == BT_RUNNING);
	assert(r.calls[L3] == 3);	/* success, success, running */
	assert(bt_tick(&t) == BT_SUCCESS);
	assert(r.calls[L3] == 4);	/* one more success, not three */
}

static const struct bt_node named_nodes[] = {
	{ BT_SEQUENCE, "seq", NULL,  react_kids, 2, 0 },
	{ BT_ACTION,   "one", leaf0, NULL,       0, 0 },
	{ BT_ACTION,   "two", leaf1, NULL,       0, 0 },
};

static const struct bt_def named_def = {
	named_nodes, ARRAY_SIZE(named_nodes), 0
};

static void test_running_and_reset(void)
{
	struct rec r;
	struct bt t;

	rec_init(&r);
	r.ret[L1] = BT_RUNNING;
	assert(bt_init(&t, &named_def, &r) == BT_OK);
	assert(bt_running(&t) == NULL);		/* nothing has run yet */

	assert(bt_tick(&t) == BT_RUNNING);
	assert(strcmp(bt_running(&t), "two") == 0);
	assert(t.cursor[0] == 1);

	/* it finishes: nobody is running any more */
	r.ret[L1] = BT_SUCCESS;
	assert(bt_tick(&t) == BT_SUCCESS);
	assert(bt_running(&t) == NULL);

	/* park it again, then wipe the memory */
	r.ret[L1] = BT_RUNNING;
	assert(bt_tick(&t) == BT_RUNNING);
	assert(t.cursor[0] == 1);

	bt_reset(&t);
	assert(bt_running(&t) == NULL);
	assert(t.cursor[0] == 0);

	rec_init(&r);
	r.ret[L1] = BT_RUNNING;
	assert(bt_tick(&t) == BT_RUNNING);
	assert(r.calls[L0] == 1);		/* started from the top again */

	bt_reset(NULL);				/* must not blow up */
	assert(bt_running(NULL) == NULL);
}

/* --- tables bt_init has to refuse ------------------------------------- */

static const int c_neg[] = { -1 };
static const int c_over[] = { 2 };
static const int c_up[] = { 0 };
static const int c_dup[] = { 1, 1 };
static const int c_two[] = { 1, 2 };

static const struct bt_node n_neg[] = {
	{ BT_SEQUENCE, "s", NULL,  c_neg, 1, 0 },
	{ BT_ACTION,   "a", leaf0, NULL,  0, 0 },
};

static const struct bt_node n_over[] = {
	{ BT_SEQUENCE, "s", NULL,  c_over, 1, 0 },
	{ BT_ACTION,   "a", leaf0, NULL,   0, 0 },
};

static const struct bt_node n_cycle[] = {
	{ BT_SEQUENCE, "s", NULL, one_kid, 1, 0 },	/* down to 1 */
	{ BT_SEQUENCE, "t", NULL, c_up,    1, 0 },	/* back up to 0 */
};

static const struct bt_node n_shared[] = {
	{ BT_SEQUENCE, "s", NULL,  c_dup, 2, 0 },	/* same child twice */
	{ BT_ACTION,   "a", leaf0, NULL,  0, 0 },
};

static const struct bt_node n_no_tick[] = {
	{ BT_SEQUENCE, "s", NULL, one_kid, 1, 0 },
	{ BT_ACTION,   "a", NULL, NULL,    0, 0 },	/* leaf with no code */
};

static const struct bt_node n_leaf_kids[] = {
	{ BT_SEQUENCE, "s", NULL,  one_kid, 1, 0 },
	{ BT_ACTION,   "a", leaf0, c_up,    1, 0 },	/* leaf with children */
};

static const struct bt_node n_empty[] = {
	{ BT_SEQUENCE, "s", NULL, one_kid, 0, 0 },	/* no children */
	{ BT_ACTION,   "a", leaf0, NULL,   0, 0 },
};

static const struct bt_node n_inv2[] = {
	{ BT_INVERT, "not", NULL,  c_two, 2, 0 },	/* invert takes one */
	{ BT_ACTION, "a",   leaf0, NULL,  0, 0 },
	{ BT_ACTION, "b",   leaf1, NULL,  0, 0 },
};

static const struct bt_node n_rep_big[] = {
	{ BT_REPEAT, "r", NULL,  one_kid, 1, 256 },	/* count is 0..255 */
	{ BT_ACTION, "a", leaf0, NULL,    0, 0 },
};

static const struct bt_node n_rep_neg[] = {
	{ BT_REPEAT, "r", NULL,  one_kid, 1, -1 },
	{ BT_ACTION, "a", leaf0, NULL,    0, 0 },
};

static const struct bt_node n_ok[] = {
	{ BT_SEQUENCE, "s", NULL,  one_kid, 1, 0 },
	{ BT_ACTION,   "a", leaf0, NULL,    0, 0 },
};

static void reject(const struct bt_def *def)
{
	struct bt t;

	assert(bt_init(&t, def, NULL) == BT_EARG);
}

static void test_bad_tables(void)
{
	static const struct bt_def bad[] = {
		{ n_neg,       2, 0 },		/* child index -1 */
		{ n_over,      2, 0 },		/* child index n_nodes */
		{ n_cycle,     2, 0 },		/* points at an ancestor */
		{ n_shared,    2, 0 },		/* one node, two parents */
		{ n_no_tick,   2, 0 },		/* leaf without a tick */
		{ n_leaf_kids, 2, 0 },		/* leaf with children */
		{ n_empty,     2, 0 },		/* composite with none */
		{ n_inv2,      3, 0 },		/* invert with two */
		{ n_rep_big,   2, 0 },		/* count out of range */
		{ n_rep_neg,   2, 0 },
		{ n_ok,        2, 2 },		/* root out of range */
		{ n_ok,        2, -1 },
		{ n_ok,        0, 0 },		/* no nodes */
		{ n_ok,        BT_MAX_NODES + 1, 0 },	/* too many nodes */
		{ NULL,        1, 0 },		/* no table */
	};
	static const struct bt_def good = { n_ok, 2, 0 };
	struct bt t;
	int i;

	for (i = 0; i < ARRAY_SIZE(bad); i++)
		reject(&bad[i]);

	assert(bt_init(NULL, &good, NULL) == BT_EARG);
	assert(bt_init(&t, NULL, NULL) == BT_EARG);

	/* and the sane one still works, with a NULL ctx */
	assert(bt_init(&t, &good, NULL) == BT_OK);
	assert(bt_tick(NULL) == BT_FAILURE);
}

int main(void)
{
	test_sequence();
	test_sequence_resumes();
	test_selector();
	test_selector_is_reactive();
	test_invert_and_condition();
	test_repeat();
	test_running_and_reset();
	test_bad_tables();

	printf("all tests passed\n");

	return 0;
}
