# include <stdio.h>
# include <strings.h>
# include <ve.h>
# include "loadTexture.h"
# ifdef OLD_VE
# include <txm.h> 
# define veRenderSetupCback veiGlSetWindowCback
# define veRenderCback veiGlSetDisplayCback
# endif


/* # define DEBUG */
/*
 * So this is just glue to ve's txm. However, we do a couple of cute things here.
 * We look for the texture in a number of paths, with a number of different extensions.
 * That way, you can convert the textures to (oh) pnm and they will load everywhere.
 */

static char *paths[] = {
  "/", "/texture/", "/text/", ""
};

static char *types[] = {
  ".pnm", ".ppm", ".PNM", ".PPM", ".jpg", ".JPG", ""
};


int loadTexture(char *root, char *fname)
{
  FILE *fd;
  char head[255], tail[255], buf[512], *p, *q, *s;
  int id, i, j;

# ifdef DEBUG
  fprintf(stderr,"loading texture |%s|\n",fname);
# endif

  p = strrchr(fname, '.');
  if(p == (char *)NULL) {
    strcpy(tail, "");
    strcpy(head, fname);
  } else {
    strcpy(tail, p+1);
    for(s=head,q=fname;q!=p;q++,s++){
      *s = *q;
    }
    *s = '\0';
  }
# ifdef DEBUG
  fprintf(stderr,"loadTexture: %s |%s| and |%s|\n",fname, head,tail);
# endif
  for(i=0;*paths[i] != '\0';i++) {
    for(j=0;*types[j] != '\0';j++) {
      sprintf(buf, "%s/%s%s%s",root, paths[i],head,types[j]);
# ifdef DEBUG
      fprintf(stderr,"loadTexture: %s as %s\n",fname,buf);
# endif
      if((fd = fopen(buf,"r")) != (FILE *)NULL) {
	(void) fclose(fd);
# ifdef DEBUG
	fprintf(stderr, "*** sending %s to txm\n",buf);
# endif
	if((id = txmAddTexFile(NULL, buf, NULL, 0)) > 0) {
# ifdef DEUBG
	  fprintf(stderr,"**** texture %s found as %s\n",fname,buf);
# endif
	  txmBindTexture(NULL, id);
	  return id;
	} else {
	  fprintf(stderr,"**** texture %s not loadable\n",fname);
	  return(-1);
	}
      }
    }
  }

  fprintf(stderr,"**** texture %s missing\n",fname);
  return(-1);
}
