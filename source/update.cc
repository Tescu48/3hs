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

#include "update.hh"

#include <ui/confirm.hh>
#include <ui/base.hh>

#include "installgui.hh"
#include "install.hh"
#include "hsapi.hh"
#include "queue.hh"
#include "i18n.hh"
#include "log.hh"


bool update_app()
{
	if(envIsHomebrew())
	{
		ilog("Used 3dsx version. Not checking for updates");
		return false;
	}

	std::string nver;
	if(R_FAILED(hsapi::get_latest_version_string(nver)))
		return false; // We will then error at index

	dlog("Fetched new version %s", nver.c_str());
	if(nver == VERSION)
		return false;

	if(!ui::Confirm::exec(PSTRING(update_to, nver)))
	{
		ilog("User declined update");
		return false;
	}

	u64 tid = 0x0; Result res = 0;
	if(R_FAILED(res = APT_GetProgramID(&tid)))
	{
		elog("APT_GetProgramID(...): %08lX", res);
		return false;
	}

	install::gui::net_cia(hsapi::update_location(nver), tid, false, true);
	return true;
}

