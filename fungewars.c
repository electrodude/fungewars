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
		default:
		{
			field[yi][xi].instr = key;
		}
		case '\r':
		case '\n':
		{
			if (cthread == ghostid && ghostid != -1)
			{
				fthreads[ghostid].mode = STEP;
			}
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
			mulzoom(1.0/1.25);
			break;
		}
		case 4:
		{
			mulzoom(1.25);
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
			{	run = STEP;
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
