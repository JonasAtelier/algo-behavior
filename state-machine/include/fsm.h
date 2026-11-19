/* SPDX-License-Identifier: GPL-3.0-only */
/*
 * Portable, table-driven finite state machine.
 *
 * States and events are plain ints, so the caller brings its own enums and
 * the machine stays universal: protocol handlers, UI screens, motor control,
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
#define FSM_OK		0	/* transition taken */
#define FSM_IGNORED	1	/* no transition matched - not an error */
#define FSM_EARG	(-1)	/* bad argument or bad table entry */

/* Wildcards. State ids must be >= 0, which leaves these free. */
#define FSM_ANY		(-1)	/* as .from: matches every state */
#define FSM_SAME	(-2)	/* as .to: act, stay put, no exit/enter */

/**
 * struct fsm_transition - one row of the transition table
 * @from: state this row applies to, or %FSM_ANY for every state
 * @event: event that fires it
 * @to: state to move to, or %FSM_SAME to act without leaving
 * @guard: optional, non-zero to allow. On refusal the scan continues, so
 *	several rows may share an event and branch on their guards.
 * @action: optional, runs after @on_exit and before @on_enter
 */
struct fsm_transition {
	int from;
	int event;
	int to;
	int (*guard)(void *ctx);
	void (*action)(void *ctx);
};

/**
 * struct fsm_state_ops - optional per-state hooks, indexed by state id
 * @on_enter: called on arriving in the state
 * @on_exit: called on leaving it
 */
struct fsm_state_ops {
	void (*on_enter)(void *ctx);
	void (*on_exit)(void *ctx);
};

/**
 * struct fsm_def - the machine definition, const and shareable, ROM-safe
 * @transitions: the table, scanned top to bottom
 * @n_transitions: number of rows in @transitions
 * @states: per-state hooks, NULL when no state needs them
 * @n_states: length of @states, 0 when it is NULL
 */
struct fsm_def {
	const struct fsm_transition *transitions;
	int n_transitions;
	const struct fsm_state_ops *states;
	int n_states;
};

/**
 * struct fsm - a running instance
 * @def: the definition it runs
 * @ctx: caller data handed to every guard, action and hook
 * @state: current state id. Read it; never assign to it.
 */
struct fsm {
	const struct fsm_def *def;
	void *ctx;
	int state;
};

int fsm_init(struct fsm *m, const struct fsm_def *def, int initial, void *ctx);
int fsm_dispatch(struct fsm *m, int event);
int fsm_accepts(const struct fsm *m, int event);

#ifdef __cplusplus
}
#endif
#endif /* FSM_H */
