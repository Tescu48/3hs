
#include <nnc/read-stream.h>
#include <nnc/smdh.h>
#include <inttypes.h>
#include <nnc/utf.h>
#include <string.h>
#include <stdio.h>

void die(const char *fmt, ...);

static const char *index_to_lang(int i)
{
	switch(i)
	{
	case NNC_TITLE_JAPANESE: return "Japanese";
	case NNC_TITLE_ENGLISH: return "English";
	case NNC_TITLE_FRENCH: return "French";
	case NNC_TITLE_GERMAN: return "German";
	case NNC_TITLE_ITALIAN: return "Italian";
	case NNC_TITLE_SPANISH: return "Spanish";
	case NNC_TITLE_SIMPLIFIED_CHINESE: return "Simplified Chinese";
	case NNC_TITLE_KOREAN: return "Korean";
	case NNC_TITLE_DUTCH: return "Dutch";
	case NNC_TITLE_PORTUGUESE: return "Portuguese";
	case NNC_TITLE_RUSSIAN: return "Russian";
	case NNC_TITLE_TRADITIONAL_CHINESE: return "Traditional Chinese";
	}
	return "Unknown";
}

static const char *get_game_ratings(nnc_u8 *ratings)
{
	static char buf[0x200];
	int of = 0;
#define DO(rating, i) if(ratings[i] & NNC_RATING_ACTIVE) of += sprintf(buf + of, rating " is %i y/o, ", NNC_RATING_MIN_AGE(ratings[i]))
	DO("CERO (Japan)", NNC_RATING_CERO);
	DO("ESRB (USA)", NNC_RATING_ESRB);
	DO("USK (Germany)", NNC_RATING_USK);
	DO("PEGI GEN (most of Europe)", NNC_RATING_PEGI_GEN);
	DO("PEGI PRT (Portugal)", NNC_RATING_PEGI_PRT);
	DO("PEGI BBFC (England)", NNC_RATING_PEGI_BBFC);
	DO("COB (Australia)", NNC_RATING_COB);
	DO("GRB (South Korea)", NNC_RATING_GRB);
	DO("CGSRR (Taiwan)", NNC_RATING_CGSRR);
#undef DO
	if(of == 0) return "(not set)";
	/* remove final ", " */
	buf[of - 2] = '\0';
	return buf;
}

static const char *get_lockout(nnc_u32 lockout)
{
	if(lockout == NNC_LOCKOUT_FREE)
		return "WORLD";
	static char buf[0x200];
	int of = 0;
#define DO(lock, name) if(lockout & NNC_LOCKOUT_##lock) { strcat(buf, name ", "); of += strlen(name ", "); }
	DO(JAPAN, "Japan");
	DO(NORTH_AMERICA, "North America");
	DO(EUROPE, "Europe");
	DO(AUSTRALIA, "Australia");
	DO(CHINA, "China");
	DO(KOREA, "Korea");
	DO(TAIWAN, "Taiwan");
#undef DO
	if(of == 0) return "no lockout set";
	/* remove final ", " */
	buf[of - 2] = '\0';
	return buf;
}

static const char *get_flags(nnc_u32 flags)
{
	static char buf[0x200];
	int of = 0;
#define DO(flag) if(flags & NNC_SMDH_FLAG_##flag) { strcat(buf, #flag ", "); of += strlen(#flag ", "); }
	DO(VISABLE);
	DO(AUTOBOOT);
	DO(ALLOW_3D);
	DO(REQUIRE_EULA);
	DO(AUTOSAVE);
	DO(EXTENDED_BANNER);
	DO(RATING_REQUIRED);
	DO(SAVEDATA);
	DO(RECORD_USAGE);
	DO(DISABLE_BACKUP);
	DO(N3DS_ONLY);
#undef DO
	if(of == 0) return "no flags set";
	/* remove final ", " */
	buf[of - 2] = '\0';
	return buf;
}

int smdh_main(int argc, char *argv[])
{
	if(argc != 2) die("usage: %s <file>", argv[0]);
	const char *smdh_file = argv[1];

	nnc_file f;
	if(nnc_file_open(&f, smdh_file) != NNC_R_OK)
		die("f->open() failed");

	nnc_smdh smdh;
	if(nnc_read_smdh(NNC_RSP(&f), &smdh) != NNC_R_OK)
		die("nnc_read_smdh() failed");

	printf("== %s ==\n", smdh_file);

	char
		s[sizeof(smdh.titles[0].short_desc) * 2 + 1],
		l[sizeof(smdh.titles[0].long_desc) * 2 + 1],
		p[sizeof(smdh.titles[0].publisher) * 2 + 1];

	printf(" == Titles ==\n");
	for(int i = 0; i < NNC_TITLE_MAX; ++i)
	{
		s[nnc_utf16_to_utf8((nnc_u8 *) s, sizeof(s), smdh.titles[i].short_desc, (sizeof(s) - 1) / 2)] = '\0';
		l[nnc_utf16_to_utf8((nnc_u8 *) l, sizeof(l), smdh.titles[i].long_desc, (sizeof(l) - 1) / 2)] = '\0';
		p[nnc_utf16_to_utf8((nnc_u8 *) p, sizeof(p), smdh.titles[i].publisher, (sizeof(p) - 1) / 2)] = '\0';

		printf(
			"  == %s ==\n"
			"    Short Description : %s\n"
			"    Long Description  : %s\n"
			"    Publisher         : %s\n"
		, index_to_lang(i), s, l, p);
	}

	printf(
		" Version                         : %04X\n"
		" Game Ratings                    : %s\n"
		" Lockout                         : %s\n"
		" Match Maker ID                  : %08X\n"
		" Match Maker BIT ID              : %016" PRIX64 "\n"
		" Flags                           : %s\n"
		" EULA Version                    : %i.%i\n"
		" Optimal Animation Default Frame : %f\n"
		" CEC ID                          : %08X\n"
	, smdh.version, get_game_ratings(smdh.game_ratings)
	, get_lockout(smdh.lockout), smdh.match_maker_id
	, smdh.match_maker_bit_id, get_flags(smdh.flags)
	, smdh.eula_version_major, smdh.eula_version_minor
	, smdh.optimal_animation_frame, smdh.cec_id);

	NNC_RS_CALL0(f, close);
	return 0;
}

