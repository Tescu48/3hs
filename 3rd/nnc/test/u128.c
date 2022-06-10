
#include <nnc/u128.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

void die(const char *fmt, ...);


int u128_main(int argc, char *argv[])
{
	if(argc != 4) die("usage: %s <hex-a> <op> <hex-b>\n<op> may be '+', '^', 'rol' or 'ror'", argv[0]);
	const nnc_u128 a_orig = nnc_u128_from_hex(argv[1]);
	nnc_u128 a = a_orig;
	nnc_u128 *(*op)(nnc_u128 *a, const nnc_u128 *b);

	if(strcmp(argv[2], "+") == 0) op = nnc_u128_add;
	else if(strcmp(argv[2], "^") == 0) op = nnc_u128_xor;
	else if(strcmp(argv[2], "ror") == 0 || strcmp(argv[2], "rol") == 0)
	{
		nnc_u128 *(*ro)(nnc_u128 *a, nnc_u8 n) = argv[2][2] == 'r' ? nnc_u128_ror : nnc_u128_rol;
		unsigned long n = strtoul(argv[3], NULL, 10);
		if(n > 64) die("invalid n (must be < 64)");
		ro(&a, (nnc_u8) n);
		printf("0x" NNC_FMT128 " %s %li = 0x" NNC_FMT128 "\n", NNC_ARG128(a_orig), argv[2], n, NNC_ARG128(a));
		return 0;
	}
	else die("invalid op");
	const nnc_u128 b_orig = nnc_u128_from_hex(argv[3]);

	op(&a, &b_orig);
	printf("0x" NNC_FMT128 " %s 0x" NNC_FMT128 " = 0x" NNC_FMT128 "\n", NNC_ARG128(a_orig), argv[2], NNC_ARG128(b_orig), NNC_ARG128(a));
	char pybuf[0x100];
	sprintf(pybuf, "print(\"0x{0:032X}\".format(0x" NNC_FMT128 " %s 0x" NNC_FMT128 "))", NNC_ARG128(a_orig), argv[2], NNC_ARG128(b_orig));
	printf("should be:                                                                 ");
	fflush(stdout);
	execlp("python3", "python3", "-c", pybuf, NULL);
	/* only reached if execlp failed */
	if(errno == ENOENT) printf("(python3 not installed)\n");
	else printf("(failed calling execlp to run python)\n");
	return 0;
}

