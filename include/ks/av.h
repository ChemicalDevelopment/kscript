/* ks/av.h - header file for the kscript audio/video/media builtin module `import av'
 *
 * The general idea of this module is to provide an audio/video encoding/decoding interface
 *   available to kscript. The most important thing, as with other kscript standard modules,
 *   is to provide a consistent and general interface that can be implemented in any number of ways
 * 
 * Right now, the following modes are supported:
 *   * No external libraries
 *     * Image formats: .gif, .jpg, .png, .tga
 *     * Audio formats: .wav, .ogg
 *   * Built with libavcodec, libavutil, libavformat, and libswscale
 *     * Video containers: Supported
 *     * Image formats: (many, many)
 *     * Audio formats: (many, many)
 * 
 * Public API:
 *   * av.open(src, mode='r') - Return an IO-like object that can be read and written from (see 'av.IO')
 *   * av.imread(src) - Reads an image from 'src', which should be an IO-like object or a filename
 *   * av.imwrite(src, data, fmt=none) - Writes an image to 'src' (image data in 'data'), with optional 'fmt' (default: inferred)
 * 
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

    /* Mutex, so the IO object is only accessed in a single thread at a time */
    ksos_mutex mut;

    /* Number of streams */
    int nstreams;

    /* Queue item, of unconsumed packets */
    struct ksav_IO_queue_item {
        /* Pointer to the next item in the queue */
        struct ksav_IO_queue_item* next;

        /* Stream index the item came from */
        int sidx;

#ifdef KS_HAVE_libav

        /* Packet which represents a still-encoded frame from
         *   'av_read_frame()'
         */
        AVPacket* packet;
#endif


    } *queue_first, *queue_last;


    /* Array of stream data */
    struct ksav_IO_stream {

#ifdef KS_HAVE_libav

        /* Index into 'fmtctx' */
        int fcidx;

        /* Encoder/decoder context */
        AVCodecContext* codctx;

#else
		int dummy;
#endif

    }* streams;

#ifdef KS_HAVE_libav

    /* libav custom IO functions */
    AVIOContext* avioctx;

    /* Whether the call back had an exception */
    bool cbexc;

    /* libav container */
    AVFormatContext* fmtctx;

    /* Quick buffers (not threadsafe, so only access when the mutex is held) */
    AVFrame* frame;
    AVPacket* packet;

    /* Software scaling context (used with 'sws_getCachedContext') */
    struct SwsContext* swsctx;

#endif

}* ksav_IO;

/* av.Stream - Single audio/video stream
 *
 */
typedef struct ksav_Stream_s {
    KSO_BASE

    /* What IO the stream is a part of */
    ksav_IO of;

    /* Stream index */
    int sidx;

}* ksav_Stream;


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

/* Opens an IO-like object (iio) as an 'av.IO' object, which treats it as a media
 *   container.
 * 
 * If 'iio' is KSO_NONE, then an IO object is created by opening 'src'
 * 
 */
KS_API ksav_IO ksav_open(ks_type tp, kso iio, ks_str src, ks_str mode);

/* Get the next data item from an 'av.IO', setting '*sidx' to the index of the stream it came from
 * 
 * Typically returns an `nx.array` containing the pixel/audio data of either:
 * 
 *   * (h, w, d) (video)
 *   * (n, c) (audio)
 * 
 * If 'nvalid >= 0', then 'valid' should point to an array of valid indexes, and all packets from other
 *   streams will be ignored temporarily and pushed onto the queue for next time
 */
KS_API kso ksav_next(ksav_IO self, int* sidx, int nvalid, int* valid);


/* Tests whether stream 'sidx' is an audio stream
 */
KS_API bool ksav_isaudio(ksav_IO self, int sidx, bool* out);

/* Test whether stream 'sidx' is a video stream
 */
KS_API bool ksav_isvideo(ksav_IO self, int sidx, bool* out);

/* Get the best video stream for 'self', and return it, or -1 if there was no video stream (and throw an error)
 */
KS_API int ksav_best_video(ksav_IO self);

/* Get the best audio stream for 'self', and return it, or -1 if there was no audio stream (and throw an error)
 */
KS_API int ksav_best_audio(ksav_IO self);


/* Get a stream wrapper for a specific index
 */
KS_API ksav_Stream ksav_getstream(ksav_IO self, int sidx);


/** One-shot API **/

/* Read an image from a file
 * 'src' may be a:
 *   str: Source string, i.e. a URL
 * Otherwise, it is assumed to be an IO-like object to read from
 */
KS_API kso ksav_imread(kso src);

/* Write an image to a file
 * 'src' may be a:
 *   str: Source string, i.e. a URL
 * Otherwise, it is assumed to be an IO-like object to write to
 */
KS_API bool ksav_imwrite(kso src, nx_t data, kso fmt);



/* Export */

KS_API_DATA ks_type
    ksavt_IO,
    ksavt_Stream
;


#endif /* KSAV_H__ */
