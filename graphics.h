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

extern float ccdx;
extern float ccdy;

extern int lastcx;
extern int lastcy;
extern int lastupdate;

extern float cdx;
extern float cdy;

extern int rswidth;
extern int rsheight;

extern int swidth;
extern int sheight;

extern int frame;
extern int timenow;
extern int timelast;
extern int timebase;

GLuint png_texture_load(const char* file_name, int* width, int* height);

void glputc(int x, int y, int c);

void focusthread(fthread* cfthread);

void focuscam(float x, float y);

void mulzoom(float factor);

void display(void);

void reshape(int width, int height);

void kb1(unsigned char key, int x, int y);

void kb1u(unsigned char key, int x, int y);

void kb2(int key, int x, int y);

void kb2u(int key, int x, int y);

void idle(void);
