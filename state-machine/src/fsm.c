// SPDX-License-Identifier: MIT
/*
 * Table-driven finite state machine. See include/fsm.h for the API.
 */

#include "fsm.h"

#include <stddef.h>

/*
 * First row matching state and event whose guard agrees, NULL if none.
 */
static const struct fsm_transition *fsm_match(const struct fsm *m, int event)
{
	const struct fsm_def *def = m->def;
	int i;

	for (i = 0; i < def->n_transitions; i++) {
		const struct fsm_transition *t = &def->transitions[i];

		if (t->event != event)
			continue;
		if (t->from != m->state && t->from != FSM_ANY)
			continue;
		if (t->guard && !t->guard(m->ctx))
			continue;

		return t;
	}

	return NULL;
}

static void fsm_enter(const struct fsm *m, int state)
{
	const struct fsm_def *def = m->def;

	if (!def->states || state < 0 || state >= def->n_states)
		return;
	if (def->states[state].on_enter)
		def->states[state].on_enter(m->ctx);
}

static void fsm_leave(const struct fsm *m, int state)
{
	const struct fsm_def *def = m->def;

	if (!def->states || state < 0 || state >= def->n_states)
		return;
	if (def->states[state].on_exit)
		def->states[state].on_exit(m->ctx);
}

/**
 * fsm_init() - start a machine in @initial
 * @m: instance to set up
 * @def: definition to run, must outlive @m
 * @initial: state to start in. Its @on_enter hook runs, if any.
 * @ctx: caller data for guards, actions and hooks, may be NULL
 *
 * Return: %FSM_OK, or %FSM_EARG on a bad argument.
 */
int fsm_init(struct fsm *m, const struct fsm_def *def, int initial, void *ctx)
{
	if (!m || !def || initial < 0)
		return FSM_EARG;
	if (def->n_transitions > 0 && !def->transitions)
		return FSM_EARG;
	if (def->states && initial >= def->n_states)
		return FSM_EARG;

	m->def = def;
	m->ctx = ctx;
	m->state = initial;
	fsm_enter(m, initial);

	return FSM_OK;
}

/**
 * fsm_dispatch() - feed an event to the machine
 * @m: running instance
 * @event: event id
 *
 * Fires the first matching, non-refused row: on_exit -> action -> on_enter.
 *
 * Return: %FSM_OK on a transition, %FSM_IGNORED when no row matched (normal,
 * not an error), %FSM_EARG on a bad argument or a bad table entry.
 */
int fsm_dispatch(struct fsm *m, int event)
{
	const struct fsm_transition *t;

	if (!m || !m->def)
		return FSM_EARG;

	t = fsm_match(m, event);
	if (!t)
		return FSM_IGNORED;

	/* internal transition: act, but never leave the state */
	if (t->to == FSM_SAME) {
		if (t->action)
			t->action(m->ctx);
		return FSM_OK;
	}

	if (t->to < 0)
		return FSM_EARG;
	if (m->def->states && t->to >= m->def->n_states)
		return FSM_EARG;

	fsm_leave(m, m->state);
	if (t->action)
		t->action(m->ctx);
	m->state = t->to;
	fsm_enter(m, m->state);

	return FSM_OK;
}

/**
 * fsm_accepts() - would an event do something right now?
 * @m: running instance
 * @event: event id
 *
 * Runs the guards, so keep them free of side effects.
 *
 * Return: 1 if a row would fire, 0 otherwise.
 */
int fsm_accepts(const struct fsm *m, int event)
{
	if (!m || !m->def)
		return 0;

	return fsm_match(m, event) != NULL;
}
