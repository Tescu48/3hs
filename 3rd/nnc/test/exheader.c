
#include <nnc/read-stream.h>
#include <nnc/exheader.h>
#include <inttypes.h>
#include <string.h>

void die(const char *fmt, ...);


static const char *sci_flags(nnc_u8 flags)
{
	static char ret[0x100];
	int of = 0;
#define CASE(flag, label) if(flags & flag) { strcat(ret, label ", "); of += strlen(label) + 2; }
	CASE(NNC_EXHDR_SCI_ENABLE_L2_CACHE, "Enable L2 Cache")
	CASE(NNC_EXHDR_SCI_CPUSPEED_804MHZ, "804 MHz CPU");
#undef CASE
	if(of == 0) return "(none)";
	/* remove last ", " */
	ret[of - 2] = '\0';
	return ret;
}

static const char *other_attribs(nnc_u8 flags)
{
	static char ret[0x100];
	int of = 0;
#define CASE(flag, label) if(flags & flag) { strcat(ret, label ", "); of += strlen(label) + 2; }
	CASE(NNC_EXHDR_ATTRIB_NO_ROMFS, "RomFS is disabled")
	CASE(NNC_EXHDR_ATTRIB_EXTENDED_SAVEDATA, "Uses extended savedata");
#undef CASE
	if(of == 0) return "(none)";
	/* remove last ", " */
	ret[of - 2] = '\0';
	return ret;
}

static const char *access_control(nnc_u16 flags)
{
	static char ret[0x200];
	int of = 0;
#define CASE(flag, label) if(flags & flag) { strcat(ret, label ", "); of += strlen(label) + 2; }
	CASE(NNC_EXHDR_MOUNT_NAND, "Mount nand:/");
	CASE(NNC_EXHDR_MOUNT_NAND_RO, "Mount nand:/ro/ (read/write)");
	CASE(NNC_EXHDR_MOUNT_TWLN, "Mount twln:/");
	CASE(NNC_EXHDR_MOUNT_WNAND, "Mount wnand:/");
	CASE(NNC_EXHDR_MOUNT_CARD_SPI, "Mount SPI card");
	CASE(NNC_EXHDR_USE_SDIF3, "Use SDIF3");
	CASE(NNC_EXHDR_CREATE_SEED, "Create seed");
	CASE(NNC_EXHDR_USE_CARD_SPI, "Use SPI card");
	CASE(NNC_EXHDR_SD_APPLICATION, "SD application");
	CASE(NNC_EXHDR_MOUNT_SDMC, "Mount sdmc:/");
#undef CASE
	if(of == 0) return "(none)";
	/* remove last ", " */
	ret[of - 2] = '\0';
	return ret;
}


static const char *kernel_flags(nnc_u16 flags)
{
	static char ret[0x200];
	int of = 0;
#define CASE(flag, label) if(flags & flag) { strcat(ret, label ", "); of += strlen(label) + 2; }
	CASE(NNC_EXHDR_KFLAG_ALLOW_DEBUG, "Allow debug")
	CASE(NNC_EXHDR_KFLAG_FORCE_DEBUG, "Force debug");
	CASE(NNC_EXHDR_KFLAG_ALLOW_NON_ALPHANUM, "Allow non-alphanumeric");
	CASE(NNC_EXHDR_KFLAG_SHARED_PAGE_WRITING, "Shared page writing");
	CASE(NNC_EXHDR_KFLAG_PRIVILEGE_PRIORITY, "Privilege priority");
	CASE(NNC_EXHDR_KFLAG_ALLOW_MAIN_ARGS, "Allow main() args");
	CASE(NNC_EXHDR_KFLAG_RUNNABLE_SLEEP, "Runnable on sleep");
	CASE(NNC_EXHDR_KFLAG_SPECIAL_MEMORY, "Special memory");
	CASE(NNC_EXHDR_KFLAG_CPU_CORE2, "Access CPU core 2");
#undef CASE
	if(of == 0) return "(none)";
	/* remove last ", " */
	ret[of - 2] = '\0';
	return ret;
}

static const char *access_info(nnc_u64 flags)
{
	static char ret[0x400];
	int amount_yes = 0;
	int of = 0;
#define CASE(flag, label) if(flags & flag) { strcat(ret, label ", "); of += strlen(label) + 2; if(++amount_yes % 4 == 3) { strcat(ret, "\n                                        "); of += 41; } }
	CASE(NNC_EXHDR_FS_SYSAPP_CAT, "System application category");
	CASE(NNC_EXHDR_FS_HWCHK_CAT, "Hardware check category");
	CASE(NNC_EXHDR_FS_FSTOOL_CAT, "FS tool category");
	CASE(NNC_EXHDR_FS_DEBUG, "Debug");
	CASE(NNC_EXHDR_FS_TWL_CARD_BACKUP, "TWL card backup");
	CASE(NNC_EXHDR_FS_TWL_NAND_DATA, "TWL NAND data");
	CASE(NNC_EXHDR_FS_BOSS, "BOSS");
	CASE(NNC_EXHDR_FS_SDMC_R, "sdmc:/ (read)");
	CASE(NNC_EXHDR_FS_CORE, "Core");
	CASE(NNC_EXHDR_FS_NAND_RO, "nand:/ro/ (read)");
	CASE(NNC_EXHDR_FS_NAND_RW, "nand:/rw/");
	CASE(NNC_EXHDR_FS_NAND_RO_W, "nand:/ro/ (write)");
	CASE(NNC_EXHDR_FS_SYS_SETTINGS_CAT, "System settings category");
	CASE(NNC_EXHDR_FS_CARDBOARD, "Cardboard");
	CASE(NNC_EXHDR_FS_EXPORT_IMPORT_IVS, "Export/import IVS");
	CASE(NNC_EXHDR_FS_SDMC_W, "sdmc:/ (write)");
	CASE(NNC_EXHDR_FS_SWITCH_CLEANUP, "Switch cleanup (3.0.0)");
	CASE(NNC_EXHDR_FS_SAVEDATA_MOVE, "Savedata move (5.0.0)");
	CASE(NNC_EXHDR_FS_SHOP, "Shop (5.0.0)");
	CASE(NNC_EXHDR_FS_SHELL, "Shell (5.0.0)");
	CASE(NNC_EXHDR_FS_HMM_CAT, "Home menu category (6.0.0)");
	CASE(NNC_EXHDR_FS_SEEDDB, "SeedDB (9.6.0)");
#undef CASE
	if(of == 0) return "(none)";
	/* remove last ", " */
	ret[of - 2] = '\0';
	return ret;
}

static const char *n3ds_mode(nnc_u8 mode)
{
	switch(mode)
	{
	case NNC_EXHDR_N3DS_LEGACY: return "Legacy/O3DS mode (64MiB of memory)";
	case NNC_EXHDR_N3DS_PROD: return "Production (124MiB of memory)";
	case NNC_EXHDR_N3DS_DEV1: return "Development 1 (178MiB of memory)";
	case NNC_EXHDR_N3DS_DEV2: return "Development 2 (124MiB of memory)";
	}
	return "(unknown)";
}

static const char *o3ds_mode(nnc_u8 mode)
{
	switch(mode)
	{
	case NNC_EXHDR_O3DS_PROD: return "Production (64MiB of memory)";
	case NNC_EXHDR_O3DS_DEV1: return "Development 1 (96MiB of memory)";
	case NNC_EXHDR_O3DS_DEV2: return "Development 2 (80MiB of memory)";
	case NNC_EXHDR_O3DS_DEV3: return "Development 3 (72MiB of memory)";
	case NNC_EXHDR_O3DS_DEV4: return "Development 4 (32MiB of memory)";
	}
	return "(unknown)";
}

int exheader_main(int argc, char *argv[])
{
	if(argc != 2) die("usage: %s <exheader-file>", argv[0]);
	const char *exheader_file = argv[1];

	nnc_file f;
	if(nnc_file_open(&f, exheader_file) != NNC_R_OK)
		die("failed to open '%s'", exheader_file);

	nnc_exheader exhdr;
	if(nnc_read_exheader(NNC_RSP(&f), &exhdr) != NNC_R_OK)
		die("failed to read exheader from '%s'", exheader_file);

	nnc_u8 major, minor, patch;
	nnc_parse_version(exhdr.remaster_version, &major, &minor, &patch);

#define SEG(label, pad) label " Segment [Address MaxPages Size]" pad ": [0x%08" PRIX32 " 0x%08" PRIX32 " 0x%08" PRIX32 "]\n"
#define SEGA(var) (var).address, (var).max_pages, (var).size
	printf(
		"== %s ==\n"
		" Application Title                    : %s\n"
		" SCI flags                            : %s\n"
		" Remaster Version (Title Version)     : %i.%i.%i (v%i)\n"
		SEG(" Text", " ") /*                   : */
		SEG("   RO", " ") /*                   : */
		SEG(" Data", " ") /*                   : */
		" Stack Size                           : 0x%" PRIX32 "\n"
		" BSS Size                             : 0x%" PRIX32 "\n"
		" Title ID Dependency List             :\n"
	, exheader_file, exhdr.application_title, sci_flags(exhdr.sci_flags)
	, major, minor, patch, exhdr.remaster_version, SEGA(exhdr.text)
	, SEGA(exhdr.ro), SEGA(exhdr.data), exhdr.stack_size, exhdr.bss_size);
#undef SEG
#undef SEGA
	for(int i = 0; i < NNC_MAX_DEPENDENCIES && exhdr.dependencies[i] != 0; ++i)
	{
		if((i % 3) == 2) { printf("%016" PRIX64 "\n", exhdr.dependencies[i]); continue; }
		if((i % 3) == 0) printf("  ");
		if(i + 1 != NNC_MAX_DEPENDENCIES && exhdr.dependencies[i + 1] != 0)
			printf("%016" PRIX64 ", ", exhdr.dependencies[i]);
		else
			printf("%016" PRIX64 "\n", exhdr.dependencies[i]);
	}
	printf(
		" Save Data Size                       : 0x%" PRIX64 "\n"
		" Jump ID                              : %016" PRIX64 "\n"
		" Title ID                             : %016" PRIX64 "\n"
		" Core Version                         : %i\n"
		" New 3DS Mode                         : %s\n"
		" Old 3DS Mode                         : %s\n"
		" Ideal Processor                      : %i\n"
		" Affinity Mask                        : %i\n"
		" Main Thread Priority                 : %i\n"
		" Resource Limit Descriptors           : ["
	, exhdr.savedata_size, exhdr.jump_id, exhdr.title_id
	, exhdr.core_version, n3ds_mode(exhdr.n3ds_mode)
	, o3ds_mode(exhdr.o3ds_mode), exhdr.ideal_processor
	, exhdr.affinity_mask, exhdr.priority);

	printf("0x%04" PRIX16, exhdr.resource_limit_descriptors[0]);
	for(int i = 1; i < NNC_EXHDR_RESOURCE_DESCRIPTORS_COUNT; ++i)
	{
		if(i == (NNC_EXHDR_RESOURCE_DESCRIPTORS_COUNT / 2))
		{
			printf(",\n                                         0x%04" PRIX16,
				exhdr.resource_limit_descriptors[i]);
		}
		else
			printf(", 0x%04" PRIX16, exhdr.resource_limit_descriptors[i]);
	}
	puts("]");
	printf("%li\n", exhdr.access_info);
	printf(
		" Extdata ID                           : %016" PRIX64 "\n"
		" System Savedata IDs                  : [%08" PRIX32 ", %08" PRIX32 "]\n"
		" Storage Accessible Unique IDs        : [%08" PRIX32 ", %08" PRIX32 "]\n"
		" Filesystem Access Information        : %s\n"
		" Other Attributes                     : %s\n"
		" Service Access Control (Pre-9.3.0)   : "
	, exhdr.extdata_id, exhdr.system_savedata_ids[0], exhdr.system_savedata_ids[1]
	, exhdr.storage_accessible_unique_ids[0], exhdr.storage_accessible_unique_ids[1]
	, access_info(exhdr.access_info), other_attribs(exhdr.other_attribs));

	int i;
	for(i = 0; i < NNC_EXHDR_SAC_PRE && exhdr.service_access_control[i][0] != '\0'; ++i)
	{
		if((i % 6) == 5)
		{
			printf("\n                                        %s",
				exhdr.service_access_control[i]);
		}
		else if(i == 0)
			printf("%s", exhdr.service_access_control[i]);
		else
			printf(", %s", exhdr.service_access_control[i]);
	}
	printf("\n Service Access Control (Post-9.3.0)  : ");
	if(i != NNC_EXHDR_SAC_PRE) puts("(none)");
	else
	{
		if(exhdr.service_access_control[NNC_EXHDR_SAC_PRE][0] != '\0')
		{
			printf("%s", exhdr.service_access_control[NNC_EXHDR_SAC_PRE]);
			if(exhdr.service_access_control[NNC_EXHDR_SAC_PRE + 1][0] != '\0')
				printf(", %s", exhdr.service_access_control[NNC_EXHDR_SAC_PRE + 1]);
		}
		puts("");
	}
	printf(" Resource Limit Category              : ");
	switch(exhdr.resource_limit_category)
	{
	case NNC_EXHDR_RESLIM_CAT_APPLICATION: puts("APPLICATION"); break;
	case NNC_EXHDR_RESLIM_CAT_SYS_APPLET: puts("SYS_APPLET"); break;
	case NNC_EXHDR_RESLIM_CAT_LIB_APPLET: puts("LIB_APPLET"); break;
	case NNC_EXHDR_RESLIM_CAT_OTHER: puts("OTHER"); break;
	default: puts("(unknown)"); break;
	}

	printf(" Allowed System Calls                 : ");
	int printed = 0;
	for(int i = 0; i < NNC_EXHDR_SYSCALL_SIZE; ++i)
	{
		for(int j = 0; j < NNC_EXHDR_SYSCALL_USEFUL_BITS; ++j)
		{
			if(exhdr.allowed_syscalls[i] & (1 << j))
			{
				nnc_u8 id = i * NNC_EXHDR_SYSCALL_USEFUL_BITS + j;
				const char *name = nnc_exheader_syscall_name(id);
				if((printed % 3) == 2)
				{
					printf("\n                                        %s (0x%02X)", name, id);
				}
				else if(printed == 0)
					printf("%s (0x%02X)", name, id);
				else
					printf(", %s (0x%02X)", name, id);
				++printed;
			}
		}
	}
	puts("");

	printf(
		" Kernel Release Version               : %d.%d\n"
		" Maximum handles                      : %" PRIX32 "\n"
		" Kernel Flags                         : %s\n"
		" Memory type                          : "
	, exhdr.kernel_release_major, exhdr.kernel_release_minor
	, exhdr.max_handles, kernel_flags(exhdr.kflags));

	switch(NNC_EXHEADER_MEMTYPE(exhdr.kflags))
	{
	case NNC_EXHDR_APPLICATION: puts("APPLICATION"); break;
	case NNC_EXHDR_SYSTEM: puts("SYSTEM"); break;
	case NNC_EXHDR_BASE: puts("BASE"); break;
	default: puts("(unknown)"); break;
	}

	printf(" Memory Mappings                      : ");
	for(i = 0; i < NNC_MAX_MMAPS && exhdr.memory_mappings[i].type != NNC_EXHDR_MMAP_END_ARRAY; ++i)
	{
		if(i != 0) printf("                                        ");
		switch(exhdr.memory_mappings[i].type)
		{
		case NNC_EXHDR_MMAP_STATIC_RW: printf("Static RW "); break;
		case NNC_EXHDR_MMAP_STATIC_RO: printf("Static R  "); break;
		case NNC_EXHDR_MMAP_IO_RW:     printf("IO     RW "); break;
		case NNC_EXHDR_MMAP_IO_RO:     printf("IO     R  "); break;
		default:                       printf("(unknown) "); break;
		}
		printf("0x%" PRIX32 "-0x%" PRIX32 "\n", exhdr.memory_mappings[i].range_start, exhdr.memory_mappings[i].range_end);
	}
	if(i == 0) puts("(none)");

	printf(
		" ARM9 Access Control                  : %s\n"
		" ARM9 Access Control Version          : %d\n"
	, access_control(exhdr.arm9_access_control), exhdr.arm9_access_control_version);

	NNC_RS_CALL0(f, close);
	return 0;
}

