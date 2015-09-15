#pragma once

#define GLFW_INCLUDE_GLU
#include <GLFW/glfw3.h>

#include "fungewars.h"

extern GLFWwindow* gr_window;

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


view* view_reset(view* this, field* f, float sx, float sy, float swidth, float sheight, float x, float y, float zoom);
view* view_new(field* f, float sx, float sy, float swidth, float sheight, float x, float y, float zoom);

void view_kill(view* this);

void view_resize(view* this, int newx, int newy, int newwidth, int newheight);

void view_render(view* this);


void gr_putc(float x, float y, int c);

void gr_loop(void);

void gr_init(void);
