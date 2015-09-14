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

int keys[N_KEYS];
int modkeys;

fthread* fthreads;
unsigned int fthreadslen = 0;

field* curr_field;

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


coord marks[N_KEYS];

pthread_t threads[NUM_THREADS];
pthread_mutex_t fthreadsmutex;

int wrap(int x, int m)
{
	int c = x%m;
	return (c<0)?(c+m):c;
}

void newgame(void)
{
	if (run == RUN)
	{
		run = STEP;
	}

	while (run == STEP);

	curr_field = field_reset(curr_field, 256, 256);

	cam_curr = view_reset(cams[0], curr_field, 0.0, 0.0, rswidth, rsheight, 0.0, 0.0, 1.0);

	view* cam2 = view_reset(cams[1], curr_field, 0.0, rsheight/2, rswidth, rsheight/2, 0.0, 0.0, 1.0);
	cam2->state = DISABLED;

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

	ghostid = newfthread(curr_field, 8, 0, 0, 0, 0, NFT_GHOST)->i;

	clrstatus();
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

	status_dirty = 1;
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

void focusthread(view* v, fthread* cfthread)
{
	cam_curr->follow = cfthread->i;
	//cam_curr->x = cfthread->x - v->zswidth/2;
	//cam_curr->y = cfthread->y - v->zsheight/2;
}

void focuscam(view* v, int x, int y)
{
	xi = x;
	yi = y;
	cam_curr->x = x*charwidth - v->zswidth/2;
	cam_curr->y = y*charheight - v->zsheight/2;
}

// search

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

void search2(search_cell* parent, int x, int y, int dx, int dy, const char* pattern, int skip)
{
	x = wrap(x, curr_field->width);
	y = wrap(y, curr_field->height);

	char pc = *pattern;
	char fc = field_get(curr_field, x, y)->instr;

	//printf("search2(%d, %d, %d, %d, '%c', 1): '%c'\n", x, y, dx, dy, pc, fc);

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

	search_cell* this = search_cell_new(parent, x, y, dx*skip, dy*skip, fc, pc);

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
			search2(this, x+1, y+0,  1,  0, nextpc, 1);
			break;
		}
		case '<':
		{
			MATCHDIR( 0, 1, '[')
			MATCHDIR( 0,-1, ']')
			MATCHDIR( 1, 0, 'r')
			search2(this, x-1, y+0, -1,  0, nextpc, 1);
			break;
		}
		case 'v':
		{
			MATCHDIR(-1, 0, '[')
			MATCHDIR( 1, 0, ']')
			MATCHDIR( 0, 1, 'r')
			search2(this, x+0, y-1,  0, -1, nextpc, 1);
			break;
		}
		case '^':
		{
			MATCHDIR( 1, 0, '[')
			MATCHDIR(-1, 0, ']')
			MATCHDIR( 0,-1, 'r')
			search2(this, x+0, y+1,  0,  1, nextpc, 1);
			break;
		}

		case '[':
		{
			MATCHDIR( 0, 1, '<')
			MATCHDIR( 0,-1, '>')
			MATCHDIR( 1, 0, '^')
			MATCHDIR(-1, 0, 'v')
			search2(this, x-dy, y+dx, -dy, dx, nextpc, 1);
			break;
		}
		case ']':
		{
			MATCHDIR( 0, 1, '>')
			MATCHDIR( 0,-1, '<')
			MATCHDIR( 1, 0, 'v')
			MATCHDIR(-1, 0, '^')
			search2(this, x+dy, y-dx, dy, -dx, nextpc, 1);
			break;
		}

		case 'r':
		{
			MATCHDIR( 0, 1, 'v')
			MATCHDIR( 0,-1, '^')
			MATCHDIR( 1, 0, '<')
			MATCHDIR(-1, 0, '>')
			search2(this, x-dx, y-dy, -dx, -dy, nextpc, 1);
			break;
		}

		case '#':
		case 's':
		case '\'':
		{
			search2(this, x+2*dx, y+2*dy, dx, dy, nextpc, 2);
			break;
		}

		case 0:
		case 'z':	// z is still a nop, right?
		case ' ':
		{
			search2(this, x+dx, y+dy, dx, dy, nextpc, 1);
			break;
		}

		case '?':
		{
			search2(this, x+1, y+0,  1,  0, nextpc, 1);
			search2(this, x-1, y+0, -1,  0, nextpc, 1);
			search2(this, x+0, y+1,  0,  1, nextpc, 1);
			search2(this, x+0, y-1,  0, -1, nextpc, 1);
			break;
		}

		case 't':
		{
			search2(this, x+dx, y+dy, dx, dy, nextpc, 1);
			search2(this, x-dx, y-dy, -dx, -dy, nextpc, 1);
			break;
		}


		default:
		{
			if (!matched)
			{
				break;
			}
			search2(this, x+dx, y+dy, dx, dy, nextpc, 1);
			break;
		}
	}

	search_status[y][x] &= !SEARCH_VISITED;

	search_cell_kill(this);
}

void search(const char* pattern)
{
	for (int y=0; y<curr_field->height; y++)
	{
		for (int x=0; x<curr_field->width; x++)
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

	int endx = wrap(xi, curr_field->width) - 1;
	int endy = wrap(yi, curr_field->height);

	if (endx < 0)
	{
		endx += curr_field->width;
		endy--;
		if (endy<0)
		{
			endy += curr_field->height;
		}
	}

	printf("presearch: (%d, %d), (%d, %d)\n", x, y, endx, endy);

	for (;; y = wrap(y+1, curr_field->height))
	{
		//printf("search row: %d\n", y);
		for (; x<curr_field->width; x++)
		{
			//printf("search: %d, %d\n", x, y);
			if (field_get(curr_field, x, y)->instr == *pattern)
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

					search2(this, x+dx, y+dy, dx, dy, pattern+1, 1);

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

		focuscam(cam_curr, search_curr_result->this->x, search_curr_result->this->y);
		search_curr_result->refs++;
	}

}

field* field_reset(field* this, int width, int height)
{
	if (this == NULL)
	{
		this = (field*)malloc(sizeof(field));
	}

	this->width = width;
	this->height = height;

	cell (*cells)[height][width] = malloc(sizeof(cell[height][width]));

	this->cells = cells;

	int x;
	int y;

	for (y=0; y<height; y++)
	{
		for (x=0; x<width; x++)
		{
			//printf("field_new: %d, %d\n", y, x);
			(*cells)[y][x].instr = 0;
			(*cells)[y][x].fg = &color_fg;
			(*cells)[y][x].bg = NULL;
		}
	}

	return this;
}

field* field_new(int width, int height)
{
	return field_reset(NULL, width, height);
}

void field_kill(field* f)
{
	free(f->cells);
	free(f);
}

cell* field_get(field* f, int x, int y)
{
	return &((cell(*)[f->width])(f->cells))[wrap(y, f->height)][wrap(x, f->width)];
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
			else if (!strcmp(cmdname, "q"))
			{
				glutLeaveMainLoop();
			}
			else if (!strcmp(cmdname, "load"))
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
				int status = loadwarrior(curr_field, xi, yi, team, path);
				if (status)
				{
					char path2[strlen(path)+strlen("warriors/?.b98")];
					sprintf(path2, "warriors/%s.b98", path);
					status = loadwarrior(curr_field, xi, yi, team, path2);
				}
				if (status)
				{
					char path2[strlen(path)+strlen("?.b98")];
					sprintf(path2, "%s.b98", path);
					status = loadwarrior(curr_field, xi, yi, team, path2);
				}
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
			else if (!strcmp(cmdname, "noh"))
			{
				search_curr_result = NULL;
			}
			else if (!strcmp(cmdname, "reset"))
			{
				newgame();
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

void keydown(unsigned int key, int x, int y)
{
	int keyused = 0;

	int t1;
	if (key == 27)
	{
		keyused = 1;

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
					keyused = 1;
					uimode = SREPLACE;
					setstatus("-- CHAR REPLACE --");
					break;
				}
				case 'R': // replace mode
				{
					keyused = 1;
					uimode = REPLACE;
					setstatus("-- REPLACE --");
					break;
				}
				case 'v': // visual mode
				{
					keyused = 1;
					xiv = xi;
					yiv = yi;

					uimode = VISUAL;
					setstatus("-- VISUAL --");
					break;
				}


				case 'm': // set mark
				{
					keyused = 1;
					uiprevmode = uimode; // = NORMAL
					uimode = MARK_SET;
					break;
				}


				case ' ': // step
				{
					keyused = 1;
					run = STEP;
					break;
				}

				case 'b':
				{
					if (cam_curr == cams[0])
					{
						cam_curr = cams[1];

						cams[1]->state = ACTIVE;
						cams[0]->state = INACTIVE;
						
						view_resize(cams[0], 0.0, 0.0, rswidth, rsheight/2);
						view_resize(cams[1], 0.0, rsheight/2, rswidth, rsheight/2);
					}
					else
					{
						cam_curr = cams[0];

						cams[0]->state = ACTIVE;
						cams[1]->state = cams[1]->state == ACTIVE ? INACTIVE : DISABLED;
					}
					break;
				}

				case 'B':
				{
					if (cams[1]->state == DISABLED)
					{
						cam_curr = cams[1];

						cams[1]->state = ACTIVE;
						cams[0]->state = INACTIVE;
						
						view_resize(cams[0], 0.0, 0.0, rswidth, rsheight/2);
						view_resize(cams[1], 0.0, rsheight/2, rswidth, rsheight/2);
					}
					else
					{
						cam_curr = cams[0];

						cams[1]->state = DISABLED;
						cams[0]->state = ACTIVE;
						
						view_resize(cams[0], 0.0, 0.0, rswidth, rsheight);
					}
					break;
				}

				case '\r':
				case '\n':
				{
					keyused = 1;

					run = (run==RUN) ? PAUSED : RUN;
					break;
				}
			}
			break;
		}
		case SREPLACE:
		{
			if (key < 256)
			{
				keyused = 1;
				field_get(curr_field, xi, yi)->instr = key;
				uimode = NORMAL;
				clrstatus();
			}
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
				/*
				case '\r':
				case '\n':
				{
					if (cam_curr->follow == ghostid && ghostid != -1)
					{
						fthreads[ghostid].mode = STEP;
					}
					break;
				}
				*/
				default:
				{
					if (key < 256)
					{
						keyused = 1;
						field_get(curr_field, xi, yi)->instr = key;
						break;
					}
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
					keyused = 1;
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

				case 357: // up
				{
					keyused = 1;
					break;
				}
				case 359: // down
				{
					keyused = 1;
					break;
				}
				case 356: // left
				{
					keyused = 1;
					break;
				}
				case 358: // right
				{
					keyused = 1;
					break;
				}

				case 9: // tab
				{
					keyused = 1;
					break;
				}

				case '\r':
				case '\n':
				{
					keyused = 1;

					uimode = NORMAL;
					clrstatus();

					docmd(excmd);

					break;
				}
				default:
				{
					if (key < 256)
					{
						keyused = 1;

						excmd[exlen] = key;
						setstatus_c(exlen++, key);

						if (exlen >= exmax)
						{
							excmd = (char*)realloc(excmd, exmax*=2);
							printf("resize ex buffer to %d bytes\n", exmax);
						}
						excmd[exlen] = 0;
					}
					break;
				}
			}
			break;
		}

		case MARK_SET:
		{
			keyused = 1;

			marks[key].x = xi;
			marks[key].y = yi;

			uimode = uiprevmode; // = NORMAL
			uiprevmode = NORMAL;
			break;
		}

		case MARK_GET:
		{
			keyused = 1;

			int x = marks[key].x;
			int y = marks[key].y;
			if (x>=0 && y>=0 && x<curr_field->width && y<curr_field->height)
			{
				focuscam(cam_curr, x, y);
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

	if (!keyused && (uimode == NORMAL || uimode == VISUAL))
	{
		switch (key)
		{
			case '/':
			case ':':
			{
				keyused = 1;

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
				keyused = 1;

				if (delay>1) delay/=2;
				printf("delay: %d\n", delay);
				break;
			}
			case '-': // slower
			{
				keyused = 1;

				if (delay<2000000) delay*=2;;
				printf("delay: %d\n", delay);
				break;
			}


			case '[': // zoom out
			{
				keyused = 1;

				cam_curr->zoom /= 1.25;
				break;
			}
			case ']': // zoom in
			{
				keyused = 1;

				cam_curr->zoom *= 1.25;
				break;
			}

			case 'n': // next search result
			{
				keyused = 1;

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
					focuscam(cam_curr, search_curr_result->this->x, search_curr_result->this->y);
					//search_curr_result->refs++;
				}
				break;
			}


			case '\'': // go to mark
			{
				keyused = 1;

				uiprevmode = uimode;
				uimode = MARK_GET;
				break;
			}
		}
	}

	if (!keyused)
	{
		switch (key)
		{
			case 9: // tab
			{
				t1 = cam_curr->follow>=0 ? cam_curr->follow : fthreadslen-1;
				do
				{
					cam_curr->follow = (cam_curr->follow+1)%fthreadslen;
				}
				while ((fthreads[cam_curr->follow].alive == DEAD  && t1 != cam_curr->follow) || cam_curr->follow == ghostid);
				if (t1==cam_curr->follow)
				{
					cam_curr->follow = -1;
				}
				else
				{
					focuscam(cam_curr, fthreads[cam_curr->follow].x, fthreads[cam_curr->follow].y);
				}

				printf("follow %d\n", cam_curr->follow);

				break;
			}
			case 25: // un-tab?
			{
				t1 = cam_curr->follow>=0 ? cam_curr->follow : 0;
				do
				{
					cam_curr->follow = (cam_curr->follow-1+fthreadslen)%fthreadslen;
				}
				while ((fthreads[cam_curr->follow].alive == DEAD  && t1 != cam_curr->follow) || cam_curr->follow == ghostid);
				if (t1==cam_curr->follow)
				{
					cam_curr->follow = -1;
				}
				else
				{
					focuscam(cam_curr, fthreads[cam_curr->follow].x, fthreads[cam_curr->follow].y);
				}

				printf("follow %d\n", cam_curr->follow);

				break;
			}
			/*
			case 8:
			case 127:
			{
				field_get(curr_field, xi, yi)->instr=0;
				if (cam_curr->follow == ghostid && ghostid != -1)
				{
					fthreads[ghostid].delta *= -1;
					fthreads[ghostid].mode = STEP;
				}
				break;
			}
			*/

			case 1+256:
			{
				if (delay>1) delay/=2;
				printf("delay: %d\n", delay);
				break;
			}
			case 2+256:
			{
				if (delay<2000000) delay*=2;;
				printf("delay: %d\n", delay);
				break;
			}
			case 3+256:
			{
				cam_curr->zoom /= 1.25;
				break;
			}
			case 4+256:
			{
				cam_curr->zoom *= 1.25;
				break;
			}
			case 8+256:
			{
				run = (run==RUN) ? PAUSED : RUN;
				break;
			}
		}
		//pthread_mutex_lock(&fthreadsmutex);
		if (cam_curr->follow >= 0)
		{
			fthread* cfthread = &fthreads[cam_curr->follow];
			if (cfthread->alive != DEAD)
			{
				switch (key)
				{
					case 5+256:
					{
						cfthread->mode = STEP;
						break;
					}
					case 6+256:
					{
						printf("stack trace for thread %d:\n", cam_curr->follow);
						int i;
						for (i=0; i<cfthread->stackidx; i++)
						{
							printf("%d: %d\n", i, cfthread->stack[i]);
						}
						break;
					}
					case 7+256:
					{
						cam_curr->follow = -1;
						break;
					}
					case 9+256:
					{
						cfthread->mode = (cfthread->mode==RUN) ? PAUSED : RUN;
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
				case 5+256:
				{
					run = STEP;
					break;
				}
				case 7+256:
				{
					cam_curr->follow = getfthread(xi, yi);

					if (cam_curr->follow == -1)
					{
						fthreads[ghostid].x = xi;
						fthreads[ghostid].y = yi;
						focusthread(cam_curr, &fthreads[ghostid]);
					}
					break;
				}
				case 9+256:
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

	uilastcmd = key;

	if (!keyused)
	{
		keys[key] = 1;
	}
}

void kb1(unsigned char key, int x, int y)
{
	modkeys = glutGetModifiers();
	keydown(key, x, y);
}

void kb1u(unsigned char key, int x, int y)
{
	modkeys = glutGetModifiers();
	keys[key] = 0;
}

void kb2(int key, int x, int y)
{
	modkeys = glutGetModifiers();
	keydown(key+256, x, y);
}

void kb2u(int key, int x, int y)
{
	modkeys = glutGetModifiers();
	keys[key+256] = 0;
}

void idle(void)
{
	if (keys[357] || ((uimode == NORMAL || uimode == VISUAL) && (keys['k'] || keys['w'] || keys['W'])))
	{
		cam_curr->y+=(3.0+6.0*(modkeys&GLUT_ACTIVE_ALT))/cam_curr->zoom_curr;
		if (cam_curr->follow != ghostid)
		{
			cam_curr->follow = -1;
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
		cam_curr->x-=(3.0+6.0*(modkeys&GLUT_ACTIVE_ALT))/cam_curr->zoom_curr;
		if (cam_curr->follow != ghostid) cam_curr->follow = -1;
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
		cam_curr->y-=(3.0+6.0*(modkeys&GLUT_ACTIVE_ALT))/cam_curr->zoom_curr;
		if (cam_curr->follow != ghostid) cam_curr->follow = -1;
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
		cam_curr->x+=(3.0+6.0*(modkeys&GLUT_ACTIVE_ALT))/cam_curr->zoom_curr;
		if (cam_curr->follow != ghostid) cam_curr->follow = -1;
		else
		{
			pthread_mutex_lock(&fthreadsmutex);
			fthreads[ghostid].dx = 1;
			fthreads[ghostid].dy = 0;
			fthreads[ghostid].mode = STEP;
			pthread_mutex_unlock(&fthreadsmutex);
		}
	}

	xi = (cam_curr->x+cam_curr->zswidth/2)/charwidth;
	yi = (cam_curr->y+cam_curr->zsheight/2)/charheight;

	glutPostRedisplay();
}

int main(int argc, char** argv)
{
	srand(time(NULL));

	exmax = 128;
	excmd = (char*)malloc(exmax*sizeof(char));
	exlen = 0;

	gl_init();

	statuslinelen = rswidth / charwidth;
	statusline = (cell*)malloc(statuslinelen*sizeof(cell));

	newgame();

	uimode = uiprevmode = NORMAL;

	for (int i=0; i<N_KEYS; i++)
	{
		marks[i].x = -1;
		marks[i].y = -1;
	}

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
