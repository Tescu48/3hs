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

#ifndef inc_hlink_hh
#define inc_hlink_hh

#include <functional>
#include <string>

#include <stddef.h>
#include <stdint.h>


namespace hlink
{
	constexpr char transaction_magic[] = "HLT";
	constexpr size_t transaction_magic_len = 3;
	constexpr int poll_timeout_body = 1000;
	constexpr int max_timeouts = 3;
	constexpr int port = 37283;
	constexpr int backlog = 2;

	enum class action : uint8_t
	{
		add_queue    = 0,
		install_id   = 1,
		install_url  = 2,
		install_data = 3,
		nothing      = 4,
		launch       = 5,
		sleep        = 6,
	};

	enum class response : uint8_t
	{
		accept       = 0,
		busy         = 1,
		untrusted    = 2,
		error        = 3,
		success      = 4,
		notfound     = 5,
	};

	void create_server(
		std::function<bool(const std::string&)> on_requester,
		std::function<void(const std::string&)> disp_error,
		std::function<void(const std::string&)> on_server_create,
		std::function<bool()> on_poll_exit,
		std::function<void(const std::string&)> disp_req
	);
}

#endif

