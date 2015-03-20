/*
 *  fungewars.c
 *  fungewars
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <time.h>

#include <math.h>
#include <string.h>
 
#include "fungewars.h"

#include "interpreter.h"
#include "graphics.h"


//#define wrap(a,b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); __typeof__ (a) c = _a % _b; (c<0)?(_b+c):(c) })

#define NUM_THREADS 1	//don't change this, better yet, remove it

#define debugout 1

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int keys[512];
int modkeys;

fthread* fthreads;
unsigned int fthreadslen = 0;

cell field[CHEIGHT][CWIDTH];

int cthread = -1;

//enum {NORMAL, EX, SREPLACE, REPLACE, VISUAL, INSERT} uimode = NORMAL;

char uilastcmd = 0;

char* excmd;
int exlen;
int exmax;

cell* statusline;
int statuslinelen;

int xi;
int yi;

int xiv;
int yiv;


pthread_t threads[NUM_THREADS];
pthread_mutex_t fthreadsmutex;

int wrap(int x, int m)
{
	int c = x%m;
	return (c<0)?(c+m):c;
}

void newgame()
{
	unsigned int i;
	for (i=0; i<fthreadslen; i++)
	{
		if (fthreads[i].alive != DEAD) killfthread(i);
	}
	free(fthreads);
	fthreads = malloc(INITIAL_FTHREADS*sizeof(fthread));
	fthreadslen = INITIAL_FTHREADS;
	for (i=0; i<INITIAL_FTHREADS; i++)
	{
		fthreads[i].alive = DEAD;
	}
	
	ghostid = newfthread(8, 0, 0, 0, 0, NFT_GHOST)->i;
}

color color_clear = {0.0, 0.0, 0.0, 0.0};

color color_status_fg = {1.0, 1.0, 1.0, 1.0};
color color_status_bg = {0.0, 0.0, 0.0, 0.75};

int status_dirty = 0;

void setstatus_color(int i, char c, color* fg, color* bg)
{
	statusline[i].instr = c;
	statusline[i].fg = fg;
	statusline[i].bg = bg;
}

void setstatus_c(int i, char c)
{
	setstatus_color(i, c, &color_status_fg, &color_status_bg);
}

void setstatus(const char* s)
{
	if (status_dirty)
	{
		clrstatus();
	}

	status_dirty = 1;

	int i=0;
	while (*s)
	{
		setstatus_c(i++, *s++);
	}
}

void clrstatus()
{
	for (int i=0; i<statuslinelen; i++)
	{
		setstatus_color(i, ' ', &color_status_fg, &color_clear);
	}
	exlen = 0;

	status_dirty = 0;
}

void focusthread(fthread* cfthread)
{
	cthread = cfthread->i;
	cx = cfthread->x - swidth/2;
	cy = cfthread->y - sheight/2;
	ccdx = 0;
	ccdy = 0;
	//delay = 100000;
}

void focuscam(float x, float y)
{
	cx = x - swidth/2;
	cy = y - sheight/2;
	ccdx = 0;
	ccdy = 0;
}

// process ex command
void doex()
{
	printf("ex command: %s\n", excmd);

	char* cmd = strtok(excmd, " ");

	if (cmd == NULL)
	{
		return;
	}

	if (!strcmp(cmd, "q"))
	{
		glutLeaveMainLoop();
	}

	if (!strcmp(cmd, "load"))
	{
		char* path = strtok(NULL, " ");
		char* team_s = strtok(NULL, " ");
		if (path == NULL)
		{
			setstatus("Usage: load <path> <team id>");
			return;
		}
		int team;
		if (team_s == NULL)
		{
			team = -1;
		}
		else
		{
			team = atoi(team_s);
		}
		int status = loadwarrior(xi, yi, team, path);
		switch (status)
		{
			case 0:
			{
				break;
			}
			case 1:
			{
				setstatus("Could not open file for reading.");
				break;
			}
		}
	}

}

void kb1(unsigned char key, int x, int y)
{
	modkeys = glutGetModifiers();
	//printf("%d down\n", key);
	keys[key] = 1;
	
	int t1;
	if (key == 27)
	{

		for (int i=0; i<exlen+1; i++)
		{
			statusline[i].instr = ' ';
		}
		uimode = NORMAL;
		clrstatus();
	}
	switch (uimode)
	{
		case NORMAL:
		{
			//int pfx = 0;

			switch (key)
			{
				// mode switching
				case 'r': // single character replace mode
				{
					uimode = SREPLACE;
					setstatus("-- CHAR REPLACE --");
					break;
				}
				case 'R': // replace mode
				{
					uimode = REPLACE;
					setstatus("-- REPLACE --");
					break;
				}
				case 'v': // visual mode
				{
					xiv = xi;
					yiv = yi;

					uimode = VISUAL;
					setstatus("-- VISUAL --");
					break;
				}

				case ' ': // step
				{
					run = STEP;
					break;
				}

				case '\r':
				case '\n':
				{
					run = (run==RUN) ? PAUSED : RUN;
					break;
				}
			}
			break;
		}
		case SREPLACE:
		{
			field[yi][xi].instr = key;
			uimode = NORMAL;
			clrstatus();
			break;
		}
		case REPLACE:
		{
			switch (key)
			{
				case 9:
				case 25:
				case 8:
				case 127:
				{
					break;
				}
				case '\r':
				case '\n':
				{
					if (cthread == ghostid && ghostid != -1)
					{
						fthreads[ghostid].mode = STEP;
					}
					break;
				}
				default:
				{
					field[yi][xi].instr = key;
					break;
				}
			}

			break;
		}
		case VISUAL:
		{
			switch (key)
			{
				case ' ': // flip selection (board wrap)
				{
					break;
				}
			}
			break;
		}
		case EX:
		{
			switch (key)
			{
				case 8:
				{
					setstatus_color(exlen, ' ', &color_status_fg, &color_clear);

					if (exlen > 0)
					{
						excmd[--exlen] = 0;
					}
					else
					{
						uimode = NORMAL;
						clrstatus();
					}
					break;
				}
				case '\r':
				case '\n':
				{
					uimode = NORMAL;
					clrstatus();

					doex();

					break;
				}
				default:
				{
					excmd[exlen++] = key;
					setstatus_c(exlen, key);

					if (exlen >= exmax)
					{
						excmd = (char*)realloc(excmd, exmax*=2);
						printf("resize ex buffer to %d bytes\n", exmax);
					}
					excmd[exlen] = 0;
					break;
				}
			}
			break;
		}
	}

	if (uimode == NORMAL || uimode == VISUAL)
	{
		switch (key)
		{
			case ':':
			{
				clrstatus();
				uimode = EX;
				setstatus_c(0, ':');
				excmd[0] = 0;
				break;
			}

			case '+': // faster
			case '=':
			{
				if (delay>1) delay/=2;
				printf("delay: %d\n", delay);
				break;
			}
			case '-': // slower
			{
				if (delay<2000000) delay*=2;;
				printf("delay: %d\n", delay);
				break;
			}


			case '[': // zoom out
			{
				czoom /= 1.25;
				break;
			}
			case ']': // zoom in
			{
				czoom *= 1.25;
				break;
			}
		}
	}

	switch (key)
	{
		case 9:
		{
			t1 = cthread>=0 ? cthread : fthreadslen-1;
			do
			{
				cthread = (cthread+1)%fthreadslen;
			}
			while ((fthreads[cthread].alive == DEAD  && t1 != cthread) || cthread == ghostid);
			if (t1==cthread)
			{
				cthread = -1;
			}
			else
			{
				focuscam(fthreads[cthread].x*charwidth, fthreads[cthread].y*charheight);
			}

			printf("follow %d\n", cthread);
			
			cdx = 0.0;
			cdy = 0.0;
			
			lastupdate = 1;
			
			break;
		}
		case 25:
		{
			t1 = cthread>=0 ? cthread : 0;
			do
			{
				cthread = (cthread-1+fthreadslen)%fthreadslen;
			}
			while ((fthreads[cthread].alive == DEAD  && t1 != cthread) || cthread == ghostid);
			if (t1==cthread)
			{
				cthread = -1;
			}
			else
			{
				focuscam(fthreads[cthread].x*charwidth, fthreads[cthread].y*charheight);
			}
			
			printf("follow %d\n", cthread);
			
			cdx = 0.0;
			cdy = 0.0;
			
			lastupdate = 1;

			break;
		}
		/*
		case '+':
		case '=':
		{
			if (delay>1) delay/=1.5;
			printf("delay: %d\n", delay);
			break;
		}
		case '-':
		case '_':
		{
			delay*=1.5;
			printf("delay: %d\n", delay);
			break;
		}
		//*/
		case 8:
		case 127:
		{
			field[yi][xi].instr=0;
			if (cthread == ghostid && ghostid != -1)
			{
				fthreads[ghostid].delta *= -1;
				fthreads[ghostid].mode = STEP;
			}
			break;
		}
	}

	uilastcmd = key;
	//glutPostRedisplay();
}

void kb1u(unsigned char key, int x, int y)
{
	modkeys = glutGetModifiers();
	//printf("%d up\n", key);
	keys[key] = 0;
}

void kb2(int key, int x, int y)
{
	modkeys = glutGetModifiers();
	//printf("%d down\n", key+256);
	keys[key+256] = 1;
	switch (key)
	{
		case 1:
		{
			if (delay>1) delay/=2;
			printf("delay: %d\n", delay);
			break;
		}
		case 2:
		{
			if (delay<2000000) delay*=2;;
			printf("delay: %d\n", delay);
			break;
		}
		case 3:
		{
			czoom /= 1.25;
			break;
		}
		case 4:
		{
			czoom *= 1.25;
			break;
		}
		case 8:
		{
			run = (run==RUN) ? PAUSED : RUN;
			break;
		}
	}
	//pthread_mutex_lock(&fthreadsmutex);
	if (cthread >= 0)
	{
		fthread cfthread = fthreads[cthread];
		if (cfthread.alive != DEAD)
		{
			switch (key)
			{
				case 5:
				{
					cfthread.mode = STEP;
					break;
				}
				case 6:
				{
					printf("stack trace for thread %d:\n", cthread);
					int i;
					for (i=0; i<cfthread.stackidx; i++)
					{
						printf("%d: %d\n", i, cfthread.stack[i]);
					}
					break;
				}
				case 7:
				{
					cthread = -1;
					break;
				}
				case 9:
				{
					cfthread.mode = (cfthread.mode==RUN) ? PAUSED : RUN; 
					break;
				}
				default:
				{
					break;
				}
			}
		}
	}
	else
	{
		switch (key)
		{
			case 5:
			{
				run = STEP;
				break;
			}
			case 7:
			{
				cthread = getfthread(xi, yi);

				if (cthread == -1)
				{
					fthreads[ghostid].x = xi;
					fthreads[ghostid].y = yi;
					focusthread(&fthreads[ghostid]);
				}
				break;
			}
			case 9:
			{	run = (run==RUN) ? PAUSED : RUN; 
				break;
			}
			default:
			{
				break;
			}
		}
	}
	//pthread_mutex_unlock(&fthreadsmutex);
}

void kb2u(int key, int x, int y)
{
	modkeys = glutGetModifiers();
	//printf("%d up\n", key+256);
	keys[key+256] = 0;
}

void idle(void)
{
	if (keys[357] || ((uimode == NORMAL || uimode == VISUAL) && (keys['k'] || keys['w'] || keys['W'])))
	{
		cy+=(3.0+6.0*(modkeys&GLUT_ACTIVE_ALT))/cczoom;
		if (cthread != ghostid)
		{
			cthread = -1;
		}
		else
		{
			pthread_mutex_lock(&fthreadsmutex);
			fthreads[ghostid].dx = 0;
			fthreads[ghostid].dy = 1;
			fthreads[ghostid].mode = STEP;
			pthread_mutex_unlock(&fthreadsmutex);
		}
	}
	if (keys[356] || ((uimode == NORMAL || uimode == VISUAL) && (keys['h'] || keys['a'] || keys['A'])))
	{
		cx-=(3.0+6.0*(modkeys&GLUT_ACTIVE_ALT))/cczoom;
		if (cthread != ghostid) cthread = -1;
		else
		{
			pthread_mutex_lock(&fthreadsmutex);
			fthreads[ghostid].dx = -1;
			fthreads[ghostid].dy = 0;
			fthreads[ghostid].mode = STEP;
			pthread_mutex_unlock(&fthreadsmutex);
		}
	}
	if (keys[359] || ((uimode == NORMAL || uimode == VISUAL) && (keys['j'] || keys['s'] || keys['S'])))
	{
		cy-=(3.0+6.0*(modkeys&GLUT_ACTIVE_ALT))/cczoom;
		if (cthread != ghostid) cthread = -1;
		else
		{
			pthread_mutex_lock(&fthreadsmutex);
			fthreads[ghostid].dx = 0;
			fthreads[ghostid].dy = -1;
			fthreads[ghostid].mode = STEP;
			pthread_mutex_unlock(&fthreadsmutex);
		}
	}
	if (keys[358] || ((uimode == NORMAL || uimode == VISUAL) && (keys['l'] || keys['d'] || keys['D'])))
	{
		cx+=(3.0+6.0*(modkeys&GLUT_ACTIVE_ALT))/cczoom;
		if (cthread != ghostid) cthread = -1;
		else
		{
			pthread_mutex_lock(&fthreadsmutex);
			fthreads[ghostid].dx = 1;
			fthreads[ghostid].dy = 0;
			fthreads[ghostid].mode = STEP;
			pthread_mutex_unlock(&fthreadsmutex);
		}
	}

	xi = (cx+swidth/2)/charwidth;
	yi = (cy+sheight/2)/charheight;

	
	/*
	int i;
	int k=0;
	for (i=0; i<512; i++)
	{
		if (keys[i])
		{
			printf("%d",i);
			k++;
		}
	}
	if (k)
		printf("\n");
	//*/
	glutPostRedisplay();
}

int main(int argc, char** argv)
{
	srand(time(NULL));

	exmax = 128;
	excmd = (char*)malloc(exmax*sizeof(char));
	exlen = 0;

	gl_init();

	statuslinelen = swidth / charwidth;
	statusline = (cell*)malloc(statuslinelen*sizeof(cell));

	clrstatus();

	newgame();
	
	//glutMainLoop();
	
	pthread_mutex_init(&fthreadsmutex, NULL);
	
	pthread_attr_t threadattr;
	pthread_attr_init(&threadattr);
	//pthread_attr_setstacksize(&threadattr, 2097152);
	
	int i;
	for (i=0; i<NUM_THREADS; i++)
	{
		printf("main: creating thread %d\n", i);
		int rc = pthread_create(&threads[i], NULL, interpreter, (void *)i);
		if (rc)
		{
			printf("ERROR; return code from pthread_create() is %d\n", rc);
			exit(-1);
		}
	}
	
	glutMainLoop();
	return EXIT_SUCCESS;
}
