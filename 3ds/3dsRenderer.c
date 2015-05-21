# include <stdio.h>
# include <ve.h>
# include <GL/gl.h>
# include <GL/glu.h>
# include "3ds.h"
# include "3dsRenderer.h"

/* # define DEBUG */

void Render3DS (struct t3DModel *pModel )
{
  int i, j, Vertex, hasTexture;
  struct t3DObject *pObject;
  struct tMaterialInfo *mat;

# ifdef DEBUG
  fprintf(stderr, "Render3DS: Rendering model\n");
# endif

  /* establish 'default' material */
  mat = (struct tMaterialInfo *) NULL;
  hasTexture = 0;
  glDisable(GL_TEXTURE_2D);
  glColor3ub(255, 0, 0);
  glBegin(GL_TRIANGLES);

  /* rendering every object */
  for(pObject=pModel->pObject;pObject != (struct t3DObject *)NULL;pObject=pObject->next) {
# ifdef DEBUG
    fprintf(stderr, "Render3DS: Rendering object %s\n", pObject->strName);
# endif

    /* every face of every object */
    for (j = 0; j < pObject->numOfFaces; j++ ) {

      /* same texture as current? */
      if(pObject->pFaces[j].mat != mat) {
	glEnd();

	/* assign surface colour properties */
	hasTexture = 0;
	if(pObject->pFaces[j].mat == (struct tMaterialInfo *)NULL) {
	  glDisable(GL_TEXTURE_2D);
	  glColor3ub(255, 255, 255);
# ifdef DEBUG
	  fprintf(stderr,"rendering unlabelled texture\n");
# endif
	  hasTexture = 0;
	} else {
	  mat = pObject->pFaces[j].mat;
	  if(*(mat->strFile) == '\0') {
	    glDisable(GL_TEXTURE_2D);
	    glColor3ub(mat->color[0], mat->color[1], mat->color[2]);
# ifdef DEBUG
	    fprintf(stderr, "rendering in colour %s\n",mat->strName);
# endif
	    hasTexture = 0;
	  } else {
	    hasTexture = 1;
# ifdef DEBUG
	    fprintf(stderr, "Render3DS: Rendering texture %s (%d)\n",mat->strName, mat->id);
# endif
	    glEnable(GL_TEXTURE_2D);
	    txmBindTexture(NULL, mat->id);
	    glColor3ub(255, 255, 255);
	  }
	}
      }
      glBegin(GL_TRIANGLES);

      /* render this polygon */
      for (Vertex = 0; Vertex < 3; Vertex++ ) {
	int index = pObject->pFaces[j].vertIndex[Vertex];
	glNormal3f(pObject->pNormals[index].x, pObject->pNormals[index].y, pObject->pNormals[index].z);
	if(hasTexture && (pObject->pTexVerts != (struct CVector2 *)NULL)) {
	  glTexCoord2f(pObject->pTexVerts[index].x, pObject->pTexVerts[index].y);
	}
        glVertex3f(pObject->pVerts[index].x, pObject->pVerts[index].y, pObject->pVerts[index].z);
      }
    }
    glEnd();
  }
# ifdef DEBUG 
  fprintf(stderr, "Render3DS: Rendering complete\n");
# endif
}


