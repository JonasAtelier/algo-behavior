// SPDX-License-Identifier: GPL-3.0-only
/*
 * A patrol robot: walk the route, but dock and charge when the battery gets
 * low. The battery check lives in one place, at the top of the tree, instead
 * of in every state of a state machine.
 */

#include "bt.h"

#include <stdio.h>

#define ARRAY_SIZE(a)	((int)(sizeof(a) / sizeof((a)[0])))

#define LOW		30	/* percent, below this it goes home */
#define WALK		7	/* percent a tick of walking costs */
#define CHARGE_RATE	25	/* percent a tick on the dock gives back */
#define LEG		3	/* ticks of walking between two corners */
#define DOCK_DISTANCE	2	/* ticks of walking to reach the dock */

/* The rows of the table below, in order. */
enum {
	ROOT,		/* selector: dock if you must, otherwise patrol */
	DOCK,		/* sequence: is it low? go home, then charge */
	BATTERY_LOW,
	GO_DOCK,
	CHARGE,
	PATROL,		/* sequence: walk a leg, then turn */
	PATROL_LEG,
	TURN,
	N_NODES
};

struct robot {
	int battery;	/* percent */
	int leg;	/* ticks left in the leg being walked, 0 between legs */
	int to_dock;	/* ticks of walking left to reach the dock */
	int corners;
};

static enum bt_status battery_low(void *ctx)
{
	const struct robot *r = (const struct robot *)ctx;

	return r->battery < LOW ? BT_SUCCESS : BT_FAILURE;
}

static enum bt_status go_dock(void *ctx)
{
	struct robot *r = (struct robot *)ctx;

	if (r->to_dock == 0)
		return BT_SUCCESS;		/* already there */

	r->battery--;
	r->to_dock--;

	return r->to_dock ? BT_RUNNING : BT_SUCCESS;
}

static enum bt_status charge(void *ctx)
{
	struct robot *r = (struct robot *)ctx;

	r->battery += CHARGE_RATE;
	if (r->battery < 100)
		return BT_RUNNING;

	r->battery = 100;
	r->to_dock = DOCK_DISTANCE;		/* it has to walk back out */

	return BT_SUCCESS;
}

static enum bt_status patrol_leg(void *ctx)
{
	struct robot *r = (struct robot *)ctx;

	if (r->leg == 0)
		r->leg = LEG;			/* starting a fresh leg */

	r->battery -= WALK;
	r->leg--;

	return r->leg ? BT_RUNNING : BT_SUCCESS;
}

static enum bt_status turn(void *ctx)
{
	((struct robot *)ctx)->corners++;

	return BT_SUCCESS;
}

static const int root_kids[] = { DOCK, PATROL };
static const int dock_kids[] = { BATTERY_LOW, GO_DOCK, CHARGE };
static const int patrol_kids[] = { PATROL_LEG, TURN };

static const struct bt_node nodes[N_NODES] = {
	/* kind          name            tick         children      n  count */
	{ BT_SELECTOR,  "root",         NULL,        root_kids,   2, 0 },
	{ BT_SEQUENCE,  "dock",         NULL,        dock_kids,   3, 0 },
	{ BT_CONDITION, "battery_low?", battery_low, NULL,        0, 0 },
	{ BT_ACTION,    "go_dock",      go_dock,     NULL,        0, 0 },
	{ BT_ACTION,    "charge",       charge,      NULL,        0, 0 },
	{ BT_SEQUENCE,  "patrol",       NULL,        patrol_kids, 2, 0 },
	{ BT_ACTION,    "patrol_leg",   patrol_leg,  NULL,        0, 0 },
	{ BT_ACTION,    "turn",         turn,        NULL,        0, 0 },
};

static const struct bt_def def = { nodes, N_NODES, ROOT };

static const char *status_name(enum bt_status st)
{
	if (st == BT_SUCCESS)
		return "SUCCESS";
	if (st == BT_FAILURE)
		return "FAILURE";

	return "RUNNING";
}

int main(void)
{
	struct robot r = { 40, 0, DOCK_DISTANCE, 0 };
	struct bt t;
	int i;

	if (bt_init(&t, &def, &r) != BT_OK) {
		puts("bad tree");
		return 1;
	}

	puts("tick  battery  running       result");

	for (i = 1; i <= 20; i++) {
		enum bt_status st = bt_tick(&t);
		const char *who = bt_running(&t);

		printf("%4d  %6d%%  %-12s  %s\n",
		       i, r.battery, who ? who : "-", status_name(st));
	}

	printf("\ncorners turned: %d, battery: %d%%\n", r.corners, r.battery);

	return 0;
}
