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

#ifndef inc_installgui_hh
#define inc_installgui_hh

/* previously all of this logic was contained in
 * queue.cc/hh but i moved it here because it
 * was kind of a mess */

#include <string>
#include <3ds.h>

#include "install.hh"
#include "hsapi.hh"

#define THEMES_CATEGORY "themes"


namespace install
{
	namespace gui
	{
		/* the interactive bool disables any "press a to continue" or similar */
		Result net_cia(const std::string& url, u64 tid, bool interactive = true, bool defaultReinstallable = false);
		Result hs_cia(const hsapi::FullTitle& meta, bool interactive = true, bool defaultReinstallable = false);
	}
}

#endif

