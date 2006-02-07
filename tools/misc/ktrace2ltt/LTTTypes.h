/*
 * LTTTypes.h
 *
 * Included by LinuxEvents.h, defines a handful of things needed by that file
 * so that we can use it unmodified from the LTT distribution. Based on the
 * file of the same name from LTT, which carries the following notice:
 *
 * Copyright (C) 2000 Karim Yaghmour (karym@opersys.com).
 *
 * This is distributed under GPL.
 */

#ifndef __TRACE_TOOLKIT_TYPES_HEADER__
#define __TRACE_TOOLKIT_TYPES_HEADER__

#if LTT_64BIT
typedef uint64_t ltt_hostptr_t;
/* always use unpacked structs in 64-bit mode, to avoid alignment issues */
# undef LTT_UNPACKED_STRUCTS
# define LTT_UNPACKED_STRUCTS 1
# define LTT_ALIGNED_FIELD __attribute__ ((aligned (8)))
#else
typedef uint32_t ltt_hostptr_t;
# define LTT_ALIGNED_FIELD
#endif

/* Structure packing */
#if LTT_UNPACKED_STRUCTS
#define LTT_PACKED_STRUCT
#else
#define LTT_PACKED_STRUCT __attribute__ ((packed))
#endif /* UNPACKED_STRUCTS */

/* Trace mask */
typedef uint64_t trace_event_mask;

/* Boolean stuff */
#define TRUE  1
#define FALSE 0

#endif /* __TRACE_TOOLKIT_TYPES_HEADER__ */
