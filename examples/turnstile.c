/* The canonical turnstile: locked until it is paid, one pass per coin. */
#include "fsm.h"
#include <stdio.h>

enum { LOCKED, UNLOCKED, N_STATES };
enum { EV_COIN, EV_PUSH };

typedef struct {
    int coins;
    int passes;
    int refused;
} till;

static void take_coin(void *ctx)  { ((till *)ctx)->coins++;   }
static void let_through(void *ctx){ ((till *)ctx)->passes++;  }
static void refuse(void *ctx)     { ((till *)ctx)->refused++; }

static void on_lock(void *ctx)    { (void)ctx; puts("  -> locked");   }
static void on_unlock(void *ctx)  { (void)ctx; puts("  -> unlocked"); }

static const fsm_transition table[] = {
    /* from      event     to         guard  action      */
    { LOCKED,    EV_COIN,  UNLOCKED,  NULL,  take_coin   },
    { LOCKED,    EV_PUSH,  FSM_SAME,  NULL,  refuse      },  /* act, stay put */
    { UNLOCKED,  EV_PUSH,  LOCKED,    NULL,  let_through },
    { UNLOCKED,  EV_COIN,  FSM_SAME,  NULL,  take_coin   },
};

static const fsm_state_ops states[N_STATES] = {
    [LOCKED]   = { on_lock,   NULL },
    [UNLOCKED] = { on_unlock, NULL },
};

static const fsm_def def = {
    table, (int)(sizeof table / sizeof table[0]),
    states, N_STATES
};

int main(void)
{
    static const int script[] = { EV_PUSH, EV_COIN, EV_COIN, EV_PUSH, EV_PUSH };
    till t = { 0, 0, 0 };
    fsm m;
    size_t i;

    fsm_init(&m, &def, LOCKED, &t);

    for (i = 0; i < sizeof script / sizeof script[0]; i++) {
        printf("%s", script[i] == EV_COIN ? "coin" : "push");
        printf(" (accepted: %d)\n", fsm_accepts(&m, script[i]));
        fsm_dispatch(&m, script[i]);
    }

    printf("coins=%d passes=%d refused=%d final=%s\n",
           t.coins, t.passes, t.refused, m.state == LOCKED ? "LOCKED" : "UNLOCKED");
    return 0;
}
