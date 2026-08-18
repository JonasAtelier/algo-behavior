#include "fsm.h"
#include <stddef.h>

/* First row that matches state+event and whose guard agrees. NULL if none. */
static const fsm_transition *match(const fsm *m, int event)
{
    const fsm_def *d = m->def;
    int i;

    for (i = 0; i < d->n_transitions; i++) {
        const fsm_transition *t = &d->transitions[i];
        if (t->event != event) continue;
        if (t->from != m->state && t->from != FSM_ANY) continue;
        if (t->guard && !t->guard(m->ctx)) continue;
        return t;
    }
    return NULL;
}

static void enter(const fsm *m, int s)
{
    const fsm_def *d = m->def;
    if (d->states && s >= 0 && s < d->n_states && d->states[s].on_enter)
        d->states[s].on_enter(m->ctx);
}

static void leave(const fsm *m, int s)
{
    const fsm_def *d = m->def;
    if (d->states && s >= 0 && s < d->n_states && d->states[s].on_exit)
        d->states[s].on_exit(m->ctx);
}

int fsm_init(fsm *m, const fsm_def *def, int initial, void *ctx)
{
    if (!m || !def || initial < 0) return FSM_EARG;
    if (def->n_transitions > 0 && !def->transitions) return FSM_EARG;
    if (def->states && initial >= def->n_states) return FSM_EARG;

    m->def = def;
    m->ctx = ctx;
    m->state = initial;
    enter(m, initial);
    return FSM_OK;
}

int fsm_dispatch(fsm *m, int event)
{
    const fsm_transition *t;

    if (!m || !m->def) return FSM_EARG;

    t = match(m, event);
    if (!t) return FSM_IGNORED;

    /* internal transition: act, but never leave the state */
    if (t->to == FSM_SAME) {
        if (t->action) t->action(m->ctx);
        return FSM_OK;
    }
    if (t->to < 0) return FSM_EARG;
    if (m->def->states && t->to >= m->def->n_states) return FSM_EARG;

    leave(m, m->state);
    if (t->action) t->action(m->ctx);
    m->state = t->to;
    enter(m, m->state);
    return FSM_OK;
}

int fsm_accepts(const fsm *m, int event)
{
    if (!m || !m->def) return 0;
    return match(m, event) != NULL;
}
