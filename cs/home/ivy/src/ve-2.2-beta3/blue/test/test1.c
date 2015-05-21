/* The first bluescript program */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bluescript.h>

BSInterp *theInterp = NULL;

static struct {
  char *name;
  int value;
} numbers[] = {
  { "zero", 0 },
  { "one", 1 },
  { "two", 2 },
  { "three", 3 },
  { "four", 4 },
  { "five", 5 },
  { "six", 6 },
  { "seven", 7 },
  { "eight", 8 },
  { "nine", 9 },
  { NULL, -1 }
};

int test_number(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "usage: number <name>";
  int k;

  if (objc != 2) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  if ((k = bsObjGetConstant(i,objv[1],numbers,
			    sizeof(numbers[0]),"number")) < 0)
    return BS_ERROR;
  assert(k >= 0 && k <= 9);
  printf("number = %d\n",numbers[k].value);
  return BS_OK;
}

/* some extra functions for testing */
int unknown(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  printf("unknown handler: procedure %s\n",
	 bsObjGetStringPtr(objv[0]));
  bsSetStringResult(i,"unknown procedure",BS_S_STATIC);
  return BS_ERROR;
}

/* mimic the Tcl incr command */
int test_incr(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "incr <var> [<value>]";
  BSInt inc = 1, val;
  BSObject *o;

  if (objc != 2 && objc != 3) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  
  bsClearResult(i);
  if (objc == 3 && bsObjGetInt(i,objv[2],&inc))
    return BS_ERROR;

  if (!(o = bsGetObj(i,NULL,objv[1],0)))
    return BS_ERROR;

  if (bsObjGetInt(i,o,&val))
    return BS_ERROR;

  bsObjSetInt(o,val + inc);
  bsSetIntResult(i,val + inc);
  return BS_OK;
}

int main(int argc, char **argv) {
  char *s;
  int k;
  if (argc != 2) {
    fprintf(stderr, "usage: %s script\n",
	    argv[0]);
    exit(1);
  }
  theInterp = bsInterpCreateStd();
  bsStream_FileProcs(theInterp);
  bsStream_StdioProcs(theInterp);
  if ((s = getenv("BS_OPT_MEMORY")) && atoi(s))
  	bsInterpOptSet(theInterp,BS_OPT_MEMORY,1);
  /* locally-added functions */
  bsSetExtProc(theInterp,"incr",test_incr,NULL);
  bsSetExtProc(theInterp,"number",test_number,NULL);
  bsSetExtUnknown(theInterp,NULL,unknown,NULL);
  /* read script */
  k = bsEvalFile(theInterp,argv[1]);
  printf("result = %s\n",bsCodeToString(k));
  if (k == BS_ERROR)
    printf("error: %s\n",bsObjGetStringPtr(theInterp->result));

  bsInterpDestroy(theInterp);
  
  exit(k == BS_OK ? 0 : 1);
}

