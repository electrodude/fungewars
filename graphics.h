#pragma once

// Linux - should be expression and not just 0 or 1
#if 1
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#endif

// OSX - should be expression and not just 0 or 1
#if 0
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include <GLUT/glut.h>
#endif

#include <png.h>

#include "fungewars.h"

extern int font;
extern int fontwidth;
extern int fontheight;
extern int charwidth;
extern int charheight;

extern float cx;
extern float cy;
extern float czoom;

extern float ccx;
extern float ccy;
extern float cczoom;

extern float ccdx;
extern float ccdy;

extern int lastcx;
extern int lastcy;
extern int lastupdate;

extern float cdx;
extern float cdy;

extern int rswidth;
extern int rsheight;

extern float swidth;
extern float sheight;

extern int frame;
extern int timenow;
extern int timelast;
extern int timebase;


extern color color_fg;

extern color colors[18];


GLuint png_texture_load(const char* file_name, int* width, int* height);

void glputc(float x, float y, int c);

void display(void);

void reshape(int width, int height);

void gl_init();
