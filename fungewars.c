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

uimode_t uimode;
uimode_t uiprevmode;

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


coord marks[256];

pthread_t threads[NUM_THREADS];
pthread_mutex_t fthreadsmutex;

int wrap(int x, int m)
{
	int c = x%m;
	return (c<0)?(c+m):c;
}

void newgame(void)
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

void clrstatus(void)
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

void focuscam(int x, int y)
{
	xi = x;
	yi = y;
	cx = x*charwidth - swidth/2;
	cy = y*charheight - sheight/2;
	ccdx = 0;
	ccdy = 0;
}


search_result* search_first_result;

search_result* search_curr_result;

int search_status[CHEIGHT][CWIDTH];

int nallocs = 0; // to check for leaks

void search_result_new(search_cell* first_cell)
{

	//printf("found %c at (%d, %d)\n", first_cell->fc, first_cell->x, first_cell->y);

	search_result* second_result = search_first_result;

	search_first_result = (search_result*)malloc(sizeof(search_result));

	search_first_result->refs = 1;

	search_first_result->next = second_result;

	search_first_result->this = first_cell;

	first_cell->refs++;

	nallocs++;

	// we don't have to kill(second_result) because we didn't refs++ it
}

void search_result_kill(search_result* this)
{
	if (this == NULL)
	{
		return;
	}

	if (--this->refs > 0)
	{
		return;
	}

	if (this->this != NULL)
	{
		search_cell_kill(this->this);
	}

	if (this->next != NULL)
	{
		search_result_kill(this->next);
	}

	nallocs--;

	free(this);
}

search_cell* search_cell_new(search_cell* parent, int x, int y, int dx, int dy, char fc, char pc)
{
	search_cell* this = (search_cell*)malloc(sizeof(search_cell));

	if (parent != NULL)
	{
		parent->refs++;
	}

	this->next = parent;

	this->refs = 1;

	this->x = x;
	this->y = y;
	this->dx = dx;
	this->dy = dy;

	this->fc = fc;
	this->pc = pc;

	nallocs++;

	return this;
}

void search_cell_kill(search_cell* this)
{
	if (this == NULL)
	{
		return;
	}

	if (--this->refs > 0)
	{
		return;
	}

	if (this->refs < 0)
	{
		printf("warning: double free!\n");
	}

	search_cell_kill(this->next);

	nallocs--;

	free(this);
}

int search_match(const char** pattern, char target)
{
	if (**pattern == target)
	{
		(*pattern)++;
		return 1;
	}
	return 0;
}

void search2(search_cell* parent, int x, int y, int dx, int dy, const char* pattern)
{
	x = wrap(x, CWIDTH);
	y = wrap(y, CHEIGHT);

	char pc = *pattern;
	char fc = field[y][x].instr;

	//printf("search2(%d, %d, %d, %d, '%c'): '%c'\n", x, y, dx, dy, pc, fc);

	if (search_status[y][x] & SEARCH_VISITED)
	{
		return;
	}


	if (pc == 0)
	{
		search_result_new(parent);
		return;
	}

	search_status[y][x] |= SEARCH_VISITED;

	search_cell* this = search_cell_new(parent, x, y, dx, dy, fc, pc);

	const char* nextpc = pattern;

	int matched = 0;

#define MATCHCHAR(c) { if (!matched && pc == c) { matched = 1; nextpc++; }}
#define MATCHDIR(dx2, dy2, c) { if (dx == dx2 && dy == dy2) { MATCHCHAR(c) }}

	MATCHCHAR(fc)

	switch (fc)
	{
		case '>':
		{
			MATCHDIR( 0,-1, '[')
			MATCHDIR( 0, 1, ']')
			MATCHDIR(-1, 0, 'r')
			search2(this, x+1, y+0,  1,  0, nextpc);
			break;
		}
		case '<':
		{
			MATCHDIR( 0, 1, '[')
			MATCHDIR( 0,-1, ']')
			MATCHDIR( 1, 0, 'r')
			search2(this, x-1, y+0, -1,  0, nextpc);
			break;
		}
		case 'v':
		{
			MATCHDIR(-1, 0, '[')
			MATCHDIR( 1, 0, ']')
			MATCHDIR( 0, 1, 'r')
			search2(this, x+0, y-1,  0, -1, nextpc);
			break;
		}
		case '^':
		{
			MATCHDIR( 1, 0, '[')
			MATCHDIR(-1, 0, ']')
			MATCHDIR( 0,-1, 'r')
			search2(this, x+0, y+1,  0,  1, nextpc);
			break;
		}

		case '[':
		{
			MATCHDIR( 0, 1, '<')
			MATCHDIR( 0,-1, '>')
			MATCHDIR( 1, 0, '^')
			MATCHDIR(-1, 0, 'v')
			search2(this, x-dy, y+dx, -dy, dx, nextpc);
			break;
		}
		case ']':
		{
			MATCHDIR( 0, 1, '>')
			MATCHDIR( 0,-1, '<')
			MATCHDIR( 1, 0, 'v')
			MATCHDIR(-1, 0, '^')
			search2(this, x+dy, y-dx, dy, -dx, nextpc);
			break;
		}

		case 'r':
		{
			MATCHDIR( 0, 1, 'v')
			MATCHDIR( 0,-1, '^')
			MATCHDIR( 1, 0, '<')
			MATCHDIR(-1, 0, '>')
			search2(this, x-dx, y-dy, -dx, -dy, nextpc);
			break;
		}

		case '#':
		case 's':
		case '\'':
		{
			search2(this, x+2*dx, y+2*dy, dx, dy, nextpc);
			break;
		}

		case 0:
		case 'z':	// z is still a nop, right?
		case ' ':
		{
			search2(this, x+dx, y+dy, dx, dy, nextpc);
			break;
		}

		case '?':
		{
			search2(this, x+1, y+0,  1,  0, nextpc);
			search2(this, x-1, y+0, -1,  0, nextpc);
			search2(this, x+0, y+1,  0,  1, nextpc);
			search2(this, x+0, y-1,  0, -1, nextpc);
			break;
		}

		case 't':
		{
			search2(this, x+dx, y+dy, dx, dy, nextpc);
			search2(this, x-dx, y-dy, -dx, -dy, nextpc);
			break;
		}


		default:
		{
			if (!matched)
			{
				break;
			}
			search2(this, x+dx, y+dy, dx, dy, nextpc);
			break;
		}
	}

	search_status[y][x] &= !SEARCH_VISITED;

	search_cell_kill(this);
}

void search(const char* pattern)
{
	for (int y=0; y<CHEIGHT; y++)
	{
		for (int x=0; x<CWIDTH; x++)
		{
			search_status[y][x] = 0;
		}
	}

	search_result_kill(search_first_result);
	search_first_result = NULL;

	search_result_kill(search_curr_result);
	search_curr_result = NULL;

	if (nallocs)
	{
		printf("Warning: memory leak! nallocs = %d\n", nallocs);
	}

	if (!*pattern)
	{
		return;
	}

	int x=xi;
	int y=yi;

	int endx = wrap(xi, CWIDTH) - 1;
	int endy = wrap(yi, CHEIGHT);

	if (endx < 0)
	{
		endx += CWIDTH;
		endy--;
		if (endy<0)
		{
			endy += CHEIGHT;
		}
	}

	printf("presearch: (%d, %d), (%d, %d)\n", x, y, endx, endy);

	for (;; y = wrap(y+1, CHEIGHT))
	{
		//printf("search row: %d\n", y);
		for (; x<CWIDTH; x++)
		{
			//printf("search: %d, %d\n", x, y);
			if (field[y][x].instr == *pattern)
			{
				for (int d=0; d<=3; d++)
				{
					if ( (d==0 && (*pattern == '<' || *pattern == '^' || *pattern == 'v'))
					  || (d==1 && (*pattern == 'v' || *pattern == '<' || *pattern == '>'))
					  || (d==2 && (*pattern == '>' || *pattern == '^' || *pattern == 'v'))
					  || (d==3 && (*pattern == '^' || *pattern == '<' || *pattern == '>')))
					{
						continue;
					}

					int dx;
					int dy;
					switch (d)
					{
						case 0:
						{
							dx = 1;
							dy = 0;
							break;
						}
						case 1:
						{
							dx = 0;
							dy = 1;
							break;
						}
						case 2:
						{
							dx = -1;
							dy = 0;
							break;
						}
						case 3:
						{
							dx = 0;
							dy = -1;
							break;
						}
					}

					//printf("search: %d, %d, %d\n", x, y, d);

					search_cell* this = search_cell_new(NULL, x, y, dx, dy, *pattern, *pattern);

					search2(this, x+dx, y+dy, dx, dy, pattern+1);

					search_cell_kill(this);
				}
			}
			if (x == endx && y == endy)
			{
				goto search_done;
			}
		}
		x=0;
	}

search_done:

	search_curr_result = search_first_result;

	if (search_first_result == NULL)
	{
		setstatus("No results");
	}
	else
	{

		focuscam(search_curr_result->this->x, search_curr_result->this->y);
		search_curr_result->refs++;
	}

}

// process command
void docmd(char* cmd)
{
	switch (excmd[0])
	{
		case ':':
		{
			printf("ex command: %s\n", cmd);

			char* cmdname = strtok(&cmd[1], " ");

			if (cmdname == NULL)
			{
				return;
			}

			if (!strcmp(cmdname, "q"))
			{
				glutLeaveMainLoop();
			}

			if (!strcmp(cmdname, "load"))
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

			if (!strcmp(cmdname, "noh"))
			{
				search_curr_result = NULL;
			}
			break;
		}
		case '/':
		{
			printf("search: %s\n", &cmd[1]);
			search(&cmd[1]);
			break;
		}
	}

}

void kb1(unsigned char key, int x, int y)
{
	modkeys = glutGetModifiers();
	//printf("%d down\n", key);
	
	int t1;
	if (key == 27)
	{

		for (int i=0; i<exlen+1; i++)
		{
			statusline[i].instr = ' ';
		}
		uimode = uiprevmode;
		uiprevmode = NORMAL;
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


				case 'm': // set mark
				{
					uiprevmode = uimode; // = NORMAL
					uimode = MARK_SET;
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


				default:
				{
					keys[key] = 1;
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

				default:
				{
					keys[key] = 1;
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
					setstatus_color(--exlen, ' ', &color_status_fg, &color_clear);

					if (exlen > 0)
					{
						excmd[exlen] = 0;
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

					docmd(excmd);

					break;
				}
				default:
				{
					excmd[exlen] = key;
					setstatus_c(exlen++, key);

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

		case MARK_SET:
		{
			marks[key].x = xi;
			marks[key].y = yi;

			uimode = uiprevmode; // = NORMAL
			uiprevmode = NORMAL;
			break;
		}

		case MARK_GET:
		{
			int x = marks[key].x;
			int y = marks[key].y;
			if (x>=0 && y>=0 && x<CWIDTH && y<CHEIGHT)
			{
				focuscam(x, y);
			}
			else
			{
				setstatus("Mark not set");
			}

			uimode = uiprevmode;
			uiprevmode = NORMAL;
			break;
		}
	}

	if (uimode == NORMAL || uimode == VISUAL)
	{
		switch (key)
		{
			case '/':
			case ':':
			{
				clrstatus();
				uimode = EX;
				setstatus_c(0, key);
				excmd[0] = key;
				excmd[1] = 0;
				exlen = 1;
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

			case 'n': // next search result
			{
				setstatus(excmd);
				if (search_curr_result != NULL)
				{
					search_result* next = search_curr_result->next;
					//search_result_kill(search_curr_result);

					search_curr_result = next;

					if (search_curr_result == NULL)
					{
						setstatus("Search wrapped around");
					}
				}

				if (search_curr_result == NULL)
				{
					search_curr_result = search_first_result;
					//setstatus("Restart results");
				}

				if (search_first_result == NULL)
				{
					setstatus("No results");
				}

				if (search_curr_result != NULL)
				{
					focuscam(search_curr_result->this->x, search_curr_result->this->y);
					//search_curr_result->refs++;
				}
				break;
			}


			case '\'': // go to mark
			{
				uiprevmode = uimode;
				uimode = MARK_GET;
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
				focuscam(fthreads[cthread].x, fthreads[cthread].y);
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
				focuscam(fthreads[cthread].x, fthreads[cthread].y);
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

	uimode = uiprevmode = NORMAL;

	for (int i=0; i<256; i++)
	{
		marks[i].x = -1;
		marks[i].y = -1;
	}
	
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
