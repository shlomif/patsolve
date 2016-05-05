/* Copyright (c) 2012 Shlomi Fish
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
/*
 * str_utils.h - string utilities.
 */
#ifndef FC_SOLVE__STR_UTILS_H
#define FC_SOLVE__STR_UTILS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "inline.h"
#include "bool.h"

#include <string.h>

static GCC_INLINE const fcs_bool_t string_starts_with(
    const char * const str,
    const char * const prefix,
    const char * const end
    )
{
    register const size_t check_len = end-str;

    return
        (
         (check_len == strlen(prefix))
            && (!strncmp(str, prefix, check_len))
        )
        ;
}

static GCC_INLINE const char * const try_str_prefix(
    const char * const str,
    const char * const prefix
)
{
    register const size_t len = strlen(prefix);

    return (strncmp(str, prefix, len) ? NULL : str+len);
}

#ifdef __cplusplus
};
#endif

#endif
