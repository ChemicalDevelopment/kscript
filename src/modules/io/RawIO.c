/* io/RawIO.c - 'io.RawIO' type
 *
 * 
 * @author: Cade Brown <cade@kscript.org>
 */
#include <ks/impl.h>

#define T_NAME "io.RawIO"


/* C-API */

ksio_RawIO ksio_RawIO_wrap(ks_type tp, int fd, bool do_close, bool is_r, bool is_w, ks_str fname) {
    ksio_RawIO self = KSO_NEW(ksio_RawIO, tp);

    self->fd = fd;
    self->do_close = do_close;
    self->is_r = is_r;
    self->is_w = is_w;
    KS_INCREF(fname);
    self->fname = fname;

    return self;
}

/* Type Functions */

static KS_TFUNC(T, init) {
    ksio_RawIO self;
    kso ssrc;
    ks_str mode = _ksv_r;
    KS_ARGS("self:* src ?mode:*", &self, ksiot_RawIO, &ssrc, &mode, kst_str);

    ks_str src = NULL;
    if (kso_issub(ssrc->type, kst_str)) {
        KS_INCREF(ssrc);
        src = (ks_str)ssrc;
    } else if (kso_issub(ssrc->type, ksost_path)) {
        src = ks_fmt("%S", ssrc);
        if (!src) return NULL;
    } else {
        KS_THROW(kst_Error, "Expected 'src' to be either 'str' or 'os.path', but got '%T' object", ssrc);
        return NULL;
    }

    bool is_r = false, is_w = false;
    int flags = 0;
    if (ks_str_eq_c(mode, "r", 1) || ks_str_eq_c(mode, "rb", 2)) {
        is_r = true;
        flags = O_RDONLY;
    } else if (ks_str_eq_c(mode, "w", 1) || ks_str_eq_c(mode, "wb", 2)) {
        is_w = true;
        flags = O_WRONLY | O_CREAT | O_TRUNC;
    } else if (ks_str_eq_c(mode, "a", 1) || ks_str_eq_c(mode, "ab", 2)) {
        is_w = true;
        flags = O_WRONLY | O_CREAT | O_APPEND;
    } else if (ks_str_eq_c(mode, "r+", 1) || ks_str_eq_c(mode, "r+b", 2) || ks_str_eq_c(mode, "rb+", 2)) {
        is_r = true;
        is_w = true;
        flags = O_RDWR;
    } else if (ks_str_eq_c(mode, "w+", 1) || ks_str_eq_c(mode, "w+b", 2) || ks_str_eq_c(mode, "wb+", 2)) {
        is_r = true;
        is_w = true;
        flags = O_RDWR | O_CREAT | O_TRUNC;
    } else if (ks_str_eq_c(mode, "a+", 1) || ks_str_eq_c(mode, "a+b", 2) || ks_str_eq_c(mode, "ab+", 2)) {
        is_r = true;
        is_w = true;
        flags = O_RDWR | O_CREAT | O_APPEND;
    } else {
        KS_THROW(kst_Error, "Invalid mode: %R", mode);
        return NULL;
    }

    /* Attempt to open via the C library 
     * TODO: check filesystem encoding
     */
    mode_t m = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    self->fd = (flags & O_CREAT) ? open(src->data, flags, m) : open(src->data, flags);
    if (self->fd < 0) {
        KS_THROW(kst_IOError, "Failed to open %R: %s", src, strerror(errno));
        return NULL;
    }

    self->fname = src;
    self->is_r = is_r;
    self->is_w = is_w;
    self->do_close = true;
    self->sz_r = self->sz_w;
    self->is_open = true;

    return KSO_NONE;
}

static KS_TFUNC(T, free) {
    ksio_RawIO self;
    KS_ARGS("self:*", &self, ksiot_RawIO);

    if (self->fname) KS_DECREF(self->fname);
    
    if (self->do_close && self->is_open && self->fd >= 0) {
        close(self->fd);
    }

    KSO_DEL(self);

    return KSO_NONE;
}

static KS_TFUNC(T, str) {
    ksio_RawIO self;
    KS_ARGS("self:*", &self, ksiot_RawIO);

    bool both = self->is_r && self->is_w;
    return (kso)ks_fmt("<%T (src=%R, mode='%sb')>", self, self->fname, both ? "r+" : self->is_r ? "r" : "w");
}

static KS_TFUNC(T, close) {
    ksio_RawIO self;
    KS_ARGS("self:*", &self, ksiot_RawIO);

    if (self->do_close && self->is_open && self->fd >= 0) {
        self->is_open = false;
        if (close(self->fd) != 0) {
            KS_THROW(kst_IOError, "Failed to close %R: %s", self, strerror(errno));
            return NULL;
        }
    }

    return KSO_NONE;
}

static KS_TFUNC(T, read) {
    ksio_RawIO self;
    ks_cint sz = KS_CINT_MAX;
    KS_ARGS("self:* ?sz:cint", &self, ksiot_RawIO, &sz);

    /* Read bytes */
    ks_ssize_t bsz = KSIO_BUFSIZ, rsz = 0;
    void* dest = NULL;
    while (rsz < sz) {
        dest = ks_realloc(dest, rsz + bsz);
        ks_ssize_t csz = ksio_readb((ksio_BaseIO)self, bsz, ((char*)dest) + rsz);
        if (csz < 0) {
            ks_free(dest);
            return NULL;
        }
        rsz += csz;
        if (csz == 0) break;
    }

    ks_bytes res = ks_bytes_new(rsz, dest);
    ks_free(dest);
    return (kso)res;
}

static KS_TFUNC(T, write) {
    ksio_RawIO self;
    kso msg;
    KS_ARGS("self:* msg", &self, ksiot_RawIO, &msg);

    /* Write bytes */
    ks_bytes vm = kso_bytes(msg);
    if (!vm) return NULL;
    if (ksio_writeb((ksio_BaseIO)self, vm->len_b, vm->data) < 0) {
        KS_DECREF(vm);
        return NULL;
    }
    KS_DECREF(vm);

    return KSO_NONE;
}


/* Export */

static struct ks_type_s tp;
ks_type ksiot_RawIO = &tp;


void _ksi_io_RawIO() {

    _ksinit(ksiot_RawIO, ksiot_BaseIO, T_NAME, sizeof(struct ksio_RawIO_s), -1, "Raw stream input/output", KS_IKV(
        {"__free",                 ksf_wrap(T_free_, T_NAME ".__free(self)", "")},
        {"__init",                 ksf_wrap(T_init_, T_NAME ".__init(self, src, mode='r')", "")},
        {"__repr",                 ksf_wrap(T_str_, T_NAME ".__repr(self)", "")},
        {"__str",                  ksf_wrap(T_str_, T_NAME ".__str(self)", "")},

        {"read",                   ksf_wrap(T_read_, T_NAME ".read(self, sz=-1)", "Reads a message from the stream")},
        {"write",                  ksf_wrap(T_write_, T_NAME ".write(self, msg)", "Writes a messate to the stream")},

    ));
}
