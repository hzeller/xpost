/* s.c - a segmented, extendable stack */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_STDBOOL_H
# include <stdbool.h>
#else
# ifndef HAVE__BOOL
#  ifdef __cplusplus
typedef bool _Bool;
#  else
#   define _Bool signed char
#  endif
# endif
# define bool _Bool
# define false 0
# define true 1
# define __bool_true_false_are_defined 1
#endif

#include <stdio.h> /* printf */
#include <stdlib.h> /* NULL */

#include "xpost_memory.h" /* mfile mfalloc findtabent */
#include "xpost_object.h" /* object size */
/* typedef long long object; */
#include "xpost_interpreter.h"
#include "xpost_error.h" // stack functions may throw errors
#include "xpost_stack.h"  // double-check prototypes
/*#define STACKSEGSZ 10 */

/*
typedef struct {
    unsigned nextseg;
    unsigned top;
    Xpost_Object data[STACKSEGSZ];
} stack;
*/

/* allocate a stack segment,
   return address */
unsigned initstack(mfile *mem)
{
    unsigned adr = mfalloc(mem, sizeof(stack));
    stack *s = (void *)(mem->base + adr);
    s->nextseg = 0;
    s->top = 0;
    return adr;
}

/* print a dump of the stack */
void dumpstack(mfile *mem,
               unsigned stackadr)
{
    stack *s = (void *)(mem->base + stackadr);
    unsigned i;
    unsigned a;
    a = 0;
    while (1) {
        for (i=0; i < s->top; i++) {
            printf("%d:", a++);
            xpost_object_dump(s->data[i]);
        }
        if (i != STACKSEGSZ) break;
        s = (void *)(mem->base + s->nextseg);
    }
}

/* free a stack segment */
void sfree(mfile *mem,
           unsigned stackadr)
{
    stack *s = (void *)(mem->base + stackadr);
    mtab *tab;
    unsigned e;
    if (s->nextseg) sfree(mem, s->nextseg);
    e = mtalloc(mem, 0, 0, 0); /* allocate entry with 0 size */
    findtabent(mem, &tab, &e);
    tab->tab[e].adr = stackadr; /* insert address */
    tab->tab[e].sz = sizeof(stack); /* insert size */
    /* discard */
}

/* count the stack */
unsigned count(mfile *mem,
               unsigned stackadr)
{
    stack *s = (void *)(mem->base + stackadr);
    unsigned ct = 0;
    while (s->top == STACKSEGSZ) {
        ct += STACKSEGSZ;
        s = (void *)(mem->base + s->nextseg);
    }
    return ct + s->top;
}

/* push an object on the stack */
void push(mfile *mem,
          unsigned stackadr,
          Xpost_Object o)
{
    unsigned newst;
    unsigned stadr;
    stack *s = (void *)(mem->base + stackadr); /* load the stack */

    while (s->top == STACKSEGSZ) { /* find top segment */
        s = (void *)(mem->base + s->nextseg);
    }

    s->data[s->top++] = o; /* push value */

    /* if push maxxed the topmost segment, link a new one */
    if (s->top == STACKSEGSZ) {
        if (s->nextseg == 0) {
            stadr = (unsigned char *)s - mem->base;
            newst = initstack(mem);
            s = (void *)(mem->base + stadr);
            s->nextseg = newst;
        } else {
            s = (void *)(mem->base + s->nextseg);
            s->top = 0;
        }
    }
}


/* index the stack from top-down */
Xpost_Object top(mfile *mem,
           unsigned stacadr,
           integer i)
{
    int cnt = count(mem, stacadr);
    return bot(mem, stacadr, cnt - 1 - i);
}


/* index from top-down and put item there.
   inverse of top. */
void pot (mfile *mem,
          unsigned stacadr,
          integer i,
          Xpost_Object o)
{
    int cnt = count(mem, stacadr);
    tob(mem, stacadr, cnt - 1 - i, o);
}

/* index from bottom up */
Xpost_Object bot (mfile *mem,
            unsigned stacadr,
            integer i)
{
    stack *s = (void *)(mem->base + stacadr);

    /* find desired segment */
    while (i >= STACKSEGSZ) {
        i -= STACKSEGSZ;
        s = (void *)(mem->base + s->nextseg);
    }
    return s->data[i];
}

/* index from bottom-up and put item there.
   inverse of bot. */
void tob (mfile *mem,
          unsigned stacadr,
          integer i,
          Xpost_Object o)
{
    stack *s = (void *)(mem->base + stacadr);

    /* find desired segment */
    while (i >= STACKSEGSZ) {
        i -= STACKSEGSZ;
        if (s->nextseg == 0) error(stackunderflow, "tob");
        s = (void *)(mem->base + s->nextseg);
    }
    s->data[i] = o;
}

/* pop an object off the stack, return object */
Xpost_Object pop (mfile *mem,
            unsigned stackadr)
{
    stack *s = (void *)(mem->base + stackadr);
    stack *p = NULL;

    /* find top segment */
    while (s->top == STACKSEGSZ) {
        p = s;
        s = (void *)(mem->base + s->nextseg);
    }
    if (s->top == 0) {
        if (p != NULL) s = p; /* back up if top is empty */
        else /* error("stack underflow"); */
            return invalid;
    }

    return s->data[--s->top]; /* pop value */
}

#ifdef TESTMODULE_S

#include <stdio.h>
#include <unistd.h>

mfile mem;
unsigned s, t;

/* initialize everything */
void init (void)
{
    pgsz = getpagesize();
    initmem(&mem, "x.mem");
    s = initstack(&mem);
    t = initstack(&mem);
}

void xit (void)
{
    exitmem(&mem);
}

int main()
{
    init();

    printf("\n^test s.c\n");
    printf("test stack by reversing a sequence\n");
    //object a = { .int_.val = 2 }, b = ( .int_.val = 12 }, c = { .int_.val = 0xF00 };
    Xpost_Object a = xpost_cons_int(2);
    Xpost_Object b = xpost_cons_int(12);
    Xpost_Object c = xpost_cons_int(0xF00);
    Xpost_Object x = a, y = b, z = c;
    printf("x = %d, y = %d, z = %d\n", x.int_.val, y.int_.val, z.int_.val);

    push(&mem, s, a);
    push(&mem, s, b);
    push(&mem, s, c);

    x = pop(&mem, s); /* x = c */
    push(&mem, t, x);
    y = pop(&mem, s); /* y = b */
    push(&mem, t, y);
    z = pop(&mem, s); /* z = a */
    push(&mem, t, z);
    printf("x = %d, y = %d, z = %d\n", x.int_.val, y.int_.val, z.int_.val);
    printf("top(0): %d\n", top(&mem, t, 0).int_.val);
    printf("top(1): %d\n", top(&mem, t, 1).int_.val);
    printf("top(2): %d\n", top(&mem, t, 2).int_.val);
    printf("bot(0): %d\n", bot(&mem, t, 0).int_.val);
    printf("bot(1): %d\n", bot(&mem, t, 1).int_.val);
    printf("bot(2): %d\n", bot(&mem, t, 2).int_.val);
    printf("tob(2, 55)\n");
    tob(&mem, t, 2, xpost_cons_int(55));
    printf("pot(1, 37)\n");
    pot(&mem, t, 1, xpost_cons_int(37));

    x = pop(&mem, t); /* x = a */
    y = pop(&mem, t); /* y = b */
    z = pop(&mem, t); /* z = c */
    printf("x = %d, y = %d, z = %d\n", x.int_.val, y.int_.val, z.int_.val);
    //z = pop(&mem, t);

    xit();
    return 0;
}

#endif
