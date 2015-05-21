/* Simple shell for BlueScript standard interpreter */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bluescript.h>

#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif /* _WIN32 */

/* "shell"-specific functions */
#define LNSZ 512

static char *prompt1 = "bssh> ";
static char *prompt2 = "> ";

static int proc_exit(BSInterp *i, int argc, char **argv, void *cdata) {
  bsInterpDestroy(i);
  exit(0);
}

int main(int argc, char **argv) {
  BSObject *o;
  BSList *l;
  BSInterp *i;
  int code;
  char *srcfile = NULL;

  i = bsInterpCreateStd();
  bsStream_FileProcs(i);
  bsStream_StdioProcs(i);
  bsStream_ExecProcs(i);
  bsSetExtProc(i,"exit",proc_exit,NULL);

  o = bsObjList(0,NULL);
  l = bsObjGetList(NULL,o);
  assert(l != NULL);

  if (argc > 1) {
    int k;
    srcfile = argv[1];
    for (k = 2; k < argc; k++)
      bsListPush(l,bsObjString(argv[k],-1,BS_S_STATIC),BS_TAIL);
  }

  bsSet(i,NULL,"argv",o);
  bsObjDelete(o);

  if (!srcfile && isatty(fileno(stdin)) && isatty(fileno(stdout))) {
    /* interactive mode */
    BSString cmd;
    char ln[LNSZ];

    bsStringInit(&cmd);

    while (1) {
      int code;
      printf("%s",prompt1);
      fflush(stdout);
      bsStringClear(&cmd);
      if (feof(stdin) || !fgets(ln,LNSZ,stdin))
	break;
      /* get a full line... */
      bsStringAppend(&cmd,ln,-1);
      while (!strchr(ln,'\n') && fgets(ln,LNSZ,stdin))
	bsStringAppend(&cmd,ln,-1);
      while (!bsScriptComplete(bsStringPtr(&cmd))) {
	printf("%s",prompt2);
	fflush(stdout);
	if (feof(stdin) || !fgets(ln,LNSZ,stdin))
	  break;
	/* get a full line... */
	bsStringAppend(&cmd,ln,-1);
	while (!strchr(ln,'\n') && fgets(ln,LNSZ,stdin))
	  bsStringAppend(&cmd,ln,-1);
      }
      /* process it */
      bsClearResult(i);
      code = bsEvalString(i,bsStringPtr(&cmd));
      if (code == BS_OK) {
	if (!bsIsResultClear(i))
	  printf("%s\n",bsObjGetStringPtr(bsGetResult(i)));
      } else if (code == BS_ERROR) {
	fprintf(stderr,"error: %s\n",
		bsObjGetStringPtr(bsGetResult(i)));
      } else {
	fprintf(stderr,"unexepcted result code: %s (%d)\n",
		bsCodeToString(code), code);
      }
    }
  } else {
    if (!srcfile || strcmp(srcfile,"-") == 0) {
      srcfile = "<stdin>";
      code = bsEvalStream(i,stdin);
    } else {
      code = bsEvalFile(i,srcfile);
    }
    if (code == BS_ERROR) {
      fprintf(stderr,"error in %s: %s\n",
	      srcfile, bsObjGetStringPtr(bsGetResult(i)));
      exit(1);
    }
  }

  bsInterpDestroy(i);
  exit(0);
}
