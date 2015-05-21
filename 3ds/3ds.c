//***********************************************************************
//                                                                      
//      "Talk to me like I'm a 3 year old!" Programming Lessons -
//
//      $Author:        DigiBen     digiben@gametutorials.com
//
//      $Program:       3DS Loader
//
//      $Description:   Demonstrates how to load a .3ds file format
//
//      $Date:          10/6/01
//***********************************************************************
//	Modified by Colossus (Giuseppe Torelli) colossus@3000.it on July 2004:
//
//	Encapsulated all the struct definitions in the class Load3ds
//	Added the code to scale and center the 3ds object
//	Corrected a small bug on counting the meshes of the 3ds file (lights and camera were counted as meshes)
//	Enhanced the code so to allow loading a new 3ds file by erasing the vectors and counters
//	Replaced the code to compute the normals (it was incredibly slow) with a very faster one
//	Added a powerful and simple texture loader class to load textures from file
//***********************************************************************
//
//      Modified to remove the C++ constructs and merge to VE
//
//      Michael Jenkin, 2005.
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <math.h>
# include "3ds.h"
# include "loadTexture.h"

/* # define DEBUG  */

// This file handles all of the code needed to load a .3DS file.
// Basically, how it works is, you load a chunk, then you check
// the chunk ID.  Depending on the chunk ID, you load the information
// that is stored in that chunk.  If you do not want to read that information,
// you read past it.  You know how many bytes to read past the chunk because
// every chunk stores the length in bytes of that chunk.

/* forward definitions */
static void ProcessNextChunk(struct t3DModel *pModel, struct tChunk *pPreviousChunk);
static void ProcessNextObjectChunk(struct t3DModel *pModel, struct t3DObject *pObject,
				   struct tChunk *pPreviousChunk);
static void ProcessNextMaterialChunk(struct t3DModel *pModel, struct tChunk *pPreviousChunk);
static void ReadChunk(struct tChunk *pChunk);
static int GetString(char *pBuffer);
static void ReadColorChunk(struct tMaterialInfo *pMaterial, struct tChunk *pChunk);
static void ReadUVCoordinates(struct t3DObject *pObject, struct tChunk *pPreviousChunk);
static void ReadVertexIndices(struct t3DObject *pObject, struct tChunk *pPreviousChunk);
static void ReadVertices(struct t3DObject *pObject, struct tChunk *pPreviousChunk);
static void ReadObjectMaterial (struct t3DModel *pModel, struct t3DObject *pObject,
				struct tChunk *pPreviousChunk);
static void ComputeNormals (struct t3DModel *pModel);
static void ScaleAndCenter (struct t3DModel *pModel );

static int read_int(unsigned int *v);
static int read_short(unsigned short *v);
static int read_float(float *v);


static unsigned char *buffer = (unsigned char *)NULL; 
static FILE *m_FilePointer;

int Import3DS (struct t3DModel *pModel,  char * root, char * strFileName)
{
  char buf[512];
  struct tChunk * m_CurrentChunk;
  struct tMaterialInfo *p;
  int i;

# ifdef DEBUG
  fprintf(stderr,"Import3DS called with filename %s\n", strFileName);
# endif

  /* reset the model */
  pModel->numOfObjects = 0;
  pModel->numOfMaterials = 0;
  pModel->pObject = (struct t3DObject *)NULL;
  pModel->pMaterials = (struct tMaterialInfo *)NULL;

  /* and lets start processing the file */
  sprintf(buf, "%s/%s",root, strFileName);
  if((m_FilePointer = fopen(buf, "r")) == (FILE *)NULL) {
    fprintf(stderr,"Import3DS: Opening of |%s| failed\n",buf);
    return(-1);
  }

  /*
    Once we have the file open, we need to read the very first data chunk
    to see if it's a 3DS file.  That way we don't read an invalid file.
    
    If it is a 3DS file, then the first chunk ID will be equal to PRIMARY (some hex num)
    Read the first chuck of the file to see if it's a 3DS file
  */
  if((m_CurrentChunk = (struct tChunk *) malloc(sizeof(struct tChunk))) == (struct tChunk *)NULL) {
    fprintf(stderr,"Out of memory in Import3DS?\n");
    return(-1);
  }
# ifdef DEBUG
  fprintf(stderr,"Import3DS reading first chunk\n");
# endif
  ReadChunk( m_CurrentChunk );
    
  /* Make sure this is a 3DS file */
  if (m_CurrentChunk->ID != PRIMARY ) return -1;
# ifdef DEBUG
  fprintf(stderr,"Import3DS primary header found\n");
# endif

  /*
    Now we actually start reading in the data.  ProcessNextChunk() is recursive
    Begin loading objects, by calling this recursive function
  */
  ProcessNextChunk(pModel, m_CurrentChunk);
# ifdef DEBUG
  fprintf(stderr,"Import3DS File processed\n");
# endif
  (void) free(m_CurrentChunk);
  fclose (m_FilePointer);

  /* After we have read the whole 3DS file, we want to calculate our own vertex normals. */
  ComputeNormals ( pModel );
    
  /*
    Added by Colossus: let's calculate the scale and the center coordinates to let the object
    fit in the center of the window
  */
  ScaleAndCenter (pModel);
    
  /*
    Load the textures and bind them appropriately
  */

  for(p=pModel->pMaterials;p!= (struct tMaterialInfo *)NULL;p=p->next) {
# ifdef DEBUG
    fprintf(stderr,"Import3DS: Processing texture %s\n",p->strName);
    if(*(p->strFile))
      fprintf(stderr, "Import3DS: Its a texture in %s\n",p->strFile);
    else
      fprintf(stderr, "Import3DS: Its colour 0x%x 0x%x 0x%x\n",p->color[0], p->color[1], p->color[2]);
# endif
    if(*(p->strFile))
      p->id = loadTexture(root, p->strFile);
# ifdef DEBUG
    fprintf(stderr, "Import3DS: Processed\n");
# endif
  }
  return 1;
}

/*
  This function reads the main sections of the .3DS file, then dives deeper with recursion
*/
static void ProcessNextChunk(struct t3DModel *pModel, struct tChunk *pPreviousChunk)
{
  struct t3DObject *newObject;			// This is used to add to our object list
  struct tMaterialInfo *newTexture;		// This is used to add to our material list
  struct tChunk *m_CurrentChunk;                 // out next chunk
  struct tChunk *m_TempChunk;                    // a temporary chunk
  unsigned int version = 0;			// This will hold the file version

  if((m_CurrentChunk = (struct tChunk *) malloc(sizeof(struct tChunk))) == (struct tChunk *)NULL) {
    fprintf(stderr,"Out of memory in ProcessNextChunk?\n");
    exit(1);
  }
  if((m_TempChunk = (struct tChunk *) malloc(sizeof(struct tChunk))) == (struct tChunk *)NULL) {
    fprintf(stderr,"Out of memory in ProcessNextChunk?\n");
    exit(1);
  }

# ifdef DEBUG
  fprintf(stderr,"ProcessNextChunk: processing a chunk\n");
# endif

  // Below we check our chunk ID each time we read a new chunk.  Then, if
  // we want to extract the information from that chunk, we do so.
  // If we don't want a chunk, we just read past it.  
	
  // Continue to read the sub chunks until we have reached the length.
  // After we read ANYTHING we add the bytes read to the chunk and then check
  // check against the length.
	
  while (pPreviousChunk->bytesRead < pPreviousChunk->length){
    // Read next Chunk
    ReadChunk(m_CurrentChunk);
    // Check the chunk ID
# ifdef DEBUG
    fprintf(stderr,"ProcessNextChunk: got chunk 0x%x len %d read %d\n", m_CurrentChunk->ID,
	    m_CurrentChunk->length, m_CurrentChunk->bytesRead);
# endif
    switch (m_CurrentChunk->ID) {
    case VERSION:   // This holds the version of the file
# ifdef DEBUG
      fprintf(stderr,"ProcessNextChunk: processing VERSION len %d read %d\n",
	      m_CurrentChunk->length, m_CurrentChunk->bytesRead);
# endif
      // This chunk has an unsigned short that holds the file version.
      // Since there might be new additions to the 3DS file format in 4.0,
      // we give a warning to that problem.

      // Read the file version and add the bytes read to our bytesRead variable
      if((m_CurrentChunk->length - m_CurrentChunk->bytesRead) !=  4) {
	fprintf(stderr,"ProcessNextChunk: version chunk incorrect format\n");
	exit(1);
      }
      m_CurrentChunk->bytesRead += read_int(&version);
                	
      // If the file version is over 3, give a warning that there could be a problem
      if (version > 0x03)
	fprintf (stderr, "This 3DS file is over version 3(%d) so it may load incorrectly\n", version);
      break;
    case OBJECTINFO:   // This holds the version of the mesh
# ifdef DEBUG
    fprintf(stderr,"ProcessNextChunk: processing OBJECTINFO\n");
# endif
      // This chunk holds the version of the mesh.  It is also the head of the MATERIAL
      // and OBJECT chunks.  From here on we start reading in the material and object info.

      // Read the next chunk
      ReadChunk(m_TempChunk);

      // Get the version of the mesh
      m_TempChunk->bytesRead += read_int(&version);
      
      // Increase the bytesRead by the bytes read from the last chunk
      m_CurrentChunk->bytesRead += m_TempChunk->bytesRead;

      // Go to the next chunk, which if the object has a texture, it should be MATERIAL, then OBJECT.
      ProcessNextChunk(pModel, m_CurrentChunk);
      break;
    case MATERIAL:                          // This holds the material information
# ifdef DEBUG
    fprintf(stderr,"ProcessNextChunk: processing MATERIAL\n");
# endif
      // This chunk is the header for the material info chunks
      // Increase the number of materials
      pModel->numOfMaterials++;

      // create a newTexture
      if((newTexture = (struct tMaterialInfo *)malloc(sizeof(struct tMaterialInfo))) ==
	 (struct tMaterialInfo *)NULL) {
	fprintf(stderr,"Out of memory in ProcessNextChunk?\n");
	exit(1);
      }
      newTexture->next = pModel->pMaterials;
      newTexture->strName[0] = '\0';
      newTexture->strFile[0] ='\0';
      pModel->pMaterials = newTexture;
      ProcessNextMaterialChunk(pModel, m_CurrentChunk);
      break;
    case OBJECT:                            // This holds the name of the object being read
# ifdef DEBUG
    fprintf(stderr,"ProcessNextChunk: processing OBJECT\n");
# endif
      // This chunk is the header for the object info chunks.  It also
      // holds the name of the object.
       
      // Add a new tObject node to our list of objects (like a link list)
      if((newObject = (struct t3DObject *)malloc(sizeof(struct t3DObject))) ==
	 (struct t3DObject *)NULL) {
	fprintf(stderr,"Out of memory in ProcessNextChunk?\n");
	exit(1);
      }

      /* create an empty object */
      newObject->numOfVerts = 0;
      newObject->numOfFaces = 0;
      newObject->numTexVertex = 0;
      newObject->strName[0] = '\0';
      newObject->pVerts = (struct CVector3 *)NULL;
      newObject->pNormals = (struct CVector3 *)NULL;
      newObject->pTexVerts = (struct CVector2 *)NULL;
      newObject->next = pModel->pObject;
      pModel->pObject = newObject;

      // Get the name of the object and store it, then add the read bytes to our byte counter.
      m_CurrentChunk->bytesRead += GetString(newObject->strName);

# ifdef DEBUG
      fprintf(stderr,"We have just created an object %s\n",newObject->strName);
# endif
            
      // Now proceed to read in the rest of the object information
      ProcessNextObjectChunk(pModel, newObject, m_CurrentChunk);
      break;
    default: 
# ifdef DEBUG
    fprintf(stderr,"ProcessNextChunk: skipping unsupported\n");
# endif
      // If we didn't care about a chunk, then we get here.  We still need
      // to read past the unknown or ignored chunk and add the bytes read to the byte counter.
      if(buffer != (unsigned char *)NULL)
        free(buffer);
      if((buffer = malloc(m_CurrentChunk->length - m_CurrentChunk->bytesRead)) == (unsigned char *)NULL) {
        fprintf(stderr,"3ds: Out of memory (die)\n");
        exit(1);
      }
      m_CurrentChunk->bytesRead += fread(buffer, 1, m_CurrentChunk->length - m_CurrentChunk->bytesRead,
					 m_FilePointer);
    }
# ifdef DEBUG
    fprintf(stderr,"ProcessNextChunk: chunk logic completed\n");
# endif
    // Add the bytes read from the last chunk to the previous chunk passed in.
    pPreviousChunk->bytesRead += m_CurrentChunk->bytesRead;
  }
  /* Free the current chunk and set it back to the previous chunk (since it started that way) */
  (void) free(m_CurrentChunk);
  (void) free(m_TempChunk);
}

/*
  This function handles all the information about the objects in the file
*/
static void ProcessNextObjectChunk(struct t3DModel *pModel, struct t3DObject *pObject,
				   struct tChunk *pPreviousChunk)
{
  struct tChunk * m_CurrentChunk;

  if((m_CurrentChunk = (struct tChunk *) malloc(sizeof(struct tChunk))) == (struct tChunk *)NULL) {
    fprintf(stderr,"Out of memory in ProcessNextChunk?\n");
    exit(1);
  }

  // Continue to read these chunks until we read the end of this sub chunk
  while (pPreviousChunk->bytesRead < pPreviousChunk->length)  {
# ifdef DEBUG
    fprintf(stderr, "ProcessNextObjectChunk: Processing subchunk read %d length %d\n",
	    pPreviousChunk->bytesRead, pPreviousChunk->length);
# endif
    // Read the next chunk
    ReadChunk(m_CurrentChunk);
    // Check which chunk we just read
    switch (m_CurrentChunk->ID)    {
    case OBJECT_MESH:                   // This lets us know that we are reading a new object
# ifdef DEBUG
      fprintf(stderr, "ProcessNextObjectChunk: OBJECT_MESH\n");
# endif
      // Increase the object count
      pModel->numOfObjects++;
      // We found a new object, so let's read in it's info using recursion
      ProcessNextObjectChunk(pModel, pObject, m_CurrentChunk);
      break;
    case OBJECT_VERTICES:               // This is the objects vertices
# ifdef DEBUG
      fprintf(stderr, "ProcessNextObjectChunk: OBJECT_VERTICES\n");
# endif
      ReadVertices(pObject, m_CurrentChunk);
      break;
    case OBJECT_FACES:                  // This is the objects face information
# ifdef DEBUG
      fprintf(stderr, "ProcessNextObjectChunk: OBJECT_FACES\n");
# endif
      ReadVertexIndices(pObject, m_CurrentChunk);
      break;
    case OBJECT_MATERIAL:               // This holds the material name that the object has
# ifdef DEBUG
      fprintf(stderr, "ProcessNextObjectChunk: OBJECT_MATERIAL\n");
# endif
      // This chunk holds the name of the material that the object has assigned to it.
      // This could either be just a color or a texture map.  This chunk also holds
      // the faces that the texture is assigned to (In the case that there is multiple
      // textures assigned to one object, or it just has a texture on a part of the object.
      // Since most of my game objects just have the texture around the whole object, and 
      // they aren't multitextured, I just want the material name.

      // We now will read the name of the material assigned to this object
      ReadObjectMaterial(pModel, pObject, m_CurrentChunk);            
      break;
    case OBJECT_UV:                     // This holds the UV texture coordinates for the object
# ifdef DEBUG
      fprintf(stderr, "ProcessNextObjectChunk: OBJECT_UV\n");
# endif
      // This chunk holds all of the UV coordinates for our object.  Let's read them in.
      ReadUVCoordinates(pObject, m_CurrentChunk);
      break;
    default:  
# ifdef DEBUG
      fprintf(stderr, "ProcessNextObjectChunk: Unsupported Chunk\n");
# endif
      // Read past the ignored or unknown chunks
      if(buffer != (unsigned char *)NULL)
        free(buffer);
      if((buffer = malloc(m_CurrentChunk->length - m_CurrentChunk->bytesRead)) == (unsigned char *)NULL) {
        fprintf(stderr,"3ds: Out of memory (die)\n");
        exit(1);
      }
      m_CurrentChunk->bytesRead += fread(buffer, 1, m_CurrentChunk->length - m_CurrentChunk->bytesRead,
					 m_FilePointer);
      break;
    }
# ifdef DEBUG
      fprintf(stderr, "ProcessNextObjectChunk: End of Chunk logic\n");
# endif
    
    // Add the bytes read from the last chunk to the previous chunk passed in.
    pPreviousChunk->bytesRead += m_CurrentChunk->bytesRead;
  }

  // Free the current chunk and set it back to the previous chunk (since it started that way)
  (void) free(m_CurrentChunk);
}

/*
  This function handles all the information about the material (Texture)
*/
static void ProcessNextMaterialChunk(struct t3DModel *pModel, struct tChunk *pPreviousChunk)
{
  struct tChunk *m_CurrentChunk;

  if((m_CurrentChunk = (struct tChunk *) malloc(sizeof(struct tChunk))) == (struct tChunk *)NULL) {
    fprintf(stderr,"Out of memory in ProcessNextChunk?\n");
    exit(1);
  }

  // Continue to read these chunks until we read the end of this sub chunk
  while (pPreviousChunk->bytesRead < pPreviousChunk->length)  {
    // Read the next chunk
    ReadChunk(m_CurrentChunk);
    // Check which chunk we just read in
    switch (m_CurrentChunk->ID)  {
    case MATNAME:                           // This chunk holds the name of the material
      // Here we read in the material name
      //printf ("numOfMaterials è: %d\n" , pModel->numOfMaterials);
      m_CurrentChunk->bytesRead += fread(pModel->pMaterials->strName, 1,
					 m_CurrentChunk->length - m_CurrentChunk->bytesRead,
					 m_FilePointer);
      break;
    case MATDIFFUSE:                        // This holds the R G B color of our object
      ReadColorChunk(pModel->pMaterials, m_CurrentChunk);
      break;
    case MATMAP:                            // This is the header for the texture info
      // Proceed to read in the material information
      ProcessNextMaterialChunk(pModel, m_CurrentChunk);
      break;
    case MATMAPFILE:                        // This stores the file name of the material
      // Here we read in the material's file name
      m_CurrentChunk->bytesRead += fread (pModel->pMaterials->strFile, 1,
					  m_CurrentChunk->length - m_CurrentChunk->bytesRead,
					  m_FilePointer);
      break;
    default:  
      // Read past the ignored or unknown chunks
      if(buffer != (unsigned char *)NULL)
        free(buffer);
      if((buffer = malloc(m_CurrentChunk->length - m_CurrentChunk->bytesRead)) == (unsigned char *)NULL) {
        fprintf(stderr,"3ds: Out of memory (die)\n");
        exit(1);
      }
      m_CurrentChunk->bytesRead += fread(buffer, 1, m_CurrentChunk->length - m_CurrentChunk->bytesRead,
					 m_FilePointer);
      break;
    }

    // Add the bytes read from the last chunk to the previous chunk passed in.
    pPreviousChunk->bytesRead += m_CurrentChunk->bytesRead;
    }

  // Free the current chunk and set it back to the previous chunk (since it started that way)
  (void) free(m_CurrentChunk);
}

/*
  This function reads in a chunk ID and it's length in bytes. A total of 6 bytes.
  Note: 3ds files are stored in little indian order.
*/
static void ReadChunk(struct tChunk *pChunk)
{

  pChunk->bytesRead = read_short(&(pChunk->ID));
  pChunk->bytesRead += read_int(&(pChunk->length));
}

/*
  This function reads in a string of characters
*/

static int GetString(char *pBuffer)
{
  int index = 0;

  // Read 1 byte of data which is the first letter of the string
  if(fread(pBuffer, 1, 1, m_FilePointer) != 1) {
    fprintf(stderr,"GetString: unexpected end of file\n");
    exit(1);
  }

  // Loop until we get NULL
  while (*(pBuffer + index++) != 0) {
    // Read in a character at a time until we hit NULL.
    if(fread(pBuffer + index, 1, 1, m_FilePointer) != 1) {
      fprintf(stderr, "GetString: unexpected end of file\n");
      exit(1);
    }
  }

# ifdef DEBUG
  fprintf(stderr,"GetString: Recovered |%s|\n", pBuffer);
# endif

  // Return the string length, which is how many bytes we read in (including the NULL)
  return strlen(pBuffer) + 1;
}


/*
  This function reads in the RGB color data
*/
static void ReadColorChunk(struct tMaterialInfo *pMaterial, struct tChunk *pChunk)
{
  struct tChunk * m_TempChunk;

  if((m_TempChunk = (struct tChunk *) malloc(sizeof(struct tChunk))) == (struct tChunk *)NULL) {
    fprintf(stderr,"Out of memory in ProcessNextChunk?\n");
    exit(1);
  }
  // Read the color chunk info
  ReadChunk(m_TempChunk);
  // Read in the R G B color (3 bytes - 0 through 255)
  m_TempChunk->bytesRead += fread(pMaterial->color, 1, m_TempChunk->length - m_TempChunk->bytesRead,
				  m_FilePointer);

  // Add the bytes read to our chunk
  pChunk->bytesRead += m_TempChunk->bytesRead;

  (void) free(m_TempChunk);
}

/*
  This function reads in the indices for the vertex array
*/
static void ReadVertexIndices(struct t3DObject *pObject, struct tChunk *pPreviousChunk)
{
  int i, j;
  unsigned short index = 0;                   // This is used to read in the current face index

  // In order to read in the vertex indices for the object, we need to first
  // read in the number of them, then read them in.  Remember,
  // we only want 3 of the 4 values read in for each face.  The fourth is
  // a visibility flag for 3D Studio Max that doesn't mean anything to us.
  
  // Read in the number of faces that are in this object (int)
  pPreviousChunk->bytesRead += read_short(&(pObject->numOfFaces));
# ifdef DEBUG
  fprintf(stderr,"ReadVertexIndices: there are %d faces\n",pObject->numOfFaces);
# endif

  // Alloc enough memory for the faces and initialize the structure
  pObject->pFaces = (struct tFace *)malloc(sizeof(struct tFace) * pObject->numOfFaces);

  // Go through all of the faces in this object
  for(i = 0; i < pObject->numOfFaces; i++)  {
    // Next, we read in the A then B then C index for the face, but ignore the 4th value.
    // The fourth value is a visibility flag for 3D Studio Max, we don't care about this.
    for(j = 0; j < 4; j++) {
      // Read the first vertice index for the current face 
      pPreviousChunk->bytesRead += read_short(&index);
      if(j < 3)  {
	// Store the index in our face structure.
	pObject->pFaces[i].vertIndex[j] = index;
	pObject->pFaces[i].coordIndex[j] = 0;
      }
    }
  }
}

/*
  This function reads in the UV coordinates for the object
*/
static void ReadUVCoordinates(struct t3DObject *pObject, struct tChunk *pPreviousChunk)
{
  int i;

  // In order to read in the UV indices for the object, we need to first
  // read in the amount there are, then read them in.

  // Read in the number of UV coordinates there are (int)
  pPreviousChunk->bytesRead += read_short(&pObject->numTexVertex);
# ifdef DEBUG
  fprintf(stderr,"ReadUVCoordinates: Reading %d coordinates\n",pObject->numTexVertex);
# endif

  // Allocate memory to hold the UV coordinates
  pObject->pTexVerts = (struct CVector2 *)malloc(sizeof(struct CVector2) * pObject->numTexVertex);

  // Read in the texture coodinates (an array 2 float)
  for(i=0;i<pObject->numTexVertex;i++) {
    pPreviousChunk->bytesRead += read_float(&(pObject->pTexVerts[i].x));
    pPreviousChunk->bytesRead += read_float(&(pObject->pTexVerts[i].y));
  }
}

/*
  This function reads in the vertices for the object
*/
static void ReadVertices(struct t3DObject *pObject, struct tChunk *pPreviousChunk)
{
  int i;
  // Like most chunks, before we read in the actual vertices, we need
  // to find out how many there are to read in.  Once we have that number
  // we then fread() them into our vertice array.

  // Read in the number of vertices (int)
  pPreviousChunk->bytesRead += read_short(&(pObject->numOfVerts));
# ifdef DEBUG
  fprintf(stderr,"ReadVertices: reading %d verticies\n", pObject->numOfVerts);
# endif

  // Allocate the memory for the verts and initialize the structure
  pObject->pVerts = (struct CVector3 *)malloc(sizeof(struct CVector3) * pObject->numOfVerts);

  // Read in the array of vertices (an array of 3 floats)
  for(i=0;i<pObject->numOfVerts;i++) {
    pPreviousChunk->bytesRead += read_float(&(pObject->pVerts[i].x));
    pPreviousChunk->bytesRead += read_float(&(pObject->pVerts[i].y));
    pPreviousChunk->bytesRead += read_float(&(pObject->pVerts[i].z));
  }

  // Now we should have all of the vertices read in.  Because 3D Studio Max
  // Models with the Z-Axis pointing up (strange and ugly I know!), we need
  // to flip the y values with the z values in our vertices.  That way it
  // will be normal, with Y pointing up.  If you prefer to work with Z pointing
  // up, then just delete this next loop.  Also, because we swap the Y and Z
  // we need to negate the Z to make it come out correctly.

  // Go through all of the vertices that we just read and swap the Y and Z values
  for(i = 0; i < pObject->numOfVerts; i++)  {
    // Store off the Y value
    float fTempY = pObject->pVerts[i].y;

    // Set the Y value to the Z value
    pObject->pVerts[i].y = pObject->pVerts[i].z;

    // Set the Z value to the Y value, 
    // but negative Z because 3D Studio max does the opposite.
    pObject->pVerts[i].z = -fTempY;
  }
}

/*
  This function reads in the material name assigned to the object and sets the material 'mat'
*/
static void ReadObjectMaterial (struct t3DModel *pModel, struct t3DObject *pObject,
				struct tChunk *pPreviousChunk)
{
  int i;
  struct tMaterialInfo *p;
  char strMaterial[255] = {0};            // This is used to hold the objects material name
  unsigned short len, index;

  // *What is a material?*  - A material is either the color or the texture map of the object.
  // It can also hold other information like the brightness, shine, etc... Stuff we don't
  // really care about.  We just want the color, or the texture map file name really.

  // Here we read the material name that is assigned to the current object.
  // strMaterial should now have a string of the material name, like "Material #2" etc..
  pPreviousChunk->bytesRead += GetString(strMaterial);

  // Now that we have a material name, we need to go through all of the materials
  // and check the name against each material.  When we find a material in our material
  // list that matches this name we just read in, then we assign the materialID
  // of the object to that material index.  You will notice that we passed in the
  // model to this function.  This is because we need the number of textures.
  // Yes though, we could have just passed in the model and not the object too.

  // Go through all of the textures
# ifdef DEBUG
  fprintf(stderr,"ReadObjectMaterial for %s\n",strMaterial);
# endif
  for(p=pModel->pMaterials;p != (struct tMaterialInfo *)NULL;p=p->next) {
    // If the material we just read in matches the current material name
    if (!strcmp(strMaterial, p->strName))  {
# ifdef DEBUG
      fprintf(stderr, "ReadObjectMaterial for object %s (%d faces) %s\n",
	      pObject->strName, pObject->numOfFaces,strMaterial);
# endif
      break;
    }
  }

# ifdef DEBUG
  fprintf(stderr,"there are %d bytes left in the chunk\n",pPreviousChunk->length-pPreviousChunk->bytesRead);
# endif
  pPreviousChunk->bytesRead += read_short(&len);

# ifdef DEBUG
  fprintf(stderr,"there are %d verticies to colour this colour\n",len);
# endif
  for(i=0;i<len;i++) {
    pPreviousChunk->bytesRead += read_short(&index);
    if(index >= pObject->numOfFaces) {
      fprintf(stderr,"ReadObjectMaterial: trying to define material for invalid face\n",index);
      exit(1);
    }
    pObject->pFaces[index].mat = p;
  }
  if(pPreviousChunk->length != pPreviousChunk->bytesRead) {
    fprintf(stderr,"ReadObjectMaterial: chunk format invalid (extra bytes)\n");

    // Read past the rest of the chunk since we don't care about shared vertices
    // You will notice we subtract the bytes already read in this chunk from the total length.
    if(buffer != (unsigned char *)NULL)
      free(buffer);
    if((buffer = malloc(pPreviousChunk->length - pPreviousChunk->bytesRead)) == (unsigned char *)NULL) {
      fprintf(stderr,"3ds: Out of memory (die)\n");
      exit(1);
    }
    pPreviousChunk->bytesRead += fread(buffer, 1, pPreviousChunk->length - pPreviousChunk->bytesRead,
				       m_FilePointer);
  }
}           

static void makeNormal(struct CVector3 *n, struct CVector3 *p, struct CVector3 *q, struct CVector3 *r)
{
  struct CVector3 v1, v2;

  v1.x = q->x - p->x;
  v1.y = q->y - p->y;
  v1.z = q->z - p->z;
  v2.x = r->x - p->x;
  v2.y = r->y - p->y;
  v2.z = r->z - p->z;
  n->x = ( v1.y * v2.z - v1.z * v2.y );
  n->y = ( v1.z * v2.x - v1.x * v2.z );
  n->z = ( v1.x * v2.y - v1.y * v2.x );
}

static void Normalize(struct CVector3 *v)
{
  float len = (float) sqrt(v->x * v->x + v->y * v->y + v->z * v->z);
  if(len > 0.0) {
    v->x /= len;
    v->y /= len;
    v->z /= len;
  }
}

static void ComputeNormals (struct t3DModel *pModel)
{
  struct CVector3 n;
  int index[3], i, j;
  struct t3DObject *pObject;

  for(pObject=pModel->pObject; pObject != (struct t3DObject *)NULL; pObject=pObject->next) {
# ifdef DEBUG
    fprintf(stderr,"Compute Normals: Processing object %s (%d verticies)\n",pObject->strName,
	    pObject->numOfVerts);
# endif

    /* create the list of normals */
    pObject->pNormals = (struct CVector3 *)malloc(sizeof(struct CVector3) * pObject->numOfVerts);
    for(i=0;i<pObject->numOfVerts;i++) {
      pObject->pNormals[i].x = 0.0;
      pObject->pNormals[i].y = 0.0;
      pObject->pNormals[i].z = 0.0;
    }

    /* process every face */
    for (j = 0; j < pObject->numOfFaces; j++ ) {
      index[0] = pObject->pFaces[j].vertIndex[0];
      index[1] = pObject->pFaces[j].vertIndex[1];
      index[2] = pObject->pFaces[j].vertIndex[2];
      makeNormal(&n, &(pObject->pVerts[index[0]]), &(pObject->pVerts[index[1]]),
                 &(pObject->pVerts[index[2]]));
      for(i=0;i<3;i++) 	{
        pObject->pNormals[index[i]].x += n.x;
	pObject->pNormals[index[i]].y += n.y;
	pObject->pNormals[index[i]].z += n.z;
      }
      Normalize (&(pObject->pNormals[index[0]]));
      Normalize (&(pObject->pNormals[index[1]]));
      Normalize (&(pObject->pNormals[index[2]]));
    }
  }
}

static void ScaleAndCenter (struct t3DModel *pModel )
{
  int i, j, Vertex;
  struct t3DObject *pObject;
  float sx, sy, sz;
  float max_x=0.0, max_y=0.0 , max_z=0.0,
    min_x =340282346638528859811704183484516925440.000000,
    min_y=340282346638528859811704183484516925440.000000,
    min_z=340282346638528859811704183484516925440.000000;

  for(pObject=pModel->pObject;pObject != (struct t3DObject *)NULL;pObject=pObject->next) {
# ifdef DEBUGX
    fprintf(stderr,"ScaleAndCenter: Processing %s (%d faces) ",pObject->strName,pObject->numOfFaces);
    if(pObject->mat == (struct tMaterialInfo *) NULL)
      fprintf(stderr," with no material property\n");
    else {
      fprintf(stderr," with material property %s ", pObject->mat->strName);
      if(pObject->mat->strFile[0] == '\0')
	fprintf(stderr, "with colour 0x%x 0x%x 0x%x\n", pObject->mat->color[0],pObject->mat->color[1],
		pObject->mat->color[02]);
      else
	fprintf(stderr, "with texture from %s\n", pObject->mat->strFile);
    }
# endif
    for (j = 0; j < pObject->numOfFaces; j++ ) {
      for (Vertex = 0; Vertex < 3; Vertex++ ) {
	int index = pObject->pFaces[j].vertIndex[Vertex];
	if ( pObject->pVerts[ index ].x > max_x) max_x = pObject->pVerts[ index ].x;
	if ( pObject->pVerts[ index ].y > max_y) max_y = pObject->pVerts[ index ].y;
	if ( pObject->pVerts[ index ].z > max_z) max_z = pObject->pVerts[ index ].z;
	
	if ( pObject->pVerts[ index ].x < min_x) min_x = pObject->pVerts[ index ].x;
	if ( pObject->pVerts[ index ].y < min_y) min_y = pObject->pVerts[ index ].y;
	if ( pObject->pVerts[ index ].z < min_z) min_z = pObject->pVerts[ index ].z;
      }
    }
  }

# ifdef DEBUG
  fprintf(stderr,"ScaleAndCenter: x %f %f y %f %f z %f %f\n",min_x,max_x,min_y,max_y,min_z,max_z);
# endif
	  	
  sx = max_x - min_x;
  sy = max_y - min_y;
  sz = max_z - min_z;
	  
  if( sx > sy ) pModel->scale = sx; else pModel->scale = sy;
  if( pModel->scale < sz ) pModel->scale = sz;
  pModel->scale = 1.0f / pModel->scale;
  pModel->center_x = (min_x + sx/2.f) * pModel->scale;
  pModel->center_y = (min_y + sy/2.f) * pModel->scale;
  pModel->center_z = (min_z + sz/2.f) * pModel->scale;
}

static int read_float(float *f) 
{
  unsigned char b[4];
  if(fread(b, 1, 4, m_FilePointer) != 4) {
    fprintf(stderr, "read_int: Unexpected end of file\n");
    exit(1);
  }
  
  *((unsigned int *)f) = (b[3] << 24) | (b[2] << 16) | (b[1] << 8) | b[0];
  return 4;
}
static int read_int(unsigned int *v) 
{
  unsigned char b[4];
  if(fread(b, 1, 4, m_FilePointer) != 4) {
    fprintf(stderr, "read_int: Unexpected end of file\n");
    exit(1);
  }
  *v = (b[3] << 24) | (b[2] << 16) | (b[1] << 8) | b[0];
  return 4;
}

static int read_short(unsigned short *v) 
{
  unsigned char b[2];
  if(fread(b, 1, 2, m_FilePointer) != 2) {
    fprintf(stderr, "read_int: Unexpected end of file\n");
    exit(1);
  }
  *v = (b[1] << 8) | b[0];
  return 2;
}

/////////////////////////////////////////////////////////////////////////////////
//
// * QUICK NOTES * 
//
// This was a HUGE amount of knowledge and probably the largest tutorial yet!
// In the next tutorial we will show you how to load a text file format called .obj.
// This is the most common 3D file format that almost ANY 3D software will import.
//
// Once again I should point out that the coordinate system of OpenGL and 3DS Max are different.
// Since 3D Studio Max Models with the Z-Axis pointing up (strange and ugly I know! :), 
// we need to flip the y values with the z values in our vertices.  That way it
// will be normal, with Y pointing up.  Also, because we swap the Y and Z we need to negate 
// the Z to make it come out correctly.  This is also explained and done in ReadVertices().
//
// CHUNKS: What is a chunk anyway?
// 
// "The chunk ID is a unique code which identifies the type of data in this chunk 
// and also may indicate the existence of subordinate chunks. The chunk length indicates 
// the length of following data to be associated with this chunk. Note, this may 
// contain more data than just this chunk. If the length of data is greater than that 
// needed to fill in the information for the chunk, additional subordinate chunks are 
// attached to this chunk immediately following any data needed for this chunk, and 
// should be parsed out. These subordinate chunks may themselves contain subordinate chunks. 
// Unfortunately, there is no indication of the length of data, which is owned by the current 
// chunk, only the total length of data attached to the chunk, which means that the only way 
// to parse out subordinate chunks is to know the exact format of the owning chunk. On the 
// other hand, if a chunk is unknown, the parsing program can skip the entire chunk and 
// subordinate chunks in one jump. " - Jeff Lewis (werewolf@worldgate.com)
//
// In a short amount of words, a chunk is defined this way:
// 2 bytes - Stores the chunk ID (OBJECT, MATERIAL, PRIMARY, etc...)
// 4 bytes - Stores the length of that chunk.  That way you know when that
//           chunk is done and there is a new chunk.
//
// So, to start reading the 3DS file, you read the first 2 bytes of it, then
// the length (using fread()).  It should be the PRIMARY chunk, otherwise it isn't
// a .3DS file.  
//
// Below is a list of the order that you will find the chunks and all the know chunks.
// If you go to www.wosit.org you can find a few documents on the 3DS file format.
// You can also take a look at the 3DS Format.rtf that is included with this tutorial.
//
//
//
//      MAIN3DS  (0x4D4D)
//     |
//     +--EDIT3DS  (0x3D3D)
//     |  |
//     |  +--EDIT_MATERIAL (0xAFFF)
//     |  |  |
//     |  |  +--MAT_NAME01 (0xA000) (See mli Doc) 
//     |  |
//     |  +--EDIT_CONFIG1  (0x0100)
//     |  +--EDIT_CONFIG2  (0x3E3D) 
//     |  +--EDIT_VIEW_P1  (0x7012)
//     |  |  |
//     |  |  +--TOP            (0x0001)
//     |  |  +--BOTTOM         (0x0002)
//     |  |  +--LEFT           (0x0003)
//     |  |  +--RIGHT          (0x0004)
//     |  |  +--FRONT          (0x0005) 
//     |  |  +--BACK           (0x0006)
//     |  |  +--USER           (0x0007)
//     |  |  +--CAMERA         (0xFFFF)
//     |  |  +--LIGHT          (0x0009)
//     |  |  +--DISABLED       (0x0010)  
//     |  |  +--BOGUS          (0x0011)
//     |  |
//     |  +--EDIT_VIEW_P2  (0x7011)
//     |  |  |
//     |  |  +--TOP            (0x0001)
//     |  |  +--BOTTOM         (0x0002)
//     |  |  +--LEFT           (0x0003)
//     |  |  +--RIGHT          (0x0004)
//     |  |  +--FRONT          (0x0005) 
//     |  |  +--BACK           (0x0006)
//     |  |  +--USER           (0x0007)
//     |  |  +--CAMERA         (0xFFFF)
//     |  |  +--LIGHT          (0x0009)
//     |  |  +--DISABLED       (0x0010)  
//     |  |  +--BOGUS          (0x0011)
//     |  |
//     |  +--EDIT_VIEW_P3  (0x7020)
//     |  +--EDIT_VIEW1    (0x7001) 
//     |  +--EDIT_BACKGR   (0x1200) 
//     |  +--EDIT_AMBIENT  (0x2100)
//     |  +--EDIT_OBJECT   (0x4000)
//     |  |  |
//     |  |  +--OBJ_TRIMESH   (0x4100)      
//     |  |  |  |
//     |  |  |  +--TRI_VERTEXL          (0x4110) 
//     |  |  |  +--TRI_VERTEXOPTIONS    (0x4111)
//     |  |  |  +--TRI_MAPPINGCOORS     (0x4140) 
//     |  |  |  +--TRI_MAPPINGSTANDARD  (0x4170)
//     |  |  |  +--TRI_FACEL1           (0x4120)
//     |  |  |  |  |
//     |  |  |  |  +--TRI_SMOOTH            (0x4150)   
//     |  |  |  |  +--TRI_MATERIAL          (0x4130)
//     |  |  |  |
//     |  |  |  +--TRI_LOCAL            (0x4160)
//     |  |  |  +--TRI_VISIBLE          (0x4165)
//     |  |  |
//     |  |  +--OBJ_LIGHT    (0x4600)
//     |  |  |  |
//     |  |  |  +--LIT_OFF              (0x4620)
//     |  |  |  +--LIT_SPOT             (0x4610) 
//     |  |  |  +--LIT_UNKNWN01         (0x465A) 
//     |  |  | 
//     |  |  +--OBJ_CAMERA   (0x4700)
//     |  |  |  |
//     |  |  |  +--CAM_UNKNWN01         (0x4710)
//     |  |  |  +--CAM_UNKNWN02         (0x4720)  
//     |  |  |
//     |  |  +--OBJ_UNKNWN01 (0x4710)
//     |  |  +--OBJ_UNKNWN02 (0x4720)
//     |  |
//     |  +--EDIT_UNKNW01  (0x1100)
//     |  +--EDIT_UNKNW02  (0x1201) 
//     |  +--EDIT_UNKNW03  (0x1300)
//     |  +--EDIT_UNKNW04  (0x1400)
//     |  +--EDIT_UNKNW05  (0x1420)
//     |  +--EDIT_UNKNW06  (0x1450)
//     |  +--EDIT_UNKNW07  (0x1500)
//     |  +--EDIT_UNKNW08  (0x2200)
//     |  +--EDIT_UNKNW09  (0x2201)
//     |  +--EDIT_UNKNW10  (0x2210)
//     |  +--EDIT_UNKNW11  (0x2300)
//     |  +--EDIT_UNKNW12  (0x2302)
//     |  +--EDIT_UNKNW13  (0x2000)
//     |  +--EDIT_UNKNW14  (0xAFFF)
//     |
//     +--KEYF3DS (0xB000)
//        |
//        +--KEYF_UNKNWN01 (0xB00A)
//        +--............. (0x7001) ( viewport, same as editor )
//        +--KEYF_FRAMES   (0xB008)
//        +--KEYF_UNKNWN02 (0xB009)
//        +--KEYF_OBJDES   (0xB002)
//           |
//           +--KEYF_OBJHIERARCH  (0xB010)
//           +--KEYF_OBJDUMMYNAME (0xB011)
//           +--KEYF_OBJUNKNWN01  (0xB013)
//           +--KEYF_OBJUNKNWN02  (0xB014)
//           +--KEYF_OBJUNKNWN03  (0xB015)  
//           +--KEYF_OBJPIVOT     (0xB020)  
//           +--KEYF_OBJUNKNWN04  (0xB021)  
//           +--KEYF_OBJUNKNWN05  (0xB022)  
//
// Once you know how to read chunks, all you have to know is the ID you are looking for
// and what data is stored after that ID.  You need to get the file format for that.
// I can give it to you if you want, or you can go to www.wosit.org for several versions.
// Because this is a proprietary format, it isn't a official document.
//
// I know there was a LOT of information blown over, but it is too much knowledge for
// one tutorial.  In the animation tutorial that I eventually will get to, some of
// the things explained here will be explained in more detail.  I do not claim that
// this is the best .3DS tutorial, or even a GOOD one :)  But it is a good start, and there
// isn't much code out there that is simple when it comes to reading .3DS files.
// So far, this is the best I have seen.  That is why I made it :)
// 
// I would like to thank www.wosit.org and Terry Caton (tcaton@umr.edu) for his help on this.
//
// Let me know if this helps you out!
// 
// 
// Ben Humphrey (DigiBen)
// Game Programmer
// DigiBen@GameTutorials.com
// Co-Web Host of www.GameTutorials.com
//
//
