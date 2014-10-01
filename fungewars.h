/*
 *  fungewars.c
 *  fungewars
 *
 */

typedef struct cell
{
	char instr;
	int fg;		// (cell is occupied) ? 1 : 0
	int bg;		// team of last IP to overrun cell
} cell;

typedef enum falive
{
	DEAD,
	ALIVE,
	GHOST,
} falive;

typedef enum fmode
{
	PAUSED,
	STEP,
	RUN
} fmode;

typedef struct fthread
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
	int *stack;
	int stackidx;
	int stacksize;
	int stringmode;
	int jmpmode;
	int repeats;
	falive alive;
	fmode mode;
} fthread;

typedef struct coord
{
	int x;
	int y;
} coord;

inline int wrap(int x, int m);

GLuint png_texture_load(const char * file_name, int * width, int * height);

void glputc(int x, int y, int c);

fthread* newfthread(unsigned int team, int x, int y, int dx, int dy, int flag);

fthread* dupfthread(fthread** parent);

fthread killfthread(int id);

int push(fthread* cfthread, int x);

int pop(fthread* cfthread);

coord* chkline(int x0, int y0, int x1, int y1, char check);

int execinstr(fthread* cfthread, cell *ccell);

void *interpreter(void *threadid);

void newgame();

void focusthread(fthread* cfthread);

void focuscam(float x, float y);

void mulzoom(float factor);

void display(void);

void reshape(int width, int height);

void kb1(unsigned char key, int x, int y);

void kb1u(unsigned char key, int x, int y);

void kb2(int key, int x, int y);

void kb2u(int key, int x, int y);

void idle(void);

int main(int argc, char** argv);
