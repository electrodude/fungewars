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

int wrap(int x, int m);

void newgame();

void clrstatus();

void setstatus(const char* s);

int main(int argc, char** argv);
