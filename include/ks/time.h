/* ks/time.h - header file for the kscript builtin module 'time'
 *
 * This provides high-level access to time, timezone, and datetime related functionality
 * 
 * Handling time is ugly. Calendars and systems of time are among the worst things humans
 *   have wrought forth. Inconsistent, imperfect, and in many cases, limited by actual
 *   physical phenomenon.
 * 
 * Therefore, this module is not a 'pure' implementation as many other kscript modules are
 *   (i.e. other modules attempt to be very mathematical, and in general, well designed
 *     for any rational beings, not just humans). However, this module, and the tasks therein
 *   are naturally bound to things like the Earth, the stars, and so we must deal with things
 *   like leap seconds, timezones, etc.
 * 
 * So, in this module, there are seemingly redundant definitions, and oddities. However,
 *   see the block comments and the documentation for how to use each. I basically implement
 *   all useful functionality needed, and try to have a pure subset avialable when possible
 *
 * @author: Cade Brown <brown.cade@gmail.com>
 */

#pragma once
#ifndef KSTIME_H__
#define KSTIME_H__

#ifndef KS_H__
#include <ks/ks.h>
#endif /* KS_H__ */

#ifdef KS_HAVE_TIME_H
 #include <time.h>
#endif
#ifdef KS_HAVE_SYS_TIME_H
 #include <sys/time.h>
#endif


/* Constants */

/* The problem with these constants is that sometimes a day is not 24 hours,
 *   or a year is not 365 days due to leap days/seconds. However, these constants
 *   are representative of a real time delta. So, they measure actual elapsed
 *   time in their pure form
 * 
 * You shouldn't manually do math like 'time.now() + HOUR_PER_DAY' and expect that
 *   to return a time representing tomorrow at the same time. But, you can use
 *   them for a rough approximation (for example, if something takes X seconds,
 *   it would be acceptable to say 'takes (X/SEC_PER_MIN) minutes')
 * 
 * So, only use these in time delta conversions, or in rough approximations.
 * 
 */
#define KSTIME_HOUR_PER_DAY        (24)

#define KSTIME_MIN_PER_HOUR        (60)

#define KSTIME_SEC_PER_MIN         (60)

#define KSTIME_MILLI_PER_SEC      (1000)
#define KSTIME_MICRO_PER_SEC      (1000000)
#define KSTIME_NANO_PER_SEC       (1000000000)

#define KSTIME_SEC_PER_DAY (KSTIME_SEC_PER_MIN * KSTIME_MIN_PER_HOUR * KSTIME_HOUR_PER_DAY)


/* Types */

/* 'time.struct' - represents a calender date
 * All values here are 0-indexed
 */
typedef struct kstime_struct_s {
    KSO_BASE

    /* Year (absolute)
     * Can be any value
     */
    ks_cint year;
    
    /* Month within the year
     * For most calendar systems, is in the range(12)
     */
    ks_cint month;

    /* Day within the month
     * For most calendar systems, is in range(31)
     */
    ks_cint day;

    /* Hour within the day
     * For most calendar systems, in range(24)
     */
    ks_cint hour;

    /* Minute within the hour
     * For most calendar systems, in range(60)
     */
    ks_cint min;

    /* Second within the minute
     * For most calendar systems, in range(60)
     */
    ks_cint sec;

    /* Day within the week
     * By convention, Monday==0
     * For most calendar systems, in range(7)
     */
    ks_cint w_day;

    /* Day within the year
     * For most calendar systems, in range(366)
     */
    ks_cint y_day;

    /* Whether it is currently in daylight savings time
     */
    bool is_dst;

    /* Timezone name (or NULL if not known) */
    ks_str zone;

    /* Seconds off of UTC (east) */
    ks_cint off_gmt;

}* kstime_struct;


/* Functions */

/** Timers/clocks **/

/* Return the time since epoch, in seconds
 */
KS_API ks_cfloat kstime_time();
KS_API ks_cint kstime_timei();


/** Time Structure Conversions **/

/* Convert a time-since-epoch value into a UTC time-struct using 'gmtime()'
 */
KS_API kstime_struct kstime_utc(ks_cint tse);

/* Convert a time-since-epoch value into a localtime time-struct using 'localtime()'
 */
KS_API kstime_struct kstime_local(ks_cint tse);


/** String <-> Time **/

/* Apply string formatting to a time-struct, using similar semantics to 'strftime()'
 * Format Codes:
 *   %y: Year modulo 100 (00...99)
 *   %Y: Year with century, in format '%-04i' (0001...9999)
 *   %b: Month in locale (abbreviated)
 *   %B: Month in locale (full)
 *   %m: Month as zero-padded number (01...12)
 *   %U: Week of the year as as a decimal number (Sunday==0)
 *         (days before the first sunday are week 0)
 *   %W: Week of the year as as a decimal number (Monday==0)
 *         (days before the first monday are week 0)
 *   %j: Yearday in '%03i' (001...366)
 *   %d: Monthday as zero-padded number (01...31)
 *   %a: Weekday in locale (abbreviated)
 *   %A: Weekday in locale (full)
 *   %w: Weekday as integer (0==Monday)
 *   %H: Hour (in 24-hour clock) as zero-padded number (00...23)
 *   %I: Hour (in 12 hour clock) as zero-padded number (00, 12)
 *   %M: Minute as zero-padded number (00-59)
 *   %S: Seconds as zero-padded decimal number (00-59)
 *   %f: Microsecond as decimal number, formatted as '%-06i'
 *   %z: Zone UTC offset in '(+|-)HHMM[SS.[ffffff]]'
 *   %Z: Zone name (or empty if there was none)
 *   %p: Locale's equiv of AM/PM
 *   %c: Locale's full default date/time representation
 *   %x: Locale's default date representation
 *   %X: Locale's default time representation
 *   %%: Literal '%'
 */
KS_API ks_str kstime_fmt(const char* fmt, kstime_struct ts);

/* Parse a string, according to a format string (see 'kstime_fmt()' for format)
 */
KS_API kstime_struct kstime_parse(const char* fmt, const char* str);


/* Convert a time-since-epoch into a text representation
 * The result will be in the local time configuration
 */
KS_API ks_str kstime_asc(ks_cint tse);

/* Convert a time-since-epoch to ISO8601 format, in UTC timezone
 * SEE: https://en.wikipedia.org/wiki/ISO_8601
 */
KS_API ks_str kstime_iso_utc(ks_cint tse);


/* Exported */

KS_API extern ks_type

    kstime_T_struct,
    kstime_T_delta

;

#endif /* KSTIME_H__ */