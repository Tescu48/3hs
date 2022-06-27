/* This file is part of 3hs
 * Copyright (C) 2021-2022 hShop developer team
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "lumalocale.hh"
#include "settings.hh"
#include "util.hh"
#include "ctr.hh"
#include "log.hh"

#include <ui/smdhicon.hh>
#include <ui/selector.hh>
#include <ui/confirm.hh>
#include <ui/base.hh>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


static bool has_region(ctr::TitleSMDH *smdh, ctr::Region region)
{
	// This is a mess
	return
		(smdh->region & (u32) ctr::RegionLockout::JPN && region == ctr::Region::JPN) ||
		(smdh->region & (u32) ctr::RegionLockout::USA && region == ctr::Region::USA) ||
		(smdh->region & (u32) ctr::RegionLockout::EUR && region == ctr::Region::EUR) ||
		(smdh->region & (u32) ctr::RegionLockout::AUS && region == ctr::Region::EUR) ||
		(smdh->region & (u32) ctr::RegionLockout::CHN && region == ctr::Region::CHN) ||
		(smdh->region & (u32) ctr::RegionLockout::KOR && region == ctr::Region::KOR) ||
		(smdh->region & (u32) ctr::RegionLockout::TWN && region == ctr::Region::TWN);
}

static const char *get_auto_lang_str(ctr::TitleSMDH *smdh)
{
	if(smdh->region & (u32) ctr::RegionLockout::JPN) return "JP";
	if(smdh->region & (u32) ctr::RegionLockout::USA) return "EN";
	if(smdh->region & (u32) ctr::RegionLockout::EUR) return "EN";
	if(smdh->region & (u32) ctr::RegionLockout::AUS) return "EN";
	if(smdh->region & (u32) ctr::RegionLockout::CHN) return "ZH";
	if(smdh->region & (u32) ctr::RegionLockout::KOR) return "KR";
	if(smdh->region & (u32) ctr::RegionLockout::TWN) return "TW";
	// Fail
	return nullptr;
}

#define LANG_INVALID 12
static const char *get_manual_lang_str(ctr::TitleSMDH *smdh)
{
	ctr::TitleSMDHTitle *title = ctr::smdh::get_native_title(smdh);
	bool focus = set_focus(true);
	ui::RenderQueue queue;

	u8 lang = LANG_INVALID;
	// EN, JP, FR, DE, IT, ES, ZH, KO, NL, PT, RU, TW
	static const std::vector<std::string> langlut = { "EN", "JP", "FR", "DE", "IT", "ES", "ZH", "KO", "NL", "PT", "RU", "TW" };
	static const std::vector<u8> enumVals = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

	ui::builder<ui::SMDHIcon>(ui::Screen::top, smdh, SMDHIconType::large)
		.x(ui::dimensions::width_top / 2 - 30)
		.y(ui::dimensions::height / 2 - 64)
		.border()
		.add_to(queue);
	ui::builder<ui::Text>(ui::Screen::top,
			ctr::smdh::u16conv(title->descShort, 0x40) + "\n" +
			ctr::smdh::u16conv(title->descLong, 0x80))
		.x(ui::layout::center_x)
		.under(queue.back())
		.wrap()
		.add_to(queue);
	ui::builder<ui::Selector<u8>>(ui::Screen::bottom, langlut, enumVals, &lang)
		.add_to(queue);

	queue.render_finite_button(KEY_B);

	set_focus(focus);
	return lang == LANG_INVALID
		? nullptr : langlut[lang].c_str();
}
#undef LANG_INVALID

static const char *get_region_str(ctr::TitleSMDH *smdh)
{
	if(smdh->region & (u32) ctr::RegionLockout::JPN) return "JPN";
	if(smdh->region & (u32) ctr::RegionLockout::USA) return "USA";
	if(smdh->region & (u32) ctr::RegionLockout::EUR) return "EUR";
	if(smdh->region & (u32) ctr::RegionLockout::AUS) return "EUR";
	if(smdh->region & (u32) ctr::RegionLockout::CHN) return "CHN";
	if(smdh->region & (u32) ctr::RegionLockout::KOR) return "KOR";
	if(smdh->region & (u32) ctr::RegionLockout::TWN) return "TWN";
	// Fail
	return nullptr;
}

static std::string ensure_path(u64 tid)
{
	// path: /luma/titles/{tid}/locale.txt
	std::string path;
#define ITER(x) do { path += (x); mkdir(path.c_str(), 0777); } while(0)
	ITER("/luma");
	ITER("/titles/");
	ITER(ctr::tid_to_str(tid));
#undef ITER
	return path + "/locale.txt";
}

static void write_file(u64 tid, const char *region, const char *lang)
{
	std::string path = ensure_path(tid);
	// locale.txt already exists
	if(access(path.c_str(), F_OK) == 0)
		return;

	std::string contents = std::string(region) + " " + std::string(lang);

	FILE *file = fopen(path.c_str(), "w");
	if(file == nullptr) return;

	fwrite(contents.c_str(), 1, contents.size(), file);
	fclose(file);
}

static bool enable_gamepatching_buf(u8 *buf)
{
	// Luma3DS config format
	// byte 0-3 : "CONF"
	// byte 4-5 : version_major
	// byte 6-7 : version_minor
	// byte ... : config

	/* is this actually a luma configuration file? */
	if(memcmp(buf, "CONF", 4) != 0)
		return true;

	// version_major != 2
	if(* (u16 *) &buf[4] != 2)
		return true;

	bool ret = buf[8] & 0x8;
	buf[8] |= 0x8;
	return ret;
}

/* returns if gamepatching was set before */
static bool enable_gamepatching()
{
	FILE *config = fopen("/luma/config.bin", "r");
	if(config == nullptr) return true;

	u8 buf[32];
	if(fread(buf, 1, 32, config) != 32)
	{ fclose(config); return true; }

	fclose(config);
	vlog("checking if gamepatching is enabled");
	bool isSet = enable_gamepatching_buf(buf);
	vlog("isSet: %i", isSet);
	if(isSet) return true;

	// We need to update settings
	config = fopen("/luma/config.bin", "w");
	if(config == nullptr) return true;

	if(fwrite(buf, 1, 32, config) != 32)
	{ fclose(config); return true; }

	fclose(config);
	return false;
}

void luma::maybe_set_gamepatching()
{
	/* disabled due to luma v11.0, may be re-implemented for config.ini at some point. */
	return;

	if(SETTING_LUMALOCALE == LumaLocaleMode::disabled)
		return;

	// We should prompt for reboot...
	if(!enable_gamepatching())
	{
		ilog("enabled game patching");
		if(ui::Confirm::exec(STRING(reboot_now), STRING(patching_reboot)))
			NS_RebootSystem();
	}
}

bool luma::set_locale(u64 tid)
{
	// we don't want to set a locale
	LumaLocaleMode mode = SETTING_LUMALOCALE;
	if(mode == LumaLocaleMode::disabled)
		return false;

	ctr::TitleSMDH *smdh = ctr::smdh::get(tid);
	ctr::Region region = ctr::Region::WORLD;
	const char *langstr = nullptr;
	const char *regstr = nullptr;

	if(smdh == nullptr) return false;
	bool ret = false;

	// We don't need to do anything
	if(smdh->region == (u32) ctr::RegionLockout::WORLD)
		goto del_smdh;

	u8 sysregion;
	if(R_FAILED(CFGU_SecureInfoGetRegion(&sysregion)))
		goto del_smdh;

	// Convert to Region
	switch(sysregion)
	{
		case CFG_REGION_AUS: case CFG_REGION_EUR: region = ctr::Region::EUR; break;
		case CFG_REGION_CHN: region = ctr::Region::CHN; break;
		case CFG_REGION_JPN: region = ctr::Region::JPN; break;
		case CFG_REGION_KOR: region = ctr::Region::KOR; break;
		case CFG_REGION_TWN: region = ctr::Region::TWN; break;
		case CFG_REGION_USA: region = ctr::Region::USA; break;
		default: goto del_smdh; // invalid region
	}

	// If we have our own region we don't need to do anything
	if(has_region(smdh, region)) goto del_smdh;
	regstr = get_region_str(smdh);

	if(mode == LumaLocaleMode::automatic)
	{
		langstr = get_auto_lang_str(smdh);
		if(!langstr) goto del_smdh; /* shouldn't happen */
		ilog("(lumalocale) Automatically deduced %s %s", regstr, langstr);
	}
	else if(mode == LumaLocaleMode::manual)
	{
		langstr = get_manual_lang_str(smdh);
		/* cancelled the selection */
		if(!langstr) goto del_smdh;
		ilog("(lumalocale) Manually selected %s %s", regstr, langstr);
	}

	write_file(tid, regstr, langstr);
	ret = true;

del_smdh:
	delete smdh;
	return ret;
}

