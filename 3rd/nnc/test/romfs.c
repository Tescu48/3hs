
#include <nnc/read-stream.h>
#include <nnc/romfs.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <nnc/utf.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

void die(const char *fmt, ...);


static char *utf16conv(const nnc_u16 *u16string, int u16len)
{
	int len = u16len * 4;
	char *str = malloc(len + 1);
	str[nnc_utf16_to_utf8((nnc_u8 *) str, len, u16string, u16len)] = '\0';
	return str;
}

static void print_dir(nnc_romfs_ctx *ctx, nnc_romfs_info *dir, int indent)
{
	nnc_romfs_iterator it = nnc_romfs_mkit(ctx, dir);
	nnc_romfs_info ent;
	while(nnc_romfs_next(&it, &ent))
	{
		char *fname = utf16conv(ent.filename, ent.filename_length);
		printf("%*s%s%s\n", indent, "", fname, ent.type == NNC_ROMFS_DIR ? "/" : "");
		free(fname);
		if(ent.type == NNC_ROMFS_DIR)
			print_dir(ctx, &ent, indent + 1);
	}
}

int romfs_main(int argc, char *argv[])
{
	if(argc != 2) die("usage: %s <file>", argv[0]);
	const char *romfs_file = argv[1];

	nnc_file f;
	if(nnc_file_open(&f, romfs_file) != NNC_R_OK)
		die("f->open() failed");

	nnc_romfs_ctx ctx;
	if(nnc_init_romfs(NNC_RSP(&f), &ctx) != NNC_R_OK)
		die("nnc_init_romfs() failed");

	printf(
		"== %s ==\n"
		" == RomFS header ==\n"
		"  Directory Hash Offset      : 0x%" PRIX64 "\n"
		"                 Length      : 0x%X\n"
		"  Directory Metadata Offset  : 0x%" PRIX64 "\n"
		"                     Length  : 0x%X\n"
		"  File Hash Offset           : 0x%" PRIX64 "\n"
		"            Length           : 0x%X\n"
		"  File Metadata Offset       : 0x%" PRIX64 "\n"
		"                Length       : 0x%X\n"
		"  Data Offset                : 0x%" PRIX64 "\n"
		" == RomFS files & directories ==\n"
	, romfs_file, ctx.header.dir_hash.offset
	, ctx.header.dir_hash.length, ctx.header.dir_meta.offset
	, ctx.header.dir_meta.length, ctx.header.file_hash.offset
	, ctx.header.file_hash.length, ctx.header.file_meta.offset
	, ctx.header.file_meta.length, ctx.header.data_offset);

	nnc_romfs_info info;
	if(nnc_get_info(&ctx, &info, "/") != NNC_R_OK)
		die("failed root directory info");
	puts(" /");
	print_dir(&ctx, &info, 2);

	nnc_free_romfs(&ctx);

	NNC_RS_CALL0(f, close);
	return 0;
}

static void extract_dir(nnc_romfs_ctx *ctx, nnc_romfs_info *info, const char *path, int baselen)
{
	if(access(path, F_OK) != 0 && mkdir(path, 0777) != 0)
		die("failed to create directory '%s': %s", path, strerror(errno));
	printf("%s/\n", path + baselen);
	nnc_romfs_iterator it = nnc_romfs_mkit(ctx, info);
	nnc_romfs_info ent;
	char pathbuf[2048];
	int len = strlen(path);
	strcpy(pathbuf, path);
	pathbuf[len++] = '/';
	while(nnc_romfs_next(&it, &ent))
	{
		char *fname = utf16conv(ent.filename, ent.filename_length);
		strcpy(pathbuf + len, fname);
		free(fname);
		if(ent.type == NNC_ROMFS_DIR)
			extract_dir(ctx, &ent, pathbuf, baselen);
		else
		{
			puts(pathbuf + baselen);
			FILE *out = fopen(pathbuf, "w");
			/* slurping the file is a bit inefficient for large
			 * files but it's fine for this test */
			nnc_u8 *cbuf = malloc(ent.u.f.size);
			nnc_subview sv;
			if(nnc_romfs_open_subview(ctx, &sv, &ent) != NNC_R_OK)
				goto file_fail;
			nnc_u32 r;
			if(NNC_RS_CALL(sv, read, cbuf, ent.u.f.size, &r) != NNC_R_OK)
				goto file_fail;
			if(r != ent.u.f.size) goto file_fail;
			if(fwrite(cbuf, ent.u.f.size, 1, out) != 1)
				goto file_fail;
			fclose(out);
			free(cbuf);
			continue;
file_fail:
			fprintf(stderr, "fail: ");
			fclose(out);
			free(cbuf);
		}
	}
}

int xromfs_main(int argc, char *argv[])
{
	if(argc != 3) die("usage: %s <file> <output-directory>", argv[0]);
	const char *romfs_file = argv[1];
	const char *output = argv[2];

	nnc_file f;
	if(nnc_file_open(&f, romfs_file) != NNC_R_OK)
		die("nnc_file_open() failed on '%s'", romfs_file);

	nnc_romfs_ctx ctx;
	if(nnc_init_romfs(NNC_RSP(&f), &ctx) != NNC_R_OK)
		die("nnc_init_romfs() failed");

	nnc_romfs_info info;
	if(nnc_get_info(&ctx, &info, "/") != NNC_R_OK)
		die("failed root directory info");
	extract_dir(&ctx, &info, output, strlen(output));

	nnc_free_romfs(&ctx);

	NNC_RS_CALL0(f, close);
	return 0;
}

