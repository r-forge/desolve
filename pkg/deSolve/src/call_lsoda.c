#include <time.h>
#include <string.h>
#include "deSolve.h"

/* definition of the calls to the FORTRAN functions - in file opkdmain.f
and in file dvode.f**/

void F77_NAME(dlsoda)(void (*)(int *, double *, double *, double *, double *, int *),
		     int *, double *, double *, double *,
		     int *, double *, double *, int *, int *,
		     int *, double *,int *,int *, int *,
		     void (*)(int *, double *, double *, int *,
			      int *, double *, int *, double *, int *),
		     int *, double *, int *);

void F77_NAME(dlsode)(void (*)(int *, double *, double *, double *, double *, int *),
		     int *, double *, double *, double *,
		     int *, double *, double *, int *, int *,
		     int *, double *,int *,int *, int *,
		     void (*)(int *, double *, double *, int *,
			      int *, double *, int *, double *, int *),
		     int *, double *, int *);

void F77_NAME(dlsodes)(void (*)(int *, double *, double *, double *, double *, int *),
		     int *, double *, double *, double *,
		     int *, double *, double *, int *, int *,
		     int *, double *, int *, int *, int *,
		     void (*)(int *, double *, double *, int *,
			      int *, int *, double *, double *, int *),   /* jacvec */
		     int *, double *, int *);

void F77_NAME(dlsodar)(void (*)(int *, double *, double *, double *, double *, int *),
		     int *, double *, double *, double *,
		     int *, double *, double *, int *, int *,
		     int *, double *,int *,int *, int *,
		     void (*)(int *, double *, double *, int *,
			      int *, double *, int *, double *, int *), int *, 
		     void (*)(int *, double *, double *, int *, double *),  /* rootfunc */
         int *, int *, double *, int *);

void F77_NAME(dvode)(void (*)(int *, double *, double *, double *,
                              double *, int *),
		     int *, double *, double *, double *,
		     int *, double *, double *, int *, int *,
		     int *, double *,int *,int *, int *,
		     void (*)(int *, double *, double *, int *,
			            int *, double *, int *, double*, int*),
		     int *, double *, int *);

/* wrapper above the derivate function that first estimates the
values of the forcing functions */

static void C_deriv_func_forc (int *neq, double *t, double *y,
                         double *ydot, double *yout, int *iout)
{
  updatedeforc(t);
  DLL_deriv_func(neq, t, y, ydot, yout, iout);
}

/* interface between FORTRAN function call and R function
   Fortran code calls C_deriv_func(N, t, y, ydot, yout, iout) 
   R code called as R_deriv_func(time, y) and returns ydot 
   Note: passing of parameter values and "..." is done in R-function lsodx*/

static void C_deriv_func (int *neq, double *t, double *y,
                          double *ydot, double *yout, int *iout)
{
  int i;
  SEXP R_fcall, ans;

                              REAL(Time)[0] = *t;
  for (i = 0; i < *neq; i++)  REAL(Y)[i] = y[i];

  PROTECT(R_fcall = lang3(R_deriv_func,Time,Y));   incr_N_Protect();
  PROTECT(ans = eval(R_fcall, R_envir));           incr_N_Protect();

  for (i = 0; i < *neq; i++)   ydot[i] = REAL(VECTOR_ELT(ans,0))[i];

  my_unprotect(2);
}

/* only if lsodar: 
   interface between FORTRAN call to root and corresponding R function */

static void C_root_func (int *neq, double *t, double *y, int *ng, double *gout)
{
  int i;
  SEXP R_fcall, ans;
                              REAL(Time)[0] = *t;
  for (i = 0; i < *neq; i++)  REAL(Y)[i] = y[i];

  PROTECT(R_fcall = lang3(R_root_func,Time,Y));   incr_N_Protect();
  PROTECT(ans = eval(R_fcall, R_envir));          incr_N_Protect();

  for (i = 0; i < *ng; i++)   gout[i] = REAL(ans)[i];

  my_unprotect(2);
}

/* interface between FORTRAN call to jacobian and R function */

static void C_jac_func (int *neq, double *t, double *y, int *ml,
		    int *mu, double *pd, int *nrowpd, double *yout, int *iout)
{
  int i;
  SEXP R_fcall, ans;

                             REAL(Time)[0] = *t;
  for (i = 0; i < *neq; i++) REAL(Y)[i] = y[i];

  PROTECT(R_fcall = lang3(R_jac_func,Time,Y));    incr_N_Protect();
  PROTECT(ans = eval(R_fcall, R_envir));          incr_N_Protect();

  for (i = 0; i < *neq * *nrowpd; i++)  pd[i] = REAL(ans)[i];

  my_unprotect(2);
}

/* only if lsodes: 
   interface between FORTRAN call to jacvec and corresponding R function */

static void C_jac_vec (int *neq, double *t, double *y, int *j,
		    int *ian, int *jan, double *pdj, double *yout, int *iout)
{
  int i;
  SEXP R_fcall, ans, J;
  PROTECT(J = NEW_INTEGER(1));                  incr_N_Protect();
                             INTEGER(J)[0] = *j;
                             REAL(Time)[0] = *t;
  for (i = 0; i < *neq; i++) REAL(Y)[i] = y[i];

  PROTECT(R_fcall = lang4(R_jac_vec,Time,Y,J));   incr_N_Protect();
  PROTECT(ans = eval(R_fcall, R_envir));          incr_N_Protect();

  for (i = 0; i < *neq ; i++)  pdj[i] = REAL(ans)[i];

  my_unprotect(3);
}


/* give name to data types */
typedef void C_root_func_type (int *, double *, double *,int *, double *);
typedef void C_jac_func_type  (int *, double *, double *, int *,
		                    int *, double *, int *, double *, int *);
typedef void C_jac_vec_type   (int *, double *, double *, int *,
		                    int *, int *, double *, double *, int *);

/* MAIN C-FUNCTION, CALLED FROM R-code */

SEXP call_lsoda(SEXP y, SEXP times, SEXP derivfunc, SEXP parms, SEXP rtol,
		SEXP atol, SEXP rho, SEXP tcrit, SEXP jacfunc, SEXP initfunc, 
    SEXP eventfunc, SEXP verbose, SEXP iTask, SEXP rWork, SEXP iWork, SEXP jT, 
    SEXP nOut, SEXP lRw, SEXP lIw, SEXP Solver, SEXP rootfunc, 
    SEXP nRoot, SEXP Rpar, SEXP Ipar, SEXP Type, SEXP flist, SEXP elist)

{
/******************************************************************************/
/******                   DECLARATION SECTION                            ******/
/******************************************************************************/

  int  i, j, k, nt, repcount, latol, lrtol, lrw, liw;
  int  maxit, solver, isForcing, isEvent;
  double *xytmp, tin, tout, *Atol, *Rtol, *dy=NULL, ss;
  int itol, itask, istate, iopt, jt, mflag,  is;
  int nroot, *jroot=NULL, isroot,  isDll, type;

  /* pointers to functions passed to FORTRAN */
  C_deriv_func_type *deriv_func;
  C_jac_func_type   *jac_func=NULL;
  C_jac_vec_type    *jac_vec;
  C_root_func_type  *root_func=NULL;

/******************************************************************************/
/******                         STATEMENTS                               ******/
/******************************************************************************/

/*                      #### initialisation ####                              */    
  init_N_Protect();

  jt  = INTEGER(jT)[0];         /* method flag */
  n_eq = LENGTH(y);             /* number of equations */ 
  nt  = LENGTH(times);
  
  maxit = 10;                   /* number of iterations */ 
  mflag = INTEGER(verbose)[0];
 
  nroot  = INTEGER(nRoot)[0];   /* number of roots (lsodar) */
  solver = INTEGER(Solver)[0];  /* 1=lsoda,2=lsode,3=lsodeS,4=lsodar,5=vode */

  /* is function a dll ?*/
  if (inherits(derivfunc, "NativeSymbol")) {
   isDll = 1;
  } else {
   isDll = 0;
  }

  /* initialise output ... */
  initOut(isDll, n_eq, nOut, Rpar, Ipar);

  /* copies of variables that will be changed in the FORTRAN subroutine */

  xytmp = (double *) R_alloc(n_eq, sizeof(double));
  for (j = 0; j < n_eq; j++) xytmp[j] = REAL(y)[j];
 
  latol = LENGTH(atol);
  Atol = (double *) R_alloc((int) latol, sizeof(double));

  lrtol = LENGTH(rtol);
  Rtol = (double *) R_alloc((int) lrtol, sizeof(double));

  liw = INTEGER (lIw)[0];
  iwork = (int *) R_alloc(liw, sizeof(int));
     for (j=0; j<LENGTH(iWork); j++) iwork[j] = INTEGER(iWork)[j];

  lrw = INTEGER(lRw)[0];
  rwork = (double *) R_alloc(lrw, sizeof(double));
     for (j=0; j<length(rWork); j++) rwork[j] = REAL(rWork)[j];

/* if a 1-D or 2-D special-purpose problem (lsodes)
   iwork will contain the sparsity structure */

  if (solver ==3)
  {
    type   = INTEGER(Type)[0];
    if (type == 2)        /* 1-D problem ; Type contains further information */
       sparsity1D( Type, iwork, n_eq, liw) ;
    else if (type == 3)  /* 2-D problem */
       sparsity2D( Type, iwork, n_eq, liw);
    else if (type == 4)  /* 3-D problem */
     sparsity3D (Type, iwork, n_eq, liw);
  }

/* initialise global R-variables...  */
  initglobals (nt);
  
/* Initialization of Parameters and Forcings (DLL functions)  */
  initParms(initfunc, parms);
  isForcing = initForcings(flist);
  isEvent = initEvents(elist, eventfunc);

/* pointers to functions deriv_func, jac_func, jac_vec, root_func, passed to FORTRAN */

  if (isDll) 
    { /* DLL address passed to FORTRAN */
      deriv_func = (C_deriv_func_type *) R_ExternalPtrAddr(derivfunc);  
      /* no need to communicate with R - but output variables set here */
      if (isOut) {dy = (double *) R_alloc(n_eq, sizeof(double));
                  for (j = 0; j < n_eq; j++) dy[j] = 0.; }
	  
	  /* here overruling deriv_func if forcing */
      if (isForcing) {
        DLL_deriv_func = deriv_func;
        deriv_func = (C_deriv_func_type *) C_deriv_func_forc;
      }
    } else {
      /* interface function between FORTRAN and C/R passed to FORTRAN */
      deriv_func = (C_deriv_func_type *) C_deriv_func; 
      /* needed to communicate with R */
      R_deriv_func = derivfunc;
      R_envir = rho;
    }

  if (!isNull(jacfunc) && solver !=3)  /* lsodes uses jac_vec */
    {
      if (isDll)
	    {
	     jac_func = (C_jac_func_type *) R_ExternalPtrAddr(jacfunc);
	    } else  {
	     R_jac_func = jacfunc;
	     jac_func = C_jac_func;
	    }
    }

  if (!isNull(jacfunc) && solver ==3)   /*lsodes*/
    {
      if (isDll)
	    {
	     jac_vec = (C_jac_vec_type *) R_ExternalPtrAddr(jacfunc);
	    } else  {
	     R_jac_vec = jacfunc;
	     jac_vec = C_jac_vec;
	    }
    }

  if (solver == 4 && nroot > 0)        /* lsodar */
  { jroot = (int *) R_alloc(nroot, sizeof(int));
     for (j=0; j<nroot; j++) jroot[j] = 0;
  
    if (isDll) 
    {
      root_func = (C_root_func_type *) R_ExternalPtrAddr(rootfunc);
    } else {
      root_func = (C_root_func_type *) C_root_func;
      R_root_func = rootfunc; 
    }
  }

/* tolerance specifications */
  if (latol == 1 && lrtol == 1 ) itol = 1;
  if (latol  > 1 && lrtol == 1 ) itol = 2;
  if (latol == 1 && lrtol  > 1 ) itol = 3;
  if (latol  > 1 && lrtol  > 1 ) itol = 4;

  for (j = 0; j < lrtol; j++) Rtol[j] = REAL(rtol)[j];
  for (j = 0; j < latol; j++) Atol[j] = REAL(atol)[j];

  itask = INTEGER(iTask)[0];   
  if (isEvent) itask = 4;
  
  istate = 1;

  iopt = 0;
  ss = 0.;
  is = 0 ;
  for (i = 5; i < 8 ; i++) ss = ss+rwork[i];
  for (i = 5; i < 10; i++) is = is+iwork[i];
  if (ss >0 || is > 0) iopt = 1; /* non-standard input */

/*                      #### initial time step ####                           */    

  REAL(YOUT)[0] = REAL(times)[0];
  for (j = 0; j < n_eq; j++) REAL(YOUT)[j+1] = REAL(y)[j];

  if (isOut == 1) {  /* function in DLL and output */
    tin = REAL(times)[0];
    deriv_func (&n_eq, &tin, xytmp, dy, out, ipar) ;
    for (j = 0; j < nout; j++) REAL(YOUT)[j + n_eq + 1] = out[j]; 
                  }

/*                     ####   main time loop   ####                           */    
  for (it = 0; it < nt-1; it++) {
    tin = REAL(times)[it];
    tout = REAL(times)[it+1];
    if (isEvent) { 
      rwork[0] = tout;
      updateevent(&tin, xytmp, &istate);
    }
    repcount = 0;
    do
	{  /* error control */
 	    if (istate == -2) {
	      for (j = 0; j < lrtol; j++) Rtol[j] *= 10.0;
	      for (j = 0; j < latol; j++) Atol[j] *= 10.0;
	      warning("Excessive precision requested.  `rtol' and `atol' have been scaled upwards by the factor %g\n",10.0);
	      istate = 3;
	    }

      if (solver == 1) {
	      F77_CALL(dlsoda) (deriv_func, &n_eq, xytmp, &tin, &tout,
			   &itol, Rtol, Atol, &itask, &istate, &iopt, rwork,
			   &lrw, iwork, &liw, jac_func, &jt, out, ipar); 
      } else if (solver == 2) {
        F77_CALL(dlsode) (deriv_func, &n_eq, xytmp, &tin, &tout,
			   &itol, Rtol, Atol, &itask, &istate, &iopt, rwork,
			   &lrw, iwork, &liw, jac_func, &jt, out, ipar); 
      } else if (solver == 3) {
        F77_CALL(dlsodes) (deriv_func, &n_eq, xytmp, &tin, &tout,
			   &itol, Rtol, Atol, &itask, &istate, &iopt, rwork,
			   &lrw, iwork, &liw, jac_vec, &jt, out, ipar); 
      } else if (solver == 4) {
        F77_CALL(dlsodar) (deriv_func, &n_eq, xytmp, &tin, &tout,
			   &itol, Rtol, Atol,  &itask, &istate, &iopt, rwork,
			   &lrw, iwork, &liw, jac_func, &jt, root_func, &nroot, jroot, 
         out, ipar); 
      } else if (solver == 5) {
 	      F77_CALL(dvode) (deriv_func, &n_eq, xytmp, &tin, &tout,
			   &itol, Rtol, Atol, &itask, &istate, &iopt, rwork,
			   &lrw, iwork, &liw, jac_func, &jt, out, ipar);
      }
    
	    if (istate == -1)  {
        warning("an excessive amount of work (> maxsteps ) was done, but integration was not successful - increase maxsteps");
      } else if (istate == 3 && solver == 4){
       /* root found - take into account if an EVENT */
        if (isEvent && rootevent) {
          tEvent=tin;
          updateevent(&tin, xytmp, &istate);
          istate = 1;
          repcount = 0;
        } else{
	       istate = -20;  repcount = 50;
	      } 
      } else if (istate == -2)  {
	      warning("Excessive precision requested.  scale up `rtol' and `atol' e.g by the factor %g\n",10.0);
	    } else if (istate == -4)  {
        warning("repeated error test failures on a step, but integration was successful - singularity ?");
      } else if (istate == -5)  {
        warning("repeated convergence test failures on a step, but integration was successful - inaccurate Jacobian matrix?");
      } else if (istate == -6)  {
        warning("Error term became zero for some i: pure relative error control (ATOL(i)=0.0) for a variable which is now vanished");
      }
	    repcount ++;
	} while (tin < tout && istate >= 0 && repcount < maxit); 
	
  if (istate == -3)  {
    error("illegal input detected before taking any integration steps - see written message");
	  unprotect_all();
	}  else	{
	  REAL(YOUT)[(it+1)*(ntot+1)] = tin;
	  for (j = 0; j < n_eq; j++)
	    REAL(YOUT)[(it+1)*(ntot + 1) + j + 1] = xytmp[j];
	  if (isOut == 1)  {
      deriv_func (&n_eq, &tin, xytmp, dy, out, ipar) ;
	    for (j = 0; j < nout; j++)
	      REAL(YOUT)[(it+1)*(ntot + 1) + j + n_eq + 1] = out[j];
    }
	}
	  
/*                    ####  an error occurred   ####                          */    
   if (istate < 0 || tin < tout) {
	  if (istate != -20) 
      returnearly (1);
    else 
      returnearly (0);  /* stop because a root was found */
    break;
    }
  }     /* end main time loop */



  /*                   ####   returning output   ####                           */    
  terminate(istate,23,0,5,10);    /* istate, iwork, rwork */
  
  if (istate == -20) INTEGER(ISTATE)[0] = 3; 	  

  if (istate == -20 && nroot > 0)  {
    isroot = 1   ;
    PROTECT(IROOT = allocVector(INTSXP, nroot));incr_N_Protect();
    for (k = 0;k<nroot;k++) INTEGER(IROOT)[k] = jroot[k];
    if (istate > 0) 
      setAttrib(YOUT, install("iroot"), IROOT);
    else 
      setAttrib(YOUT2, install("iroot"), IROOT);
  }

/*                       ####   termination   ####                            */    
  unprotect_all();
  if (istate > 0)
    return(YOUT);
  else
    return(YOUT2);
}

