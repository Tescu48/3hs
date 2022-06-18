
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <3ds.h>

#include "config.h"
#include "util.h"

/* https://stackoverflow.com/questions/51534284/how-to-circumvent-format-truncation-warning-in-gcc */
#define swnprintf(...) (snprintf(__VA_ARGS__) < 0 ? abort() : (void)0)


int main(int argc, char* argv[])
{

	/* Init */
	gfxInitDefault();
	consoleInit(GFX_TOP, NULL);
	romfsInit();
	amInit();

	/* Parse config file */

	KVParser parser;
	KeyValue curr;
	
	char *file_cont = read_file("romfs:/config.ini");

	/* Setup parser */
	set_parser_seek(&parser, KVPARSER_DEFAULT_SEEK);
	set_parser_seperator(&parser, '=', '\n');
	set_parser_data(&parser, file_cont);

	char destbuf[(128 * 2) + 7];
	char locbuf[(128 * 2) + 5];

	while(aptMainLoop())
	{
		curr = get_next_token(&parser);
		/* Data end */
		if(curr.status & KVS_FAILED)
			break;

		if(STRCMP(curr.key, "location"))
			snprintf(locbuf, sizeof(locbuf), "romfs:/%s", curr.value);
		if(STRCMP(curr.key, "destination"))
			snprintf(destbuf, sizeof(destbuf), "sdmc:/%s", curr.value);
	}

	if(STRCMP(destbuf, "") || STRCMP(locbuf, "")) {
		printf("ERROR: Bad romfs:/config.ini");
		goto exit;
	}

	printf("%s --> %s\nIs this okay?\n", locbuf, destbuf);
	hang("Press any key but HOME and POWER accept ...\n");
	printf("Please wait, copying file ...\n");

	mkdirp(destbuf);
	fcpy(locbuf, destbuf);
	printf("Done copying!\n");

exit:

	/* Deinit */
	hang("Press any key to exit ...\n");

// #ifdef __CIA_BUILD
	printf("Self destructing ...\n");
	self_destruct();
// #else
	// printf("Not a CIA, not destructing.\n");
// #endif

	free(file_cont);
	romfsExit();
	gfxExit();
	amExit();
	return 0;
}
