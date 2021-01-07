/* ks/av.h - header file for the kscript audio/video/media builtin module `import av'
 *
 * 
 * SEE: https://github.com/leandromoreira/ffmpeg-libav-tutorial#intro
 * 
 * @author: Cade Brown <cade@kscript.org>
 */

#pragma once
#ifndef KSAV_H__
#define KSAV_H__

#ifndef KS_H__
#include <ks/ks.h>
#endif /* KS_H__ */


/* We use the NumeriX library */
#include <ks/nx.h>


/* libav (--with-libav) 
 *
 * Adds support for different media formats
 *
 */
#ifdef KS_HAVE_libav
  #include <libavutil/opt.h>
  #include <libavutil/imgutils.h>
  #include <libavcodec/avcodec.h>
  #include <libavformat/avformat.h>

  #include <libswscale/swscale.h>
#else

#endif


/* Types */

/* av.IO - Represents an audio/video IO, which can be readable or writable
 *
 * Essentially, this is like a container type, which can contain multiple media streams,
 *   which can be read to or written from (and may be of different types)
 * 
 * This is kind of a low-level interface, typically tracking the 'AVFormatContext*' functionality
 *   very closely, albeit with a more object-oriented and sanely named methods
 * 
 */
typedef struct ksav_IO_s {
    KSO_BASE

    /* Source URL of the file that it was read from  */
    ks_str src;

    /* Mode the IO is in */
    ks_str mode;

    /* Internal IO-like object to read/write bytes to */
    kso iio;


    /* Number of streams */
    int nstreams;

    /* Array of stream data */
    struct ksav_IO_stream {

#ifdef KS_HAVE_libav

        /* Index into 'fmtctx' */
        int fcidx;

        /* Encoder/decoder context */
        AVCodecContext* codctx;

#endif

    }* streams;

#ifdef KS_HAVE_libav

    /* libav container */
    AVFormatContext* fmtctx;

    /* Quick buffers (not threadsafe, so they are set to NULL when being used) */
    AVFrame* qf;
    AVPacket* qp;

#endif

}* ksav_IO;




/* Functions */


#ifdef KS_HAVE_libav

/* Custom callback negotiator for 'codctx->get_format' to pick our preferred formats
 */
KS_API enum AVPixelFormat ksav_AV_getformat(struct AVCodecContext* codctx, const enum AVPixelFormat* fmt);

/* Filtering bad pixel formats and returning a better one (retains memory layout)
 * SEE: https://stackoverflow.com/questions/23067722/swscaler-warning-deprecated-pixel-format-used
 */
KS_API enum AVPixelFormat ksav_AV_filterfmt(enum AVPixelFormat pix_fmt);

#endif

/* Opens a media IO (can be file, audio, etc) and return the 'av.IO' container for it
 */
KS_API ksav_IO ksav_open(ks_type tp, ks_str src, ks_str mode);


/* Get the next data item from an 'av.IO', setting '*sidx' to the index of the stream it came from
 */
KS_API kso ksav_next(ksav_IO self, int* sidx);


/* Tests whether stream 'sidx' is an audio stream
 */
KS_API bool ksav_isaudio(ksav_IO self, int sidx, bool* out);

/* Test whether stream 'sidx' is a video stream
 */
KS_API bool ksav_isvideo(ksav_IO self, int sidx, bool* out);

/* Get the best video stream for 'self', and return it, or -1 if there was no video stream (and throw an error)
 */
KS_API int ksav_bestvideo(ksav_IO self);

/* Get the best audio stream for 'self', and return it, or -1 if there was no audio stream (and throw an error)
 */
KS_API int ksav_bestaudio(ksav_IO self);


/* Export */

KS_API extern ks_type
    ksavt_IO
;


#endif /* KSAV_H__ */