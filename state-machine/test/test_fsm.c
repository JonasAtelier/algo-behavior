// SPDX-License-Identifier: MIT
/*
 * Self-check: assert-based, no framework. Build with `make check`.
 */

#include "fsm.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>

#define ARRAY_SIZE(a)	((int)(sizeof(a) / sizeof((a)[0])))

enum { A, B, N_STATES };
enum { EV_GO, EV_BACK, EV_SELF, EV_INTERNAL, EV_BRANCH, EV_RESET, EV_NONE };

struct rec {
	char log[64];
	int n;
	int allow;
};

static void put(void *ctx, char c)
{
	struct rec *r = (struct rec *)ctx;

	if (r->n < (int)sizeof(r->log) - 1)
		r->log[r->n++] = c;
	r->log[r->n] = '\0';
}

static void enter_a(void *ctx)
{
	put(ctx, 'A');
}

static void exit_a(void *ctx)
{
	put(ctx, 'a');
}

static void enter_b(void *ctx)
{
	put(ctx, 'B');
}

static void exit_b(void *ctx)
{
	put(ctx, 'b');
}

static void act(void *ctx)
{
	put(ctx, '*');
}

static void act2(void *ctx)
{
	put(ctx, '!');
}

static int allow(void *ctx)
{
	return ((struct rec *)ctx)->allow;
}

static int refuse(void *ctx)
{
	(void)ctx;

	return 0;
}

static const struct fsm_transition table[] = {
	{ A,        EV_GO,       B,         NULL,   act  },
	{ B,        EV_BACK,     A,         NULL,   NULL },
	{ A,        EV_SELF,     A,         NULL,   act  },  /* self */
	{ A,        EV_INTERNAL, FSM_SAME,  NULL,   act  },  /* internal */
	{ A,        EV_BRANCH,   B,         allow,  act  },  /* first choice */
	{ A,        EV_BRANCH,   A,         NULL,   act2 },  /* fallback */
	{ A,        EV_NONE,     B,         refuse, act  },  /* never fires */
	{ FSM_ANY,  EV_RESET,    A,         NULL,   NULL },
};

static const struct fsm_state_ops states[N_STATES] = {
	[A] = { enter_a, exit_a },
	[B] = { enter_b, exit_b },
};

static const struct fsm_def def = {
	table, ARRAY_SIZE(table), states, N_STATES
};

static struct fsm start(struct rec *r)
{
	struct fsm m;

	r->n = 0;
	r->log[0] = '\0';
	r->allow = 1;

	assert(fsm_init(&m, &def, A, r) == FSM_OK);
	assert(m.state == A);
	assert(strcmp(r->log, "A") == 0);	/* init runs on_enter */

	return m;
}

static void test_basic(void)
{
	struct rec r;
	struct fsm m = start(&r);

	assert(fsm_dispatch(&m, EV_GO) == FSM_OK);
	assert(m.state == B);
	assert(strcmp(r.log, "Aa*B") == 0);	/* exit -> action -> enter */

	assert(fsm_dispatch(&m, EV_BACK) == FSM_OK);
	assert(m.state == A);
	assert(strcmp(r.log, "Aa*BbA") == 0);
}

static void test_ignored(void)
{
	struct rec r;
	struct fsm m = start(&r);

	assert(fsm_dispatch(&m, EV_BACK) == FSM_IGNORED); /* wrong state */
	assert(fsm_dispatch(&m, 999) == FSM_IGNORED); /* unknown event */
	assert(m.state == A);
	assert(strcmp(r.log, "A") == 0); /* nothing ran */
}

static void test_self_vs_internal(void)
{
	struct rec r;
	struct fsm m = start(&r);

	assert(fsm_dispatch(&m, EV_SELF) == FSM_OK);
	assert(strcmp(r.log, "Aa*A") == 0); /* self-transition cycles hooks */
	assert(m.state == A);

	r.n = 0;
	r.log[0] = '\0';

	assert(fsm_dispatch(&m, EV_INTERNAL) == FSM_OK);
	assert(strcmp(r.log, "*") == 0);	/* internal: action only */
	assert(m.state == A);
}

static void test_guards(void)
{
	struct rec r;
	struct fsm m = start(&r);

	assert(fsm_dispatch(&m, EV_BRANCH) == FSM_OK);
	assert(m.state == B); /* guard allowed the first row */

	m = start(&r);
	r.allow = 0;

	assert(fsm_dispatch(&m, EV_BRANCH) == FSM_OK);
	assert(m.state == A);			/* refused -> fell through */
	assert(strcmp(r.log, "Aa!A") == 0);

	assert(fsm_dispatch(&m, EV_NONE) == FSM_IGNORED); /* only row refuses */
}

static void test_wildcard(void)
{
	struct rec r;
	struct fsm m = start(&r);

	fsm_dispatch(&m, EV_GO);
	assert(m.state == B);
	assert(fsm_dispatch(&m, EV_RESET) == FSM_OK);
	assert(m.state == A);

	m = start(&r);
	assert(fsm_dispatch(&m, EV_RESET) == FSM_OK);	/* also from A */
	assert(m.state == A);
}

static void test_accepts(void)
{
	struct rec r;
	struct fsm m = start(&r);

	assert(fsm_accepts(&m, EV_GO) == 1);
	assert(fsm_accepts(&m, EV_BACK) == 0);
	assert(fsm_accepts(&m, EV_NONE) == 0);		/* guard refuses */
	assert(fsm_accepts(NULL, EV_GO) == 0);
}

static void test_no_hooks(void)
{
	static const struct fsm_transition t[] = {
		{ A, EV_GO, B, NULL, NULL }
	};
	static const struct fsm_def bare = { t, 1, NULL, 0 };
	struct fsm m;

	assert(fsm_init(&m, &bare, A, NULL) == FSM_OK);	/* states may be NULL */
	assert(fsm_dispatch(&m, EV_GO) == FSM_OK);
	assert(m.state == B);
}

static void test_bad_args(void)
{
	struct fsm m;

	assert(fsm_init(NULL, &def, A, NULL) == FSM_EARG);
	assert(fsm_init(&m, NULL, A, NULL) == FSM_EARG);
	assert(fsm_init(&m, &def, -1, NULL) == FSM_EARG);
	/* out of range */
	assert(fsm_init(&m, &def, N_STATES, NULL) == FSM_EARG);
	assert(fsm_dispatch(NULL, EV_GO) == FSM_EARG);
}

static void test_bad_table(void)
{
	static const struct fsm_transition t[] = {
		{ A, EV_GO, 99, NULL, NULL }
	};
	static const struct fsm_def broken = { t, 1, states, N_STATES };
	struct fsm m;
	struct rec r = { { 0 }, 0, 1 };

	assert(fsm_init(&m, &broken, A, &r) == FSM_OK);
	assert(fsm_dispatch(&m, EV_GO) == FSM_EARG); /* target out of range */
	assert(m.state == A); /* and it did not move */
}

int main(void)
{
	test_basic();
	test_ignored();
	test_self_vs_internal();
	test_guards();
	test_wildcard();
	test_accepts();
	test_no_hooks();
	test_bad_args();
	test_bad_table();

	printf("all tests passed\n");

	return 0;
}
