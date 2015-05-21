/* Simple code for doing least-squares approximation using QR decomposition */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifndef FMAX
#define FMAX(a,b) ((a)>(b)?(a):(b))
#endif
#ifndef SQR
#define SQR(a) ((a)*(a))
#endif
#ifndef SGN
#define SGN(a) ((a)<0?-1:1)
#endif
#ifndef SIGN
#define SIGN(a,b) (SGN(a)*SGN(b))
#endif

/* This is adapte from "NR in C" - it *should* work for rectangular
   matrices (m x n) where m >= n */
/* Likely you want this to be used in conjunction with qrsolv/rsolv */   
int qrdcmp(float **a, int m, int n, float *c, float *d) {
  int i,j,k;
  float scale,sigma,sum,tau;
  int sing = 0;
  
  for(k = 0; k < n-1; k++) {
    scale = 0.0;
    for (i = k; i < m; i++)
      scale = FMAX(scale,fabs(a[i][k]));
    if (scale == 0.0) {
      sing = 1;
      c[k] = d[k] = 0.0;
    } else {
      for(i = k; i < m; i++)
	a[i][k] /= scale;
      for(sum = 0.0, i = k; i < m; i++)
	sum += SQR(a[i][k]);
      sigma = sqrt(sum)*SGN(a[k][k]);
      a[k][k] += sigma;
      c[k] = sigma*a[k][k];
      d[k] = -scale*sigma;
      for (j = k+1; j < n; j++) {
	for(sum = 0.0, i = k; i < m; i++)
	  sum += a[i][k]*a[i][j];
	tau = sum/c[k];
	for(i = k; i < m; i++)
	  a[i][j] -= tau*a[i][k];
      }
    }
  }
  d[n-1] = a[n-1][n-1];
  if (d[n-1] == 0.0)
    sing = 1;

  return sing;
}

/* solves Rx = b, based upon QR decomp */
void rsolv(float **a, int n, float *d, float *b) {
  int i,j;
  float sum;
  b[n-1] /= d[n-1];
  for (i = n-2; i >= 0; i--) {
    for(sum = 0.0, j = i+1; j < n; j++)
      sum += a[i][j]*b[j];
    b[i] = (b[i] - sum)/d[i];
  }
}

/* solves Ax = b, based upon QR decomp */
void qrsolv(float **a, int n, float *c, float *d, float *b) {
  int i,j;
  float sum, tau;
  
  for (j = 0; j < n-1; j++) {
    for (sum = 0.0, i = j; i < n; i++)
      sum += a[i][j]*b[i];
    tau = sum/c[j];
    for (i = j; i < n; i++)
      b[i] -= tau*a[i][j];
    rsolv(a,n,d,b);
  }
}

int leastsq(float **a, int m, int n, float *b) {
  int i,j;
  float *d, *c;

  printf("leastsq(%d x %d)\n",m,n);
  for(i = 0; i < m; i++) {
    printf("[ ");
    for(j = 0; j < n; j++) {
      printf(" %2.4f", a[i][j]);
    }
    printf(" ]");
    printf(" = %g\n", b[i]);
  }

  c = malloc(sizeof(float)*n);
  assert(c != NULL);
  d = malloc(sizeof(float)*n);
  assert(d != NULL);
  if (qrdcmp(a,m,n,c,d))
    return -1;
  qrsolv(a,n,c,d,b);
  free(c);
  free(d);
}
