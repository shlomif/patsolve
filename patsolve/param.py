#! /usr/bin/env python

import sys

filename = sys.argv[1]

with open('param.c', 'w') as c:
    with open('param.h', 'w') as h:

        NXPARAM = 11
        NYPARAM = 3

        h.write("""/* This file was generated by param.py.  Do not edit. */
#pragma once
#include "freecell-solver/fcs_pats_xy_param.h"

extern const fcs_pats_xy_params freecell_solver_pats__x_y_params_preset[];

""")

        c.write("""/* This file was generated by param.py.  Do not edit. */

#include "param.h"

const fcs_pats_xy_params freecell_solver_pats__x_y_params_preset[] = {
""")

        p = 0
        with open(filename) as infh:
            for line in infh:
                if line[0] == '#' or line[0] == '\n':
                    continue
                fields = line.split()
                h.write(
                    "#define FC_SOLVE_PATS__PARAM_PRESET__%s %d\n" %
                    (fields[0], p)
                )
                c.write(" {{ ")
                for x in fields[1:NXPARAM + 1]:
                    c.write("%s, " % x)
                c.write("}, { ")
                for y in fields[NXPARAM + 1:]:
                    c.write("%s, " % y)
                c.write("}},\n")
                p += 1

        c.write("};\n")

        h.write(
            "#define FC_SOLVE_PATS__PARAM_PRESET__LastParam %d\n" % (p - 1)
        )
