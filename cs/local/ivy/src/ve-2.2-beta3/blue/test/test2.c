/* Test for construction of nested lists... */
/* Searching for bug processing WMI data in another application... */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <bluescript.h>

int testit(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  BSObject *m = bsObjList(0,NULL);
  BSList *ml = bsObjGetList(NULL,m);

  int k = 100;

  assert(ml != NULL);
  while (k > 0) {
    BSObject *ro = bsObjList(0,NULL); /* object list */
    BSList *rl = bsObjGetList(NULL,ro);
    int j = 5;
    assert(rl != NULL);
    while (j > 0) {
      BSObject *ov[2];
      ov[0] = bsObjInt(k);
      ov[1] = bsObjInt(j);
      bsListPush(rl,bsObjList(2,ov),BS_TAIL);
      j--;
    }
    bsObjInvalidate(ro,BS_T_LIST);
    bsListPush(ml,ro,BS_TAIL);
    k--;
  }
  bsObjInvalidate(m,BS_T_LIST);
  bsSetObjResult(i,m);
  return BS_OK;
}

int main(int argc, char **argv) {
  BSInterp *i;
  char *s;
  if (argc != 1) {
    fprintf(stderr, "usage: %s \n", argv[0]);
    exit(1);
  }
  i = bsInterpCreateStd();
  bsStream_FileProcs(i);
  bsStream_StdioProcs(i);
  if ((s = getenv("BS_OPT_MEMORY")) && atoi(s))
    bsInterpOptSet(i,BS_OPT_MEMORY,1);
  bsSetExtProc(i,"testit",testit,NULL);
  if (bsEvalString(i,"set x [testit]") != BS_OK) {
    fprintf(stderr,"failed to testit: %s\n",bsObjGetStringPtr(bsGetResult(i)));
    exit(1);
  }
  if (bsEvalString(i,"[stdout] writeln \"x = $x\"") != BS_OK) {
    fprintf(stderr,"failed to writeln: %s\n",bsObjGetStringPtr(bsGetResult(i)));
    exit(1);
  }
  bsInterpDestroy(i);
  exit(0);
}
