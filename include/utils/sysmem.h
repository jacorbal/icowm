/**
 * @file utils/sysmem.h
 *
 * @brief Memory reading from the Linux @c /proc pseudo-filesystem:
 *        system-wide available memory, and this process's
 *        resident memory usage
 *
 * Used by restricted-memory mode @c (icowm -M).  The parameter
 * @p sysmem_available_mib decides whether there is enough free system
 * memory to start at all, and @c sysmem_self_rss_mib later watches this
 * process's usage against the configured ceiling once running.
 *
 * @see @c defs/main.h and @c wm.c for how the two are wired together
 *
 * @ingroup utils
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef UTILS_SYSMEM_H
#define UTILS_SYSMEM_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>


/** Path to the Linux kernel's system memory statistics pseudo-file */
#define SYSMEM_MEMINFO_FILE "/proc/meminfo"

/** Path to the calling process's status pseudo-file */
#define SYSMEM_SELF_STATUS_FILE "/proc/self/status"

/**
 * @brief Maximum bytes read from either @c SYSMEM_MEMINFO_FILE or
 *        @c SYSMEM_SELF_STATUS_FILE in one call
 *
 * Either real file is a few hundred bytes on any actual Linux kernel,
 * this is generous headroom, not a value tuned to today's exact size of
 * either.
 */
#define SYSMEM_LINE_MAX_LEN (4096)


/**
 * @brief Read the kernel's estimate of currently available system
 *        memory
 *
 * Reads the @c MemAvailable line from @c SYSMEM_MEMINFO_FILE: the
 * kernel's estimate of how much memory is available for starting
 * new applications without swapping, already accounting for reclaimable
 * caches and buffers, so this is a meaningfully better signal than
 * @c MemFree alone would be.  Linux-specific; on any other kernel, or
 * if the file cannot be read or parsed, returns @c false and leaves
 * @p out_mib untouched.
 *
 * @param out_mib Receives the available memory, in mebibytes, rounded
 *                down
 *
 * @return @c true on success
 *
 * @note Complexity: @e O(1), a single short file read
 */
bool sysmem_available_mib(uint32_t *out_mib);

/**
 * @brief Read the calling process's current resident memory usage
 *
 * Reads the @c VmRSS line from @c SYSMEM_SELF_STATUS_FILE.  The portion
 * of this process's memory currently held in RAM (as opposed to
 * swapped out, or merely reserved address space never actually
 * touched), the same figure tools like @c top and @c ps report.
 *
 * @param out_mib Receives this process's resident memory usage, in
 *                mebibytes, rounded down
 *
 * @return @c true on success
 *
 * @remark Linux-specific; on any other kernel, or if the file cannot be
 *         read or parsed, returns @c false and leaves @p out_mib
 *         untouched
 *
 * @note Complexity: @e O(1), a single short file read
 */
bool sysmem_self_rss_mib(uint32_t *out_mib);


#endif  /* ! UTILS_SYSMEM_H */
