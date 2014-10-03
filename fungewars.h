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

typedef struct
{
	char instr;
	int fg;		// (cell is occupied) ? 1 : 0
	int bg;		// team of last IP to overrun cell
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



extern float colors[18][3];

int wrap(int x, int m);

void newgame();

int main(int argc, char** argv);
