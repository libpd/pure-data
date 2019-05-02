/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#include <stdlib.h>
#include <string.h>
#include "m_pd.h"
#include "m_imp.h"
#include "g_canvas.h"   /* just for LB_LOAD */

    /* FIXME no out-of-memory testing yet! */

t_pd *pd_new(t_class *c)
{
    t_pd *x;
    if (!c)
        bug ("pd_new: apparently called before setup routine");
    x = (t_pd *)t_getbytes(c->c_size);
    *x = c;
    if (c->c_patchable)
    {
        ((t_object *)x)->ob_inlet = 0;
        ((t_object *)x)->ob_outlet = 0;
    }
    return (x);
}

void pd_free(t_pd *x)
{
    t_class *c = *x;
    if (c->c_freemethod) (*(t_gotfn)(c->c_freemethod))(x);
    if (c->c_patchable)
    {
        while (((t_object *)x)->ob_outlet)
            outlet_free(((t_object *)x)->ob_outlet);
        while (((t_object *)x)->ob_inlet)
            inlet_free(((t_object *)x)->ob_inlet);
        if (((t_object *)x)->ob_binbuf)
            binbuf_free(((t_object *)x)->ob_binbuf);
    }
    if (c->c_size) t_freebytes(x, c->c_size);
}

void gobj_save(t_gobj *x, t_binbuf *b)
{
    t_class *c = x->g_pd;
    if (c->c_savefn)
        (c->c_savefn)(x, b);
}

/* deal with several objects bound to the same symbol.  If more than one, we
actually bind a collection object to the symbol, which forwards messages sent
to the symbol. */

static t_class *bindlist_class;

typedef struct _bindelem
{
    t_pd *e_who;
    struct _bindelem *e_next;
    int e_ref;
} t_bindelem;

/* CHR: the purpose of the b_current field is to safely unbind symbols
while receiving from them. If we try to unbind the current element,
pd_unbind() detects this and points b_current to the next valid element.
In case this next element also gets unbound, b_current points to the
element after that and so on...
We have to save/restore b_current everytime to make the methods reentrant.
Also, t_bindlist and t_bindelem have to be reference counted to avoid segfaults. */

typedef struct _bindlist
{
    t_pd b_pd;
    t_bindelem *b_list;
    t_bindelem *b_current;
    int b_ref;
} t_bindlist;

static void bindlist_unref(t_bindlist *x)
{
    if (--x->b_ref == 0)
    {
        freebytes(x->b_list, sizeof(t_bindelem));
        pd_free(&x->b_pd);
    }
}

static t_bindelem * bindlist_next(t_bindlist *x, t_bindelem *e)
{
    /* check if current element has been unbound */
    t_bindelem *next = (e != x->b_current) ? x->b_current : e->e_next;
    if (--e->e_ref == 0)
        freebytes(e, sizeof(t_bindelem));
    return next;
}

static void bindlist_bang(t_bindlist *x)
{
    t_bindelem *e = x->b_list, *old;
    x->b_ref++;
    while (e)
    {
        old = x->b_current;
        x->b_current = e;
        e->e_ref++;
        pd_bang(e->e_who);
        e = bindlist_next(x, e);
        x->b_current = old;
    }
    bindlist_unref(x);
}

static void bindlist_float(t_bindlist *x, t_float f)
{
    t_bindelem *e = x->b_list, *old;
    x->b_ref++;
    while (e)
    {
        old = x->b_current;
        x->b_current = e;
        e->e_ref++;
        pd_float(e->e_who, f);
        e = bindlist_next(x, e);
        x->b_current = old;
    }
    bindlist_unref(x);
}

static void bindlist_symbol(t_bindlist *x, t_symbol *s)
{
    t_bindelem *e = x->b_list, *old;
    x->b_ref++;
    while (e)
    {
        old = x->b_current;
        x->b_current = e;
        e->e_ref++;
        pd_symbol(e->e_who, s);
        e = bindlist_next(x, e);
        x->b_current = old;
    }
    bindlist_unref(x);
}

static void bindlist_pointer(t_bindlist *x, t_gpointer *gp)
{
    t_bindelem *e = x->b_list, *old;
    x->b_ref++;
    while (e)
    {
        old = x->b_current;
        x->b_current = e;
        e->e_ref++;
        pd_pointer(e->e_who, gp);
        e = bindlist_next(x, e);
        x->b_current = old;
    }
    bindlist_unref(x);
}

static void bindlist_list(t_bindlist *x, t_symbol *s,
    int argc, t_atom *argv)
{
    t_bindelem *e = x->b_list, *old;
    x->b_ref++;
    while (e)
    {
        old = x->b_current;
        x->b_current = e;
        e->e_ref++;
        pd_list(e->e_who, s, argc, argv);
        e = bindlist_next(x, e);
        x->b_current = old;
    }
    bindlist_unref(x);
}

static void bindlist_anything(t_bindlist *x, t_symbol *s,
    int argc, t_atom *argv)
{
    t_bindelem *e = x->b_list, *old;
    x->b_ref++;
    while (e)
    {
        old = x->b_current;
        x->b_current = e;
        e->e_ref++;
        pd_anything(e->e_who, s, argc, argv);
        e = bindlist_next(x, e);
        x->b_current = old;
    }
    bindlist_unref(x);
}

void m_pd_setup(void)
{
    bindlist_class = class_new(gensym("bindlist"), 0, 0,
        sizeof(t_bindlist), CLASS_PD, 0);
    class_addbang(bindlist_class, bindlist_bang);
    class_addfloat(bindlist_class, (t_method)bindlist_float);
    class_addsymbol(bindlist_class, bindlist_symbol);
    class_addpointer(bindlist_class, bindlist_pointer);
    class_addlist(bindlist_class, bindlist_list);
    class_addanything(bindlist_class, bindlist_anything);
}

void pd_bind(t_pd *x, t_symbol *s)
{
    if (s->s_thing)
    {
        if (*s->s_thing == bindlist_class)
        {
            t_bindlist *b = (t_bindlist *)s->s_thing;
            t_bindelem *e = (t_bindelem *)getbytes(sizeof(t_bindelem));
            e->e_next = b->b_list;
            e->e_who = x;
            e->e_ref = 1;
            b->b_list = e;
        }
        else
        {
            t_bindlist *b = (t_bindlist *)pd_new(bindlist_class);
            t_bindelem *e1 = (t_bindelem *)getbytes(sizeof(t_bindelem));
            t_bindelem *e2 = (t_bindelem *)getbytes(sizeof(t_bindelem));
            b->b_list = e1;
            b->b_current = 0;
            b->b_ref = 1;
            e1->e_who = x;
            e1->e_next = e2;
            e1->e_ref = 1;
            e2->e_who = s->s_thing;
            e2->e_next = 0;
            e2->e_ref = 1;
            s->s_thing = &b->b_pd;
        }
    }
    else s->s_thing = x;
}

void pd_unbind(t_pd *x, t_symbol *s)
{
    if (s->s_thing == x) s->s_thing = 0;
    else if (s->s_thing && *s->s_thing == bindlist_class)
    {
            /* bindlists always have at least two elements... if the number
            goes down to one, get rid of the bindlist and bind the symbol
            straight to the remaining element. */

        t_bindlist *b = (t_bindlist *)s->s_thing;
        t_bindelem *e, *e2;
        if ((e = b->b_list)->e_who == x)
        {
            if (e == b->b_current)
                b->b_current = e->e_next;
            b->b_list = e->e_next;
            if (--e->e_ref == 0)
                freebytes(e, sizeof(t_bindelem));
        }
        else for (e = b->b_list; (e2 = e->e_next); e = e2)
            if (e2->e_who == x)
        {
            if (e2 == b->b_current)
                b->b_current = e2->e_next;
            e->e_next = e2->e_next;
            if (--e2->e_ref == 0)
                freebytes(e2, sizeof(t_bindelem));
            break;
        }
        if (!b->b_list->e_next)
        {
            s->s_thing = b->b_list->e_who;
            if (--b->b_ref == 0)
            {
                freebytes(b->b_list, sizeof(t_bindelem));
                pd_free(&b->b_pd);
            }
        }
    }
    else pd_error(x, "%s: couldn't unbind", s->s_name);
}

t_pd *pd_findbyclass(t_symbol *s, const t_class *c)
{
    t_pd *x = 0;

    if (!s->s_thing) return (0);
    if (*s->s_thing == c) return (s->s_thing);
    if (*s->s_thing == bindlist_class)
    {
        t_bindlist *b = (t_bindlist *)s->s_thing;
        t_bindelem *e, *e2;
        int warned = 0;
        for (e = b->b_list; e; e = e->e_next)
            if (*e->e_who == c)
        {
            if (x && !warned)
            {
                post("warning: %s: multiply defined", s->s_name);
                warned = 1;
            }
            x = e->e_who;
        }
    }
    return x;
}

/* stack for maintaining bindings for the #X symbol during nestable loads.
*/

typedef struct _gstack
{
    t_pd *g_what;
    t_symbol *g_loadingabstraction;
    struct _gstack *g_next;
} t_gstack;

static t_gstack *gstack_head = 0;
static t_pd *lastpopped;
static t_symbol *pd_loadingabstraction;

int pd_setloadingabstraction(t_symbol *sym)
{
    t_gstack *foo = gstack_head;
    for (foo = gstack_head; foo; foo = foo->g_next)
        if (foo->g_loadingabstraction == sym)
            return (1);
    pd_loadingabstraction = sym;
    return (0);
}

void pd_pushsym(t_pd *x)
{
    t_gstack *y = (t_gstack *)t_getbytes(sizeof(*y));
    y->g_what = s__X.s_thing;
    y->g_next = gstack_head;
    y->g_loadingabstraction = pd_loadingabstraction;
    pd_loadingabstraction = 0;
    gstack_head = y;
    s__X.s_thing = x;
}

void pd_popsym(t_pd *x)
{
    if (!gstack_head || s__X.s_thing != x) bug("gstack_pop");
    else
    {
        t_gstack *headwas = gstack_head;
        s__X.s_thing = headwas->g_what;
        gstack_head = headwas->g_next;
        t_freebytes(headwas, sizeof(*headwas));
        lastpopped = x;
    }
}

void pd_doloadbang( void)
{
    if (lastpopped)
        pd_vmess(lastpopped, gensym("loadbang"), "f", LB_LOAD);
    lastpopped = 0;
}

void pd_bang(t_pd *x)
{
    (*(*x)->c_bangmethod)(x);
}

void pd_float(t_pd *x, t_float f)
{
    (*(*x)->c_floatmethod)(x, f);
}

void pd_pointer(t_pd *x, t_gpointer *gp)
{
    (*(*x)->c_pointermethod)(x, gp);
}

void pd_symbol(t_pd *x, t_symbol *s)
{
    (*(*x)->c_symbolmethod)(x, s);
}

void pd_list(t_pd *x, t_symbol *s, int argc, t_atom *argv)
{
    (*(*x)->c_listmethod)(x, &s_list, argc, argv);
}

void pd_anything(t_pd *x, t_symbol *s, int argc, t_atom *argv)
{
    (*(*x)->c_anymethod)(x, s, argc, argv);
}

void mess_init(void);
void obj_init(void);
void conf_init(void);
void glob_init(void);
void garray_init(void);
void ooura_term(void);

void pd_init(void)
{
    static int initted = 0;
    if (initted)
        return;
    initted = 1;
#ifdef PDINSTANCE
    pd_instances = (t_pdinstance **)getbytes(sizeof(*pd_instances));
    pd_instances[0] = &pd_maininstance;
    pd_ninstances = 1;
#endif
    pd_init_systems();
}

EXTERN void pd_init_systems(void) {
    mess_init();
    sys_lock();
    obj_init();
    conf_init();
    glob_init();
    garray_init();
    sys_unlock();
}

EXTERN void pd_term_systems(void) {
    sys_lock();
    /* ooura_term();  oops - this doesn't seem to have got merged from PR  */
    sys_unlock();
}

EXTERN t_canvas *pd_getcanvaslist(void)
{
    return (pd_this->pd_canvaslist);
}

