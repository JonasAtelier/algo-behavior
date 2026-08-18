/* The header claims C++ can use it. Prove the extern "C" block works. */
#include "fsm.h"

#include <cassert>
#include <cstddef>

enum { LOCKED, UNLOCKED };
enum { EV_COIN, EV_PUSH };

static const fsm_transition table[] = {
	{ LOCKED,   EV_COIN, UNLOCKED, NULL, NULL },
	{ UNLOCKED, EV_PUSH, LOCKED,   NULL, NULL },
};

static const fsm_def def = { table, 2, NULL, 0 };

int main()
{
	fsm m;

	assert(fsm_init(&m, &def, LOCKED, NULL) == FSM_OK);
	assert(fsm_accepts(&m, EV_COIN) == 1);
	assert(fsm_accepts(&m, EV_PUSH) == 0);

	assert(fsm_dispatch(&m, EV_COIN) == FSM_OK);
	assert(m.state == UNLOCKED);
	assert(fsm_dispatch(&m, EV_COIN) == FSM_IGNORED);
	return 0;
}
