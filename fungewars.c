/*
 *  fungewars.c
 *  fungewars
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

#include <time.h>

#include <png.h>

#include <math.h>
#include <string.h>

// Linux - should be expression and not just 0 or 1
#if 1
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#endif

// OSX - should be expression and not just 0 or 1
#if 0
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include <GLUT/glut.h>
#endif
 
#include "fungewars.h"

#define max(a,b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a > _b ? _a : _b; })
#define min(a,b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a < _b ? _a : _b; })

//#define wrap(a,b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); __typeof__ (a) c = _a % _b; (c<0)?(_b+c):(c) })

inline int wrap(int x, int m)
{
	int c = x%m;
	return (c<0)?(c+m):c;
}

#define CWIDTH 256
#define CHEIGHT 256

#define INITIAL_FTHREADS 16

#define INITIAL_STACKSIZE 1024

#define NUM_THREADS 1	//don't change this, better yet, remove it

#define NFT_NO_STACK	0x0
#define NFT_GHOST		0x1

#define debugout 1

int font;
int fontwidth;
int fontheight;
int charwidth;
int charheight;

GLuint png_texture_load(const char * file_name, int * width, int * height)
{
	png_byte header[8];
	
	FILE *fp = fopen(file_name, "rb");
	if (fp == 0)
	{
		perror(file_name);
		return 0;
	}
	
	// read the header
	fread(header, 1, 8, fp);
	
	if (png_sig_cmp(header, 0, 8))
	{
		fprintf(stderr, "error: %s is not a PNG.\n", file_name);
		fclose(fp);
		return 0;
	}
	
	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr)
	{
		fprintf(stderr, "error: png_create_read_struct returned 0.\n");
		fclose(fp);
		return 0;
	}
	
	// create png info struct
	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
	{
		fprintf(stderr, "error: png_create_info_struct returned 0.\n");
		png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
		fclose(fp);
		return 0;
	}
	
	// create png info struct
	png_infop end_info = png_create_info_struct(png_ptr);
	if (!end_info)
	{
		fprintf(stderr, "error: png_create_info_struct returned 0.\n");
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp) NULL);
		fclose(fp);
		return 0;
	}
	
	// the code in this if statement gets called if libpng encounters an error
	if (setjmp(png_jmpbuf(png_ptr))) {
		fprintf(stderr, "error from libpng\n");
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		fclose(fp);
		return 0;
	}
	
	// init png reading
	png_init_io(png_ptr, fp);
	
	// let libpng know you already read the first 8 bytes
	png_set_sig_bytes(png_ptr, 8);
	
	// read all the info up to the image data
	png_read_info(png_ptr, info_ptr);
	
	// variables to pass to get info
	int bit_depth, color_type;
	png_uint_32 temp_width, temp_height;
	
	// get info about png
	png_get_IHDR(png_ptr, info_ptr, &temp_width, &temp_height, &bit_depth, &color_type,
			   NULL, NULL, NULL);
	
	if (width){ *width = temp_width; }
	if (height){ *height = temp_height; }
	
	//printf("%s: %lux%lu %d\n", file_name, temp_width, temp_height, color_type);
	
	if (bit_depth != 8)
	{
		fprintf(stderr, "%s: Unsupported bit depth %d. Must be 8.\n", file_name, bit_depth);
		return 0;
	}
	
	GLint format;
	switch(color_type)
	{
		case PNG_COLOR_TYPE_RGB:
			format = GL_RGB;
			break;
		case PNG_COLOR_TYPE_RGB_ALPHA:
			format = GL_RGBA;
			break;
		default:
			fprintf(stderr, "%s: Unknown libpng color type %d.\n", file_name, color_type);
			return 0;
	}
	
	// Update the png info struct.
	png_read_update_info(png_ptr, info_ptr);
	
	// Row size in bytes.
	int rowbytes = png_get_rowbytes(png_ptr, info_ptr);
	
	// glTexImage2d requires rows to be 4-byte aligned
	rowbytes += 3 - ((rowbytes-1) % 4);
	
	// Allocate the image_data as a big MemBlock, to be given to opengl
	png_byte * image_data = (png_byte *)malloc(rowbytes * temp_height * sizeof(png_byte)+15);
	if (image_data == NULL)
	{
		fprintf(stderr, "error: could not allocate memory for PNG image data\n");
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		fclose(fp);
		return 0;
	}
	
	// row_pointers is for pointing to image_data for reading the png with libpng
	png_byte ** row_pointers = (png_byte **)malloc(temp_height * sizeof(png_byte *));
	if (row_pointers == NULL)
	{
		fprintf(stderr, "error: could not allocate memory for PNG row pointers\n");
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		free(image_data);
		fclose(fp);
		return 0;
	}
	
	// set the individual row_pointers to point at the correct offsets of image_data
	unsigned int i;
	for (i = 0; i < temp_height; i++)
	{
		row_pointers[temp_height - 1 - i] = image_data + i * rowbytes;
	}
	
	// read the png into image_data through row_pointers
	png_read_image(png_ptr, row_pointers);
	
	// Generate the OpenGL texture object
	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, format, temp_width, temp_height, 0, format, GL_UNSIGNED_BYTE, image_data);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	
	// clean up
	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
	free(image_data);
	free(row_pointers);
	fclose(fp);
	return texture;
}

float cx=0;
float cy=0;
float czoom=1.0;
int cthread = -1;

float ccx=0.0;
float ccy=0.0;

float ccdx=0.0;
float ccdy=0.0;

int lastcx=0.0;
int lastcy=0.0;
int lastupdate=1;

float cdx=0.0;
float cdy=0.0;

unsigned int ghostid;

void glputc(int x, int y, int c)
{
	c&=255;
	float charx = ((float)(c%16))/16;
	float chary = ((float)(c/16))/16;
	glEnable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);
	glTexCoord2f(charx,        1.0-chary-0.0625);	glVertex2i(x,    y);
	glTexCoord2f(charx+0.0625, 1.0-chary-0.0625);	glVertex2i(x+charwidth, y);
	glTexCoord2f(charx+0.0625, 1.0-chary);			glVertex2i(x+charwidth, y+charheight);
	glTexCoord2f(charx,        1.0-chary);			glVertex2i(x,    y+charheight);
	glEnd();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int keys[512];
int modkeys;

fthread* fthreads;
unsigned int fthreadslen = 0;

#define BOF 0.0
#define BON 0.7
#define FOF 0.3
#define FON 1.0

float colors[18][3]=
{
	{BOF, BOF, BOF},
	{1.0, 1.0, 1.0},
	
	{BON, BOF, BOF},
	{FON, FOF, FOF},
	
	{BOF, BON, BOF},
	{FOF, FON, FOF},
	
	{BOF, BOF, BON},
	{FOF, FOF, FON},
	
	{BOF, BON, BON},
	{FOF, FON, FON},
	
	{BON, BOF, BON},
	{FON, FOF, FON},
	
	{BON, BON, BOF},
	{FON, FON, FOF},
	
	{0.7, 0.7, 0.7},
	{1.0, 1.0, 1.0},
	
	{0.4, 0.4, 0.4},
	{0.4, 0.4, 0.4},
};

int rswidth = 1280;
int rsheight = 752;

int swidth = 1280;
int sheight = 752;

int delay = 500000;
fmode run = PAUSED;

cell field[CHEIGHT][CWIDTH];

pthread_t threads[NUM_THREADS];
pthread_mutex_t fthreadsmutex;

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
	fthread *cfthread = fthreads[id] = malloc(sizeof(fthread));
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
		printf("offset=%X, (parent-fthreads)/sizeof(fthread)=%X\n", offset, ((*parent)-fthreads)/sizeof(fthread));
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

fthread killfthread(int id)
{
	free(fthreads[id].stack);
	fthreads[id].alive = DEAD;
	if (id == cthread) cthread = -1;
	
#if debugout >= 2
	printf("kill thread, id=%d\n", id);
#endif
}

int push(fthread *cfthread, int x)
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

int pop(fthread *cfthread)
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

int execinstr(fthread *cfthread, cell *ccell)
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
		cell *ccell2;
		coord *culprit;
		switch (ccell->instr&127)
		{
			case '!':
				push(cfthread, !pop(cfthread));
				break;
			case '"':
				cfthread->stringmode = !cfthread->stringmode;
				break;
			case '#':
				cfthread->delta++;
				break;
			case '$':
				pop(cfthread);
				break;
			case '%':
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
			case '\'':
				cfthread->delta++;
				ccell2 = &field[wrap(cfthread->y + cfthread->dy,CHEIGHT)][wrap(cfthread->x + cfthread->dx, CWIDTH)];
				push(cfthread, ccell2->instr);
				ccell2->bg = cfthread->team*2;
				break;
			case '*':
				push(cfthread, pop(cfthread) * pop(cfthread));
				break;
			case '+':
				push(cfthread, pop(cfthread) + pop(cfthread));
				break;
			case '-':
				t1 = pop(cfthread);
				push(cfthread, pop(cfthread)-t1);
				break;
			case '/':
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
				push(cfthread, ccell->instr - '0');
				break;
			case ':':
				push(cfthread, push(cfthread, pop(cfthread)));	//please optimise me!
				break;
			case ';':
				cfthread->jmpmode = !cfthread->jmpmode;
				break;
			case '<':
				cfthread->dx = -1;
				cfthread->dy = 0;
				break;
			case '>':
				cfthread->dx = 1;
				cfthread->dy = 0;
				break;
			case '?':
				switch (rand()&3)
				{
					case 0:
						cfthread->dx = 1;
						cfthread->dy = 0;
						break;
					case 1:
						cfthread->dx = -1;
						cfthread->dy = 0;
						break;
					case 2:
						cfthread->dx = 0;
						cfthread->dy = 1;
						break;
					case 3:
						cfthread->dx = 0;
						cfthread->dy = -1;
						break;
				}
				break;
			case '@':
			case 'q':
				if (cfthread->i == ghostid) break;
				killfthread(cfthread->i);
				cfthread->x = wrap(cfthread->x - cfthread->dx, CWIDTH);
				cfthread->y = wrap(cfthread->y - cfthread->dy, CHEIGHT);
				ccell->fg = 1;
				ccell->bg = cfthread->team*2;
				break;
			case 'B':		//reflect if pop()>0
				if (pop(cfthread)>0)
				{
					cfthread->dx = -cfthread->dx;
					cfthread->dy = -cfthread->dy;
				}
				break;
			case 'H':		//_ but backwards
				cfthread->dx = pop(cfthread)>0 ? 1 : -1;
				cfthread->dy = 0;
				break;
			case 'I':		//| but backwards
				cfthread->dx = 0;
				cfthread->dy = pop(cfthread)>0 ? -1 : 1;
				break;
			case 'O':		//forth OVER = 1 PICK
				push(cfthread, 1);
			case 'P':		//forth PICK
				t1 = -pop(cfthread)-1;
				t1 += cfthread->stackidx;
				if (t1<0) t1=0;
				if (t1>cfthread->stackidx) t1 = cfthread->stackidx;
				push(cfthread, cfthread->stack[t1]);
				break;
			/*
			case 'R':		//forth ROLL
				
				break;
			case 'T':		//forth TUCK
				
				break;
			//*/
			case 'W':		//w but backwards
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
			case '[':
				t1 = cfthread->dx;
				cfthread->dx = -cfthread->dy;
				cfthread->dy = t1;
				break;
			case '\\':
				t1 = pop(cfthread);
				t2 = pop(cfthread);
				push(cfthread, t1);
				push(cfthread, t2);
				break;
			case ']':
				t1 = cfthread->dx;
				cfthread->dx = cfthread->dy;
				cfthread->dy = -t1;
				break;
			case '^':
				cfthread->dx = 0;
				cfthread->dy = 1;
				break;
			case '_':
				cfthread->dx = pop(cfthread)>0 ? -1 : 1;
				cfthread->dy = 0;
				break;
			case '`':
				push(cfthread, pop(cfthread) < pop(cfthread));
				break;
			case 'a':
			case 'b':
			case 'c':
			case 'd':
			case 'e':
			case 'f':
				push(cfthread, ccell->instr - 'a' + 10); 
				break;
			case 'g':
				t1 = pop(cfthread);
				t2 = pop(cfthread);
				destx = wrap(t2*cfthread->dx - t1*cfthread->dy + cfthread->x, CWIDTH);
				desty = wrap(t1*cfthread->dx + t2*cfthread->dy + cfthread->y, CHEIGHT);
				ccell2 = &field[desty][destx];
				push(cfthread, ccell2->instr);
				ccell2->bg = cfthread->team*2;
				break;
			case 'j':
				cfthread->delta += pop(cfthread)-1;
				break;
			case 'k':
				//focusthread(cfthread);
				cfthread->repeats = pop(cfthread);
				cfthread->delta--;	//decrement delta because we're doing one here
				cfthread->x = wrap(cfthread->x + cfthread->dx, CWIDTH);
				cfthread->y = wrap(cfthread->y + cfthread->dy, CHEIGHT);
				//delta += execinstr(cfthread, ccell)-1;
				break;
			case 'n':
				cfthread->stackidx = 0;
				break;
			case 'p':
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
				//q is with @
				//r is below t
			case 's':
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
			case 't':		//clone
				if (cfthread->i == ghostid) break;
				dupfthread(&cfthread);

			case 'X':		//running an X is the same as an r; X's main purpose is to MemBlock p, g, x, j, ", and ;
			case 'r':		//reflect
				cfthread->dx = -cfthread->dx;
				cfthread->dy = -cfthread->dy;
				break;
			case 'v':		//go down
				cfthread->dx = 0;
				cfthread->dy = -1;
				break;
			case 'w':
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
			case 'x':
				cfthread->dy = pop(cfthread);
				cfthread->dx = pop(cfthread);
				break;
			case '|':
				cfthread->dx = 0;
				cfthread->dy = pop(cfthread)>0 ? 1 : -1;
				break;
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

void *interpreter(void *threadid)
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
	
	for (i=0; i<7; i++)
	{
		//char *fname = asprintf("warriors/%d.b98", i);
		//fp = fopen(fname, "r");
		//free(fname);
		char *fname;
		asprintf(&fname, "warriors/%d.b98", i);
		FILE *fp = fopen(fname, "r");
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
					y2--;
					if (y2<0) y2+=CHEIGHT;
					x2=x;
					break;
				default:
					//;cell ccell = ;
					field[y2][x2].instr = cchar;
					field[y2][x2++].bg = (i+1)*2;
					x2%=CWIDTH;
					break;
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
			fthread *cfthread = &fthreads[i];
			if (cfthread->alive != DEAD && (((run2 != PAUSED) && cfthread->mode == RUN) || cfthread->mode == STEP))
			{
				cell *ccell = &field[cfthread->y][cfthread->x];
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

int frame=0;
int timenow=0;
int timelast=0;
int timebase=0;

void focusthread(fthread *cfthread)
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

void mulzoom(float factor)
{
	int cxc=((int)(cx+swidth/2))/charwidth;
	int cyc=((int)(cy+sheight/2))/charheight;
	int ccxc=((int)(ccx+swidth/2))/charwidth;
	int ccyc=((int)(ccy+sheight/2))/charheight;
	
	czoom*=factor;
	swidth=rswidth/czoom;
	sheight=rsheight/czoom;
	
	cx = cxc*charwidth-swidth/2;
	cy = cyc*charheight-sheight/2;
	ccx = ccxc*charwidth-swidth/2;
	ccy = ccyc*charheight-sheight/2;
}

void display(void)
{	
	swidth=rswidth/czoom;
	sheight=rsheight/czoom;
	
	if (cthread >= 0)
	{
		fthread cfthread = fthreads[cthread];
		cx = cfthread.x*charwidth - swidth/2;
		ccdx += cfthread.dx*cfthread.delta*charwidth;
		cy = cfthread.y*charheight - sheight/2;
		ccdy += cfthread.dy*cfthread.delta*charheight;
	}
	
	if (abs(ccx-cx)>swidth || abs(ccy-cy)>sheight)
	{
		ccx = lastcx = cx;
		ccy = lastcy = cy;
		lastupdate = 0;
	}
	else if (ccdx || ccdy)
	{
		cdx = ccdx/lastupdate;
		ccdx = 0.0;
		cdy = ccdy/lastupdate;
		ccdy = 0.0;
		lastupdate = 0;
		lastcx = cx;
		lastcy = cy;
	}
	else if (abs(cx-lastcx)>=4*charwidth || abs(cy-lastcy)>=4*charheight)
	{
		cdx = (cx-lastcx)/lastupdate;
		cdy = (cy-lastcy)/lastupdate;
		lastupdate = 0;
		lastcx = cx;
		lastcy = cy;
	}

	if (0 && cthread >= 0)
	{
		ccx += cdx + ((abs(cx-ccx)>2*charwidth ) ? (cx-ccx)/10 : 0);
		ccy += cdy + ((abs(cy-ccy)>2*charheight) ? (cy-ccy)/10 : 0);
	}
	else
	{
		if (ccx<cx) ccx+=sqrt(cx-ccx);
		if (ccx>cx) ccx-=sqrt(ccx-cx);
		if (ccy<cy) ccy+=sqrt(cy-ccy);
		if (ccy>cy) ccy-=sqrt(ccy-cy);
	}

	
	/*
	if (ccx != cx || ccy != cy)
	{
		float r = sqrt((ccx-cx)*(ccx-cx)+(ccy-cy)*(ccy-cy));
		float cdx = (cx-ccx)/r;
		float cdy = (cy-ccy)/r;
		ccx+=cdx;
		ccy+=cdy;
	}
	//*/
	
	//printf("cam: (%f, %f)\n", ccx, ccy);
	/*
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	//*/
	glOrtho(0.0, 1.0, 0.0, 1.0, -1.0, 1.0);
	
	glClear(GL_COLOR_BUFFER_BIT);
	glLoadIdentity();
	
	gluLookAt(0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);
	glTranslatef(0.0, -1, -1);

	glScalef(czoom, czoom, 1);
	
	//glBindTexture(GL_TEXTURE_2D, font);
	
	int x;
	int y;
	
	glEnable(GL_TEXTURE_2D);
	for (y=max(ccy/charheight,0); y<min((sheight+ccy)/charheight+1,CHEIGHT); y++)
	{
		for (x=max(ccx/charwidth,0); x<min((swidth+ccx)/charwidth+1,CWIDTH); x++)
		{
			cell current = field[y][x];
			//timenow=glutGet(GLUT_ELAPSED_TIME);
			glColor4f(colors[current.bg][0], colors[current.bg][1], colors[current.bg][2], 1.0);
			glputc(x*charwidth-ccx, y*charheight-ccy, 0xDB);

			glColor4f(colors[current.fg][0], colors[current.fg][1], colors[current.fg][2], 0.5);
			glputc(x*charwidth-ccx, y*charheight-ccy, current.instr);
			//glDisable(GL_TEXTURE_2D);
		}
		//glPopMatrix();
	}
	glDisable(GL_TEXTURE_2D);
	
	float chx = (((int)(cx+swidth/2))/charwidth)*charwidth +charwidth/2-ccx;
	float chy = (((int)(cy+sheight/2))/charheight)*charheight +charheight/2-ccy;
	
	glColor4f(1.0, 1.0, 1.0, 0.5);
	glBegin(GL_LINES);
	glVertex2f(chx, 0);
	glVertex2f(chx, chy-charheight/2);
	glVertex2f(chx, chy+charheight/2);
	glVertex2f(chx, sheight);
	
	glVertex2f(0, chy);
	glVertex2f(chx-charwidth/2-1, chy);
	glVertex2f(chx+charwidth/2-1, chy);
	glVertex2f(swidth, chy);
	glEnd();
	
	
	//cx+=3;
	//cy+=5;
	//cx-=cx>CWIDTH*12;
	//cy-=cy>CHEIGHT*16;
	/*
	glBegin(GL_LINES);
	glVertex3f(y, x, 0);
	glEnd();
	*/
	
	frame++;
	timelast=glutGet(GLUT_ELAPSED_TIME);
	if (timelast - timebase > 1000)
	{
		//printf("FPS:%4.2f\n",frame*1000.0/(timelast-timebase));
		timebase = timelast;
		frame = 0;
	}
	
	lastupdate++;
	
	glutSwapBuffers();
}

/*
void reshape(int width, int height)
{
	glViewport(0, 0, width, height);
}
*/
void reshape(int w, int h)
{
	swidth = (rswidth=w)/czoom;
	sheight = (rsheight=h)/czoom;
	GLdouble size;
	GLdouble aspect;
	
	/* Use the whole window. */
	glViewport(0, 0, w, h);
	
	/* We are going to do some 2-D orthographic drawing. */
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, w, 0, h, -100000.0, 100000.0);
	//size = (GLdouble)((w >= h) ? w : h) / 2.0;
	if (w <= h) {
		aspect = (GLdouble)h/(GLdouble)w;
		//glOrtho(0, size*2, 0, size*aspect*2, -100000.0, 100000.0);
	}
	else {
		aspect = (GLdouble)w/(GLdouble)h;
		//glOrtho(0, size*aspect*2, 0, size*2, -100000.0, 100000.0);
	}
	
	//printf("aspect: %g\n", aspect);
	/* Make the world and window coordinates coincide so that 1.0 in */
	/* model space equals one pixel in window space.                 */
	//glScaled(aspect, aspect, 1.0);
	
	/* Now determine where to draw things. */
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	
	glutPostRedisplay();
}

void kb1(unsigned char key, int x, int y)
{
	modkeys = glutGetModifiers();
	//printf("%d down\n", key);
	keys[key] = 1;
	
	int xi = (cx+swidth/2)/charwidth;
	int yi = (cy+sheight/2)/charheight;
	
	int t1;
	switch (key)
	{
		case 9:
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
			
		case 25:
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
		/*
		case '+':
		case '=':
			if (delay>1) delay/=1.5;
			printf("delay: %d\n", delay);
			break;
		case '-':
		case '_':
			delay*=1.5;
			printf("delay: %d\n", delay);
			break;
		//*/
		case 8:
		case 127:
			field[yi][xi].instr=0;
			if (cthread == ghostid && ghostid != -1)
			{
				fthreads[ghostid].delta *= -1;
				fthreads[ghostid].mode = STEP;
			}
			break;
		default:
			field[yi][xi].instr = key;
		case '\r':
		case '\n':
			if (cthread == ghostid && ghostid != -1)
			{
				fthreads[ghostid].mode = STEP;
			}
	}
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
	int xi = (cx+swidth/2)/charwidth;
	int yi = (cy+sheight/2)/charheight;
	
	modkeys = glutGetModifiers();
	//printf("%d down\n", key+256);
	keys[key+256] = 1;
	switch (key)
	{
		case 1:
			if (delay>1) delay/=2;
			printf("delay: %d\n", delay);
			break;
		case 2:
			if (delay<2000000) delay*=2;;
			printf("delay: %d\n", delay);
			break;
		case 3:
			mulzoom(1.0/1.25);
			break;
		case 4:
			mulzoom(1.25);
			break;
		case 8:
			run = (run==RUN) ? PAUSED : RUN;
			break;
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
					cfthread.mode = STEP;
					break;
				case 6:
					printf("stack trace for thread %d:\n", cthread);
					int i;
					for (i=0; i<cfthread.stackidx; i++)
					{
						printf("%d: %d\n", i, cfthread.stack[i]);
					}
					break;
				case 9:
					cfthread.mode = (cfthread.mode==RUN) ? PAUSED : RUN; 
					break;
				default:
					break;
			}
		}
	}
	else
	{
		switch (key)
		{
			case 5:
				run = STEP;
				break;
			case 7:
				if (cthread != ghostid)
				{
					fthreads[ghostid].x = xi;
					fthreads[ghostid].y = yi;
					focusthread(&fthreads[ghostid]);
				}
				else
				{
					cthread = -1;
				}

				break;
			case 9:
				run = (run==RUN) ? PAUSED : RUN; 
				break;
			default:
				break;
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
	if (/*keys['w'] || keys['W'] ||*/ keys[357])
	{
		cy+=(2.0+6.0*(modkeys&GLUT_ACTIVE_ALT))/czoom;
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
	if (/*keys['a'] || keys['A'] ||*/ keys[356])
	{
		cx-=(2.0+6.0*(modkeys&GLUT_ACTIVE_ALT))/czoom;
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
	if (/*keys['s'] || keys['S'] ||*/ keys[359])
	{
		cy-=(2.0+6.0*(modkeys&GLUT_ACTIVE_ALT))/czoom;
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
	if (/*keys['d'] || keys['D'] ||*/ keys[358])
	{
		cx+=(2.0+6.0*(modkeys&GLUT_ACTIVE_ALT))/czoom;
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
	
	//printf("%g, %g, %g, %g\n", 1/DX, 1/DY, 1/DX2, 1/DY2);
	glutInit(&argc, argv);
	
	//glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowSize(swidth, sheight);
	
	glutCreateWindow("Funge Wars");
	
	//glEnable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	
	glClearColor(0.0, 0.0, 0.0, 1.0);
	
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_DST_COLOR);
	//glBlendFunc(GL_SRC_COLOR, GL_DST_COLOR);
	
	glLineWidth(1);
	
	glutDisplayFunc(display);
	glutReshapeFunc(reshape);
	glutKeyboardFunc(kb1);
	glutKeyboardUpFunc(kb1u);
	glutSpecialFunc(kb2);
	glutSpecialUpFunc(kb2u);
	//glutSpecialFunc(keyboard);
	glutIdleFunc(idle);
	
	font = png_texture_load("curses_640x300_2.png", &fontwidth, &fontheight);
	printf("font tex id: %d\n", font);
	
	charwidth = fontwidth/16;
	charheight = fontheight/16;
	
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
