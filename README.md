# fsm

A tiny finite state machine library in C.

A state machine is just this: **your thing is in one state, an event happens,
it moves to another state.** A door is `OPEN` or `CLOSED`. A download is
`IDLE`, `RUNNING` or `DONE`. Writing that with flags and nested `if`s gets
messy fast. Here you write it as a table and the library runs it.

- One header, one source file. Drop them into any C project.
- No dependencies, no `malloc` — works fine on a microcontroller.
- Works for anything: your states and events are just numbers you name.

## Try it

```sh
make check    # run the tests
make run      # run the turnstile example
```

## A complete example

A turnstile: it stays locked until you put a coin in, then lets one person
through.

```c
#include "fsm.h"
#include <stdio.h>

/* 1. Name your states and events. Any names you like. */
enum { LOCKED, UNLOCKED, N_STATES };
enum { EV_COIN, EV_PUSH };

/* 2. Write what should happen. ctx is your own data. */
static void take_coin(void *ctx)   { (*(int *)ctx)++; }
static void let_through(void *ctx) { (void)ctx; puts("clunk"); }

/* 3. Write the table: from this state, on this event, go to that state. */
static const struct fsm_transition table[] = {
	/* from      event     to         guard  action      */
	{ LOCKED,    EV_COIN,  UNLOCKED,  NULL,  take_coin   },
	{ UNLOCKED,  EV_PUSH,  LOCKED,    NULL,  let_through },
};

static const struct fsm_def def = { table, 2, NULL, 0 };

int main(void)
{
	int coins = 0;
	struct fsm m;

	fsm_init(&m, &def, LOCKED, &coins);   /* start in LOCKED */

	fsm_dispatch(&m, EV_PUSH);            /* locked - nothing happens */
	fsm_dispatch(&m, EV_COIN);            /* now UNLOCKED, coins == 1 */
	fsm_dispatch(&m, EV_PUSH);            /* prints "clunk", back to LOCKED */

	printf("coins=%d state=%d\n", coins, m.state);
	return 0;
}
```

That is the whole idea. Everything below is optional extra.

## The three functions

```c
fsm_init(&m, &def, START_STATE, &my_data);   /* set it up */
fsm_dispatch(&m, EV_SOMETHING);              /* feed it an event */
fsm_accepts(&m, EV_SOMETHING);               /* 1 if that event would do something */
```

Read the current state with `m.state`. Don't assign to it — send an event
instead.

`fsm_dispatch` gives you back:

- `FSM_OK` — it moved
- `FSM_IGNORED` — no rule matched, nothing happened (normal, not an error)
- `FSM_EARG` — you passed something broken

## The table row, slot by slot

```c
{ from, event, to, guard, action }
```

- **from** — the state this rule applies to. Use `FSM_ANY` for "no matter
  which state we are in" — handy for an abort or reset event.
- **event** — the event that triggers it.
- **to** — the state to move to. Use `FSM_SAME` for "run the action but stay
  where you are".
- **guard** — optional. A function returning non-zero for yes. If it says no,
  the library keeps looking at the rows below, so you can have two rules for
  the same event and pick between them:

  ```c
  { IDLE, EV_START, RUNNING, have_fuel, NULL },   /* if fuelled, go */
  { IDLE, EV_START, ERROR,   NULL,      beep },   /* otherwise, complain */
  ```

- **action** — optional. A function to run during the move.

Rules are checked top to bottom and the first one that fits wins.

## Doing something on arriving or leaving

Optional. One entry per state, in state order:

```c
static const struct fsm_state_ops states[N_STATES] = {
	[LOCKED]   = { light_red,   NULL },      /* { on_enter, on_exit } */
	[UNLOCKED] = { light_green, NULL },
};

static const struct fsm_def def = { table, 2, states, N_STATES };
```

Order during a move: **on_exit → action → on_enter**. `fsm_init` runs the
starting state's `on_enter` too.

Don't need them? Pass `NULL, 0` like the example above does.

## What `ctx` is for

The `void *` you give `fsm_init` is passed to every guard, action and hook.
Put your own struct there — a counter, a socket, a device handle. The library
never looks inside it.

## Files

```
include/fsm.h          the API, with comments
src/fsm.c              the implementation, ~130 lines
examples/turnstile.c   a bigger example, with guards and hooks
test/test_fsm.c        the tests
```

Written in Linux kernel C style: tabs, `struct` types (no typedefs),
kernel-doc comments, 80-column lines.

## Limits, so you are not surprised

- Flat machines only — no states inside states.
- No event queue and no timers. You call `fsm_dispatch` yourself, from your
  main loop or an interrupt.
- Rules are searched one by one. Fine up to a few dozen; sort and bisect if
  you ever have hundreds.
- One machine used from two threads needs your own lock. Separate machines
  sharing the same table are fine.
