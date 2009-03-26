/*==========================================================================*/
/* Runge-Kutta Solvers, (C) Th. Petzoldt, License: GPL >=2                  */
/*  rk4 Fixed Step Integrator                                               */
/*    (special version for speed comparison with the general solution)      */
/*==========================================================================*/

#include "rk_util.h"

SEXP call_rk4(SEXP Xstart, SEXP Times, SEXP Func, SEXP Initfunc,
	      SEXP Parms, SEXP Nout, SEXP Rho, SEXP Verbose,
	      SEXP Rpar, SEXP Ipar) {

  /**  Initialization **/
  init_N_Protect();

  double *tt = NULL, *xs = NULL;
  double *tmp, *FF, *out;

  SEXP  R_y, R_f, R_f1, R_f2, R_f3, R_f4;
  double *y,  *f,  *f1,  *f2,  *f3,  *f4;

  SEXP  R_y0, R_yout;
  double *y0,  *yout;


  double t, dt;
  int i = 0, it=0, nt = 0, neq=0;

  /**************************************************************************/
  /****** Check Arguments and Convert to C types                       ******/
  /**************************************************************************/

  PROTECT(Times = AS_NUMERIC(Times)); incr_N_Protect();
  tt = NUMERIC_POINTER(Times);
  nt = length(Times);

  PROTECT(Xstart = AS_NUMERIC(Xstart)); incr_N_Protect();
  xs  = NUMERIC_POINTER(Xstart);
  neq = length(Xstart);

  tmp =  (double *) R_alloc(neq, sizeof(double));
  FF  =  (double *) R_alloc(neq, sizeof(double));

  int  nout  = (int)REAL(Nout)[0]; // n of external outputs if func is in a DLL
  out  =  (double *) R_alloc(nout, sizeof(double));

  int verbose = (int)REAL(Verbose)[0];

  /**************************************************************************/
  /****** DLL, ipar, rpar (to be compatible with lsoda)                ******/
  /**************************************************************************/
  // provisional code; copy from call_rkauto
  int isDll = 0;
  if (inherits(Func, "NativeSymbol")) isDll = 1;

  /**************************************************************************/
  /****** Allocation of Workspace                                      ******/
  /**************************************************************************/

  PROTECT(R_y0 = allocVector(REALSXP, neq)); incr_N_Protect();
  PROTECT(R_f  = allocVector(REALSXP, neq)); incr_N_Protect();
  PROTECT(R_y  = allocVector(REALSXP, neq)); incr_N_Protect();
  PROTECT(R_f1 = allocVector(REALSXP, neq)); incr_N_Protect();
  PROTECT(R_f2 = allocVector(REALSXP, neq)); incr_N_Protect();
  PROTECT(R_f3 = allocVector(REALSXP, neq)); incr_N_Protect();
  PROTECT(R_f4 = allocVector(REALSXP, neq)); incr_N_Protect();
  y0 = REAL(R_y0);
  f  = REAL(R_f);
  y  = REAL(R_y);
  f1 = REAL(R_f1);
  f2 = REAL(R_f2);
  f3 = REAL(R_f3);
  f4 = REAL(R_f4);

  // matrix for holding the outputs
  PROTECT(R_yout = allocMatrix(REALSXP, nt, neq + nout + 1)); incr_N_Protect();
  yout = REAL(R_yout);

  // attribute that stores state information, similar to lsoda
  SEXP R_istate;
  int *istate;
  PROTECT(R_istate = allocVector(INTSXP, 22)); incr_N_Protect();
  istate = INTEGER(R_istate);
  istate[0] = 0; // assume succesful return
  for (i = 0; i < 22; i++) istate[i] = 0;

  //PROTECT(RSTATE = allocVector(REALSXP, 5));incr_N_Protect();
  //for (k = 0;k<5;k++) REAL(RSTATE)[k] = rwork[k+10];

  /**************************************************************************/
  /****** Initialization of Parameters (for DLL functions)             ******/
  /**************************************************************************/

  initParms(Initfunc, Parms);

  /**************************************************************************/
  /****** Initialization of Integration Loop                           ******/
  /**************************************************************************/

  yout[0] = tt[0]; //initial time
  for (i = 0; i < neq; i++) {
    y0[i]              = xs[i];
    yout[(i + 1) * nt] = y0[i];      // <--- check this
  }

  /**************************************************************************/
  /****** Main Loop                                                    ******/
  /**************************************************************************/
  for (it = 0; it < nt - 1; it++) {
    t = tt[it];
    dt = tt[it + 1] - t;
    if (verbose)
      Rprintf("Time steps = %d / %d time = %e\n", it + 1, nt, t);
    derivs(Func, t, y0, Parms, Rho, f1, out, 0, neq, nout, isDll);
    for (i = 0; i < neq; i++) {
      f1[i] = dt * f1[i];
      f[i]  = y0[i] + 0.5 * f1[i];
    }
    derivs(Func, t + 0.5*dt, f, Parms, Rho, f2, out, 0, neq, nout, isDll);
    for (i = 0; i < neq; i++) {
      f2[i] = dt * f2[i];
      f[i]  = y0[i] + 0.5 * f2[i];
    }
    derivs(Func, t + 0.5*dt, f, Parms, Rho, f3, out, 0, neq, nout, isDll);
    for (i = 0; i < neq; i++) {
      f3[i] = dt * f3[i];
      f[i] = y0[i] + f3[i];
    }
    derivs(Func, t + dt, f, Parms, Rho, f4, out, 0, neq, nout, isDll);
    for (i = 0; i < neq; i++) {
      f4[i] = dt * f4[i];
    }
    // Final computation of y
    for (i = 0; i < neq; i++) {
      f[i]  = (f1[i] + 2.0 * f2[i] + 2.0 * f3[i] + f4[i]) / 6.0;
      y[i]  = y0[i] + f[i];
      y0[i] = y[i]; // next time step
    }
    // store outputs
    if (it < nt) {
      yout[it + 1] = t + dt;
      for (i = 0; i < neq; i++) yout[it + 1 + nt * (1 + i)] = y[i];
    }
  } // end of rk main loop

  /**************************************************************************/
  /* call derivs again to get external outputs                              */
  /**************************************************************************/
  // j= -1 suppresses internal copying
  for (int j = 0; j < nt; j++) {
    t = yout[j];
    for (i = 0; i < neq; i++) tmp[i] = yout[j + nt * (1 + i)];
    derivs(Func, t, tmp, Parms, Rho, FF, out, -1, neq, nout, isDll);
    for (i = 0; i < nout; i++) {
      yout[j + nt * (1 + neq + i)] = out[i];
    }
  }
  // attach essential internal information (codes are compatible to lsoda)
  // ToDo: respect function evaluations due to external outputs
  // ToDo: it_tot; stage; fsal; qerr
  //setIstate(R_yout, R_istate, istate, it_tot, stage, fsal, qerr);

  // release R resources
  unprotect_all();
  //init_N_Protect();
  return(R_yout);
}
