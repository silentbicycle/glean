#ifndef GLEAN_H
#define GLEAN_H

#define GLN_VERSION_STRING "000101"

#ifdef NDEBUG
#define DEBUG 0
#else
#define DEBUG 0
#endif

#define DB_DEBUG (DEBUG || 0)

#define DEBUG_HASH 0

#define MAX_WORD_SZ 512
#define MIN_WORD_SZ 3

#define DEF_ZLIB_COMPRESS 6
#define PROFILE_COMPRESSION 0

/* Compression helps quite a bit more with the filename DB than the token DB,
 * but it's cheap, so use it. */
#define SKIP_COMPRESS 0

/* Should we use u_int16_t or u_int32_t for hashes? */
#define FOUR_BYTE_HASH 1

#if FOUR_BYTE_HASH
typedef u_int32_t hash_t;
#else
typedef u_int16_t hash_t;
#endif

#define HASH_BYTES sizeof(hash_t)

/* How many tokenizer processes? */
#define DEF_WORKER_CT 8
#define MAX_WORKER_CT 64

/* Number of 'X's to use for database data alignment */
#define DB_X_CT 1

/* If using stop words, how many relatively-flat differences are
 * needed before stopping. TODO configure */
#define FLAT_CT 5
#define CHANGE_FACTOR 0.75

/* If file's first read has less % printable bytes than this, skip it. */
#define MIN_TOKEN_PRINTABLE 0.8

/* How many matches per token is worth warning about? */
#define TOO_MANY_MATCHES 25

/* When showing progress, print a message every time this many files are enqueued */
#define ENQUEUE_PROGRESS_CT 1000

/* Just die if we ever use more than this. */
#define MAX_MEMORY (/* 1 GB */ 1024 * 1024 * 1024)

#if defined(__APPLE__)
typedef unsigned int uint;
typedef unsigned long ulong;
#endif

#include "alloc.h"

/* compiler flag for code not-yet-implemented */
#define NYI 0

#endif
