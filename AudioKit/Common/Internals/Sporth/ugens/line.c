#include "plumber.h"

int sporth_line(sporth_stack *stack, void *ud)
{
    plumber_data *pd = ud;
    SPFLOAT trig;
    SPFLOAT out;
    SPFLOAT a;
    SPFLOAT dur;
    SPFLOAT b;
    sp_line *line;

    switch(pd->mode) {
        case PLUMBER_CREATE:

#ifdef DEBUG_MODE
            fprintf(stderr, "line: Creating\n");
#endif

            sp_line_create(&line);
            plumber_add_ugen(pd, SPORTH_LINE, line);
            if(sporth_check_args(stack, "ffff") != SPORTH_OK) {
                fprintf(stderr,"Not enough arguments for line\n");
                stack->error++;
                return PLUMBER_NOTOK;
            }
            sporth_stack_push_float(stack, 0);
            break;
        case PLUMBER_INIT:

#ifdef DEBUG_MODE
            fprintf(stderr, "line: Initialising\n");
#endif

            b = sporth_stack_pop_float(stack);
            dur = sporth_stack_pop_float(stack);
            a = sporth_stack_pop_float(stack);
            trig = sporth_stack_pop_float(stack);
            line = pd->last->ud;
            sp_line_init(pd->sp, line);
            sporth_stack_push_float(stack, 0);
            break;
        case PLUMBER_COMPUTE:
            b = sporth_stack_pop_float(stack);
            dur = sporth_stack_pop_float(stack);
            a = sporth_stack_pop_float(stack);
            trig = sporth_stack_pop_float(stack);
            line = pd->last->ud;
            line->a = a;
            line->dur = dur;
            line->b = b;
            sp_line_compute(pd->sp, line, &trig, &out);
            sporth_stack_push_float(stack, out);
            break;
        case PLUMBER_DESTROY:
            line = pd->last->ud;
            sp_line_destroy(&line);
            break;
        default:
            fprintf(stderr, "line: Uknown mode!\n");
            break;
    }
    return PLUMBER_OK;
}
