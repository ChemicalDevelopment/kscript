/* fmax.c - 'fmax' kernel
 *
 * @author: Cade Brown <cade@kscript.org>
 */
#include <ks/impl.h>
#include <ks/nx.h>
#include <ks/nxt.h>
#include <ks/nxm.h>

#define K_NAME "fmax"

#define LOOPI(TYPE, NAME) static int kern_##NAME(int N, nx_t* args, int len, void* extra) { \
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
        TYPE x = *(TYPE*)pX, y = *(TYPE*)pY; \
        *(TYPE*)pR = x > y ? x : y; \
    } \
    return 0; \
}

#define LOOPF(TYPE, NAME) static int kern_##NAME(int N, nx_t* args, int len, void* extra) { \
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
        *(TYPE*)pR = TYPE##fmax(*(TYPE*)pX, *(TYPE*)pY); \
    } \
    return 0; \
}

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
        TYPE x = *(TYPE*)pX, y = *(TYPE*)pY; \
        TYPE##r x2 = x.re*x.re + x.im*x.im; \
        TYPE##r y2 = y.re*y.re + y.im*y.im; \
        *(TYPE*)pR = x2 > y2 ? x : y; \
    } \
    return 0; \
}

NXT_PASTE_I(LOOPI);
NXT_PASTE_F(LOOPF);
NXT_PASTE_C(LOOPC);


bool nx_fmax(nx_t X, nx_t Y, nx_t R) {
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
