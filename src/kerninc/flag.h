#ifndef __SP_FLAG_H__
#define __SP_FLAG_H__

#define CONSOLE_WIDTH  (800)
#define CONSOLE_HEIGHT (600)

static void clean_screen(unsigned int *fb, int width, int height)
{
	int i, j;

	for (j = 0; j <= height; ++j) {
		for (i = 0; i < width; ++i) {
			/* White */
			fb[i + j * width] = (unsigned int) 0xffffff;
		}
	}
}

/* Print the Spanish flag */
static void __attribute__((used)) print_flag(unsigned int *fb, int width, int height)
{
	int i, j;

	/* Make screen all white */
	clean_screen(fb,width,height);

	for (j = 0; j <= 600; ++j) {
		for (i = 0; i < 80; ++i) {
			/* Red */
			fb[j + i * width] = (unsigned int) 0xaa151b;
		}
		for (; i < 200; ++i) {
			/* Yellow */
			fb[j + i * width] = (unsigned int) 0xf1bf00;
		}
		for (; i < 280; ++i) {
			/* Red */
			fb[j + i * width] = (unsigned int) 0xaa151b;
		}	
	}
}

#endif