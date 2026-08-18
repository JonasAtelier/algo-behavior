# fsm

Portable, table-driven finite state machine in one C file.

- C99, no dependencies at all — not even `libm`
- No `malloc`, no globals, no I/O — safe on MCUs and in interrupt context
- The machine definition is `const`, so it lives in ROM and can be shared by
  many instances
- Universal: states and events are plain `int`, context is `void *`. The
  library never needs to know what a state *means* — protocol handlers, UI
  screens, motor control, parsers, retry logic, all the same code.

## Build

```sh
make          # libfsm.a + example + tests
make check    # run the self-check
make run      # run the turnstile example
```

Or just compile `src/fsm.c` with your own build.

## The API

Three functions, four types.

### 1. Bring your own enums

```c
enum { LOCKED, UNLOCKED, N_STATES };   /* state ids must start at 0 */
enum { EV_COIN, EV_PUSH };
```

### 2. Write the table

```c
static const fsm_transition table[] = {
    /* from      event     to         guard  action      */
    { LOCKED,    EV_COIN,  UNLOCKED,  NULL,  take_coin   },
    { LOCKED,    EV_PUSH,  FSM_SAME,  NULL,  refuse      },
    { UNLOCKED,  EV_PUSH,  LOCKED,    NULL,  let_through },
    { FSM_ANY,   EV_ABORT, LOCKED,    NULL,  NULL        },
};
```

The whole machine is readable in one screen, which is the point of a table
instead of a `switch` nest.

| slot | meaning |
|------|---------|
| `from`   | a state id, or `FSM_ANY` to match every state |
| `to`     | a state id, or `FSM_SAME` for an internal transition (run the action, stay put, no exit/enter) |
| `guard`  | optional `int (*)(void *ctx)` — return non-zero to allow |
| `action` | optional `void (*)(void *ctx)` — runs between exit and enter |

Rows are scanned top to bottom and the first match wins. A guard that refuses
does not stop the scan, so several rows can share an event and branch on
their guards — that is how you express conditional transitions without
writing any dispatch logic.

### 3. Optional entry/exit hooks

```c
static const fsm_state_ops states[N_STATES] = {
    [LOCKED]   = { on_lock,   NULL },     /* { on_enter, on_exit } */
    [UNLOCKED] = { on_unlock, NULL },
};
```

Indexed by state id. Pass `NULL` and `0` if you do not need them.

### 4. Run it

```c
static const fsm_def def = { table, 4, states, N_STATES };

fsm m;
fsm_init(&m, &def, LOCKED, &my_context);   /* runs LOCKED's on_enter */

fsm_dispatch(&m, EV_COIN);                 /* feed events */

if (fsm_accepts(&m, EV_PUSH)) { /* ... grey out the button, etc. */ }

int now = m.state;                         /* read it; do not write it */
```

`ctx` is handed to every guard, action and hook. It is the only state the
library carries for you — everything else is yours.

### Return codes

| code | meaning |
|------|---------|
| `FSM_OK` (0)      | transition taken |
| `FSM_IGNORED` (1) | nothing matched — usually fine, not an error |
| `FSM_EARG` (-1)   | bad argument, or a table row pointing at a state that does not exist |

### Order of operations

A transition runs `on_exit` → `action` → `on_enter`, then the state changes are
visible. A transition to the state you are already in *does* cycle the hooks;
use `FSM_SAME` when you want the action without them.

## Layout

```
include/fsm.h          the whole public API
src/fsm.c              the whole implementation (~80 lines)
examples/turnstile.c   coin-operated turnstile, with guards and hooks
test/test_fsm.c        assert-based self-check
```

## Scope

Flat machines only. No hierarchical/nested states, no orthogonal regions, no
built-in event queue, no timers — feed it from your own loop or ISR. Table
lookup is a linear scan, which is the right answer up to a few dozen rows; if
you have hundreds, sort by state and bisect.

Not thread-safe by itself: one `fsm` touched from two threads needs your own
lock. Separate `fsm` instances sharing one `const fsm_def` are fine.
