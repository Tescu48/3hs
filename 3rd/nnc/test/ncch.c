
#include <nnc/read-stream.h>
#include <nnc/exefs.h>
#include <nnc/ncch.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define NO_CRYPT "(unable to find support files for encrypted NCCHs)"

void nnc_dumpmem(const nnc_u8 *b, nnc_u32 size);
void die(const char *fmt, ...);
void print_hash(nnc_u8 *b);


static const char *get_crypt_support(nnc_u8 crypt)
{
	switch(crypt)
	{
	case NNC_CRYPT_INITIAL: return "All Versions";
	case NNC_CRYPT_700: return "Since 7.0.0-X";
	case NNC_CRYPT_930: return "Since 9.3.0-X";
	case NNC_CRYPT_960: return "Since 9.6.0-X";
	}
	return "Unknown";
}

static const char *get_platform(nnc_u8 plat)
{
	switch(plat)
	{
	case NNC_NCCH_O3DS: return "Old 3ds/2ds (XL/LL)";
	case NNC_NCCH_N3DS: return "New 3ds/2ds (XL/LL)";
	}
	return "Unknown";
}

static const char *get_type(nnc_u8 typ)
{
	static char ret[0x100];
	int of = 0;
#define CASE(flag, label) if(typ & flag) { strcat(ret, label ", "); of += strlen(label) + 2; }
	CASE(NNC_NCCH_DATA, "Data")
	CASE(NNC_NCCH_EXE, "Executable")
	CASE(NNC_NCCH_SYS_UPDATE, "System Update")
	CASE(NNC_NCCH_MANUAL, "Manual")
	CASE(NNC_NCCH_TRIAL, "Trial")
#undef CASE
	if(of == 0) return "(none)";
	/* remove last ", " */
	ret[of - 2] = '\0';
	if((typ & (NNC_NCCH_DATA | NNC_NCCH_EXE)) == NNC_NCCH_DATA)
		strcat(ret, " (CFA)");
	else if(typ & NNC_NCCH_EXE)
		strcat(ret, " (CXI)");
	return ret;
}

static const char *get_flags(nnc_u8 flags)
{
	static char ret[0x100];
	int of = 0;
#define CASE(flag, label) if(flags & flag) { strcat(ret, label ", "); of += strlen(label) + 2; }
	CASE(NNC_NCCH_FIXED_KEY, "Fixed Encryption Key")
	CASE(NNC_NCCH_NO_ROMFS, "No RomFS")
	CASE(NNC_NCCH_NO_CRYPTO, "No Encryption")
	CASE(NNC_NCCH_USES_SEED, "Uses Seed")
#undef CASE
	if(of == 0) return "(none)";
	/* remove last ", " */
	ret[of - 2] = '\0';
	return ret;
}

int ncch_info_main(int argc, char *argv[])
{
	if(argc != 2) die("usage: %s <ncch-file>", argv[0]);
	const char *ncch_file = argv[1];

	nnc_file f;
	if(nnc_file_open(&f, ncch_file) != NNC_R_OK)
		die("failed to open '%s'", ncch_file);

	nnc_ncch_header header;
	if(nnc_read_ncch_header(NNC_RSP(&f), &header) != NNC_R_OK)
		die("failed to read ncch header from '%s'", ncch_file);

	nnc_seeddb seeddb;
	nnc_keyset ks = NNC_KEYSET_INIT;
	if(nnc_scan_seeddb(&seeddb) != NNC_R_OK)
		fprintf(stderr, "Failed to find a seeddb. Titles with seeds will not work.\n");
	nnc_keyset_default(&ks, false);
	nnc_keypair kpair; int crypt = 1;
	if(nnc_fill_keypair(&kpair, &ks, &seeddb, &header) != NNC_R_OK)
		fprintf(stderr, "failed to fill keypair, crypto will not work.\n"), crypt = 0;

	printf(
		"== %s ==\n"
		" KeyY Raw                     : " NNC_FMT128 "\n"
		" KeyY Seed Decrypted          : "
	, ncch_file, NNC_ARG128(header.keyy));
	if(header.flags & NNC_NCCH_USES_SEED)
	{
		nnc_u8 *seed;
		if((seed = nnc_get_seed(&seeddb, header.title_id)) != NULL)
		{
			nnc_u128 keyy;
			if(nnc_keyy_seed(&header, &keyy, seed) == NNC_R_OK)
				printf(NNC_FMT128 "\n", NNC_ARG128(keyy));
			else puts("(seed not valid)");
		}
		else puts("(seed not found)");
	}
	else
	{
		puts("(not required)");
	}
#define MU_PAIR  "%u MU (0x%X)"
#define MU_PAIRB "%u MU (0x%X bytes)"
#define MU_ARG(v) v, NNC_MU_TO_BYTE(v)
	printf(
		" Content Size                 : " MU_PAIRB "\n"
		" Partition ID                 : %016" PRIX64 "\n"
		" Maker Code                   : %s (0x%04X)\n"
		" NCCH Version                 : %02X\n"
		" Seed Check Hash              : %08X\n"
		" Title ID                     : %016" PRIX64 "\n"
		" Product Code                 : %s\n"
		" Extended Header Size         : 0x%X bytes\n"
		" System Cryptography Support  : %s%s\n"
		" Platform                     : %s\n"
		" Type                         : %s\n"
		" Content Unit Size            : 0x%X\n"
		" Flags                        : %s\n"
		" Plain Region Offset          : " MU_PAIR "\n"
		"                Size          : " MU_PAIRB "\n"
		" Logo Region Offset           : " MU_PAIR "\n"
		"               Size           : " MU_PAIRB "\n"
		" ExeFS Region Offset          : " MU_PAIR "\n"
		"                Size          : " MU_PAIRB "\n"
		" RomFS Region Offset          : " MU_PAIR "\n"
		"                Size          : " MU_PAIRB "\n"
	, MU_ARG(header.content_size)
	, header.partition_id, header.maker_code, * (nnc_u32 *) header.maker_code, header.version
	, header.seed_hash, header.title_id, header.product_code
	, header.exheader_size , get_crypt_support(header.crypt_method)
	/* i don't think this could ever be true, but it's here just in case */
	, (header.flags & NNC_NCCH_USES_SEED && header.crypt_method != NNC_CRYPT_960)
		? " (Uses seed, so really since 9.6.0-X)" : ""
	, get_platform(header.platform), get_type(header.type), header.content_unit
	, get_flags(header.flags), MU_ARG(header.plain_offset), MU_ARG(header.plain_size)
	, MU_ARG(header.logo_offset), MU_ARG(header.logo_size), MU_ARG(header.exefs_offset)
	, MU_ARG(header.exefs_size), MU_ARG(header.romfs_offset), MU_ARG(header.romfs_size));
	printf(" Logo Region Hash             : "); print_hash(header.logo_hash); puts("");
	printf(" Extended Header Region Hash  : "); print_hash(header.exheader_hash); puts("");
	printf(" ExeFS Region Hash            : "); print_hash(header.exefs_hash); puts("");
	printf(" RomFS Region Hash            : "); print_hash(header.romfs_hash); puts("");

	nnc_ncch_section_stream romfs;
	printf(" RomFS Block0                 : ");
	if(!(header.flags & NNC_NCCH_NO_CRYPTO) && !crypt)
		puts(NO_CRYPT);
	else if(nnc_ncch_section_romfs(&header, NNC_RSP(&f), &kpair, &romfs) == NNC_R_OK)
	{
		nnc_u8 block0[0x10];
		nnc_u32 total;
		if(NNC_RS_CALL(romfs, read, block0, 0x10, &total) == NNC_R_OK && total == 0x10)
			nnc_dumpmem(block0, 0x10);
		else
			puts("(failed to read)");
		NNC_RS_CALL0(romfs, close);
	}
	else
		puts("(failed to read)");
	nnc_ncch_section_stream exefs;
	printf(" ExeFS Header Block0          : ");
	if(!(header.flags & NNC_NCCH_NO_CRYPTO) && !crypt)
		puts(NO_CRYPT "\n   /icon (SMDH) Block0        : " NO_CRYPT);
	else if(nnc_ncch_section_exefs_header(&header, NNC_RSP(&f), &kpair, &exefs) == NNC_R_OK)
	{
		nnc_u8 block0[0x10];
		nnc_u32 total;
		if(NNC_RS_CALL(exefs, read, block0, 0x10, &total) == NNC_R_OK && total == 0x10)
			nnc_dumpmem(block0, 0x10);
		else
			puts("(failed to read)");


		NNC_RS_CALL(exefs, seek_abs, 0);
		nnc_exefs_file_header headers[NNC_EXEFS_MAX_FILES];
		nnc_read_exefs_header(NNC_RSP(&exefs), headers, NULL);
		nnc_i8 index = nnc_find_exefs_file_index("icon", headers);
		printf("   /icon (SMDH) Block0        : ");
		if(index != -1)
		{
			nnc_ncch_section_stream icon;
			nnc_ncch_exefs_subview(&header, NNC_RSP(&f), &kpair, &icon, &headers[index]);

			if(NNC_RS_CALL(icon, read, block0, 0x10, &total) == NNC_R_OK && total == 0x10)
				nnc_dumpmem(block0, 0x10);
			else
				puts("(failed to read)");

			NNC_RS_CALL0(icon, close);
		}
		else
			puts("(not present)");

		NNC_RS_CALL0(exefs, close);
	}
	else
		puts("(failed to read)");

	nnc_ncch_section_stream exheader;
	printf(" Extended Header Block0       : ");
	if(!(header.flags & NNC_NCCH_NO_CRYPTO) && !crypt)
		puts(NO_CRYPT);
	else if(nnc_ncch_section_exheader(&header, NNC_RSP(&f), &kpair, &exheader) == NNC_R_OK)
	{
		nnc_u8 block0[0x10]; nnc_u32 total;
		if(NNC_RS_CALL(exheader, read, block0, 0x10, &total) == NNC_R_OK && total == 0x10)
			nnc_dumpmem(block0, 0x10);
		else
			puts("(failed to read)");
	}
	else
		puts("(failed to read)");

	nnc_subview plain;
	printf(" Plain Section Block0         : ");
	if(nnc_ncch_section_plain(&header, NNC_RSP(&f), &plain) == NNC_R_OK)
	{
		nnc_u8 block0[0x10]; nnc_u32 total;
		if(NNC_RS_CALL(plain, read, block0, 0x10, &total) == NNC_R_OK && total == 0x10)
			nnc_dumpmem(block0, 0x10);
		else
			puts("(failed to read)");
	}
	else
		puts("(not available)");

	nnc_subview logo;
	printf(" Logo Section Block0          : ");
	if(nnc_ncch_section_logo(&header, NNC_RSP(&f), &logo) == NNC_R_OK)
	{
		nnc_u8 block0[0x10]; nnc_u32 total;
		if(NNC_RS_CALL(logo, read, block0, 0x10, &total) == NNC_R_OK && total == 0x10)
			nnc_dumpmem(block0, 0x10);
		else
			puts("(failed to read)");
	}
	else
		puts("(not available)");


	nnc_free_seeddb(&seeddb);
	NNC_RS_CALL0(f, close);
	return 0;
}

