# bt

<p align="center">
  <img alt="platform: portable C99" src="https://img.shields.io/badge/platform-portable%20C99-70e1b2?style=flat-square&labelColor=0d2238">
  <img alt="language: C" src="https://img.shields.io/badge/language-C-70e1b2?style=flat-square&labelColor=0d2238">
  <img alt="license: MIT" src="https://img.shields.io/badge/license-MIT-f2bd68?style=flat-square&labelColor=0d2238">
</p>

A tiny behaviour tree library in C. One header, one `.c` file, no
dependencies, no `malloc` — fine on a microcontroller.

## When a state machine stops scaling

You wrote a state machine for the robot, and then the battery went flat, so
you added a low-battery check to `PATROLLING`. Then to `TURNING`, and to
`SCANNING`, and to every state you add from now on. Each one needs its own
arrow back to `DOCKING`, and each one has to remember where it was.

A behaviour tree turns that inside out. You write the check **once**, at the
top, and the tree re-asks it every tick before anything below it gets a turn.
Priority is where the node sits, not a rule copied into every state.

```
selector          try each in turn, first one that works wins
├── sequence      is the battery low? then go home and charge
│   ├── battery_low?
│   ├── go_dock
│   └── charge
└── sequence      otherwise, walk a leg and turn the corner
    ├── patrol_leg
    └── turn
```

## Try it

```sh
make check    # run the tests
make run      # run the patrol robot for 20 ticks
```

`make run` prints the robot breaking off its patrol to dock:

```
tick  battery  running       result
   1      33%  patrol_leg    RUNNING
   2      26%  patrol_leg    RUNNING
   3      25%  go_dock       RUNNING     <- battery check took over
   4      49%  charge        RUNNING
```

## The six node kinds

Every node reports one of three things: `BT_SUCCESS`, `BT_FAILURE`, or
`BT_RUNNING` — not finished, tick me again.

| kind | children | what it does |
| --- | --- | --- |
| `BT_ACTION` | none | your function. Any of the three answers. |
| `BT_CONDITION` | none | your function. A question, so `RUNNING` counts as `FAILURE`. |
| `BT_SEQUENCE` | many | **and**: each child in turn until one fails or runs. All succeed → `SUCCESS`. |
| `BT_SELECTOR` | many | **or**: each child in turn until one succeeds or runs. All fail → `FAILURE`. |
| `BT_INVERT` | one | swaps `SUCCESS` and `FAILURE`, passes `RUNNING` through. |
| `BT_REPEAT` | one | ticks its child again after each `SUCCESS`, up to `count` times. |

Actions and conditions are the only rows where you write code. The other four
just decide who gets ticked.

## Writing a tree

The tree is a plain `const` array, one row per node, children named by index —
the same shape as an `fsm` transition table, and just as ROM-friendly.

```c
/* Name the rows, so the indices below read as names. */
enum { ROOT, DOCK, BATTERY_LOW, GO_DOCK, CHARGE, PATROL, LEG, TURN, N_NODES };

static const int root_kids[]   = { DOCK, PATROL };
static const int dock_kids[]   = { BATTERY_LOW, GO_DOCK, CHARGE };
static const int patrol_kids[] = { LEG, TURN };

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
```

A leaf is one function taking your `ctx`:

```c
static enum bt_status battery_low(void *ctx)
{
	return ((struct robot *)ctx)->battery < 30 ? BT_SUCCESS : BT_FAILURE;
}
```

`bt_init` reads the whole table once and refuses a broken one: an index out
of range, a cycle, a node with two parents, a leaf without a function, a
composite without children. After that, ticking checks nothing.

## Ticking it

```c
struct robot r = { 40, 0, 2, 0 };
struct bt t;

bt_init(&t, &def, &r);

for (;;) {
	bt_tick(&t);                  /* one pass over the tree */
	printf("%s\n", bt_running(&t) ? bt_running(&t) : "idle");
}
```

One `bt_tick` is one pass from the root. It is not a loop and it does not
block: an action that needs three ticks to finish returns `BT_RUNNING` twice
and `BT_SUCCESS` on the third. Call it from your main loop, as often as you
like.

That leaves the question of what happens next tick, and the two composites
answer it differently:

> **A sequence remembers where it was; a selector asks again every tick, and
> resets the branch it abandons.**

So the dock sequence, once it is charging, keeps charging — it does not
re-ask `battery_low?` half way through. But the selector above it re-runs the
whole dock branch every tick, which is exactly how a low battery interrupts a
patrol leg that was in the middle of running. The abandoned patrol sequence
goes back to its first child, so it cannot resume from the middle an hour
later.

`bt_reset(&t)` throws all of that away: the next tick starts from the top.
Your `ctx` is yours, and is never touched.

## The functions

| call | what it does |
| --- | --- |
| `bt_init(&t, &def, &my_data)` | check the table, start with nothing running |
| `bt_tick(&t)` | one pass from the root, returns the root's status |
| `bt_running(&t)` | name of the leaf that returned `BT_RUNNING`, or `NULL` |
| `bt_reset(&t)` | forget every running child |

`bt_init` returns `BT_OK` or `BT_EARG`. `bt_tick` returns a status, so a
broken argument comes back as `BT_FAILURE`.

## What it does not do

- **No parallel node.** One leaf runs at a time. Two things at once means two
  trees, ticked one after the other.
- **No blackboard.** `ctx` is your blackboard — a struct you define, handed to
  every leaf. The library never looks inside it.
- **No decorators beyond invert and repeat.** No cooldowns, no timeouts, no
  retry limits. They are three lines in an action that owns a counter in your
  `ctx`.
- **No dynamic trees.** The table is `const` at compile time. That is what
  makes it shareable and ROM-safe.
- Up to `BT_MAX_NODES` (64) nodes, and a repeat `count` of 0 to 255.
- One tree ticked from two threads needs your own lock. Separate instances
  sharing one table are fine.

## Files

```
include/bt.h        the API, with comments
src/bt.c            the implementation, ~260 lines
examples/patrol.c   the robot above, 20 ticks of it
test/test_bt.c      the tests
```

Written in Linux kernel C style: tabs, `struct` types (no typedefs),
kernel-doc comments, 80-column lines.

## License

MIT. See [LICENSE](LICENSE).
