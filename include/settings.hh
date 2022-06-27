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

#ifndef inc_settings_hh
#define inc_settings_hh

#include <3ds/types.h>
#include "i18n.hh"


enum class LumaLocaleMode {
	disabled  = 0,
	automatic = 1,
	manual    = 2,
};
#define LUMALOCALE_SHIFT 6
enum class SortDirection {
	ascending  = 0,
	descending = 1,
};
#define SORTDIRECTION_SHIFT 8
enum class SortMethod {
	alpha     = 0,
	tid       = 1,
	size      = 2,
	downloads = 3,
	id        = 4,
};
#define SORTMETHOD_SHIFT 9

struct NewSettings {
	u64 flags0;
	lang::type lang;
	u8 max_elogs;
	std::string theme_path;
	u16 proxy_port;
	std::string proxy_host;
	std::string proxy_username;
	std::string proxy_password;
};

enum NewSettings_flags0 {
	FLAG0_RESUME_DOWNLOADS = 0x1,
	FLAG0_LOAD_FREE_SPACE  = 0x2,
	FLAG0_SHOW_BATTERY     = 0x4,
	FLAG0_SHOW_NET         = 0x8,
	FLAG0_BAD_TIME_FORMAT  = 0x10,
	FLAG0_PROGBAR_TOP      = 0x20,
	FLAG0_LUMALOCALE0      = 0x40,
	FLAG0_LUMALOCALE1      = 0x80,
	FLAG0_SORTDIRECTION0   = 0x100,
	FLAG0_SORTMETHOD0      = 0x200,
	FLAG0_SORTMETHOD1      = 0x400,
	FLAG0_SORTMETHOD2      = 0x800,
	FLAG0_SORTMETHOD3      = 0x1000,
	FLAG0_SEARCH_ECONTENT  = 0x2000,
	FLAG0_WARN_NO_BASE     = 0x4000,
	FLAG0_ALLOW_LED        = 0x8000,
};

#define ISET_RESUME_DOWNLOADS (get_nsettings()->flags0 & FLAG0_RESUME_DOWNLOADS)
#define ISET_LOAD_FREE_SPACE (get_nsettings()->flags0 & FLAG0_LOAD_FREE_SPACE)
#define ISET_SHOW_BATTERY (get_nsettings()->flags0 & FLAG0_SHOW_BATTERY)
#define ISET_SHOW_NET (get_nsettings()->flags0 & FLAG0_SHOW_NET)
#define ISET_BAD_TIME_FORMAT (get_nsettings()->flags0 & FLAG0_BAD_TIME_FORMAT)
#define ISET_PROGBAR_TOP (get_nsettings()->flags0 & FLAG0_PROGBAR_TOP)
#define SETTING_LUMALOCALE ((LumaLocaleMode) ((get_nsettings()->flags0 & (FLAG0_LUMALOCALE0 | FLAG0_LUMALOCALE1)) >> LUMALOCALE_SHIFT))
#define SETTING_DEFAULT_SORTDIRECTION ((SortDirection) ((get_nsettings()->flags0 & FLAG0_SORTDIRECTION0) >> SORTDIRECTION_SHIFT))
#define SETTING_DEFAULT_SORTMETHOD ((SortMethod) ((get_nsettings()->flags0 & (FLAG0_SORTMETHOD0 | FLAG0_SORTMETHOD1 | FLAG0_SORTMETHOD2 | FLAG0_SORTMETHOD3)) >> SORTMETHOD_SHIFT))
#define ISET_SEARCH_ECONTENT (get_nsettings()->flags0 & FLAG0_SEARCH_ECONTENT)
#define ISET_WARN_NO_BASE (get_nsettings()->flags0 & FLAG0_WARN_NO_BASE)
#define ISET_ALLOW_LED (get_nsettings()->flags0 & FLAG0_ALLOW_LED)


void reset_settings(bool set_default_lang = false);
SortMethod settings_sort_switch();
NewSettings *get_nsettings();
bool settings_are_ready();
void load_current_theme();
void ensure_settings();
void show_theme_menu();
void cleanup_themes();
void show_settings();
void log_settings();

namespace ui { class Theme; }
std::vector<ui::Theme>& themes();

#endif

