/***********************************************************************
//
//	TextureLoader class
//	By Colossus (Giuseppe Torelli) on 07 August 2004:
//
//	This class loads a TGA / JPG / BMP file
//	and sets the OpenGL appropriate parameters
//	
//	Credit for the image loaders go to the respective authors.
//	I just assembled them all in one class for convenience
//	This texture loader class changes the case of the texture file names when not found
*************************************************************************
 */
 
#ifndef _TEXTURELOADER_H
#define _TEXTURELOADER_H
int loadTexture(char *root, char *fname);
#endif
