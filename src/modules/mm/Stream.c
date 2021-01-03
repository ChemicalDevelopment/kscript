/* mm/Stream.c - 'mm.Stream' type
 *
 * 
 * @author: Cade Brown <cade@kscript.org>
 */
#include <ks/impl.h>
#include <ks/mm.h>

#define T_NAME "mm.Stream"

/* Internals */

/* C-API */

nx_array ksmm_get_image(ksmm_Stream self) {

#ifdef KS_HAVE_libav

    /* Format for a 2D image */
    AVInputFormat* inpfmt = NULL;
    AVFormatContext* fmtctx = self->fmtctx->val;

    /*  */
    if (!(inpfmt = av_find_input_format("image2"))) {
        KS_THROW(kst_Error, "Failed to find input format: 'image2'");
        return NULL;
    }

    /* Get the stream we are reading from */
    AVStream* stream = fmtctx->streams[self->idx];

    /* Options dictionary */
    AVDictionary* opt = NULL;

    /* Data */
    AVFrame* frame = NULL;
    AVPacket* packet = NULL;

    /* Codec */
    AVCodecParameters* codpar = stream->codecpar;


    /* libav status */
    int avst;

    /* Get a context for the specific codec */
    AVCodecContext* codctx = avcodec_alloc_context3(stream->codec->codec);
    if (!codctx) {
        KS_THROW(kst_Error, "Failed: 'avcodec_alloc_context3()'");
        avcodec_free_context(&codctx);
        return NULL;
    }

    /* Get parameters */
    if ((avst = avcodec_parameters_to_context(codctx, codpar)) < 0) {
        KS_THROW(kst_Error, "Failed 'avcodec_parameters_to_context()': %s [%i]", av_err2str(avst), avst);
        avcodec_free_context(&codctx);
        return NULL;
    }

    /* Allocate data buffers */
    if (!(frame = av_frame_alloc())) {
        KS_THROW(kst_Error, "Failed 'av_frame_alloc()'");
        avcodec_free_context(&codctx);
        return NULL;
    }
    if (!(packet = av_packet_alloc())) {
        KS_THROW(kst_Error, "Failed 'av_packet_alloc()'");
        avcodec_free_context(&codctx);
        av_frame_free(&frame);
        return NULL;
    }

    /* Set options */
    av_dict_set(&opt, "thread_type", "slice", 0);

    /* Set pixel format negotiator */
    codctx->get_format = ksmm_AV_getformat;


    /* Open the codec context */
    if ((avst = avcodec_open2(codctx, codctx->codec, &opt)) < 0) {
        KS_THROW(kst_Error, "Failed to open codec for %R: %s [%i]", self->src, av_err2str(avst), avst);
        return NULL;
    }

    /* Read a frame (still encoded) into packet */
    if ((avst = av_read_frame(fmtctx, packet)) < 0) {
        KS_THROW(kst_Error, "Failed to read frame from file %R: %s [%i]", self->src, av_err2str(avst), avst);
        return NULL;
    }

    /* Send packet to the decode */
    if ((avst = avcodec_send_packet(codctx, packet)) < 0) {
        KS_THROW(kst_Error, "Failed to read frame from file %R: %s [%i]", self->src, av_err2str(avst), avst);
        return NULL;
    }

    /* Receive a frame back */
    if ((avst = avcodec_receive_frame(codctx, frame)) < 0) {
        KS_THROW(kst_Error, "Failed to read frame from file %R: %s [%i]", self->src, av_err2str(avst), avst);
        return NULL;
    }

    /** Actually decode **/

    /* Shape of the tensor (chn < 0 indiciates it is unknown) */
    int h = frame->height, w = frame->width, out_chn = -1, in_chn = -1;

    /* Channel stride (padding per channel) */
    int in_chn_stride = -1;

    /* Input datatype */
    nx_dtype indt = NULL;

    /* Actual pixel format */
    enum AVPixelFormat pix_fmt = frame->format;

    /* Filter to replace bad formats */
    pix_fmt = ksmm_AV_filterfmt(pix_fmt);

    /* Result array */
    nx_array res = NULL;

    /* Return data type */
    nx_dtype rdt = nxd_float;

    /* Formats that are easy to process */
    if (pix_fmt == AV_PIX_FMT_RGBA || pix_fmt == AV_PIX_FMT_RGB24 || pix_fmt == AV_PIX_FMT_RGB0) {

        /* RGB[A] formats are unsigned 8 bit values */

        /**/ if (pix_fmt == AV_PIX_FMT_RGBA)  { in_chn = 4; in_chn_stride = 4; }
        else if (pix_fmt == AV_PIX_FMT_RGB0)  { in_chn = 3; in_chn_stride = 4; }
        else if (pix_fmt == AV_PIX_FMT_RGB24) { in_chn = 3; in_chn_stride = 3; }


        /* Input datatype is bytes */
        indt = nxd_uchar;
        if (out_chn <= 0) out_chn = in_chn;

        assert(in_chn > 0 && in_chn_stride > 0 && out_chn >= 0 && "layout was not calculated!");

        /* Construct result */
        res = nx_array_newc(nxt_array, rdt, 3, (ks_size_t[]){ h, w, out_chn }, NULL, NULL);

        /* Scaling factor */
        float scale_fac = 1.0;
        if (indt->kind == NX_DTYPE_KIND_CINT && (rdt->kind == NX_DTYPE_KIND_CFLOAT || rdt->kind == NX_DTYPE_KIND_CCOMPLEX)) {
            /* Converting requires a fixed point scale */
            scale_fac = 1.0 / ((1ULL << (8 * indt->size)) - 1);
        }

        /* Scale input */   
        if (!nx_mul(
            res->ar,
            (nxar_t) {
                frame->data[0],
                indt,
                3,
                (ks_size_t[]){ h, w, in_chn },
                (ks_size_t[]){ frame->linesize[0], in_chn_stride * indt->size, indt->size }
            },
            (nxar_t) {
                (void*)&scale_fac,
                nxd_float,
                0,
                NULL,
                NULL
            }
        )) {
            KS_DECREF(res);
            res = NULL;
            return NULL;
        }

    } else {
        /* Weird format, so scale in software */

        // first, try to convert to 8-bit greyscale
        /* Convert to grayscale (or RGBA) */
        enum AVPixelFormat new_fmt = AV_PIX_FMT_GRAY8;

        /* Convert to RGB if we would lose chroma */
        if (av_get_pix_fmt_loss(new_fmt, pix_fmt, true) & FF_LOSS_CHROMA) {
            new_fmt = AV_PIX_FMT_RGB24;
        }

        /* Ensure alpha is included if the format would lose it */
        if (av_get_pix_fmt_loss(new_fmt, pix_fmt, true) & FF_LOSS_ALPHA) {
            new_fmt = AV_PIX_FMT_RGBA;
        }

        /**/ if (new_fmt == AV_PIX_FMT_GRAY8)   { in_chn = 1; in_chn_stride = 1; } 
        else if (new_fmt == AV_PIX_FMT_RGB24)   { in_chn = 3; in_chn_stride = 3; } 
        else if (new_fmt == AV_PIX_FMT_RGBA)    { in_chn = 4; in_chn_stride = 4; }

        if (out_chn <= 0) out_chn = in_chn;

        /* Input is bytes */
        indt = nxd_uchar;

        assert(in_chn > 0 && in_chn_stride > 0 && out_chn >= 0 && "layout was not calculated!");

        static struct SwsContext *sws_context = NULL;

        /* Get the conversion context */
        sws_context = sws_getCachedContext(sws_context,
            w, h, pix_fmt,
            w, h, new_fmt,
            0, NULL, NULL, NULL
        );

        /* Compute line size and temporary data */
        ks_size_t linesize = in_chn_stride * w;
        linesize += (16 - linesize) % 16;

        char* tmp_data = ks_malloc(linesize * h);


        /* Convert data into 'tmp_data' */
        sws_scale(sws_context, (const uint8_t* const*)frame->data, frame->linesize, 0, h, (uint8_t* const[]){ tmp_data, NULL, NULL, NULL }, (int[]){ linesize, 0, 0, 0 });

        /* Construct result */
        res = nx_array_newc(nxt_array, rdt, 3, (ks_size_t[]){ h, w, out_chn }, NULL, NULL);


        /* Scaling factor */
        float scale_fac = 1.0;
        if (indt->kind == NX_DTYPE_KIND_CINT && (rdt->kind == NX_DTYPE_KIND_CFLOAT || rdt->kind == NX_DTYPE_KIND_CCOMPLEX)) {
            /* Converting requires a fixed point scale */
            scale_fac = 1.0 / ((1ULL << (8 * indt->size)) - 1);
        }

        /* Scale input */   
        if (!nx_mul(
            res->ar,
            (nxar_t) {
                tmp_data,
                indt,
                3,
                (ks_size_t[]){ h, w, in_chn },
                (ks_size_t[]){ frame->linesize[0], in_chn_stride * indt->size, indt->size }
            },
            (nxar_t) {
                (void*)&scale_fac,
                nxd_float,
                0,
                NULL,
                NULL
            }
        )) {
            KS_DECREF(res);
            res = NULL;
            return NULL;
        }
    }


    /* Free resources */
    avcodec_free_context(&codctx);
    av_frame_free(&frame);
    av_packet_free(&packet);

    return res;

#else
    KS_THROW(kst_Error, "Cannot get image from stream, no libav");
    return NULL;
#endif
}


/* Type Functions */

static KS_TFUNC(T, free) {
    ksmm_Stream self;
    KS_ARGS("self:*", &self, ksmmt_Stream);


    KSO_DEL(self);
    return KSO_NONE;
}

static KS_TFUNC(T, readimg) {
    ksmm_Stream self;
    KS_ARGS("self:*", &self, ksmmt_Stream);
    
    return (kso)ksmm_get_image(self);
}

/* Export */

static struct ks_type_s tp;
ks_type ksmmt_Stream = &tp;

void _ksi_mm_Stream() {
    _ksinit(ksmmt_Stream, kst_object, T_NAME, sizeof(struct ksmm_Stream_s), -1, "", KS_IKV(
        {"__free",                 ksf_wrap(T_free_, T_NAME ".__free(self)", "")},

        {"readimg",                ksf_wrap(T_readimg_, T_NAME ".readimg(self)", "Reads an image")},

    ));
}
