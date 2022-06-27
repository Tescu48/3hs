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

#include "proxy.hh"

#include <stdio.h>

#include "settings.hh"
#include "install.hh"
#include "panic.hh"
#include "i18n.hh"
#include "log.hh"

#define PROXYFILE "/3ds/3hs/proxy"


Result proxy::apply(httpcContext *context)
{
	NewSettings *ns = get_nsettings();
	if(ns->proxy_port != 0)
	{
		return httpcSetProxy(context, ns->proxy_port, ns->proxy_host,
			ns->proxy_username, ns->proxy_password);
	}

	// Not set is a success..
	return 0;
}

static bool is_CRLF(const std::string& buf, std::string::size_type i)
{
	return buf[i == 0 ? 0 : i - 1] == '\r';
}

static void put_semisep(const std::string& buf, std::string& p1, std::string& p2)
{
	std::string::size_type semi = buf.find(":");
	if(semi == std::string::npos) panic(STRING(invalid_proxy));

	p1 = buf.substr(0, semi);
	// +1 to remove semi
	p2 = buf.substr(semi + 1);
}

static bool validate(const proxy::legacy::Params& p)
{
	/* no proxy set is always valid */
	if(p.host == "")
		return true;

	// 0xFFFF = overflow, 0xFFFF+ are invalid ports
	if(p.port == 0 || p.port >= 0xFFFF)
		return false;

	return true;
}


void proxy::legacy::parse(proxy::legacy::Params& p)
{
	FILE *proxyfile = fopen(PROXYFILE, "r");
	if(proxyfile == NULL) return;

	fseek(proxyfile, 0, SEEK_END);
	size_t fsize = ftell(proxyfile);
	fseek(proxyfile, 0, SEEK_SET);

	if(fsize == 0)
	{
		fclose(proxyfile);
		return;
	}

	char *buf = new char[fsize];
	if(fread(buf, 1, fsize, proxyfile) != fsize)
		panic(STRING(invalid_proxy));
	std::string strbuf(buf, fsize);

	fclose(proxyfile);
	delete [] buf;

	std::string::size_type line = strbuf.find("\n");
	if(line == std::string::npos)
		line = strbuf.size() - 1;

	bool crlf = is_CRLF(strbuf, line);
	std::string proxyport = strbuf.substr(0, crlf ? line - 1 : line);

	if(line != strbuf.size() - 1)
	{
		std::string::size_type lineuser = strbuf.find("\n", line);
		if(line == std::string::npos)
			line = strbuf.size() - 1;
		crlf = is_CRLF(strbuf, lineuser);

		// Skip \n
		std::string userpasswd = strbuf.substr(line + 1, crlf ? line - 1 : line);
		if(userpasswd != "")
			put_semisep(userpasswd, p.username, p.password);
	}

	std::string port;
	put_semisep(proxyport, p.host, port);

	p.port = strtoul(port.c_str(), nullptr, 10);
	if(!validate(p)) panic(STRING(invalid_proxy));
}

void proxy::legacy::del()
{
	remove(PROXYFILE);
}

