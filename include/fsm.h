/* fsm.h - portable, table-driven finite state machine.
 *
 * States and events are plain ints, so you bring your own enums and the
 * machine stays universal: protocol handlers, UI screens, motor control,
 * parsers, anything.
 *
 * C99, no dependencies, no malloc, no globals, no I/O.
 */
#ifndef FSM_H
#define FSM_H

#ifdef __cplusplus
extern "C" {
#endif

/* Return codes. */
#define FSM_OK       0    /* transition taken */
#define FSM_IGNORED  1    /* no transition matched - not an error */
#define FSM_EARG    (-1)  /* bad argument or bad table entry */

/* Wildcards. State ids must be >= 0, which leaves these free. */
#define FSM_ANY     (-1)  /* as .from: matches every state */
#define FSM_SAME    (-2)  /* as .to: run the action, stay put, no exit/enter */

/* One row of the transition table.
 *
 *   guard  - optional. Return non-zero to allow. If it refuses, the machine
 *            keeps scanning, so several rows can share an event and branch
 *            on the guard.
 *   action - optional. Runs after on_exit and before on_enter. */
typedef struct {
    int from;
    int event;
    int to;
    int  (*guard)(void *ctx);
    void (*action)(void *ctx);
} fsm_transition;

/* Optional per-state hooks, indexed by state id. */
typedef struct {
    void (*on_enter)(void *ctx);
    void (*on_exit)(void *ctx);
} fsm_state_ops;

/* The machine definition. Const, shareable, safe in ROM. */
typedef struct {
    const fsm_transition *transitions;
    int n_transitions;
    const fsm_state_ops *states;   /* NULL if you need no enter/exit hooks */
    int n_states;                  /* length of states[], 0 when it is NULL */
} fsm_def;

/* A running instance. Read .state; do not write it. */
typedef struct {
    const fsm_def *def;
    void *ctx;
    int state;
} fsm;

/* Start the machine in `initial`. Runs that state's on_enter, if any.
 * ctx is passed to every guard, action and hook; it may be NULL. */
int fsm_init(fsm *m, const fsm_def *def, int initial, void *ctx);

/* Feed an event. Fires the first matching, non-refused transition:
 * on_exit -> action -> on_enter.
 * Returns FSM_OK, FSM_IGNORED (nothing matched) or FSM_EARG. */
int fsm_dispatch(fsm *m, int event);

/* Does an event have a transition available right now? Runs guards, so keep
 * them side-effect free. Returns 1 or 0. */
int fsm_accepts(const fsm *m, int event);

#ifdef __cplusplus
}
#endif
#endif /* FSM_H */
