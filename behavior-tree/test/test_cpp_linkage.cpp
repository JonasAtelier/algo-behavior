// SPDX-License-Identifier: GPL-3.0-only
/*
 * The header claims C++ can use it. Prove the extern "C" block works.
 */

#include "bt.h"

#include <cassert>
#include <cstddef>
#include <cstring>

static enum bt_status hungry(void *ctx)
{
	return *(int *)ctx < 30 ? BT_SUCCESS : BT_FAILURE;
}

static enum bt_status eat(void *ctx)
{
	*(int *)ctx += 10;

	return BT_RUNNING;
}

static enum bt_status idle(void *ctx)
{
	(void)ctx;

	return BT_SUCCESS;
}

static const int root_kids[] = { 1, 4 };
static const int eat_kids[] = { 2, 3 };

static const struct bt_node nodes[] = {
	{ BT_SELECTOR,  "sel",    NULL,   root_kids, 2, 0 },
	{ BT_SEQUENCE,  "feed",   NULL,   eat_kids,  2, 0 },
	{ BT_CONDITION, "hungry", hungry, NULL,      0, 0 },
	{ BT_ACTION,    "eat",    eat,    NULL,      0, 0 },
	{ BT_ACTION,    "idle",   idle,   NULL,      0, 0 },
};

static const struct bt_def def = { nodes, 5, 0 };

int main()
{
	int belly = 0;
	struct bt t;

	assert(bt_init(&t, &def, &belly) == BT_OK);
	assert(bt_running(&t) == NULL);

	assert(bt_tick(&t) == BT_RUNNING);
	assert(std::strcmp(bt_running(&t), "eat") == 0);
	assert(belly == 10);

	while (belly < 30)
		bt_tick(&t);

	bt_reset(&t);
	assert(bt_tick(&t) == BT_SUCCESS);	/* full: falls to idle */
	assert(bt_running(&t) == NULL);

	return 0;
}
