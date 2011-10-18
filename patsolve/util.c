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

#include <stdarg.h>

#include "util.h"
#include "pat.h"

/* Just print a message. */

void fc_solve_msg(char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	vfprintf(stderr, msg, ap);
	va_end(ap);
}


/* Allocate some space and return a pointer to it.  See new() in util.h. */

void *new_(fc_solve_soft_thread_t * soft_thread, size_t s)
{
	void *x;

	if (s > soft_thread->Mem_remain) {
#if 0
		POSITION *pos;

		/* Try to get some space back from the freelist. A vain hope. */

		while (Freepos) {
			pos = Freepos->queue;
			free_array(Freepos, u_char, sizeof(POSITION) + Ntpiles);
			Freepos = pos;
		}
		if (s > soft_thread->Mem_remain) {
			Status = FAIL;
			return NULL;
		}
#else
		Status = FAIL;
		return NULL;
#endif
	}

	if ((x = (void *)malloc(s)) == NULL) {
		Status = FAIL;
		return NULL;
	}

	soft_thread->Mem_remain -= s;
	return x;
}
