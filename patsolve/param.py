#! /usr/bin/env python

import sys, string
# from util import *

# usage("""filename""")

filename = 'param.dat'

c = open('param.c', 'w')
h = open('param.h', 'w')

NXPARAM = 11
NYPARAM = 3

h.write("""/* This file was generated by param.py.  Do not edit. */

#ifndef PATSOLVE_PARAM_H
#define PATSOLVE_PARAM_H

#define NXPARAM %d
#define NYPARAM %d

typedef struct {
	int x[NXPARAM];
	double y[NYPARAM];
} XYparam_t;

extern const XYparam_t XYparam[];

#endif /* PATSOLVE_PARAM_H */
""" % (NXPARAM, NYPARAM))

c.write("""/* This file was generated by param.py.  Do not edit. */

#include "param.h"

const XYparam_t XYparam[] = {
""")

p = 0
f = open(filename)
for line in f.xreadlines():
	if line[0] == '#' or line[0] == '\n':
		continue
	l = string.split(line)
	h.write("#define %s %d\n" % (l[0], p))
	c.write(" {{ ")
	for x in l[1:NXPARAM + 1]:
		c.write("%s, " % x)
	c.write("}, { ")
	for y in l[NXPARAM + 1:]:
		c.write("%s, " % y)
	c.write("}},\n")
	p += 1

c.write("};\n")

h.write("#define LastParam %d\n" % (p - 1))

c.close()
h.close()
