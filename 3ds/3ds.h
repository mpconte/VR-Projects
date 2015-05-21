#ifndef _3DS_H
#define _3DS_H

//>------ Primary Chunk, at the beginning of each file
#define PRIMARY		0x4D4D

//>------ Main Chunks
#define OBJECTINFO	0x3D3D                // This gives the version of the mesh and is found right before the material and object information
#define VERSION		0x0002                // This gives the version of the .3ds file
#define EDITKEYFRAME	0xB000                // This is the header for all of the key frame info

//>------ sub defines of OBJECTINFO
#define MATERIAL	0xAFFF                // This stored the texture info
#define OBJECT		0x4000                // This stores the faces, vertices, etc...

//>------ sub defines of MATERIAL
#define MATNAME		0xA000                // This holds the material name
#define MATDIFFUSE	0xA020                // This holds the color of the object/material
#define MATMAP		0xA200                // This is a header for a new material
#define MATMAPFILE	0xA300                // This holds the file name of the texture

#define OBJECT_MESH	0x4100                // This lets us know that we are reading a new object

//>------ sub defines of OBJECT_MESH
#define OBJECT_VERTICES	0x4110          // The objects vertices
#define OBJECT_FACES	0x4120          // The objects faces
#define OBJECT_MATERIAL	0x4130          // This is found if the object has a material, either texture map or color
#define OBJECT_UV	0x4140          // The UV texture coordinates

typedef unsigned char BYTE;

/* 
   This is our 3D point structure.  This will be used to store the vertices of our model.
*/
struct CVector3 {
  float x, y, z;
};
	
/* 
   This is our 2D point structure to store the UV coordinates.
*/
struct CVector2 {
  float x, y;
};
	
/*
  This holds the 3ds chunk info
*/

struct tChunk 	{
  unsigned short int ID;                  // The chunk's ID       
  unsigned int length;                    // The length of the chunk
  unsigned int bytesRead;                 // The amount of bytes read within that chunk
};
	
/*
  This is our face structure.  This is is used for indexing into the vertex 
   and texture coordinate arrays.  From this information we know which vertices
   from our vertex array go to which face, along with the correct texture coordinates.
*/
struct tFace 	{
  int vertIndex[3];           // indicies for the verts that make up this triangle
  int coordIndex[3];          // indicies for the tex coords to texture this face
  struct tMaterialInfo *mat;  // material for this face.
};
	
/*
  This holds the information for a material.  It may be a texture map of a color.
  Some of these are not used, but I left them because you will want to eventually
  read in the UV tile ratio and the UV tile offset for some models.
*/
struct tMaterialInfo {
  char  strName[255];         // The texture name
  char  strFile[255];         // The texture file name (If this is set it's a texture map)
  int id;                     // texture id (used for rendering)
  BYTE  color[3];             // The color of the object (R, G, B)
  float uTile;                // u tiling of texture  (Currently not used)
  float vTile;                // v tiling of texture  (Currently not used)
  float uOffset;              // u offset of texture  (Currently not used)
  float vOffset;              // v offset of texture  (Currently not used)
  struct tMaterialInfo *next; // The next material info
} ;

	
/*
  This holds all the information for our model/scene.
*/
struct t3DObject {
  unsigned short numOfVerts;	// The number of verts in the model 
  unsigned short numOfFaces;	// The number of faces in the model
  unsigned short numTexVertex;	// The number of texture coordinates
  char strName[255];            // the name of the object
  struct CVector3  *pVerts;     // The object's vertices
  struct CVector3  *pNormals;	// The object's normals
  struct CVector2  *pTexVerts;	// The texture's UV coordinates
  struct tFace *pFaces;		// The faces information of the object
  struct t3DObject *next;       // next object in the list of objects
};
	
/*
  This holds our model information.
*/
struct t3DModel  {                       // This reads the objects vertices
  int numOfObjects;                      // The number of objects in the model    
  int numOfMaterials;                    // The number of materials for the model
  float scale;				 // The scale factor for the model
  float center_x , center_y , center_z;  // Centre of the model
  struct t3DObject *pObject;             // The objects
  struct tMaterialInfo *pMaterials;      // The materials
};

/* This is the function that you call to load the 3DS */
int Import3DS (struct t3DModel *pModel, char * root, char * strFileName );
#endif
