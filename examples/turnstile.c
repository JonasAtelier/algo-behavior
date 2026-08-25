// SPDX-License-Identifier: MIT
/*
 * The canonical turnstile: locked until it is paid, one pass per coin.
 */

#include "fsm.h"

#include <stdio.h>

#define ARRAY_SIZE(a)	((int)(sizeof(a) / sizeof((a)[0])))

enum { LOCKED, UNLOCKED, N_STATES };
enum { EV_COIN, EV_PUSH };

struct till {
	int coins;
	int passes;
	int refused;
};

static void take_coin(void *ctx)
{
	((struct till *)ctx)->coins++;
}

static void let_through(void *ctx)
{
	((struct till *)ctx)->passes++;
}

static void refuse(void *ctx)
{
	((struct till *)ctx)->refused++;
}

static void on_lock(void *ctx)
{
	(void)ctx;
	puts("  -> locked");
}

static void on_unlock(void *ctx)
{
	(void)ctx;
	puts("  -> unlocked");
}

static const struct fsm_transition table[] = {
	/* from       event     to         guard  action      */
	{ LOCKED,     EV_COIN,  UNLOCKED,  NULL,  take_coin   },
	{ LOCKED,     EV_PUSH,  FSM_SAME,  NULL,  refuse      },
	{ UNLOCKED,   EV_PUSH,  LOCKED,    NULL,  let_through },
	{ UNLOCKED,   EV_COIN,  FSM_SAME,  NULL,  take_coin   },
};

static const struct fsm_state_ops states[N_STATES] = {
	[LOCKED]   = { on_lock,   NULL },
	[UNLOCKED] = { on_unlock, NULL },
};

static const struct fsm_def def = {
	table, ARRAY_SIZE(table), states, N_STATES
};

int main(void)
{
	static const int script[] = {
		EV_PUSH, EV_COIN, EV_COIN, EV_PUSH, EV_PUSH
	};
	struct till t = { 0, 0, 0 };
	struct fsm m;
	int i;

	fsm_init(&m, &def, LOCKED, &t);

	for (i = 0; i < ARRAY_SIZE(script); i++) {
		printf("%s", script[i] == EV_COIN ? "coin" : "push");
		printf(" (accepted: %d)\n", fsm_accepts(&m, script[i]));
		fsm_dispatch(&m, script[i]);
	}

	printf("coins=%d passes=%d refused=%d final=%s\n",
	       t.coins, t.passes, t.refused,
	       m.state == LOCKED ? "LOCKED" : "UNLOCKED");

	return 0;
}
