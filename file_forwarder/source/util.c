
#include "util.h"


void hang(char *message)
{
	printf(message);
	while(aptMainLoop())
	{
		gspWaitForVBlank();
		gfxSwapBuffers();
		hidScanInput();
		if(hidKeysDown())
			break;
	}
}

char *read_file(char *fname)
{

	FILE *file = fopen(fname, "r");

	/* Get filesize */
	fseek(file, 0, SEEK_END);
	long fsize = ftell(file);
	fseek(file, 0, SEEK_SET);

	/* Read file */
	char *string = malloc(fsize + 1);
	fread(string, 1, fsize, file);
	fclose(file);

	string[fsize] = '\0';
	return string;
}

void mkdirp(char *loc)
{
	char pathbuf[128 * 2];
	for(int i = 0; loc[i] != '\0'; ++i) {
		if(loc[i] == '/' || loc[i] == '\\') {
			pathbuf[i] = '\0';
			mkdir(pathbuf, 0777);
		}
		pathbuf[i] = loc[i];
	}
}

void fcpy(char *loc, char *dest)
{
	FILE *target = fopen(dest, "wb");
	FILE *source = fopen(loc, "rb");

	char buf[BUFSIZ];
	size_t size;

    while((size = fread(buf, 1, BUFSIZ, source)))
	{
		fwrite(buf, 1, size, target);
	}

	fclose(target);
	fclose(source);
}

void self_destruct(void)
{
	Result res = AM_DeleteTitle(MEDIATYPE_SD, PROGRAM_TID);
	if(R_FAILED(res)) {
		printf("Failed to self-destruct:\n");
		printf("0x%08lX\n", res);
		hang("press any key to continue ...\n");
	}
}
