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

#include "install.hh"
#include "panic.hh"
#include "i18n.hh"
#include "log.hh"

#define PROXYFILE "/3ds/3hs/proxy"

static proxy::Params g_proxy;


static inline bool validate() { return proxy::validate(g_proxy); }
bool proxy::validate(const proxy::Params& p)
{
	/* no proxy set is always valid */
	if(p.host == "")
		return true;

	// 0xFFFF = overflow, 0xFFFF+ are invalid ports
	if(p.port == 0 || p.port >= 0xFFFF)
		return false;

	return true;
}

proxy::Params& proxy::proxy()
{ return g_proxy; }

void proxy::write()
{
	if(!proxy::is_set())
	{
		/* remove in case it existed */
		remove(PROXYFILE);
		return;
	}
	FILE *proxyfile = fopen(PROXYFILE, "w");
	if(proxyfile == nullptr) return;

	std::string data =
		g_proxy.host     + ":" + std::to_string(g_proxy.port) + "\n" +
		g_proxy.username + ":" + g_proxy.password;

	fwrite(data.c_str(), data.size(), 1, proxyfile);
	fclose(proxyfile);
}

void proxy::clear()
{
	g_proxy.password = "";
	g_proxy.username = "";
	g_proxy.host = "";
	g_proxy.port = 0;
}

Result proxy::apply(httpcContext *context)
{
	if(g_proxy.port != 0)
	{
		return httpcSetProxy(context, g_proxy.port, g_proxy.host,
			g_proxy.username, g_proxy.password);
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

void proxy::init()
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
			put_semisep(userpasswd, g_proxy.username, g_proxy.password);
	}

	std::string port;
	put_semisep(proxyport, g_proxy.host, port);

	g_proxy.port = strtoul(port.c_str(), nullptr, 10);
	if(!::validate()) panic(STRING(invalid_proxy));

	ilog("Using proxy: |http(s)://%s:%s@%s:%i/|", g_proxy.username.c_str(), g_proxy.password.c_str(),
		g_proxy.host.c_str(), g_proxy.port);
}

bool proxy::is_set()
{
	return g_proxy.port != 0;
}

