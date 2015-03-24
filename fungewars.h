/*
 *  fungewars.c
 *  fungewars
 *
 */

#pragma once

#include <pthread.h>

#define max(a,b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a > _b ? _a : _b; })
#define min(a,b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a < _b ? _a : _b; })

#define CWIDTH 256
#define CHEIGHT 256

// should this be in graphics.h?
typedef float color[4];

typedef struct
{
	char instr;
	color* fg;
	color* bg;
} cell;

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
} fthread;

typedef struct
{
	int x;
	int y;
} coord;

extern pthread_mutex_t fthreadsmutex;

extern cell field[CHEIGHT][CWIDTH];

extern fthread* fthreads;
extern unsigned int fthreadslen;

extern int cthread;

enum {NORMAL, EX, SREPLACE, REPLACE, VISUAL, INSERT} uimode;

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




int wrap(int x, int m);

void newgame(void);

void setstatus_color(int i, char c, color* fg, color* bg);

void setstatus_c(int i, char c);

void setstatus(const char* s);

void clrstatus(void);

void focusthread(fthread* cfthread);

void focuscam(int x, int y);

void docmd(char* cmd);

void kb1(unsigned char key, int x, int y);

void kb1u(unsigned char key, int x, int y);

void kb2(int key, int x, int y);

void kb2u(int key, int x, int y);

void idle(void);

int main(int argc, char** argv);
