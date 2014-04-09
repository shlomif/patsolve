/* This file was generated by param.py.  Do not edit. */

#ifndef PATSOLVE_PARAM_H
#define PATSOLVE_PARAM_H

#define NXPARAM 11
#define NYPARAM 3

typedef struct {
    int x[NXPARAM];
    double y[NYPARAM];
} fcs_pats_xy_param_t;

extern const fcs_pats_xy_param_t freecell_solver_pats__x_y_params_preset[];

#define FreecellSpeed 0
#define FreecellBest 1
#define FreecellBestA 2
#define SeahavenBest 3
#define SeahavenBestA 4
#define SeahavenSpeed 5
#define SeahavenKing 6
#define SeahavenKingSpeed 7
#define LastParam 7

#endif /* PATSOLVE_PARAM_H */
