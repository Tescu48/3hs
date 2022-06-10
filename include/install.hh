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

#ifndef inc_game_hh
#define inc_game_hh

#include <functional>
#include <string>
#include <3ds.h>

#include "hsapi.hh"


typedef std::function<void(u64 /* done */, u64 /* total */)> prog_func;
typedef std::function<std::string(Result&)> get_url_func;
static void default_prog_func(u64, u64)
{ }

static inline get_url_func makeurlwrap(const std::string& url)
{
	return [url](Result& r) -> std::string {
		r = 0;
		return url;
	};
}


Result httpcSetProxy(httpcContext *context, u16 port, u32 proxylen, const char *proxy,
	u32 usernamelen, const char *username, u32 passwordlen, const char *password);

// C++ wrapper
static inline Result httpcSetProxy(httpcContext *context, u16 port,
	const std::string& proxy, const std::string& username, const std::string& password)
{
	return httpcSetProxy(
		context, port, proxy.size(), proxy.c_str(),
		username.size(), username.size() == 0 ? nullptr : username.c_str(),
		password.size(), password.size() == 0 ? nullptr : password.c_str()
	);
}

Result install_forwarder(u8 *data, size_t len);

namespace install
{
	Result net_cia(get_url_func get_url, u64 tid, prog_func prog = default_prog_func,
		bool reinstallable = false);
	Result hs_cia(const hsapi::FullTitle& meta, prog_func prog = default_prog_func,
		bool reinstallable = false);
}

#endif

