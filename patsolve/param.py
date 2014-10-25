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

#define FC_SOLVE_PATS__NUM_X_PARAM %d
#define FC_SOLVE_PATS__NUM_Y_PARAM %d

typedef struct {
    int x[FC_SOLVE_PATS__NUM_X_PARAM];
    double y[FC_SOLVE_PATS__NUM_Y_PARAM];
} fcs_pats_xy_param_t;

extern const fcs_pats_xy_param_t freecell_solver_pats__x_y_params_preset[];

""" % (NXPARAM, NYPARAM))

c.write("""/* This file was generated by param.py.  Do not edit. */

#include "param.h"

const fcs_pats_xy_param_t freecell_solver_pats__x_y_params_preset[] = {
""")

p = 0
f = open(filename)
for line in f.xreadlines():
    if line[0] == '#' or line[0] == '\n':
        continue
    l = string.split(line)
    h.write("#define FC_SOLVE_PATS__PARAM_PRESET__%s %d\n" % (l[0], p))
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

h.write("\n#endif /* PATSOLVE_PARAM_H */\n")

c.close()
h.close()
