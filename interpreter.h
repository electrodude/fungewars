#pragma once

#include "fungewars.h"

#define INITIAL_FTHREADS 16

#define INITIAL_STACKSIZE 1024

#define NFT_NO_STACK	0x1
#define NFT_GHOST		0x2

//#define max(a,b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a > _b ? _a : _b; })
//#define min(a,b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a < _b ? _a : _b; })

extern fmode run;

extern unsigned int ghostid;

extern int delay;

extern int lastfid;

fthread* newfthread(unsigned int team, int x, int y, int dx, int dy, int flag);

fthread* dupfthread(fthread** parent);

void killfthread(int id);

int getfthread(int x, int y);

int push(fthread* cfthread, int x);

int pop(fthread* cfthread);

coord* chkline(int x0, int y0, int x1, int y1, char check);

int execinstr(fthread* cfthread, cell* ccell);

void* interpreter(void* threadid);
