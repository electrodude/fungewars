#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "fungewars.h"

#include "graphics.h"

int font;
int fontwidth;
int fontheight;
int charwidth;
int charheight;

view* cam_curr;

view** cams;
int ncams;
int maxcams;

int rswidth = 1280;
int rsheight = 752;

int frame=0;
int timenow=0;
int timelast=0;
int timebase=0;

GLFWwindow* gr_window;


#define B0 0.0
#define B1 0.5
#define F0 0.0
#define F1 1.0

color color_fg =
	{F1,  F1,  F1,  1.0};

color colors[18]=
{
	{B1,  B0,  B0,  1.0},
	{F1,  F0,  F0,  1.0},

	{B0,  B1,  B0,  1.0},
	{F0,  F1,  F0,  1.0},

	{B0,  B0,  B1,  1.0},
	{F0,  F0,  F1,  1.0},

	{B0,  B1,  B1,  1.0},
	{F0,  F1,  F1,  1.0},

	{B1,  B0,  B1,  1.0},
	{F1,  F0,  F1,  1.0},

	{B1,  B1,  B0,  1.0},
	{F1,  F1,  F0,  1.0},

	{0.7, 0.7, 0.7, 1.0},
	{1.0, 1.0, 1.0, 1.0},

	{0.4, 0.4, 0.4, 1.0},
	{0.4, 0.4, 0.4, 1.0},

};

static void gr_error_callback(int error, const char* description)
{
	fputs(description, stderr);
}


void gr_putc(float x, float y, int c)
{
	c&=255;
	float charx = ((float)(c%16))/16;
	float chary = ((float)(c/16))/16;
	glBegin(GL_QUADS);
	glTexCoord2f(charx,        1.0-chary-0.0625);   glVertex2f(x,           y);
	glTexCoord2f(charx+0.0625, 1.0-chary-0.0625);   glVertex2f(x+charwidth, y);
	glTexCoord2f(charx+0.0625, 1.0-chary);          glVertex2f(x+charwidth, y+charheight);
	glTexCoord2f(charx,        1.0-chary);          glVertex2f(x,           y+charheight);
	glEnd();
}

void gr_putcell(float x, float y, cell* c)
{
	if (c->bg != NULL)
	{
		glColor4fv((float*)c->bg);
		gr_putc(x, y, 0xDB);
	}

	if (c->instr && c->fg != NULL)
	{
		glColor4fv((float*)c->fg);
		gr_putc(x, y, c->instr);
	}
}


view* view_reset(view* this, field* f, float sx, float sy, float swidth, float sheight, float x, float y, float zoom)
{
	int isnew = this == NULL;

	if (isnew)
	{
		this = malloc(sizeof(view));
	}

	this->field = f;

	this->follow = -1;

	this->sx = sx;
	this->sy = sy;
	this->swidth = swidth;
	this->sheight = sheight;

	this->x_curr = this->x = x;
	this->y_curr = this->y = y;
	this->zoom_curr = this->zoom = zoom;

	this->zswidth = this->swidth / this->zoom_curr;
	this->zsheight = this->sheight / this->zoom_curr;

	this->state = ACTIVE;

	if (isnew)
	{
		if (ncams == maxcams)
		{
			int oldmaxcams = maxcams;
			maxcams *= 2;
			cams = realloc(cams, maxcams*sizeof(view*));
			for (int i=oldmaxcams; i<maxcams; i++)
			{
				cams[i] = NULL;
			}
		}

		this->id = ncams;
		cams[ncams++] = this;

		printf("New view: id = %d\n", this->id);

	}

	return this;
}

view* view_new(field* f, float sx, float sy, float swidth, float sheight, float x, float y, float zoom)
{
	return view_reset(NULL, f, sx, sy, swidth, sheight, x, y, zoom);
}

void view_kill(view* this)
{
	cams[this->id] = NULL;
	free(this);
}

void view_resize(view* this, int newx, int newy, int newwidth, int newheight)
{
	float cxc = this->x + this->zswidth/2;
	float cyc = this->y + this->zsheight/2;
	float ccxc = this->x_curr + this->zswidth/2;
	float ccyc = this->y_curr + this->zsheight/2;

	this->sx = newx;
	this->sy = newy;
	this->swidth = newwidth;
	this->sheight = newheight;

	this->zswidth = this->swidth / this->zoom_curr;
	this->zsheight = this->sheight / this->zoom_curr;

	this->x = cxc - this->zswidth/2;
	this->y = cyc - this->zsheight/2;
	this->x_curr = ccxc - this->zswidth/2;
	this->y_curr = ccyc - this->zsheight/2;
}

void view_render(view* this)
{
	if (this->follow != -1)
	{
		fthread* cfthread = &fthreads[this->follow];
		this->x = cfthread->x*charwidth - this->zswidth/2;
		this->y = cfthread->y*charheight - this->zsheight/2;
	}

	if (this->zoom_curr < this->zoom) this->zoom_curr *= sqrt(this->zoom / this->zoom_curr);
	if (this->zoom_curr > this->zoom) this->zoom_curr /= sqrt(this->zoom_curr / this->zoom);

	float cxc = this->x + this->zswidth/2;
	float cyc = this->y + this->zsheight/2;
	float ccxc = this->x_curr + this->zswidth/2;
	float ccyc = this->y_curr + this->zsheight/2;

	this->zswidth = this->swidth / this->zoom_curr;
	this->zsheight = this->sheight / this->zoom_curr;

	this->x = cxc - this->zswidth/2;
	this->y = cyc - this->zsheight/2;
	this->x_curr = ccxc - this->zswidth/2;
	this->y_curr = ccyc - this->zsheight/2;


	if (this->x_curr<this->x) this->x_curr+=sqrt(this->x-this->x_curr);
	if (this->x_curr>this->x) this->x_curr-=sqrt(this->x_curr-this->x);

	if (this->y_curr<this->y) this->y_curr+=sqrt(this->y-this->y_curr);
	if (this->y_curr>this->y) this->y_curr-=sqrt(this->y_curr-this->y);


	float cx = this->x;
	float cy = this->y;
	float czoom = this->zoom;

	float ccx = this->x_curr;
	float ccy = this->y_curr;
	float cczoom = this->zoom_curr;

	field* f = this->field;

	glPushMatrix();

	glEnable(GL_SCISSOR_TEST);

	glBegin(GL_LINE_LOOP);
	glColor4f(1.0, 0.0, 0.0, 1.0);

	glVertex2f(this->sx, this->sy);
	glVertex2f(this->sx+this->swidth, this->sy);
	glVertex2f(this->sx+this->swidth, this->sy+this->sheight);
	glVertex2f(this->sx, this->sy+this->sheight);

	glEnd();

	glScissor(this->sx, this->sy, this->swidth, this->sheight);

	glTranslatef(this->sx, this->sy, 0);

	glScalef(this->zoom_curr, this->zoom_curr, 1);

	// board contents
	int x;
	int y;

	glEnable(GL_TEXTURE_2D);
	for (y=max(ccy/charheight,0); y<min((this->zsheight+ccy)/charheight+1,f->height); y++)
	{
		for (x=max(ccx/charwidth,0); x<min((this->zswidth+ccx)/charwidth+1,f->width); x++)
		{
			gr_putcell(x*charwidth - ccx, y*charheight - ccy, field_get(f, x, y));
		}
	}
	glDisable(GL_TEXTURE_2D);

	// current search result
	if (search_curr_result != NULL)
	{
		search_curr_result->refs++; // make sure this result doesn't get GC'd while we're drawing it

		search_cell* prev = search_curr_result->this;
		search_cell* curr = prev->next;

		glColor4f(0.0, 1.0, 0.0, 1.0);
		glLineWidth(3.0);
		glBegin(GL_LINES);
		while (curr != NULL)
		{
			//glColor4f(0.0, 1.0, 0.0, 1.0);
			glVertex2f(curr->x*charwidth - ccx + charwidth/2, curr->y*charheight - ccy + charheight/2);

			int d = abs(prev->dx) + abs(prev->dy) - 1;
			if (prev != NULL && d>0)
			{
				float dx = prev->dy>0 ? 0.5 : (prev->dy<0 ? -0.5 : 0);
				float dy = prev->dx>0 ? -0.5 : (prev->dx<0 ? 0.5: 0);
				float midx = (curr->x + dx)*charwidth - ccx + charwidth/2;
				float midy = (curr->y + dy)*charheight - ccy + charheight/2;

				//glColor4f(1.0, 0.0, 0.0, 1.0);
				glVertex2f(midx, midy);
				glVertex2f(midx, midy);
			}

			//glColor4f(0.0, 0.0, 1.0, 1.0);
			glVertex2f((curr->x+prev->dx)*charwidth - ccx + charwidth/2, (curr->y+prev->dy)*charheight - ccy + charheight/2);

			prev = curr;
			curr = curr->next;
		}

		glEnd();

		search_result_kill(search_curr_result);
	}

	if (1 || this->state == ACTIVE)
	{
		int xi = (cx+this->zswidth/2)/charwidth;
		int yi = (cy+this->zsheight/2)/charheight;

		// crosshairs
		float chx = xi*charwidth  + charwidth/2  - ccx;
		float chy = yi*charheight + charheight/2 - ccy;

		glLineWidth(1.0);
		glColor4f(1.0, 1.0, 1.0, this->state == ACTIVE ? 1.0 : 0.5);
		glBegin(GL_LINES);
		glVertex2f(chx, 0.0);
		glVertex2f(chx, chy-charheight/2);
		glVertex2f(chx, chy+charheight/2);
		glVertex2f(chx, this->zsheight);

		glVertex2f(0.0, chy);
		glVertex2f(chx-charwidth/2, chy);
		glVertex2f(chx+charwidth/2, chy);
		glVertex2f(this->zswidth, chy);
		glEnd();

		// visual selection box
		if (uimode == VISUAL)
		{
			float chx2 = chx - charwidth/2;
			float chy2 = chy - charheight/2;

			float chxv = xiv*charwidth  - ccx;
			float chyv = yiv*charheight - ccy;

			if (xi > xiv)
			{
				chx2 += charwidth;
			}
			else
			{
				chxv += charwidth;
			}

			if (yi > yiv)
			{
				chy2 += charheight;
			}
			else
			{
				chyv += charheight;
			}

			glColor4f(1.0, 0.0, 0.0, this->state == ACTIVE ? 1.0 : 0.5);
			glBegin(GL_LINE_LOOP);
			glVertex2f(chx2, chy2);
			glVertex2f(chxv, chy2);
			glVertex2f(chxv, chyv);
			glVertex2f(chx2, chyv);
			glEnd();
		}
	}
	// board border
	glColor4f(0.0, 1.0, 1.0, this->state == ACTIVE ? 1.0 : 0.5);
	glBegin(GL_LINE_LOOP);
	glVertex2f(-ccx, -ccy);
	glVertex2f(-ccx, f->height*charheight + 1 - ccy);
	glVertex2f(f->width*charwidth + 1 - ccx, f->height*charheight + 1 - ccy);
	glVertex2f(f->width*charwidth + 1 - ccx, -ccy);
	glEnd();

	glDisable(GL_SCISSOR_TEST);

	glPopMatrix();
}

void gr_loop(void)
{
	while (!glfwWindowShouldClose(gr_window))
	{
		glfwGetFramebufferSize(gr_window, &rswidth, &rsheight);

		if (cams[1]->state != DISABLED)
		{
			view_resize(cams[0], 0.0, 0.0,        rswidth, rsheight/2);
			view_resize(cams[1], 0.0, rsheight/2, rswidth, rsheight/2);
		}
		else
		{
			view_resize(cams[0], 0.0, 0.0,        rswidth, rsheight);
		}

		glViewport(0, 0, rswidth, rsheight);

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, rswidth, 0, rsheight, -100000.0, 100000.0);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		glOrtho(0.0, 1.0, 0.0, 1.0, -1.0, 1.0);

		glClear(GL_COLOR_BUFFER_BIT);
		glLoadIdentity();

		gluLookAt(0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);
		glTranslatef(0.0, -1, -1);

		for (int i=0; i<maxcams; i++)
		{
			if (cams[i] != NULL && cams[i]->state != DISABLED)
			{
				view_render(cams[i]);
			}
		}

		// status line
		glEnable(GL_TEXTURE_2D);
		for (int x=0; x<statuslinelen; x++)
		{
			gr_putcell(x*charwidth, 1.5, &statusline[x]);
		}
		glDisable(GL_TEXTURE_2D);


		frame++;
		timelast=glfwGetTime();
		if (timelast - timebase > 1000)
		{
			//printf("FPS:%4.2f\n",frame*1000.0/(timelast-timebase));
			timebase = timelast;
			frame = 0;
		}

		glfwSwapBuffers(gr_window);

		gr_idle();

		glfwPollEvents();
	}
}

void gr_init(void)
{
	glfwSetErrorCallback(gr_error_callback);

	if (!glfwInit())
	{
		puts("glfwInit() failed!");
		glfwTerminate();
		exit(0);
	}

	gr_window = glfwCreateWindow(rswidth, rsheight, "Fungewars", NULL, NULL);

	if (!gr_window)
	{
		puts("glfwCreateWindow() failed!");
		glfwTerminate();
		exit(0);
	}

	glfwMakeContextCurrent(gr_window);
	
	glfwGetWindowSize(gr_window, &rswidth, &rsheight);

	//glEnable(GL_TEXTURE_2D);

	glClearColor(0.0, 0.0, 0.0, 1.0);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	glLineWidth(1);

	glfwSetKeyCallback(gr_window, gr_key_callback);
	glfwSetCharModsCallback(gr_window, gr_charmods_callback);

	font = png_texture_load("curses_640x300_2.png", &fontwidth, &fontheight);
	printf("font tex id: %d\n", font);

	charwidth = fontwidth/16;
	charheight = fontheight/16;

	ncams = 0;
	maxcams = 4;
	cams = malloc(maxcams*sizeof(view*));
	for (int i=0; i<maxcams; i++)
	{
		cams[i] = NULL;
	}
}
