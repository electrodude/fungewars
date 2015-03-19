#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <time.h>

#include <math.h>
#include <string.h>

#include "interpreter.h"

fmode run = PAUSED;

unsigned int ghostid;

int delay = 500000;

int lastfid=0;

fthread* newfthread(unsigned int team, int x, int y, int dx, int dy, int flag)
{
	unsigned int i;
	for (i=0; i<fthreadslen && fthreads[i].alive != DEAD; i++);
	if (i>=fthreadslen)
	{
#if debugout >= 1
		printf("fthreadslen*=2\n");
#endif
		unsigned int i2 = fthreadslen;
		fthreadslen*=2;

		pthread_mutex_lock(&fthreadsmutex);
		fthreads = realloc(fthreads, fthreadslen*sizeof(fthread));
		pthread_mutex_unlock(&fthreadsmutex);

		for (; i2<fthreadslen; i2++)
		{
			fthreads[i2].alive = DEAD;
		}
#if debugout >= 1
		printf("fthreadslen = %d\n", fthreadslen);
#endif
	}
	fthread* cfthread = &fthreads[i];
	cfthread->i = i;
	cfthread->id = lastfid++;
	cfthread->parent = -1;
	cfthread->team = team;
	cfthread->x = x;
	cfthread->y = y;
	cfthread->dx = dx;
	cfthread->dy = dy;
	cfthread->delta = 1;
	cfthread->stringmode = 0;
	cfthread->jmpmode = 0;
	cfthread->repeats = 0;
	cfthread->alive = ALIVE;
	cfthread->mode = RUN;
	
	if (!(flag & NFT_NO_STACK))
	{	
		cfthread->stacksize = INITIAL_STACKSIZE;
		cfthread->stack = malloc(cfthread->stacksize*sizeof(int));
		cfthread->stackidx = 0;
	}
	if (flag & NFT_GHOST)
	{
		cfthread->alive = GHOST;
	}
	
	field[y][x].fg = 1;//cfthread->team*2+1;
	field[y][x].bg = cfthread->team*2+1;
	
	//cthread = id;
	
#if debugout >= 3
	printf("new thread, i=%d, id=%d, team=%d, x=%d, y=%d, dx=%d, dy=%d\n", i, cfthread->id, team, x, y, dx, dy);
#endif
	
	return &fthreads[i];
}

fthread* dupfthread(fthread** parent)
{
	/*
	int id=0;
	for (id=0; id<fthreadslen && fthreads[id]; id++);
	fthread* cfthread = fthreads[id] = malloc(sizeof(fthread));
	cfthread->id = id;
	cfthread->parent = parent->id;
	cfthread->team = parent->team;
	cfthread->x = parent->x;
	cfthread->y = parent->y;
	cfthread->dx = parent->dx;
	cfthread->dy = parent->dy;
	cfthread->stacksize = parent->stacksize;
	cfthread->stack = malloc(cfthread->stacksize*sizeof(int));
	//*/
	fthread* oldptr = fthreads;
	fthread* cfthread = newfthread((*parent)->team, (*parent)->x, (*parent)->y, (*parent)->dx, (*parent)->dy, NFT_NO_STACK);
	size_t offset = fthreads - oldptr;
	
	*parent += offset;

	if (offset)
	{
		printf("offset=%X, (parent-fthreads)/sizeof(fthread)=0x%X\n", offset, ((*parent)-fthreads)/sizeof(fthread));
	}
	
	cfthread->parent = (*parent)->id;
	cfthread->stacksize = (*parent)->stacksize;
	cfthread->stack = malloc(cfthread->stacksize*sizeof(int));
	memcpy(cfthread->stack, (*parent)->stack, cfthread->stacksize);
	cfthread->stackidx = (*parent)->stackidx;
	//cfthread->status = (*parent)->status;
	
	cfthread->x = wrap(cfthread->x + cfthread->dx, CWIDTH);
	cfthread->y = wrap(cfthread->y + cfthread->dy, CHEIGHT);
	
#if debugout >= 2
	printf("dup thread, i=%d, id=%d, parent.i=%d, parent.id=%d\n", cfthread->i, cfthread->id, (*parent)->i, (*parent)->id);
#endif
	
	return cfthread;
}

void killfthread(int id)
{
	free(fthreads[id].stack);
	fthreads[id].alive = DEAD;
	if (id == cthread) cthread = -1;
	
#if debugout >= 2
	printf("kill thread, id=%d\n", id);
#endif
}

int getfthread(int x, int y)
{
	for (int i=0; i<fthreadslen; i++)
	{
		fthread* cfthread = &fthreads[i];
		if (cfthread->x == x && cfthread->y == y)
		{
			return i;
		}
	}

	return -1;
}

int push(fthread* cfthread, int x)
{
	if (cfthread->stackidx > cfthread->stacksize)
	{
		printf("stack overflow!\n");
		killfthread(cfthread->i);
		return x;
	}
	cfthread->stack[cfthread->stackidx++] = x;
	return x;
}

int pop(fthread* cfthread)
{
	if (cfthread->stackidx<=0) return 0;
	return cfthread->stack[--cfthread->stackidx];
}

coord* chkline(int x0, int y0, int x1, int y1, char check)	//modified off of Wikipedia's example for Bresenham's line algorithm
{
	int dx = abs(x1-x0);
	int sx = x0<x1 ? 1 : -1;
	int dy = abs(y1-y0);
	int sy = y0<y1 ? 1 : -1; 
	int err = (dx>dy ? dx : -dy)/2;
	int e2;
	static coord culprit;
	
	while (1)
	{
		if (field[y0][x0].instr == check)
		{
			culprit.x = x0;
			culprit.y = y0;
			return &culprit;
		}
		if (x0==x1 && y0==y1) break;
		e2 = err;
		if (e2 >-dx) { err -= dy; x0 += sx; }
		if (e2 < dy) { err += dx; y0 += sy; }
	}
	return NULL;
}

int execinstr(fthread* cfthread, cell* ccell)
{
	//printf("id=%d, instr=%c, x=%d, y=%d, tos=%d\n", cfthread->i, ccell.instr, cfthread->x, cfthread->y, cfthread->stack[cfthread->stackidx-1]);
	if (cfthread->mode == STEP) cfthread->mode = PAUSED;
#if debugout >= 3					
	if (cfthread->repeats || cfthread->delta!=1)
		printf("i=%d, delta=%d, repeats=%d\n", cfthread->i, cfthread->delta, cfthread->repeats);
#endif
	if ((!cfthread->repeats) && cfthread->stringmode && ccell->instr != '"')
	{
		push(cfthread, ccell->instr);
	}
	else if (cfthread->repeats || (!cfthread->jmpmode && ccell->instr != ';'));
	{
		int t1=0;
		int t2=0;
		int destx;
		int desty;
		cell* ccell2;
		coord* culprit;
		switch (ccell->instr&127)
		{
			case '!':
			{
				push(cfthread, !pop(cfthread));
				break;
			}
			case '"':
			{
				cfthread->stringmode = !cfthread->stringmode;
				break;
			}
			case '#':
			{
				cfthread->delta++;
				break;
			}
			case '$':
			{
				pop(cfthread);
				break;
			}
			case '%':
			{
				t1 = pop(cfthread);
				t2 = pop(cfthread);
				if (t1)
				{
					push(cfthread, t2%t1);
				}
				else
				{
					push(cfthread, 0);
				}
				
				break;
			}
			case '\'':
			{
				cfthread->delta++;
				ccell2 = &field[wrap(cfthread->y + cfthread->dy,CHEIGHT)][wrap(cfthread->x + cfthread->dx, CWIDTH)];
				push(cfthread, ccell2->instr);
				ccell2->bg = cfthread->team*2;
				break;
			}
			case '*':
			{
				push(cfthread, pop(cfthread) * pop(cfthread));
				break;
			}
			case '+':
			{
				push(cfthread, pop(cfthread) + pop(cfthread));
				break;
			}
			case '-':
			{
				t1 = pop(cfthread);
				push(cfthread, pop(cfthread)-t1);
				break;
			}
			case '/':
			{
				t1 = pop(cfthread);
				t2 = pop(cfthread);
				if (t1)
				{
					push(cfthread, t2/t1);
				}
				else
				{
					push(cfthread, 0);
				}
				break;
			}
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			{
				push(cfthread, ccell->instr - '0');
				break;
			}
			case ':':
			{
				push(cfthread, push(cfthread, pop(cfthread)));	//please optimise me!
				break;
			}
			case ';':
			{
				cfthread->jmpmode = !cfthread->jmpmode;
				break;
			}
			case '<':
			{
				cfthread->dx = -1;
				cfthread->dy = 0;
				break;
			}
			case '>':
			{
				cfthread->dx = 1;
				cfthread->dy = 0;
				break;
			}
			case '?':
			{
				switch (rand()&3)
				{
					case 0:
					{
						cfthread->dx = 1;
						cfthread->dy = 0;
						break;
					}
					case 1:
					{
						cfthread->dx = -1;
						cfthread->dy = 0;
						break;
					}
					case 2:
					{
						cfthread->dx = 0;
						cfthread->dy = 1;
						break;
					}
					case 3:
					{
						cfthread->dx = 0;
						cfthread->dy = -1;
						break;
					}
				}
				break;
			}
			case '@':
			case 'q':
			{
				if (cfthread->i == ghostid) break;
				killfthread(cfthread->i);
				cfthread->x = wrap(cfthread->x - cfthread->dx, CWIDTH);
				cfthread->y = wrap(cfthread->y - cfthread->dy, CHEIGHT);
				ccell->fg = 1;
				ccell->bg = cfthread->team*2;
				break;
			}
			case 'B':		//reflect if pop()>0
			{
				if (pop(cfthread)>0)
				{
					cfthread->dx = -cfthread->dx;
					cfthread->dy = -cfthread->dy;
				}
				break;
			}
			case 'H':		//_ but backwards
			{
				cfthread->dx = pop(cfthread)>0 ? 1 : -1;
				cfthread->dy = 0;
				break;
			}
			case 'I':		//| but backwards
			{
				cfthread->dx = 0;
				cfthread->dy = pop(cfthread)>0 ? -1 : 1;
				break;
			}
			case 'O':		//forth OVER = 1 PICK
			{
				push(cfthread, 1);
			}
			case 'P':		//forth PICK
			{
				t1 = -pop(cfthread)-1;
				t1 += cfthread->stackidx;
				if (t1<0) t1=0;
				if (t1>cfthread->stackidx) t1 = cfthread->stackidx;
				push(cfthread, cfthread->stack[t1]);
				break;
			}
			/*
			case 'R':		//forth ROLL
			{
				break;
			}
			case 'T':		//forth TUCK
			{	
				break;
			}
			//*/
			case 'W':		//w but backwards
			{
				t1 = pop(cfthread);
				t2 = pop(cfthread);
				if (t1<t2)
				{
					t1 = cfthread->dx;
					cfthread->dx = cfthread->dy;
					cfthread->dy = -t1;
				}
				else if (t1>t2)
				{
					t1 = cfthread->dx;
					cfthread->dx = -cfthread->dy;
					cfthread->dy = t1;
				}
				break;
			}
			case '[':
			{
				t1 = cfthread->dx;
				cfthread->dx = -cfthread->dy;
				cfthread->dy = t1;
				break;
			}
			case '\\':
			{
				t1 = pop(cfthread);
				t2 = pop(cfthread);
				push(cfthread, t1);
				push(cfthread, t2);
				break;
			}
			case ']':
			{
				t1 = cfthread->dx;
				cfthread->dx = cfthread->dy;
				cfthread->dy = -t1;
				break;
			}
			case '^':
			{
				cfthread->dx = 0;
				cfthread->dy = 1;
				break;
			}
			case '_':
			{
				cfthread->dx = pop(cfthread)>0 ? -1 : 1;
				cfthread->dy = 0;
				break;
			}
			case '`':
			{
				push(cfthread, pop(cfthread) < pop(cfthread));
				break;
			}
			case 'a':
			case 'b':
			case 'c':
			case 'd':
			case 'e':
			case 'f':
			{
				push(cfthread, ccell->instr - 'a' + 10); 
				break;
			}
			case 'g':
			{
				t1 = pop(cfthread);
				t2 = pop(cfthread);
				destx = wrap(t2*cfthread->dx - t1*cfthread->dy + cfthread->x, CWIDTH);
				desty = wrap(t1*cfthread->dx + t2*cfthread->dy + cfthread->y, CHEIGHT);
				ccell2 = &field[desty][destx];
				push(cfthread, ccell2->instr);
				ccell2->bg = cfthread->team*2;
				break;
			}
			case 'j':
			{
				cfthread->delta += pop(cfthread)-1;
				break;
			}
			case 'k':
			{
				//focusthread(cfthread);
				cfthread->repeats = pop(cfthread);
				cfthread->delta--;	//decrement delta because we're doing one here
				cfthread->x = wrap(cfthread->x + cfthread->dx, CWIDTH);
				cfthread->y = wrap(cfthread->y + cfthread->dy, CHEIGHT);
				//delta += execinstr(cfthread, ccell)-1;
				break;
			}
			case 'n':
			{
				cfthread->stackidx = 0;
				break;
			}
			case 'p':
			{
				t1 = pop(cfthread);
				t2 = pop(cfthread);
				if (cfthread->i == ghostid) break;
				destx = wrap(t2*cfthread->dx - t1*cfthread->dy + cfthread->x, CWIDTH);
				desty = wrap(t1*cfthread->dx + t2*cfthread->dy + cfthread->y, CHEIGHT);
				if (!(culprit = chkline(cfthread->x, cfthread->y, destx, desty, 'X')))
				{
					ccell2 = &field[desty][destx];
					ccell2->instr = pop(cfthread);
					ccell2->bg = cfthread->team*2;
				}
				break;
			}
				//q is with @
				//r is below t
			case 's':
			{
				if (cfthread->i == ghostid)
				{
					pop(cfthread);
					break;
				}
				cfthread->delta++;
				ccell2 = &field[wrap(cfthread->y + cfthread->dy,CHEIGHT)][wrap(cfthread->x + cfthread->dx, CWIDTH)];
				ccell2->instr = pop(cfthread);
				ccell2->bg = cfthread->team*2;
				break;
			}
			case 't':		//clone
			{
				if (cfthread->i == ghostid) break;
				dupfthread(&cfthread);
			}
			case 'X':		//running an X is the same as an r; X's main purpose is to MemBlock p, g, x, j, ", and ;
			case 'r':		//reflect
			{
				cfthread->dx = -cfthread->dx;
				cfthread->dy = -cfthread->dy;
				break;
			}
			case 'v':		//go down
			{
				cfthread->dx = 0;
				cfthread->dy = -1;
				break;
			}
			case 'w':
			{
				t1 = pop(cfthread);
				t2 = pop(cfthread);
				if (t1>t2)
				{
					t1 = cfthread->dx;
					cfthread->dx = cfthread->dy;
					cfthread->dy = -t1;
				}
				else if (t1<t2)
				{
					t1 = cfthread->dx;
					cfthread->dx = -cfthread->dy;
					cfthread->dy = t1;
				}
				break;
			}
			case 'x':
			{
				cfthread->dy = pop(cfthread);
				cfthread->dx = pop(cfthread);
				break;
			}
			case '|':
			{
				cfthread->dx = 0;
				cfthread->dy = pop(cfthread)>0 ? 1 : -1;
				break;
			}
			//default:
				//break;
		}
		if (cfthread->repeats) cfthread->repeats--;
	}				
	
	if (!cfthread->repeats)
	{
		cfthread->x = wrap(cfthread->x + cfthread->dx*cfthread->delta, CWIDTH);
		cfthread->y = wrap(cfthread->y + cfthread->dy*cfthread->delta, CHEIGHT);
	}
	field[cfthread->y][cfthread->x].fg = 1;//cfthread->team*2+1;
	field[cfthread->y][cfthread->x].bg = cfthread->team*2+1;
	if (!cfthread->repeats) cfthread->delta=1;
	//printf("id=%d, instr=%c, x=%d, y=%d, tos=%d\n", cfthread->i, ccell.instr, cfthread->x, cfthread->y, cfthread->stack[cfthread->stackidx-1]);
}

void* interpreter(void* threadid)
{
	int id = (int)threadid;
	
	int x;
	int y;
	
	for (y=0; y<CHEIGHT; y++)
	{
		for (x=0; x<CWIDTH; x++)
		{
			field[y][x].instr = 0;//*(rand()&1) ? 0 :*/ rand()%96 + 32;
			field[y][x].fg = 1;//rand() % 8;
			field[y][x].bg = 0;//(rand()&31) ? 0 : rand() % 8;
		}
	}
	
	unsigned int i;
	
	int x2;
	int y2;
	char cchar;
	
	for (i=0; i<8; i++)
	{
		//char* fname = asprintf("warriors/%d.b98", i);
		//fp = fopen(fname, "r");
		//free(fname);
		char* fname;
		asprintf(&fname, "warriors/%d.b98", i);
		FILE* fp = fopen(fname, "r");
		free(fname);
		x2=x= rand()%CWIDTH;
		y2=y= rand()%CHEIGHT;
		//x2=x= 16;
		//y2=y;//= 128-i*16;
		while (fread(&cchar, 1, 1, fp))
		{
			switch (cchar)
			{
				case '\n':
				{
					y2--;
					if (y2<0) y2+=CHEIGHT;
					x2=x;
					break;
				}
				default:
				{
					//cell ccell = ;
					field[y2][x2].instr = cchar;
					field[y2][x2++].bg = (i+1)*2;
					x2%=CWIDTH;
					break;
				}
			}
		}
		newfthread(i+1, x, y, 1, 0, 0);
		//y=y2-16;
		fclose(fp);
	
	}
	
	
	while (1)
	{
		fmode run2 = run;
		//unsigned int i;
		for (i=0; i<fthreadslen; i++)
		{
			fthread* cfthread = &fthreads[i];
			if (cfthread->alive != DEAD && (((run2 != PAUSED) && cfthread->mode == RUN) || cfthread->mode == STEP))
			{
				cell* ccell = &field[cfthread->y][cfthread->x];
				field[cfthread->y][cfthread->x].fg = 1;
				field[cfthread->y][cfthread->x].bg = cfthread->team*2;
				execinstr(cfthread, ccell);
			}
		}
		if (run2 == STEP) run = PAUSED;
		if (delay > 4)
		{
			int delayed;
			int cdelay;
			for (delayed=0, cdelay=delay/4; delayed<delay; delayed+=(cdelay=min(delay/4, delay-delayed)))
			{
				usleep(cdelay);
			}
		}
	}
	
	printf("ERROR: interpreter thread quitting!\n", id);
	pthread_exit(NULL);
}
