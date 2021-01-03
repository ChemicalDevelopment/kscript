/* atan.c - 'atan' kernel
 *
 * @author: Cade Brown <cade@kscript.org>
 */
#include <ks/impl.h>
#include <ks/nx.h>
#include <ks/nxt.h>

#define K_NAME "atan"


static int kern(int N, nxar_t* inp, int len, void* _data) {
    assert(N == 2);
    assert(inp[0].dtype == inp[1].dtype);
    assert(inp[0].dtype->kind == NX_DTYPE_KIND_CFLOAT || inp[0].dtype->kind == NX_DTYPE_KIND_CCOMPLEX);

    ks_cint i;
    nx_dtype dtype = inp[0].dtype;
    ks_uint pR = (ks_uint)inp[0].data, pX = (ks_uint)inp[1].data;
    ks_cint sR = inp[0].strides[0], sX = inp[1].strides[0];

    if (dtype == nxd_float) {
        #define TYPE nxc_float
        for (i = 0; i < len; i++, pR += sR, pX += sX) {
            TYPE vX = *(TYPE*)pX;
        #ifdef KS_HAVE_atanf
            *(TYPE*)pR = atanf(vX);
        #else
            KS_THROW(kst_Error, "Kernel '%s' is not supported for dtype %R, function(s) not found: atanf", K_NAME, dtype);
            return 1;
        #endif
        }

        return 0;
        #undef TYPE
    } else if (dtype == nxd_double) {
        #define TYPE nxc_double
        for (i = 0; i < len; i++, pR += sR, pX += sX) {
            TYPE vX = *(TYPE*)pX;
        #ifdef KS_HAVE_atan
            *(TYPE*)pR = atan(vX);
        #elif defined(KS_HAVE_atanf)
            *(TYPE*)pR = atanf(vX);
        #else
            KS_THROW(kst_Error, "Kernel '%s' is not supported for dtype %R, function(s) not found: atan, atanf", K_NAME, dtype);
            return 1;
        #endif
        }

        return 0;
        #undef TYPE
    } else if (dtype == nxd_longdouble) {
        #define TYPE nxc_longdouble
        for (i = 0; i < len; i++, pR += sR, pX += sX) {
            TYPE vX = *(TYPE*)pX;
        #ifdef KS_HAVE_atanl
            *(TYPE*)pR = atanl(vX);
        #elif defined(KS_HAVE_atan)
            *(TYPE*)pR = atan(vX);
        #elif defined(KS_HAVE_atanf)
            *(TYPE*)pR = atanf(vX);
        #else
            KS_THROW(kst_Error, "Kernel '%s' is not supported for dtype %R, function(s) not found: atanl, atan, atanf", K_NAME, dtype);
            return 1;
        #endif
        }

        return 0;
        #undef TYPE
    } else if (dtype == nxd_float128) {
        #define TYPE nxc_float128
        for (i = 0; i < len; i++, pR += sR, pX += sX) {
            TYPE vX = *(TYPE*)pX;
        #ifdef KS_HAVE_atanf128
            *(TYPE*)pR = atanf128(vX);
        #elif defined(KS_HAVE_atanl)
            *(TYPE*)pR = atanl(vX);
        #elif defined(KS_HAVE_atan)
            *(TYPE*)pR = atan(vX);
        #elif defined(KS_HAVE_atanf)
            *(TYPE*)pR = atanf(vX);
        #else
            KS_THROW(kst_Error, "Kernel '%s' is not supported for dtype %R, function(s) not found: atanf128, atanl, atan, atanf", K_NAME, dtype);
            return 1;
        #endif
        }

        return 0;
        #undef TYPE
    }

    KS_THROW(kst_Error, "Unsupported type for kernel '%s': %R", K_NAME, dtype);
    return 1;
}

bool nx_atan(nxar_t r, nxar_t x) {
    /* Determine result type */
    if (r.dtype->kind != NX_DTYPE_KIND_CFLOAT && r.dtype->kind != NX_DTYPE_KIND_CCOMPLEX) {
        KS_THROW(kst_Error, "Result type for '%s' must be float or complex, but was %R", K_NAME, r.dtype->name);
        return false;
    }

    nxar_t tX;
    kso rX;
    if (!nx_getcast(x, r.dtype, &tX, &rX)) {
        return false;
    }

    bool res = !nx_apply_elem(kern, 2, (nxar_t[]){ r, tX }, NULL);

    KS_NDECREF(rX);
    return res;
}

