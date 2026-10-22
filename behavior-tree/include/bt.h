/* SPDX-License-Identifier: MIT */
/*
 * Portable, table-driven behaviour tree.
 *
 * The tree is a const table of nodes, one row each, children named by index,
 * so it lives in ROM and several instances can share it. The leaves are
 * plain functions you write; the tree only decides who gets ticked and in
 * what order.
 *
 * The task layer above a state machine: instead of adding "and if the
 * battery is low" to every state, you write that check once, at the top.
 *
 * C99, no dependencies, no malloc, no globals, no I/O.
 */
#ifndef BT_H
#define BT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Biggest tree, and the recursion depth limit that goes with it. */
#define BT_MAX_NODES	64

/* Return codes. */
#define BT_OK		0
#define BT_EARG		(-1)	/* bad argument, or a bad table */

/**
 * enum bt_status - what a node reports
 * @BT_SUCCESS: finished, it worked
 * @BT_FAILURE: finished, it did not work
 * @BT_RUNNING: not finished, tick it again next time
 */
enum bt_status {
	BT_SUCCESS = 0,
	BT_FAILURE = 1,
	BT_RUNNING = 2
};

/**
 * enum bt_kind - what a node is
 * @BT_ACTION: leaf: tick() returns SUCCESS, FAILURE or RUNNING
 * @BT_CONDITION: leaf: tick() returns SUCCESS or FAILURE, RUNNING counts
 *	as FAILURE
 * @BT_SEQUENCE: children in order until one fails or runs, all succeed ->
 *	SUCCESS. Remembers where it was.
 * @BT_SELECTOR: children in order until one succeeds or runs, all fail ->
 *	FAILURE. Starts from the first child every tick.
 * @BT_INVERT: one child, swaps SUCCESS and FAILURE, passes RUNNING through
 * @BT_REPEAT: one child, ticks it again after SUCCESS up to @count times per
 *	tick. RUNNING passes through, FAILURE stops it, @count 0 means forever
 *	(one child tick per tick, always RUNNING).
 */
enum bt_kind {
	BT_ACTION,
	BT_CONDITION,
	BT_SEQUENCE,
	BT_SELECTOR,
	BT_INVERT,
	BT_REPEAT
};

/**
 * struct bt_node - one row of the tree table
 * @kind: what this node is
 * @name: for your logs, may be NULL
 * @tick: leaf function, NULL on composites
 * @children: indices into the table, NULL on leaves
 * @n_children: length of @children, 1 for %BT_INVERT and %BT_REPEAT
 * @count: %BT_REPEAT only, 0 to 255, 0 means forever
 */
struct bt_node {
	enum bt_kind kind;
	const char *name;
	enum bt_status (*tick)(void *ctx);
	const int *children;
	int n_children;
	int count;
};

/**
 * struct bt_def - the tree definition, const and shareable, ROM-safe
 * @nodes: the table
 * @n_nodes: number of rows, 1 to %BT_MAX_NODES
 * @root: index of the node the tick starts at
 */
struct bt_def {
	const struct bt_node *nodes;
	int n_nodes;
	int root;
};

/**
 * struct bt - a running instance
 * @def: the tree it runs
 * @ctx: caller data handed to every leaf function
 * @cursor: per-node scratch, one byte each. A sequence keeps the child it
 *	resumes at, a selector the child that ran last tick plus one (0 for
 *	none), a repeat the iterations it has done. Yours to read, never
 *	to write.
 * @running: index of the leaf that returned %BT_RUNNING on the last tick,
 *	or -1
 */
struct bt {
	const struct bt_def *def;
	void *ctx;
	unsigned char cursor[BT_MAX_NODES];
	int running;
};

int bt_init(struct bt *t, const struct bt_def *def, void *ctx);
enum bt_status bt_tick(struct bt *t);
void bt_reset(struct bt *t);
const char *bt_running(const struct bt *t);

#ifdef __cplusplus
}
#endif
#endif /* BT_H */
