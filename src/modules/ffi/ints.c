/* ffi/ints.c - 'ffi' integer types
 *
 * 
 * @author: Cade Brown <cade@kscript.org>
 */
#include <ks/impl.h>
#include <ks/ffi.h>


kso ksffi_new_int(ks_type tp, ks_cint val) {
    #define _KSCASE(_name, _ctp) else if (kso_issub(tp, ksffit_##_name)) { \
        ksffi_##_name r = KSO_NEW(ksffi_##_name, tp); \
        r->val = val; \
        return (kso)r; \
    }

    if (false) {}
    KSFFI_DO_INTS(_KSCASE);
    #undef _KSCASE

    KS_THROW(kst_TypeError, "Failed to create C-style integer of type '%T'", tp);
    return NULL;
}
kso ksffi_new_intu(ks_type tp, ks_uint val) {
    #define _KSCASE(_name, _ctp) else if (kso_issub(tp, ksffit_##_name)) { \
        ksffi_##_name r = KSO_NEW(ksffi_##_name, tp); \
        r->val = val; \
        return (kso)r; \
    }

    if (false) {}
    KSFFI_DO_INTS(_KSCASE);
    #undef _KSCASE

    KS_THROW(kst_TypeError, "Failed to create C-style integer of type '%T'", tp);
    return NULL;
}

/* Type definitions */

#define _KSCASE(_name, _ctp) \
static KS_TFUNC(T_##_name, new) { \
    ks_type tp; \
    ks_cint val = 0; \
    KS_ARGS("tp:* ?val:cint", &tp, kst_type, &val); \
    ksffi_##_name self = KSO_NEW(ksffi_##_name, tp); \
    self->val = val; \
    return (kso)self; \
} \
static KS_TFUNC(T_##_name, str) { \
    ksffi_##_name self; \
    KS_ARGS("self:*", &self, ksffit_##_name); \
    return (kso)ks_fmt("%l", (ks_cint)self->val);\
} \
static KS_TFUNC(T_##_name, integral) { \
    ksffi_##_name self; \
    KS_ARGS("self:*", &self, ksffit_##_name); \
    return (kso)ks_int_new(self->val); \
}

KSFFI_DO_INTS(_KSCASE)

#undef _KSCASE


/* Export */

#define _KSCASE(_name, _ctp) \
static struct ks_type_s tp_##_name; \
ks_type ksffit_##_name = &tp_##_name;

KSFFI_DO_INTS(_KSCASE)

#undef _KSCASE


void _ksi_ffi_ints() {

    #define _KSCASE(_name, _ctp) \
    _ksinit(ksffit_##_name, kst_number, "ffi." #_name, sizeof(struct ksffi_##_name##_s), -1, "C-style integer of type " #_ctp, KS_IKV( \
        {"__new",                  ksf_wrap(T_##_name##_new_, "ffi." #_name ".__new(tp, val=0)", "")}, \
        {"__repr",                 ksf_wrap(T_##_name##_str_, "ffi." #_name ".__repr(self)", "")}, \
        {"__str",                  ksf_wrap(T_##_name##_str_, "ffi." #_name ".__str(self)", "")}, \
        {"__integral",             ksf_wrap(T_##_name##_integral_, "ffi." #_name ".__integral(self)", "")}, \
        {"__sizeof",               (kso)ks_int_newu(sizeof(_ctp))}, \
    ));

    KSFFI_DO_INTS(_KSCASE)

    #undef _KSCASE

}


