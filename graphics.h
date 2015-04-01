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

#include "fungewars.h"

extern int font;
extern int fontwidth;
extern int fontheight;
extern int charwidth;
extern int charheight;

extern view* cam_curr;

extern view** cams;
extern int ncams;
extern int maxcams;

extern int rswidth;
extern int rsheight;

extern int frame;
extern int timenow;
extern int timelast;
extern int timebase;


extern color color_fg;

extern color colors[18];

// should this be in a seperate png_load.h?
GLuint png_texture_load(const char* file_name, int* width, int* height);


view* view_new(field* f, float sx, float sy, float swidth, float sheight, float x, float y, float zoom);

void view_kill(view* this);

void view_resize(view* this, int newx, int newy, int newwidth, int newheight);

void view_render(view* this);


void glputc(float x, float y, int c);

void display(void);

void reshape(int width, int height);

void gl_init();
