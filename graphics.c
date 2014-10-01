#include "graphics.h"

int font;
int fontwidth;
int fontheight;
int charwidth;
int charheight;

float cx=0;
float cy=0;
float czoom=1.0;

float ccx=0.0;
float ccy=0.0;

float ccdx=0.0;
float ccdy=0.0;

int lastcx=0.0;
int lastcy=0.0;
int lastupdate=1;

float cdx=0.0;
float cdy=0.0;

int rswidth = 1280;
int rsheight = 752;

int swidth = 1280;
int sheight = 752;

int frame=0;
int timenow=0;
int timelast=0;
int timebase=0;

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
