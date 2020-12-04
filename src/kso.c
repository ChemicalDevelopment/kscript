/* kso.c - routines dealing with object management
 *
 * @author: Cade Brown <cade@kscript.org>
 */
#include <ks/impl.h>


bool kso_issub(ks_type a, ks_type b) {
    if (a->type == b->type) return true;
    if (a->type == kst_object) return false;
    return kso_issub(a->i__base, b);
}
bool kso_isinst(kso a, ks_type b) {
    return kso_issub(a->type, b);
}

bool kso_truthy(kso ob, bool* out) {
    if (kso_issub(ob->type, kst_int)) {
        ks_int obi = (ks_int)ob;
        #ifdef KS_INT_GMP
        *out = mpz_cmp_si(obi->val, 0) != 0;
        return true;
        #endif
    } else if (kso_issub(ob->type, kst_float)) {
        ks_float obf = (ks_float)ob;
        *out = obf->val != 0;
        return true;
    } else if (kso_issub(ob->type, kst_complex)) {
        ks_complex obf = (ks_complex)ob;
        *out = !KS_CC_EQRI(obf->val, 0, 0);
        return true;
    } else if (ob->type->i__bool) {
        kso bo = kso_call(ob->type->i__bool, 1, &ob);
        if (!bo) return NULL;

        bool res = kso_truthy(bo, out);
        KS_DECREF(bo);
        return res;
    }

    /*KS_THROW_EXC(ks_T_TypeError, "Could not convert '%T' object to 'bool'", obj);
    return false;*/
    *out = true;
    return true;
}

bool kso_get_ci(kso ob, ks_cint* val) {
    if (kso_issub(ob->type, kst_int)) {
        ks_int obi = (ks_int)ob;
        #ifdef KS_INT_GMP
        if (mpz_fits_slong_p(obi->val)) {
            *val = mpz_get_si(obi->val);
            return true;
        } else {
            /* TODO: check for systems like Windows */
        }
        #endif

        KS_THROW(kst_OverflowError, "'%T' object was too large to convert to a C-style integer", ob);
        return false;
    }

    KS_THROW(kst_TypeError, "Failed to convert '%T' object to C-style integer");
    return false;
}
bool kso_get_ui(kso ob, ks_uint* val) {
    if (kso_issub(ob->type, kst_int)) {
        ks_int obi = (ks_int)ob;
        #ifdef KS_INT_GMP
        if (mpz_fits_ulong_p(obi->val)) {
            *val = mpz_get_ui(obi->val);
            return true;
        } else {
            /* TODO: check for systems like Windows */
        }
        #endif

        KS_THROW(kst_OverflowError, "'%T' object was too large to convert to a C-style integer", ob);
        return false;
    }
    
    KS_THROW(kst_TypeError, "Failed to convert '%T' object to C-style integer");
    return false;
}
bool kso_get_cf(kso ob, ks_cfloat* val) {
    if (kso_issub(ob->type, kst_int)) {
        ks_int obi = (ks_int)ob;
        #ifdef KS_INT_GMP
        *val = mpz_get_d(obi->val);
        return true;
        #endif
    } else if (kso_issub(ob->type, kst_float)) {
        *val = ((ks_float)ob)->val;
        return true;
    } else if (ob->type->i__float) {
        kso obf = kso_call(ob->type->i__float, 1, &ob);
        if (!obf) return false;

        bool res = kso_get_cf(obf, val);
        KS_DECREF(obf);
        return res;
    }
    
    KS_THROW(kst_TypeError, "Failed to convert '%T' object to C-style float");
    return false;
}
bool kso_get_cc(kso ob, ks_ccomplex* val) {
    if (kso_issub(ob->type, kst_int)) {
        ks_int obi = (ks_int)ob;
        #ifdef KS_INT_GMP
        *val = KS_CC_MAKE(mpz_get_d(obi->val), 0);
        return true;
        #endif
    } else if (kso_issub(ob->type, kst_float)) {
        *val = KS_CC_MAKE(((ks_float)ob)->val, 0);
        return true;
    } else if (kso_issub(ob->type, kst_complex)) {
        *val = ((ks_complex)ob)->val;
        return true;
    } else if (ob->type->i__complex) {
        kso obf = kso_call(ob->type->i__complex, 1, &ob);
        if (!obf) return false;

        bool res = kso_get_cc(obf, val);
        KS_DECREF(obf);
        return res;
    } else if (ob->type->i__float) {
        kso obf = kso_call(ob->type->i__float, 1, &ob);
        if (!obf) return false;

        bool res = kso_get_cc(obf, val);
        KS_DECREF(obf);
        return res;
    }
    
    KS_THROW(kst_TypeError, "Failed to convert '%T' object to C-style complex float");
    return false;
}


ks_str kso_str(kso ob) {
    if (kso_issub(ob->type, kst_str)) {
        KS_INCREF(ob);
        return (ks_str)ob;
    } else {
        return ks_fmt("%S", ob);
    }
}
ks_bytes kso_bytes(kso ob) {
    if (kso_issub(ob->type, kst_bytes)) {
        KS_INCREF(ob);
        return (ks_bytes)ob;
    } else if (ob->type->i__bytes) {
        ks_bytes r = (ks_bytes)kso_call(ob->type->i__bytes, 1, &ob);
        if (!r) return NULL;

        if (!kso_issub(r->type, kst_bytes)) {
            KS_THROW(kst_TypeError, "'%T.__bytes()' returned non-'bytes' object of type '%T'", ob, r);
            KS_DECREF(r);
            return NULL;
        }

        return r;
    }

    KS_THROW(kst_TypeError, "Failed to convert '%T' object to 'bytes'");
    return NULL;
}
ks_str kso_repr(kso ob) {
    return ks_fmt("%R", ob);
}
ks_number kso_number(kso ob) {
    if (kso_issub(ob->type, kst_number)) {
        KS_INCREF(ob);
        return (ks_number)ob;
    }
    return (ks_number)kso_call((kso)kst_number, 1, &ob);
}
ks_int kso_int(kso ob) {
    if (kso_issub(ob->type, kst_int)) {
        KS_INCREF(ob);
        return (ks_int)ob;
    }

    KS_THROW(kst_TypeError, "Failed to convert '%T' object to 'int'");
    return NULL;
}


kso kso_call(kso func, int nargs, kso* args) {

    return NULL;
}

void* kso_throw(ks_Exception exc) {
    return NULL;
}

void* kso_throw_c(ks_type tp, const char* cfile, const char* cfunc, int cline, const char* fmt, ...) {
    ks_Exception exc = ks_Exception_new_c(tp, cfile, cfunc, cline, fmt);

    return kso_throw(exc);
}
ks_Exception kso_catch() {
    ksos_thread th = ksos_thread_get();

    ks_Exception res = th->exc;
    if (th->exc) th->exc = NULL;

    return res;
}

bool kso_catch_ignore() {
    ks_Exception exc = kso_catch();
    if (exc) {
        KS_DECREF(exc);
        return true;
    }
    return false;
}


bool kso_catch_ignore_print() {
    ks_Exception exc = kso_catch();
    if (exc) {
        printf("HAD EXC\n");
        KS_DECREF(exc);
        return true;
    }
    return false;
}

void kso_exit_if_err() {
    ks_Exception exc = kso_catch();
    if (exc) {
        printf("HAD EXC\n");
        KS_DECREF(exc);
        assert(false);
    }
}




/** Internal methods **/

kso _kso_new(ks_type tp) {
    kso res = ks_zmalloc(1, tp->ob_sz);
    memset(res, 0, tp->ob_sz);

    KS_INCREF(tp);
    res->type = tp;
    res->refs = 1;

    if (tp->ob_attr > 0) {
        /* Initialize attribute dictionary */
        ks_dict* attr = (ks_dict*)(((ks_uint)res + tp->ob_attr));
        *attr = ks_dict_new(NULL);
    }

    return res;
}

void _kso_del(kso ob) {
    KS_DECREF(ob->type);

    if (ob->type->ob_attr > 0) {
        /* Initialize attribute dictionary */
        ks_dict* attr = (ks_dict*)(((ks_uint)ob + ob->type->ob_attr));
    
        KS_DECREF(*attr);
    }
}

kso _ks_newref(kso ob) {
    KS_INCREF(ob);
    return ob;
}

void _kso_free(kso obj, const char* file, const char* func, int line) {
    /*
    if (obj->refs != 0) {
        fprintf(stderr, "[ks] Trying to free <'%s' obj @ %p>, which had %i refs\n", obj->type->i__fullname__->chr, obj, (int)obj->refs);
    }
    */
    kso i__free = obj->type->i__free;

    if (i__free) {
        kso res = NULL;
        /* Call deconstructor */
        if (i__free->type == kst_func && ((ks_func)i__free)->is_cfunc) {
            /* At some point we have to call without creating a stack frame, since that
             *   would create infinite recursion
             */
            res = ((ks_func)i__free)->cfunc(1, &obj);
        } else {
            res = kso_call(i__free, 1, &obj);
        }
    } else {
        KSO_DEL(obj);
    }
}

