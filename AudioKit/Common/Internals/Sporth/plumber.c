#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "plumber.h"

#define SPORTH_UGEN(key, func, macro) int func(sporth_stack *stack, void *ud);
#include "ugens.h"
#undef SPORTH_UGEN

enum {
    SPACE,
    QUOTE,
    LEX_START,
    LEX_FLOAT,
    LEX_FLOAT_DOT,
    LEX_FLOAT_POSTDOT,
    LEX_POS,
    LEX_NEG,
    LEX_FUNC,
    LEX_ERROR
};

int sporth_f_default(sporth_stack *stack, void *ud)
{
    plumber_data *pd = ud;
    switch(pd->mode) {
        case PLUMBER_CREATE:

#ifdef DEBUG_MODE
            fprintf(stderr, "Default user function in create mode.\n");
#endif

            break;
        case PLUMBER_INIT:

#ifdef DEBUG_MODE
            fprintf(stderr, "Default user function in init mode.\n");
#endif
            break;

        case PLUMBER_COMPUTE:
            break;

        case PLUMBER_DESTROY:
#ifdef DEBUG_MODE
            fprintf(stderr, "Default user function in destroy mode.\n");
#endif
            break;

        default:
            fprintf(stderr, "aux (f)unction: unknown mode!\n");
            break;
    }
    return PLUMBER_OK;
}

int plumbing_init(plumbing *pipes)
{
    pipes->last = &pipes->root;
    pipes->npipes = 0;
    return PLUMBER_OK;
}

int plumber_init(plumber_data *plumb)
{
    plumb->mode = PLUMBER_CREATE;
    plumb->current_pipe = 0;
    plumb->ftmap = plumb->ft1;
    plumb->pipes= &plumb->main;
    plumb->tmp = &plumb->main;
    plumbing_init(plumb->pipes);
    plumb->nchan = 1;
    sporth_stack_init(&plumb->sporth.stack);
    plumber_ftmap_delete(plumb, 1);
    plumber_ftmap_init(plumb);
    plumb->seed = (int) time(NULL);
    plumb->fp = NULL;
    int pos;
    for(pos = 0; pos < 16; pos++) plumb->p[pos] = 0;
    for(pos = 0; pos < 16; pos++) plumb->f[pos] = sporth_f_default;
    return PLUMBER_OK;
}

int plumbing_compute(plumber_data *plumb, plumbing *pipes, int mode)
{
    plumb->mode = mode;
    plumber_pipe *pipe = pipes->root.next;
    uint32_t n;
    float *fval;
    char *sval;
    sporth_data *sporth = &plumb->sporth;
    /* swap out the current plumbing */
    plumbing *prev = plumb->pipes;
    plumb->pipes = pipes;
    for(n = 0; n < pipes->npipes; n++) {
        plumb->next = pipe->next;
        switch(pipe->type) {
            case SPORTH_FLOAT:
                fval = pipe->ud;
                if(mode != PLUMBER_DESTROY)
                    sporth_stack_push_float(&sporth->stack, *fval);
                break;
            case SPORTH_STRING:
                sval = pipe->ud;
                if(mode == PLUMBER_INIT) sporth_stack_push_string(&sporth->stack, sval);
                break;
            default:
                plumb->last = pipe;
                sporth->flist[pipe->type - SPORTH_FOFFSET].func(&sporth->stack,
                                                                sporth->flist[pipe->type - SPORTH_FOFFSET].ud);
                break;
        }
        pipe = plumb->next;
    }
    /* re-swap the main pipes */
    plumb->pipes = prev;
    return PLUMBER_OK;
}

int plumber_compute(plumber_data *plumb, int mode)
{
    plumbing_compute(plumb, plumb->pipes, mode);
    return PLUMBER_OK;
}

int plumber_show_pipes(plumber_data *plumb)
{
/* DEPRECATED 
    plumber_pipe *pipe = plumb->root.next, *next;
    uint32_t n;
    float *fval;
    for(n = 0; n < plumb->npipes; n++) {
        next = pipe->next;
        fprintf(stderr,"type = %d size = %ld", pipe->type, (long)pipe->size);
        if(pipe->type == SPORTH_FLOAT) {
            fval = pipe->ud;
            fprintf(stderr," val = %g\n", *fval);
        } else {
            fprintf(stderr,"\n");
        }
        pipe = next;
    }
*/
    return PLUMBER_OK;
}

int plumbing_destroy(plumbing *pipes)
{
#ifdef DEBUG_MODE
    fprintf(stderr, "----Plumber Destroy----\n");
#endif
    uint32_t n;
    plumber_pipe *pipe, *next;
    pipe = pipes->root.next;
    for(n = 0; n < pipes->npipes; n++) {
        next = pipe->next;
#ifdef DEBUG_MODE
        fprintf(stderr, "Pipe %d\ttype %d\n", n, pipe->type);
#endif

        if(pipe->type == SPORTH_FLOAT || pipe->type == SPORTH_STRING)
            free(pipe->ud);
        free(pipe);
        pipe = next;
    }
    return PLUMBER_OK;
}

int plumber_clean(plumber_data *plumb)
{
    plumber_compute(plumb, PLUMBER_DESTROY);
    sporth_htable_destroy(&plumb->sporth.dict);
    plumbing_destroy(plumb->pipes);
    plumber_ftmap_destroy(plumb);
    if(plumb->fp != NULL) fclose(plumb->fp);
    free(plumb->sporth.flist);
    return PLUMBER_OK;
}

int plumbing_add_pipe(plumbing *pipes, plumber_pipe *pipe)
{
    pipes->last->next = pipe;
    pipes->last = pipe;
    pipes->npipes++;
    return PLUMBER_OK;
}

int plumber_add_float(plumber_data *plumb, plumbing *pipes, float num)
{
    plumber_pipe *new = malloc(sizeof(plumber_pipe));

    if(new == NULL) {
        fprintf(stderr,"Memory error\n");
        return PLUMBER_NOTOK;
    }

    new->type = SPORTH_FLOAT;
    new->size = sizeof(SPFLOAT);
    new->ud = malloc(new->size);
    float *val = new->ud;
    *val = num;
    if(new->ud == NULL) {
        fprintf(stderr,"Memory error\n");
        return PLUMBER_NOTOK;
    }

    plumbing_add_pipe(pipes, new);
    return PLUMBER_OK;
}

int plumber_add_string(plumber_data *plumb, plumbing *pipes, const char *str)
{
    plumber_pipe *new = malloc(sizeof(plumber_pipe));

    if(new == NULL) {
        fprintf(stderr,"Memory error\n");
        return PLUMBER_NOTOK;
    }

    new->type = SPORTH_STRING;
    new->size = sizeof(char) * strlen(str) + 1;
    new->ud = malloc(new->size);
    char *sval = new->ud;
    strncpy(sval, str, new->size);
    if(new->ud == NULL) {
        fprintf(stderr,"Memory error\n");
        return PLUMBER_NOTOK;
    }

    plumbing_add_pipe(pipes, new);
    return PLUMBER_OK;
}

int plumber_add_ugen(plumber_data *plumb, uint32_t id, void *ud)
{
    plumber_pipe *new = malloc(sizeof(plumber_pipe));

    if(new == NULL) {
        fprintf(stderr,"Memory error\n");
        return PLUMBER_NOTOK;
    }

    new->type = id;
    new->ud = ud;

    plumbing_add_pipe(plumb->tmp, new);
    return PLUMBER_OK;
}

int plumber_parse_string(plumber_data *plumb, char *str)
{
    return plumbing_parse_string(plumb, plumb->pipes, str);
}

int plumber_lexer(plumber_data *plumb, plumbing *pipes, char *out, uint32_t len)
{
    char *tmp;
    float flt = 0;
    switch(sporth_lexer(out, len)) {
        case SPORTH_FLOAT:
#ifdef DEBUG_MODE
            fprintf(stderr, "%s is a float!\n", out);
#endif
            flt = atof(out);
            plumber_add_float(plumb, pipes, flt);
            sporth_stack_push_float(&plumb->sporth.stack, flt);
            break;
        case SPORTH_STRING:
            tmp = out;
            tmp[len - 1] = '\0';
            tmp++;
#ifdef DEBUG_MODE
            fprintf(stderr, "%s is a string!\n", out);
#endif
            plumber_add_string(plumb, pipes, tmp);
            sporth_stack_push_string(&plumb->sporth.stack, out + 1);
            break;
        case SPORTH_FUNC:
#ifdef DEBUG_MODE
            fprintf(stderr, "%s is a function!\n", out);
#endif
            if(sporth_exec(&plumb->sporth, out) == PLUMBER_NOTOK) {
#ifdef DEBUG_MODE
            fprintf(stderr, "plumber_lexer: error with function %s\n", out);
#endif
                plumb->sporth.stack.error++;
                return PLUMBER_NOTOK;
            }
            break;
        case SPORTH_IGNORE:
            break;
        default:
#ifdef DEBUG_MODE
            fprintf(stderr,"No idea what %s is!\n", out);
#endif
            break;
    }
    return PLUMBER_OK;
}

int plumbing_parse(plumber_data *plumb, plumbing *pipes)
{
    FILE *fp = plumb->fp;
    char *line = NULL;
    size_t length = 0;
    ssize_t read;
    char *out;
    uint32_t pos = 0, len = 0;
    int err = PLUMBER_OK;
    plumb->mode = PLUMBER_CREATE;
    while((read = getline(&line, &length, fp)) != -1) {
        pos = 0;
        len = 0;
        while(pos < read - 1) {
            out = sporth_tokenizer(line, (unsigned int)read - 1, &pos);
            len = (unsigned int)strlen(out);
            err = plumber_lexer(plumb, pipes, out, len);
            free(out);
            if(err == PLUMBER_NOTOK) break;
        }
    }
    free(line);
    return err;

}

int plumbing_parse_string(plumber_data *plumb, plumbing *pipes, char *str)
{
    char *out;
    uint32_t pos = 0, len = 0;
    uint32_t size = (unsigned int)strlen(str);
    int err = PLUMBER_OK;
    pos = 0;
    len = 0;
    plumb->mode = PLUMBER_CREATE;
    while(pos < size) {
        out = sporth_tokenizer(str, size, &pos);
        len = (unsigned int)strlen(out);
        err = plumber_lexer(plumb, pipes, out, len);
        free(out);
        if(err == PLUMBER_NOTOK) break;
    }
    return err;
}

int plumber_parse(plumber_data *plumb)
{
    return plumbing_parse(plumb, plumb->pipes);
}

int plumber_reinit(plumber_data *plumb)
{
    plumbing *newpipes;
    if(plumb->current_pipe == 0) {
        fprintf(stderr, "compiling to alt\n");
        newpipes = &plumb->alt;
        plumb->current_pipe = 1;
        plumb->ftmap = plumb->ft2;
        plumb->ftnew = plumb->ft2;
        plumb->ftold = plumb->ft1;
    } else {
        fprintf(stderr, "compiling to main\n");
        newpipes = &plumb->main;
        plumb->current_pipe = 0;
        plumb->ftmap = plumb->ft1;
        plumb->ftnew = plumb->ft1;
        plumb->ftold = plumb->ft2;
    }

    plumbing_init(newpipes);
    plumb->tmp = newpipes;
    if(plumb->fp != NULL) fseek(plumb->fp, 0L, SEEK_SET);
    sporth_stack_init(&plumb->sporth.stack);
    plumber_ftmap_init(plumb);
    return PLUMBER_OK;
}

int plumber_reparse(plumber_data *plumb) 
{
    if(plumbing_parse(plumb, plumb->tmp) == PLUMBER_OK) {
        fprintf(stderr, "Successful parse...\n");
        plumbing_compute(plumb, plumb->tmp, PLUMBER_INIT);
        fprintf(stderr, "at stack position %d\n",
                plumb->sporth.stack.pos);
        fprintf(stderr, "%d errors\n",
                plumb->sporth.stack.error);
    } else {
       return PLUMBER_NOTOK;
    }
    return PLUMBER_OK;
}

int plumber_reparse_string(plumber_data *plumb, char *str) 
{
    if(plumbing_parse_string(plumb, plumb->tmp, str) == PLUMBER_OK) {
        fprintf(stderr, "Successful parse...\n");
        plumbing_compute(plumb, plumb->tmp, PLUMBER_INIT);
        fprintf(stderr, "at stack position %d\n",
                plumb->sporth.stack.pos);
        fprintf(stderr, "%d errors\n",
                plumb->sporth.stack.error);
    } else {
        return PLUMBER_NOTOK;
    }
    return PLUMBER_OK;
}

int plumber_swap(plumber_data *plumb, int error)
{
    if(error == PLUMBER_NOTOK) {
        fprintf(stderr, "Did not recompile...\n");
        plumbing_compute(plumb, plumb->tmp, PLUMBER_DESTROY);
        plumbing_destroy(plumb->tmp);
        sporth_stack_init(&plumb->sporth.stack);
        plumber_ftmap_destroy(plumb);
        plumb->ftmap = plumb->ftold;
        plumb->current_pipe = (plumb->current_pipe == 0) ? 1 : 0;
        if(plumb->current_pipe == 1) {
#ifdef DEBUG_MODE
            fprintf(stderr, "Reverting to alt\n");
            plumb->pipes = &plumb->alt;
#endif
        } else {
#ifdef DEBUG_MODE
            fprintf(stderr, "Reverting to main\n");
            plumb->pipes = &plumb->main;
#endif

        }
        plumb->sp->pos = 0;
    } else {
        fprintf(stderr, "Recompiling...\n");
        plumbing_compute(plumb, plumb->pipes, PLUMBER_DESTROY);
        plumbing_destroy(plumb->pipes);
        plumb->ftmap = plumb->ftold;
        plumber_ftmap_destroy(plumb);
        plumb->ftmap = plumb->ftnew;
        plumb->pipes = plumb->tmp;
        plumb->sp->pos = 0;
    }
    return PLUMBER_OK;
}

int plumber_recompile(plumber_data *plumb)
{
    int error;
    plumber_reinit(plumb);
    error = plumber_reparse(plumb);
    plumber_swap(plumb, error);
    return PLUMBER_OK;
}

int plumber_recompile_string(plumber_data *plumb, char *str)
{

    int error;
#ifdef DEBUG_MODE
    fprintf(stderr, "** Attempting to compile string '%s' **\n", str);
#endif
    /* file pointer needs to be NULL for reinit to work with strings */
    plumb->fp = NULL;
    plumber_reinit(plumb);
    error = plumber_reparse_string(plumb, str);
    plumber_swap(plumb, error);
    return PLUMBER_OK;
}

int plumber_error(plumber_data *plumb, const char *str)
{
    fprintf(stderr,"%s\n", str);
    exit(1);
}

int plumber_ftmap_init(plumber_data *plumb)
{
    int pos;

    for(pos = 0; pos < 256; pos++) {
        plumb->ftmap[pos].nftbl = 0;
        plumb->ftmap[pos].root.to_delete = plumb->delete_ft;
        plumb->ftmap[pos].last= &plumb->ftmap[pos].root;
    }

    return PLUMBER_OK;
}

int plumber_ftmap_add(plumber_data *plumb, const char *str, sp_ftbl *ft)
{
#ifdef DEBUG_MODE
    fprintf(stderr, "Adding new table %s\n", str + 1);
#endif
    uint32_t pos = sporth_hash(str);
    plumber_ftentry *entry = &plumb->ftmap[pos];
    entry->nftbl++;
    plumber_ftbl *new = malloc(sizeof(plumber_ftbl));
    new->ud = (void *)ft;
    new->type = 1;
    new->to_delete = plumb->delete_ft;
    new->name = malloc(sizeof(char) * strlen(str) + 1);
    strcpy(new->name, str);
    entry->last->next = new;
    entry->last = new;
    return PLUMBER_OK;
}

int plumber_ftmap_search(plumber_data *plumb, const char *str, sp_ftbl **ft)
{
    uint32_t pos = sporth_hash(str);

    uint32_t n;
    plumber_ftentry *entry = &plumb->ftmap[pos];
    plumber_ftbl *ftbl = entry->root.next;
    plumber_ftbl *next;
    fprintf(stderr, "ftmap_search: looking at %d ftbls\n", entry->nftbl);
    for(n = 0; n < entry->nftbl; n++) {
        next = ftbl->next;
#ifdef DEBUG_MODE
    fprintf(stderr, "ftmap_search: comparing %s with %s\n", str, ftbl->name);
#endif
        if(!strcmp(str, ftbl->name)){
            *ft = (sp_ftbl *)ftbl->ud;
            return PLUMBER_OK;
        }
        ftbl = next;
    }
    fprintf(stderr,"Could not find an ftable match for %s.\n", str);
    return PLUMBER_NOTOK;
}

int plumber_ftmap_delete(plumber_data *plumb, char mode)
{
    plumb->delete_ft = mode;
    return PLUMBER_OK;
}
int plumber_ftmap_destroy(plumber_data *plumb)
{
    int pos, n;
    plumber_ftbl *ftbl, *next;
    for(pos = 0; pos < 256; pos++) {
        ftbl = plumb->ftmap[pos].root.next;
        for(n = 0; n < plumb->ftmap[pos].nftbl; n++) {
            next = ftbl->next;
            free(ftbl->name);
            if(ftbl->to_delete) {
                if(ftbl->type == 1) sp_ftbl_destroy((sp_ftbl **)&ftbl->ud);
                else free(ftbl->ud);
            }
            free(ftbl);
            ftbl = next;
        }
    }

    return PLUMBER_OK;
}

int plumber_register(plumber_data *plumb)
{
#define SPORTH_UGEN(key, func, macro) {key, func, plumb},
    sporth_func flist[] = {
#include "ugens.h"
        {NULL, NULL, NULL}
    };
#undef SPORTH_UGEN

    sporth_htable_init(&plumb->sporth.dict);
    sporth_register_func(&plumb->sporth, flist);

    sporth_func *flist2 = malloc(sizeof(sporth_func) * plumb->sporth.nfunc);
    flist2 = memcpy(flist2, flist, sizeof(sporth_func) * plumb->sporth.nfunc);
    plumb->sporth.flist = flist2;
    return PLUMBER_OK;
}

//static uint32_t str2time(plumber_data *pd, char *str)
//{
//    int len = strlen(str);
//    char last = str[len - 1];
//    switch(last) {
//        case 's':
//            str[len - 1] = '\0';
//            return atof(str) * pd->sp->sr;
//            break;
//        default:
//            return atoi(str);
//            break;
//    }
//}
