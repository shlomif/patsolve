/* Copyright (c) 2002 Tom Holroyd
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
/* Standard utilities. */

#ifndef _UTIL_H
#define _UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#ifdef  __cplusplus
extern "C" {
#endif

/* A function and some macros for allocating memory. */

extern void *new_(size_t s);
extern long Mem_remain;

#define new(type) (type *)new_(sizeof(type))
#define free_ptr(ptr, type) free(ptr); Mem_remain += sizeof(type)

#define new_array(type, size) (type *)new_((size) * sizeof(type))
#define free_array(ptr, type, size) free(ptr); \
				    Mem_remain += (size) * sizeof(type)

extern void free_buckets(void);
extern void free_clusters(void);
extern void free_blocks(void);

/* Error messages. */
extern void fc_solve_msg(char *msg, ...);

#ifdef  __cplusplus
}
#endif

#endif  /* _UTIL_H */
