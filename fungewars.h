/*
 *  fungewars.c
 *  fungewars
 *
 */

#pragma once

#include <pthread.h>

#define GLFW_INCLUDE_GLU
#include <GLFW/glfw3.h>

#define max(a,b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a > _b ? _a : _b; })
#define min(a,b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a < _b ? _a : _b; })

#define CWIDTH 256
#define CHEIGHT 256

#define N_KEYS GLFW_KEY_LAST

// should this be in graphics.h?
typedef float color[4];

typedef struct
{
	char instr;
	color* fg;
	color* bg;
} cell;

typedef struct
{
	cell (*cells)[1][1];
	int width;
	int height;
} field;

typedef enum
{
	DEAD,
	ALIVE,
	GHOST,
} falive;

typedef enum
{
	PAUSED,
	STEP,
	RUN
} fmode;

typedef struct
{
	unsigned int i;
	unsigned int id;
	int parent;
	int team;
	int x;
	int y;
	int dx;
	int dy;
	int delta;
	int* stack;
	int stackidx;
	int stacksize;
	int stringmode;
	int jmpmode;
	int repeats;
	falive alive;
	fmode mode;
	field* field;
} fthread;

typedef struct
{
	int x;
	int y;
} coord;

typedef struct view
{
	field* field;

	float sx;
	float sy;
	float swidth;
	float sheight;

	float x;
	float y;
	float dx;
	float dy;
	float zoom;

	int follow;	// index of followed thread
//private:
	float x_curr;
	float y_curr;
	float dx_curr;
	float dy_curr;
	float zoom_curr;

	float zswidth;
	float zsheight;

	int id;

	enum {DISABLED, INACTIVE, ACTIVE} state;
} view;

extern coord marks[N_KEYS];

extern pthread_mutex_t fthreadsmutex;

extern field* curr_field;

int keys[N_KEYS];
int modkeys;

extern fthread* fthreads;
extern unsigned int fthreadslen;

typedef enum {NORMAL, EX, SREPLACE, REPLACE, VISUAL, INSERT, MARK_SET, MARK_GET} uimode_t;

extern uimode_t uimode;
extern uimode_t uiprevmode;

extern cell* statusline;
extern int statuslinelen;

extern int xi;
extern int yi;

extern int xiv;
extern int yiv;

// search

#define SEARCH_VISITED 0x1

typedef struct search_cell
{
	struct search_cell* next;
	int x;
	int y;
	int dx;
	int dy;

	char fc;
	char pc;

	int refs;
} search_cell;

typedef struct search_result
{
	struct search_result* next;
	search_cell* this;

	int refs;
} search_result;


extern search_result* search_first_result;

extern search_result* search_curr_result;


void search_result_new(search_cell* first_cell);

void search_result_kill(search_result* this);

search_cell* search_cell_new(search_cell* parent, int x, int y, int dx, int dy, char fc, char pc);

void search_cell_kill(search_cell* this);

field* field_reset(field* this, int width, int height);
field* field_new(int width, int height);

void field_kill(field* f);

cell* field_get(field* f, int x, int y);


int wrap(int x, int m);

void newgame(void);

void setstatus_color(int i, char c, color* fg, color* bg);

void setstatus_c(int i, char c);

void setstatus(const char* s);

void clrstatus(void);

void focusthread(view* v, fthread* cfthread);

void focuscam(view* v, int x, int y);

void docmd(char* cmd);

void keydown(unsigned int key, int mods);

void gr_charmods_callback(GLFWwindow* window, unsigned int key, int mods);

void gr_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);

void gr_idle(void);

int main(int argc, char** argv);
