<p align="center">
  <img src="assets/states.svg"
       alt="A state machine stepping through three states"
       width="100%">
</p>

# fsm

<p align="center">
  <img alt="platform: portable C99" src="https://img.shields.io/badge/platform-portable%20C99-70e1b2?style=flat-square&labelColor=0d2238">
  <img alt="language: C" src="https://img.shields.io/badge/language-C-70e1b2?style=flat-square&labelColor=0d2238">
  <img alt="license: MIT" src="https://img.shields.io/badge/license-MIT-f2bd68?style=flat-square&labelColor=0d2238">
</p>

A tiny state machine library in C. One header, one `.c` file, no dependencies,
no `malloc` — fine on a microcontroller.

## The idea, in one picture

Your thing is in one **state**. An **event** happens. It moves to another state.

```mermaid
stateDiagram-v2
    direction LR
    [*] --> LOCKED
    LOCKED --> UNLOCKED: coin
    UNLOCKED --> LOCKED: push
```

A turnstile: locked until you pay, then it lets one person through.

You draw the arrows. This library runs them — you just type them out as a table:

| from | on event | go to | and run |
| --- | --- | --- | --- |
| `LOCKED` | `EV_COIN` | `UNLOCKED` | `take_coin` |
| `UNLOCKED` | `EV_PUSH` | `LOCKED` | `let_through` |

which in C is that same table, one row per arrow:

```c
static const struct fsm_transition table[] = {
	{ LOCKED,   EV_COIN, UNLOCKED, NULL, take_coin   },
	{ UNLOCKED, EV_PUSH, LOCKED,   NULL, let_through },
};
```

That is the whole library. Everything below is optional.

## Try it

```sh
make check    # run the tests
make run      # run the turnstile
```

## The whole program

```c
#include "fsm.h"
#include <stdio.h>

/* 1. Name your states and events. Any names you like. */
enum { LOCKED, UNLOCKED, N_STATES };
enum { EV_COIN, EV_PUSH };

/* 2. Say what should happen. ctx is your own data. */
static void take_coin(void *ctx)   { (*(int *)ctx)++; }
static void let_through(void *ctx) { (void)ctx; puts("clunk"); }

/* 3. Draw the arrows. */
static const struct fsm_transition table[] = {
	{ LOCKED,   EV_COIN, UNLOCKED, NULL, take_coin   },
	{ UNLOCKED, EV_PUSH, LOCKED,   NULL, let_through },
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

## What one `fsm_dispatch` does

It walks the table from the top and takes the **first row that fits** — right
state, right event, and a guard that agrees.

![How fsm_dispatch picks a row and runs it](docs/dispatch.svg)

## The three calls

| call | what it does |
| --- | --- |
| `fsm_init(&m, &def, LOCKED, &my_data)` | set it up, start in `LOCKED` |
| `fsm_dispatch(&m, EV_COIN)` | feed it an event |
| `fsm_accepts(&m, EV_COIN)` | `1` if that event would do something |

Read the state with `m.state`. Never assign to it — send an event instead.

`fsm_dispatch` returns `FSM_OK` (it moved), `FSM_IGNORED` (no row fit) or
`FSM_EARG` (you passed something broken).

## A row, slot by slot

![The five slots of a transition row](docs/row.svg)

| write | in slot | means |
| --- | --- | --- |
| `FSM_ANY` | `from` | any state — good for a reset or abort event |
| `FSM_SAME` | `to` | run the action, stay where you are |

`FSM_SAME` is worth a closer look, because it is not the same as an arrow
pointing back at your own state:

![FSM_SAME versus a real self-transition](docs/same-vs-self.svg)

## Two arrows, same event

Give both rows the same event and let a guard pick.

```mermaid
stateDiagram-v2
    direction LR
    IDLE --> RUNNING: start (have_fuel)
    IDLE --> ERROR: start (otherwise)
```

```c
{ IDLE, EV_START, RUNNING, have_fuel, NULL },   /* fuelled? go */
{ IDLE, EV_START, ERROR,   NULL,      beep },   /* otherwise, complain */
```

A guard that says no does not stop the search — it just falls through to the
next row. Rows are read top to bottom and the first fit wins.

## Doing something on arriving or leaving

Optional, one entry per state:

```c
static const struct fsm_state_ops states[N_STATES] = {
	[LOCKED]   = { light_red,   NULL },   /* { on_enter, on_exit } */
	[UNLOCKED] = { light_green, NULL },
};

static const struct fsm_def def = { table, 2, states, N_STATES };
```

They always run in one order — **on_exit → action → on_enter** — the same one
the dispatch picture above shows.

`fsm_init` runs the starting state's `on_enter` too. Don't need hooks? Pass
`NULL, 0` like the turnstile above.

## `ctx` — your own data

![The ctx pointer reaching every callback](docs/ctx.svg)

Put a counter, a socket, a device handle in there. The library never looks
inside it.

## Files

```
include/fsm.h          the API, with comments
src/fsm.c              the implementation, ~130 lines
examples/turnstile.c   a bigger example, with guards and hooks
test/test_fsm.c        the tests
docs/*.svg             the diagrams in this README
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
