/* types/float.c - 'float' type
 *
 *
 * @author: Cade Brown <cade@kscript.org>
 */
#include <ks/impl.h>

#define T_NAME "float"


/* Internals */

/* Returns digit value */
static int I_digv(ks_ucp c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    } else if (c >= 'a' && c <= 'z') {
        return (c - 'a') + 10;
    } else if (c >= 'A' && c <= 'Z') {
        return (c - 'A') + 10;
    } else {
        /* invalid */
        return -1;
    }
}


/* C-API */

ks_float ks_float_new(ks_cfloat val) {
    ks_float self = KSO_NEW(ks_float, kst_float);

    self->val = val;

    return self;
}


bool ks_cfloat_from_str(const char* str, int sz, ks_cfloat* out) {
    bool nt = sz < 0;
    if (nt) sz = strlen(str);
    const char* o_str = str;
    int o_sz = sz;

    /* Consume '_n' characters */
    #define EAT(_n) do { \
        str += _n; \
        sz -= _n; \
    } while (0)

    /* Strip string */
    char c;
    while ((c = *str) == ' ' || c == '\n' || c == '\t' || c == '\r') EAT(1);
    while (sz > 0 && ((c = str[sz - 1]) == ' ' || c == '\n' || c == '\t' || c == '\r')) sz--;

    /* Take out a sign */
    bool isNeg = *str == '-';
    if (isNeg || *str == '+') EAT(1);

    int base = 0;
    if (sz >= 2 && str[0] == '0') {
        c = str[1];
        /**/ if (c == 'b' || c == 'B') base =  2;
        else if (c == 'o' || c == 'O') base =  8;
        else if (c == 'x' || c == 'X') base = 16;
        else if (c == 'd' || c == 'D') base = 10;
    }

    if (base == 0) base = 10;
    else {
        EAT(2);
    }


    if (base == 10) {
        char* rve = NULL;
        int st = 0;
        if (nt) {
            *out = strtod(str, &rve);
            if (rve != str + sz) {
                KS_THROW(kst_ValError, "Invalid format for base %i float: '%.*s' (invalid digits)", base, o_sz, o_str);
                return false;
            }

        } else {
            char* ts = ks_malloc(sz + 1);
            memcpy(ts, str, sz);
            ts[sz] = '\0';
            *out = strtod(ts, &rve);
            ks_free(ts);
            if (rve != ts + sz) {
                KS_THROW(kst_ValError, "Invalid format for base %i float: '%.*s' (invalid digits)", base, o_sz, o_str);
                return false;
            }
        }

        return true;
    }

    /* Parse integral part */
    int i = 0;
    ks_cfloat val = 0;
    while (i < sz && str[i] != '.' && !(base == 10 && (str[i] == 'e' || str[i] == 'E'))) {
        int dig = I_digv(str[i]);
        if (dig < 0 || dig >= base) {
            KS_THROW(kst_ValError, "Invalid format for base %i float: '%.*s' (invalid digits)", base, o_sz, o_str);
            return false;
        }

        val = base * val + dig;
        i++;
    }

    if (str[i] == '.') {
        /* Parse fractional part */
        i++;
        ks_cfloat frac = 1.0;

        while (i < sz && !(base == 10 && (str[i] == 'e' || str[i] == 'E'))) {
            int dig = I_digv(str[i]);
            if (dig < 0 || dig >= base) {
                KS_THROW(kst_ValError, "Invalid format for base %i float: '%.*s' (invalid digits)", base, o_sz, o_str);
                return false;
            }

            frac /= base;
            val += frac * dig;
            i++;
        }
    }

    if (str[i] == 'e' || str[i] == 'E') {
        /* parse base-10 exponent */
        i++;

        bool is_neg_exp = str[i] == '-';
        if (is_neg_exp || str[i] == '+') i++;

        /* Parse exponent */
        int i_exp = 0;
        int _nec = 0;
        while (i < sz) {
            int cdig = I_digv(str[i]);
            if (cdig < 0 || cdig >= 10) break;

            /* overflow check? */
            i_exp = 10 * i_exp + cdig;
            i++;
            _nec++;
        }
        if (_nec <= 0) {
            KS_THROW(kst_ValError, "Invalid format for base %i float: '%.*s' (invalid exponent)", base, o_sz, o_str);
            return false;
        }

        /* range check? */
        /* apply exponent */
        ks_cfloat xscl = pow(10, i_exp);
        if (is_neg_exp) {
            val /= xscl;
        } else {
            val *= xscl;
        }
        /* weird rounding; prefer this for small numbers */
        /*
        if ((i_exp < KSF_DIG / 4 && i_exp > -KSF_DIG / 4)) val *= (double)xscl;
        else val *= xscl;
        */
    }


    if (i != sz) {
        KS_THROW(kst_ValError, "Invalid format for base %i float: '%.*s'", base, o_sz, o_str);
        return false;
    }

    *out = isNeg ? -val : val;
    return true;
}


int ks_cfloat_to_str(char* str, int sz, ks_cfloat val, bool sci, int prec, int base) {

    int i = 0, j, k; /* current position */
    /* Adds a single character */
    #define ADDC(_c) do { \
        if (i < sz) { \
            str[i] = _c; \
        } \
        i++; \
    } while (0)

    #define ADDS(_s) do { \
        char* _cp = _s; \
        while (*_cp) { \
            ADDC(*_cp); \
            _cp++; \
        } \
    } while (0)

    if (val != val) {
        /* 'nan' */
        ADDS("nan");
        return i;
    } else if (val == KS_CFLOAT_INF) {
        /* 'inf' */
        ADDS("inf");
        return i;
    } else if (val == -KS_CFLOAT_INF) {
        /* '-inf' */
        ADDS("-inf");
        return i;
    }

    /* Now, we are working with a real number */
    bool is_neg = val < 0;
    if (is_neg) {
        val = -val;
        ADDC('-');
    }

    /* Handle 0.0 */
    if (val == 0.0) {
        ADDS("0.0");
        return i;
    }

    /* Now, val > 0 */

    /**/ if (base ==  2) ADDS("0b");
    else if (base ==  8) ADDS("0o");
    else if (base == 16) ADDS("0x");

    int sciexp = 0;

    /* extract exponent */
    if (sci) {
        while (val >= base) {
            sciexp++;
            val /= base;
        }
        while (val < 1) {
            sciexp--;
            val *= base;
        }
    }

    /* Handle actual digits, break into integer and floating point type */
    static const char digc[] = "0123456789ABCDEF";

    #if (defined(KS_FLOAT_float128) && defined(KS_HAVE_strfromf128)) \
     || (defined(KS_FLOAT_long_double) && defined(KS_HAVE_strfromld)) \
     || (defined(KS_FLOAT_double) && defined(KS_HAVE_strfromd))
    if (base == 10) {
    #else
    if (false) {
    #endif
        char fmt[64];
        int sz_fmt = snprintf(fmt, sizeof(fmt) - 1, "%%.%if", prec);
        assert(sz_fmt <= sizeof(fmt) - 1);
        i += strfromd(str+i, sz-i, fmt, val);
    } else {
        int i_num = i;
        ks_cfloat vi;
        ks_cfloat vf = modf(val, &vi);

        /* Integral part */
        do {
            ks_cfloat digf = fmod(vi, base);
            int dig = (int)floor(digf);
            
            vi = (vi - digf) / base;

            ADDC(digc[dig]);

        } while (vi > 0.5);

        /* Reverse digit order */
        if (i < sz) for (j = i_num, k = i - 1; j < k; ++j, --k) {
            char t = str[j];
            str[j] = str[k];
            str[k] = t;
        }

        ADDC('.');

        /* Shift over, and generate fractional part */
        vf *= base;

        /* number of digits */
        int ndl = prec <= 0 ? sz - i : prec;

        do {
            ks_cfloat digf = floor(vf);
            int dig = (((int)digf) % base + base) % base;
            assert(dig >= 0 && dig < base); 
            vf = (vf - digf) * base;

            ADDC(digc[dig]);
        } while (vf > 1e-9 && ndl-- > 0);
    }

    /* Now, remove trailing zeros */
    if (i < sz) {
        while (i > 2 && str[i - 1] == '0' && str[i - 2] != '.') i--;
    }
    /* add sciexp */
    if (sci) {
        ADDC('e');
        ADDC(sciexp >= 0 ? '+' : '-');
        if (sciexp < 0) sciexp = -sciexp;

        j = i;

        /* always exponent in base-10 */

        do {
            int sdig = sciexp % 10;

            ADDC(digc[sdig]);
            sciexp /= 10;

        } while (sciexp > 0);

        /* Reverse digits */
        if (i < sz) for (k = i - 1; j < k; j++, k--) {
            char t = str[j];
            str[j] = str[k];
            str[k] = t;
        }
    }
    return i;
}

/* Export */

static struct ks_type_s tp;
ks_type kst_float = &tp;

void _ksi_float() {
    _ksinit(kst_float, kst_number, T_NAME, sizeof(struct ks_float_s), -1, NULL);
}
