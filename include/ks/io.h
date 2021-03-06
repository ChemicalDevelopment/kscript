/* ks/io.h - header for the 'io' (input/output) module of kscript
 *
 * Provides general interfaces to file streams, buffer streams, and more
 * 
 * General methods (on io.BaseIO): 
 *   s.read(sz=none): Read a given size (default: everything left) message and return it
 *   s.write(msg): Write a (string, bytes, or object) to the stream
 *   s.seek(pos, whence=io.Seek.SET): Seek to a position
 *   s.trunc(sz=none): Truncate an open stream
 *   s.close(): Close a stream
 *   s.eof(): Tells if at the EOF
 * 
 * @author:    Cade Brown <cade@kscript.org>
 */

#pragma once
#ifndef KSIO_H__
#define KSIO_H__

#ifndef KS_H__
#include <ks/ks.h>
#endif



/** Constants **/


/* Buffer size */
#ifndef KSIO_BUFSIZ
  #ifdef BUFSIZ
    #define KSIO_BUFSIZ BUFSIZ
  #elif
    #define KSIO_BUFSIZ 1024
  #endif
#endif


/* Enum for 'ksio_seek()' function */
enum {

    /* Relative to the start of the stream */
    KSIO_SEEK_SET = 0,

    /* Relative to the current position */
    KSIO_SEEK_CUR = 1,

    /* Relative to the end of the stream */
    KSIO_SEEK_END = 2,

};

/** Types **/

/* Any Input/Output stream */
typedef kso ksio_BaseIO;


#define KSIO_RAWIO_BASE \
    KSO_BASE \
    /* File descriptor (from 'open()' or similar) */ \
    int fd; \
    /* If true, 'close(fd)' after the IO is done */ \
    bool do_close; \
    /* Number of bytes read and written (not rigorous, don't rely on these) */ \
    ks_ssize_t sz_r, sz_w; \
    /* The name of the source, and mode it was opened in */ \
    ks_str src, mode; \
    /* Mode information */ \
    bool mr, mw, mb;


/* 'io.RawIO' - represents an open file descriptor
 */
typedef struct ksio_RawIO_s {
    KSIO_RAWIO_BASE

}* ksio_RawIO;


/* 'io.FileIO' - represents a file that can be read or written
 */
typedef struct ksio_FileIO_s {
    KSO_BASE

    /* File pointer (from 'fopen()' or similar) */
    FILE* fp;

    /* If true, actually close 'fp' once the object has been closed */
    bool do_close;

    /* Number of bytes read and written (not rigorous, don't rely on these) */
    ks_ssize_t sz_r, sz_w;

    /* The name of the source, and mode it was opened in */
    ks_str src, mode;

    /* Mode information */
    bool mr, mw, mb;

}* ksio_FileIO;


/* 'io.StringIO' - in-memory stream of Unicode text
 */
typedef struct ksio_StringIO_s {
    KSO_BASE

    /* Array of data, allocated with 'ks_malloc()' */
    unsigned char* data;

    /* Current position in 'data' */
    ks_ssize_t pos_b, pos_c;

    /* Current size of 'data' */
    ks_ssize_t len_b, len_c;

    /* Maximum length 'data' is allocated for */
    ks_ssize_t max_len_b;

    /* Number of bytes read and written (not rigorous, don't rely on these) */
    ks_ssize_t sz_r, sz_w;


}* ksio_StringIO;

/* 'io.BytesIO' - like 'io.StringIO', but for binary data
 */
typedef ksio_StringIO ksio_BytesIO;


/** Unicode Translation **/

/* Yields whether or not a byte is a continuation byte in UTF8 */
#define KS_UCP_IS_CONT(_bt) (((_bt) & 0xC0) == 0x80)

/* Decodes a single character from UTF8 source
 *
 * Set '_to' (which should be a 'ks_ucp') to the ordinal given by '_src' (in UTF8 encoding, as a 'char*') 
 * And, set '_n' to the number of bytes read
 */
#define KS_UCP_FROM_UTF8(_to, _src, _n) do { \
    unsigned char _c = (_src)[0]; \
    if (_c < 0x80) { \
        _to = _c; \
        _n = 1;   \
    } else if ((_c & 0xE0) == 0xC0) { \
        _to = ((ks_ucp)(_c & 0x1F) << 6) \
            | ((ks_ucp)((_src)[1] & 0x3F) << 0); \
        _n = 2; \
    } else if ((_c & 0xF0) == 0xE0) { \
        _to = ((ks_ucp)(_c & 0x0F) << 12) \
            | ((ks_ucp)((_src)[1] & 0x3F) << 6) \
            | ((ks_ucp)((_src)[2] & 0x3F) << 0); \
        _n = 3; \
    } else if ((_c & 0xF8) == 0xF0 && (_c <= 0xF4)) { \
        _to = ((ks_ucp)(_c & 0x07) << 18) \
            | ((ks_ucp)((_src)[1] & 0x3F) << 12) \
            | ((ks_ucp)((_src)[2] & 0x3F) << 6) \
            | ((ks_ucp)((_src)[3] & 0x3F) << 0); \
        _n = 4; \
    } else { \
        _to = 0; \
        _n = -1; \
    } \
} while (0)

/* Encode a single character to UTF8
 *
 * Expects '_src' to be a 'ks_ucp', '_to' to be a 'char*',
 *   and '_n' be an assignable name which tells how many bytes were written
 *   to '_to'. '_to' should have at least 4 bytes allocated after it
 * 
 * See: https://www.fileformat.info/info/unicode/utf8.htm
 */
#define KS_UCP_TO_UTF8(_to, _n, _src) do { \
    if (_src <= 0x7F) { \
        _n = 1; \
        _to[0] = _src; \
    } else if (_src <= 0x7FF) { \
        _n = 2; \
        _to[0] = 0xC0 | ((_src >>  6) & 0x1F); \
        _to[1] = 0x80 | ((_src >>  0) & 0x3F); \
    } else if (_src <= 0xFFFF) { \
        _n = 3; \
        _to[0] = 0xE0 | ((_src >> 12) & 0x0F); \
        _to[1] = 0x80 | ((_src >>  6) & 0x3F); \
        _to[2] = 0x80 | ((_src >>  0) & 0x3F); \
    } else if (_src <= 0x10FFFF) { \
        _n = 4; \
        _to[0] = 0xF0 | ((_src >> 18) & 0x0F); \
        _to[1] = 0x80 | ((_src >> 12) & 0x3F); \
        _to[2] = 0x80 | ((_src >>  6) & 0x3F); \
        _to[3] = 0x80 | ((_src >>  0) & 0x3F); \
    } else { \
        _n = -1; \
        _to[0] = 0; \
    } \
} while (0)


/** Functions **/

/* Close the IO object
 */
KS_API bool ksio_close(ksio_BaseIO self);

/* Seek the IO object to 'pos' from 'whence' (which is one of 'KSIO_SEEK_*')
 */
KS_API bool ksio_seek(ksio_BaseIO self, ks_cint pos, int whence);

/* Tell where the current position is, from the start of the stream, or negative if an error was thrown
 */
KS_API ks_cint ksio_tell(ksio_BaseIO self);

/* Tell whether we've hit the EOF
 */
KS_API bool ksio_eof(ksio_BaseIO self, bool* out);

/* Truncate the IO object to 'sz'
 */
KS_API bool ksio_trunc(ksio_BaseIO self, ks_cint sz);

/* Read up to 'sz_b' bytes, and store in 'data'
 *
 * Number of bytes written is returned, or negative number on an error
 */
KS_API ks_ssize_t ksio_readb(ksio_BaseIO self, ks_ssize_t sz_b, void* data);

/* Read up to 'sz_c' characters (real number stored in '*num_c')
 *
 * Writes characters in UTF8 format to 'data', which should have been allocated for 'sz_c * 4' bytes
 * Number of bytes written is returned, or negative number on an error
 */
KS_API ks_ssize_t ksio_reads(ksio_BaseIO self, ks_ssize_t sz_c, void* data, ks_ssize_t* num_c);

/* Write 'sz_b' bytes of data to the file
 */
KS_API bool ksio_writeb(ksio_BaseIO self, ks_ssize_t sz_b, const void* data);

/* Write 'sz_b' of 'data' (which should be in UTF8 encoding) to the file
 */
KS_API bool ksio_writes(ksio_BaseIO self, ks_ssize_t sz_b, const void* data);


/* Adds a C-style printf-like formatting to the output stream
 */
KS_API bool ksio_fmt(ksio_BaseIO self, const char* fmt, ...);
KS_API bool ksio_fmtv(ksio_BaseIO self, const char* fmt, va_list ap);

/* Macro for automatic casting */
#define ksio_add(_self, ...) (ksio_fmt((ksio_BaseIO)(_self), __VA_ARGS__))
#define ksio_addv(_self, ...) (ksio_fmtv((ksio_BaseIO)(_self), __VA_ARGS__))
#define ksio_addbuf(_self, ...) (ksio_writeb((ksio_BaseIO)(_self), __VA_ARGS__))

/** Specific Types **/

/* Return a wrapper around an opened C-style file descriptor
 */
KS_API ksio_RawIO ksio_RawIO_wrap(ks_type tp, int fd, bool do_close, ks_str src, ks_str mode);

/* Return a wrapper around an opened C-style FILE*
 */
KS_API ksio_FileIO ksio_FileIO_wrap(ks_type tp, FILE* fp, bool do_close, ks_str src, ks_str mode);

/* Open a file descriptor
*/
KS_API ksio_FileIO ksio_FileIO_fdopen(int fd, ks_str src, ks_str mode);

/* Create a new StringIO
 */
KS_API ksio_StringIO ksio_StringIO_new();

/* Get the current string
 * 'getf' also calls 'KS_DECREF(self)'
 */
KS_API ks_str ksio_StringIO_get(ksio_StringIO self);
KS_API ks_str ksio_StringIO_getf(ksio_StringIO self);


/* Create a new BytesIO
 */
KS_API ksio_BytesIO ksio_BytesIO_new();

/* Get the current contents
 * 'getf' also calls 'KS_DECREF(self)'
 */
KS_API ks_bytes ksio_BytesIO_get(ksio_BytesIO self);
KS_API ks_bytes ksio_BytesIO_getf(ksio_BytesIO self);

/** Misc. Utils **/

/* Get mode information about a given stream or mode
 */
KS_API bool ksio_info(ksio_BaseIO self, bool* is_r, bool* is_w, bool* is_b);
KS_API bool ksio_modeinfo(ks_str mode, bool* is_r, bool* is_w, bool* is_b);


/* Read entire file and return as a string. Returns NULL and throws an error if there was a problem
 */
KS_API ks_str ksio_readall(ks_str fname);

/* Read entire object (may be 'str' for source, or IO object)
 */
KS_API ks_bytes ksio_readallo(kso obj);

/* Portable implementation of the 'getline()' function, which reads an entire line from a FILE* pointer
 *
 * Example:
 * char* line = NULL;
 * ks_ssize_t sz = 0;
 * sz = ksu_getline(&line, &n, stdin);
 * ks_free(line);
 *
 */
KS_API ks_ssize_t ksu_getline(char** lineptr, ks_ssize_t* n, FILE* fp);

/* Types */
KS_API_DATA ks_type
    ksiot_BaseIO,
    ksiot_RawIO,
    ksiot_FileIO,
    ksiot_StringIO,
    ksiot_BytesIO
;

/* Enums */
KS_API_DATA ks_type
    ksioe_Seek
;

#endif /* KSIO_H__ */
