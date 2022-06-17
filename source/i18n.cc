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

#include <ui/base.hh> /* for UI_GLYPH_* */

#ifndef HS_SITE_LOC
	/* do not translate this */
	#define PAGE_3HS "(unset)"
	#define PAGE_THEMES "(unset)"
#else
	#define PAGE_3HS HS_SITE_LOC "/3hs"
	#define PAGE_THEMES HS_SITE_LOC "/wiki/theme-installation"
#endif

#include "settings.hh"
#include "panic.hh"
#include "i18n.hh"

#include <3ds.h>

#define RAW(lid, sid) lang_strtab[lid][sid]
#include "i18n_tab.cc"

const char *i18n::getstr(str::type id)
{
	return RAW(get_settings()->language, id);
}

const char *i18n::getstr(str::type sid, lang::type lid)
{
	return RAW(lid, sid);
}

const char *i18n::getsurestr(str::type sid)
{
	ensure_settings();
	return RAW(get_settings()->language, sid);
}

// https://www.3dbrew.org/wiki/Country_Code_List
//  only took over those that we actually use
namespace CountryCode
{
	enum _codes {
		canada  = 18,
		greece  = 79,
		hungary = 80,
		latvia  = 84,
		poland  = 97,
		romania = 99,
	};
}

// Not documented so gotten through a test application
namespace ProvinceCode
{
	enum _codes {
		japan_osaka   = 28,
		japan_okinawa = 48,
	};
}

lang::type i18n::default_lang()
{
	u8 syslang = 0;
	u8 countryinfo[4];
	if(R_FAILED(CFGU_GetSystemLanguage(&syslang)))
		syslang = CFG_LANGUAGE_EN;
	/* countryinfo */
	if(R_FAILED(CFGU_GetConfigInfoBlk2(4, 0x000B0000, countryinfo)))
	{
		countryinfo[2] = 0;
		countryinfo[3] = 0;
	}

	switch(countryinfo[3])
	{
	case CountryCode::hungary: return lang::hungarian;
	case CountryCode::romania: return lang::romanian;
	case CountryCode::latvia: return lang::latvian;
	case CountryCode::poland: return lang::polish;
	case CountryCode::greece: return lang::greek;
	}

	switch(syslang)
	{
	case CFG_LANGUAGE_JP:
		switch(countryinfo[2])
		{
		case ProvinceCode::japan_okinawa: return lang::ryukyuan;
		case ProvinceCode::japan_osaka: return lang::jp_osaka;
		}
		return lang::japanese;
	case CFG_LANGUAGE_FR:
		return countryinfo[3] == CountryCode::canada
			? lang::fr_canada : lang::french;
	case CFG_LANGUAGE_DE: return lang::german;
	case CFG_LANGUAGE_IT: return lang::italian;
	case CFG_LANGUAGE_ES: return lang::spanish;
	case CFG_LANGUAGE_KO: return lang::korean;
	case CFG_LANGUAGE_NL: return lang::dutch;
	case CFG_LANGUAGE_PT: return lang::portuguese;
	case CFG_LANGUAGE_RU: return lang::russian;
	case CFG_LANGUAGE_EN: // fallthrough
	case CFG_LANGUAGE_ZH: // fallthrough
	case CFG_LANGUAGE_TW: // fallthrough
	default: return lang::english;
	}
}

