/* div.c - 'div' kernel
 *
 * @author: Cade Brown <cade@kscript.org>
 */
#include <ks/impl.h>
#include <ks/nx.h>
#include <ks/nxt.h>

#define K_NAME "div"


#define LOOPR(TYPE, NAME) static int kern_##NAME(int N, nx_t* args, int len, void* extra) { \
    assert(N == 3); \
    nx_t X = args[0], Y = args[1], R = args[2]; \
    assert(X.dtype == Y.dtype && Y.dtype == R.dtype); \
    ks_uint \
        pX = (ks_uint)X.data, \
        pY = (ks_uint)Y.data, \
        pR = (ks_uint)R.data  \
    ; \
    ks_cint \
        sX = X.strides[0], \
        sY = Y.strides[0], \
        sR = R.strides[0]  \
    ; \
    ks_cint i; \
    for (i = 0; i < len; i++, pX += sX, pY += sY, pR += sR) { \
        *(TYPE*)pR = *(TYPE*)pX / *(TYPE*)pY; \
    } \
    return 0; \
}


/* Complex division: a / b = a * ~b / (|b|^2) */

#define LOOPC(TYPE, NAME) static int kern_##NAME(int N, nx_t* args, int len, void* extra) { \
    assert(N == 3); \
    nx_t X = args[0], Y = args[1], R = args[2]; \
    assert(X.dtype == Y.dtype && Y.dtype == R.dtype); \
    ks_uint \
        pX = (ks_uint)X.data, \
        pY = (ks_uint)Y.data, \
        pR = (ks_uint)R.data  \
    ; \
    ks_cint \
        sX = X.strides[0], \
        sY = Y.strides[0], \
        sR = R.strides[0]  \
    ; \
    ks_cint i; \
    for (i = 0; i < len; i++, pX += sX, pY += sY, pR += sR) { \
        TYPE t = *(TYPE*)pY; \
        t.im = -t.im; \
        ((TYPE*)pR)->re = ((TYPE*)pX)->re * t.re - ((TYPE*)pX)->im * t.im; \
        ((TYPE*)pR)->im = ((TYPE*)pX)->re * t.im + ((TYPE*)pX)->im * t.re; \
        /* Divide by modulus */ \
        t.re = t.re * t.re + t.im * t.im; \
        ((TYPE*)pR)->re /= t.re; \
        ((TYPE*)pR)->im /= t.re; \
    } \
    return 0; \
}


NXT_PASTE_I(LOOPR);
NXT_PASTE_F(LOOPR);

NXT_PASTE_C(LOOPC);


bool nx_div(nx_t X, nx_t Y, nx_t R) {
    nx_t cX, cY;
    void *fX = NULL, *fY = NULL;
    if (!nx_getcast(X, R.dtype, &cX, &fX)) {
        return false;
    }
    if (!nx_getcast(Y, R.dtype, &cY, &fY)) {
        ks_free(fX);
        return false;
    }

    #define LOOP(NAME) do { \
        bool res = !nx_apply_elem(kern_##NAME, 3, (nx_t[]){ cX, cY, R }, NULL); \
        ks_free(fX); \
        ks_free(fY); \
        return res; \
    } while (0);

    NXT_PASTE_ALL(R.dtype, LOOP);
    #undef LOOP

    ks_free(fX);
    ks_free(fY);

    KS_THROW(kst_TypeError, "Unsupported types for kernel '%s': %R, %R, %R", K_NAME, X.dtype, Y.dtype, R.dtype);
    return false;
}