
#include "./hlink.h"
#include "./hstx.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

typedef uint64_t u64;


static int getulong(const char *s, unsigned long *ul)
{
	errno = 0;
	char *end;
	*ul = strtoul(s, &end, 10);
	return !(end == s || errno == ERANGE);
}

static int get64(const char *s, u64 *ull)
{
	errno = 0;
	char *end;
	*ull = strtoull(s, &end, 10);
	return !(end == s || errno == ERANGE);
}

static u64 gettid(const char *s)
{
	if(strncmp(s, "0004", 4) != 0)
		return 0;
	errno = 0;
	char *end;
	u64 ret = strtoull(s, &end, 16);
	return (end == s || errno == ERANGE)
		? 0 : ret;
}

static int hlink(int argc, char *argv[])
{
	if(argc < 2)
	{
		fprintf(stderr, "Usage: hlink [address] [cmd [arg...]...]\n\n"
			"Options:\n"
			"  -s, --sleep           sleep the 3ds for 5 seconds\n"
			"  -a, --add-queue IDs   add IDs to the 3ds queue\n"
			"  -l, --launch TID      launch TID on the 3ds\n"
			"  -w, --wait MS         wait MS milliseconds\n");
		return 1;
	}

	hLink link;
	int res;

	if((res = hl_makelink(&link, argv[1])) != 0)
	{
		fprintf(stderr, "hl_makelink(): %s\n", hl_makelink_geterror(res));
		return 1;
	}
	if((res = hl_auth(&link)) != 0)
	{
		fprintf(stderr, "hl_auth(): %s\n", hl_geterror(res));
		hl_destroylink(&link);
		return 1;
	}

	const char *arg = NULL;
#define TAKEARG() ((++i == argc) ? NULL : (argv[i][0] == '-' ? --i, NULL : argv[i]))
	for(int i = 2; i < argc; ++i)
	{
		hl_waittimeout();
		if(strcmp(argv[i], "--sleep") == 0)
			goto opt_sleep;
		else if(strcmp(argv[i], "--wait") == 0)
			goto opt_wait;
		else if(strcmp(argv[i], "--add-queue") == 0)
			goto opt_add_queue;
		else if(strcmp(argv[i], "--launch") == 0)
			goto opt_launch;
		else if(strncmp(argv[i], "--", 2) == 0)
			fprintf(stderr, "unknown option: '%s'\n", argv[i]);
		else if(argv[i][0] == '-')
		{
			for(int j = 1; argv[i][j] != '\0'; ++j)
			{
				switch(argv[i][j])
				{
				case 's':
opt_sleep:
					if((res = hl_sleep(&link)) != 0)
						fprintf(stderr, "hl_sleep(): %s\n", hl_geterror(res));
					break;
opt_wait:
				case 'w':
					while((arg = TAKEARG()))
					{
						unsigned long t;
						if(!getulong(arg, &t))
							continue;
						usleep(t * 1000);
					}
					goto break_loop;
opt_add_queue:
				case 'a':
				{
					const int maxtids = 10;
					int amount = 0;
					u64 ids[10];
					while(amount < maxtids && (arg = TAKEARG()))
					{
						if(get64(arg, &ids[amount]))
							++amount;
					}
					if((res = hl_addqueue(&link, ids, amount)) != 0)
						fprintf(stderr, "hl_addqueue(): %s\n", hl_geterror(res));
					goto break_loop;
				}
opt_launch:
				case 'l':
				{
					u64 tid;
					if(!(arg = TAKEARG()))
						fprintf(stderr, "launch: expected argument\n");
					else if((tid = gettid(arg)) == 0)
						fprintf(stderr, "launch: failed to parse title id\n");
					else if((res = hl_launch(&link, tid)) != 0)
						fprintf(stderr, "hl_launch(): %s\n", hl_geterror(res));
					goto break_loop;
				}
				default:
					fprintf(stderr, "unknown option: '-%c'\n", argv[i][j]);
					break;
				}
			}
		}
		else fprintf(stderr, "unexpected argument: %s\n", argv[i]);
break_loop:
		continue;
	}

	hl_destroylink(&link);
	return 0;
}

static int maketheme(int argc, char *argv[])
{
	if(argc < 3)
	{
		fprintf(stderr, "Usage: maketheme [input-file] [output-file]\n");
		return 1;
	}
	return make_hstx(argv[2], argv[1]);
}

int main(int argc, char *argv[])
{
	if(argc < 2)
	{
error:
		fprintf(stderr, "Usage: %s [hlink | maketheme]\n", argv[0]);
		return 1;
	}
	if(strcmp(argv[1], "hlink") == 0)
		return hlink(argc - 1, &argv[1]);
	if(strcmp(argv[1], "maketheme") == 0)
		return maketheme(argc - 1, &argv[1]);
	goto error;
}

