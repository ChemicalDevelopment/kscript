/* os/thread.c - 'os.thread' type
 *
 * 
 * @author: Cade Brown <cade@kscript.org>
 */
#include <ks/impl.h>

#define T_NAME "os.thread"

#ifndef KS_HAVE_pthreads
#warning Building kscript without pthread support, so threading is disabled
#endif


/* Internals */


ksos_thread ksg_main_thread = NULL;


/* Set of threads currently active */
static ks_set active_threads = NULL;

/* Thread-local key which we store the thread instance on */
static pthread_key_t this_thread_key;


#ifdef KS_HAVE_pthreads

/* Initialize and begin pthreads-specific */
static void* init_thread_pthreads(void* _self) {
    ksos_thread self = (ksos_thread)_self;

    pthread_setspecific(this_thread_key, (void*)self);
    KS_GIL_LOCK();
    self->is_active = true;
    self->is_queue = false;
    
    /* Execute the code */
    kso res = kso_call(self->of, self->args->len, self->args->elems);
    if (!res) {
        kso_exit_if_err();    
    }
    KS_DECREF(res);

    self->is_active = false;
    KS_GIL_UNLOCK();

    return NULL;
}

#endif


/* C-API */
ksos_thread ksos_thread_new(ks_type tp, ks_str name, kso of, ks_tuple args) {
    if (ksg_main_thread) {
        #ifndef KS_HAVE_pthreads
        ks_warn("os.thread", "Attempting to create a new thread when pthreads was not included (this may be disastrous)");
        #endif
    }

    ksos_thread self = KSO_NEW(ksos_thread, tp);

    static int cc = 1;
    if (name) {
        KS_INCREF(name);
    } else {
        name = ks_fmt("%p", self);
    }

    self->name = name;

    KS_NINCREF(of);
    self->of = of;
    KS_INCREF(args);
    self->args = args;

    self->scopename = ks_fmt("");

    self->inrepr = ks_list_new(0, NULL);

    /* Initialize execution environment */
    self->stk = ks_list_new(0, NULL);
    self->frames = ks_list_new(0, NULL);

    self->exc = NULL;


    return self;
}

ksos_thread ksos_thread_get() {
    ksos_thread res = NULL;
    #ifdef KS_HAVE_pthreads
    res = (ksos_thread)pthread_getspecific(this_thread_key);
    #else

    #endif
    return res ? res : ksg_main_thread;
}

bool ksos_thread_start(ksos_thread self) {
    if (self->is_active || self->is_queue) return true;
    self->is_queue = true;

    #if defined(KS_HAVE_pthreads)

    /* Start pthread up */
    /*if (!ks_set_add(active_threads, (kso)self)) {
        ks_catch_ignore();
    }*/

    int stat = pthread_create(&self->pth_, NULL, init_thread_pthreads, self);
    if (stat != 0) {
        KS_THROW(kst_OSError, "Failed to start thread: %s", strerror(stat));
        return false;
    }

    return true;
    #else
    KS_THROW(kst_OSError, "No thrading library present, so cannot 'start' threads");
    return false;
    #endif
}


bool ksos_thread_join(ksos_thread self) {
    if (!self->is_active && !self->is_queue) return true;

    #if defined(KS_HAVE_pthreads)

    /* unlock temporarily to allow other thread to finish */
    KS_GIL_UNLOCK();
    int stat = pthread_join(self->pth_, NULL);
    KS_GIL_LOCK();
    if (stat != 0) {
        KS_THROW(kst_OSError, "Failed to join thread: %s", strerror(stat));
        return false;
    }
    self->is_active = false;

    /*if (!is_joining_active) {
        bool found;
        if (!ks_set_del(active_threads, (kso)self, &found)) ks_catch_ignore();
    }*/
    
    return true;
    #else

    KS_THROW(kst_OSError, "Failed to 'join'; no pthreads detected");
    return false;
    #endif
}

/* Type Functions */

static KS_TFUNC(T, free) {
    ksos_thread self;
    KS_ARGS("self:*", &self, ksost_thread);

    KS_DECREF(self->name);
    KS_NDECREF(self->args);


    ks_free(self->handlers);

    KSO_DEL(self);
    return KSO_NONE;
}

static KS_TFUNC(T, new) {
    ks_type tp;
    kso of;
    kso args = (kso)_ksv_emptytuple;
    KS_ARGS("tp:* of ?args", &tp, kst_type, &of, &args);

    ks_tuple rr = ks_tuple_newi(args);
    if (!rr) return NULL;
    ksos_thread res = ksos_thread_new(tp, NULL, of, rr);
    KS_DECREF(rr);

    return (kso)res;
}
static KS_TFUNC(T, str) {
    ksos_thread self;
    KS_ARGS("self:*", &self, ksost_thread);
    return (kso)ks_fmt("<thread %R>", self->name);
}


/* Export */

static struct ks_type_s tp;
ks_type ksost_thread = &tp;

void _ksi_os_thread() {
    _ksinit(ksost_thread, kst_object, T_NAME, sizeof(struct ksos_thread_s), -1, "Thread of execution, which is a single strand of execution happening (possibly) at the same time as other threads\n\n    Although these are typically wrapped by OS-level threads, there is also the Global Interpreter Lock (GIL) which prevents bytecodes from executing at the same time", KS_IKV(
        {"__new",                  ksf_wrap(T_new_, T_NAME ".__new(tp, of, args=none)", "")},
        {"__str",                  ksf_wrap(T_str_, T_NAME ".__str(self)", "")},
        {"__repr",                 ksf_wrap(T_str_, T_NAME ".__repr(self)", "")},
    ));

    ks_str tmp = ks_str_new(-1, "main");
    ksg_main_thread = ksos_thread_new(ksost_thread, tmp, NULL, _ksv_emptytuple);
    KS_DECREF(tmp);

    #ifdef KS_HAVE_pthreads

    /* Create a per-thread keyed variable */
    int stat = pthread_key_create(&this_thread_key, NULL);
    if (stat != 0) {
        KS_THROW(kst_Error, "Failed to create pthread key: %s", strerror(stat));
        kso_exit_if_err();
    }

    /* Set the variable for this thread */
    pthread_setspecific(this_thread_key, (void*)ksg_main_thread);

    #endif /* KS_HAVE_pthreads */



}




