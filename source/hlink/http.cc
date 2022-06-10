// vim: foldmethod=marker
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

#include "panic.hh"
#include "log.hh"

#include "hlink/hlink.hh"
#include "hlink/http.hh"

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <poll.h>

/* {{{1 Default status pages */
void hlink::HTTPRequestContext::serve_400()
{
	this->respond(400,
		"<!DOCTYPE html>"
		"<html>"
			"<head>"
				"<meta charset=\"utf-8\"/>"
				"<title>hLink - Bad Request</title>"
			"</head>"
			"<body>"
				"<center>"
					"<h1>400 - Bad Request</h1>"
					"<hr/>"
					"<p>Your browser (?) sent a request the server couldn't understand</p>"
				"</center>"
			"</body>"
		"</html>"
		"</html>"
	, { });
}

void hlink::HTTPRequestContext::serve_403()
{
	this->respond(403,
		"<!DOCTYPE html>"
		"<html>"
			"<head>"
				"<meta charset=\"utf-8\"/>"
				"<title>hLink - Forbidden</title>"
			"</head>"
			"<body>"
				"<center>"
					"<h1>403 - Forbidden</h1>"
					"<hr/>"
					"<p>The requested resource is forbidden</p>"
				"</center>"
			"</body>"
		"</html>"
	, { });
}

void hlink::HTTPRequestContext::serve_404() { this->serve_404(this->path); }
void hlink::HTTPRequestContext::serve_404(const std::string& fname)
{
	this->respond(404,
		"<!DOCTYPE html>"
		"<html>"
			"<head>"
				"<meta charset=\"utf-8\"/>"
				"<title>hLink - Not Found</title>"
			"</head>"
			"<body>"
				"<center>"
					"<h1>404 - Not Found</h1>"
					"<hr/>"
					"<p>The requested resource "+fname+" could not be found</p>"
				"</center>"
			"</body>"
		"</html>"
		"</html>"
	, { });
}

void hlink::HTTPRequestContext::serve_500()
{
	this->respond(500,
		"<!DOCTYPE html>"
		"<html>"
			"<head>"
				"<meta charset=\"utf-8\"/>"
				"<title>hLink - Internal Server Error</title>"
			"</head>"
			"<body>"
				"<center>"
					"<h1>500 - Internal Server Error</h1>"
					"<hr/>"
					"<p>The server did something wrong. (maybe the 3hs logs tell anything?)</p>"
				"</center>"
			"</body>"
		"</html>"
		"</html>"
	, { });
}
/* 1}}} */

static std::unordered_map<std::string, std::string> file_cache;


int hlink::HTTPServer::make_fd()
{
	panic_assert(this->fd == -1, "tried to re-create bound socket");

	int serverfd = -1;
	if((serverfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		return errno;

	struct sockaddr_in servaddr;
	memset(&servaddr, 0x0, sizeof(servaddr));
	servaddr.sin_family = AF_INET; // IPv4 only (3ds doesn't support IPv6)
	servaddr.sin_addr.s_addr = gethostid();
	servaddr.sin_port = htons(8000); /* can't bind on sub 1000 so no port 80 */

	if(bind(serverfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0)
	{
		::close(serverfd);
		return errno;
	}

	if(listen(serverfd, hlink::backlog) < 0)
	{
		::close(serverfd);
		return errno;
	}

	this->root = "romfs:/public"; /* default root dir is romfs:/public/ */
	this->fd = serverfd;
	return 0;
}

std::string hlink::HTTPServer::errmsg(int code)
{
	return strerror(code);
}

void hlink::HTTPServer::close()
{
	panic_assert(this->fd != -1, "tried to close unbound socket");
	::close(this->fd);
	this->fd = -1;
}

using hlink::HTTPRequestContext;

void hlink::HTTPRequestContext::close()
{
	panic_assert(this->fd != -1, "tried to close unbound context");
	::close(this->fd);
	this->fd = -1;
}

void hlink::HTTPRequestContext::redirect(const std::string& location)
{
	this->respond(303, { { "Location", location } });
}

void hlink::HTTPRequestContext::respond(int status, const std::string& data, HTTPHeaders headers)
{
	headers["Content-Length"] = std::to_string(data.size());
	this->respond(status, headers);
	this->send(data);
}

void hlink::HTTPRequestContext::respond_chunked(int status, HTTPHeaders headers)
{
	headers["Transfer-Encoding"] = "chunked";
	this->respond(status, headers);
}

void hlink::HTTPRequestContext::respond(int status, const HTTPHeaders& headers)
{
	panic_assert(this->fd != -1, "tried to respond to unbound context");
	const char *msg = nullptr;
	switch(status)
	{
	case 100: msg = "Continue"; break;
	case 101: msg = "Switching Protocols"; break;
	case 102: panic("102 Processing -- is invalid");
	case 103: msg = "Early Hints"; break;
	case 200: msg = "OK"; break;
	case 201: msg = "Created"; break;
	case 202: msg = "Accepted"; break;
	case 203: msg = "Non-Authoritative Information"; break;
	case 204: msg = "No Content"; break;
	case 205: msg = "Reset Content"; break;
	case 206: msg = "Partial Content"; break;
	case 207: panic("207 Multi-Status -- is invalid");
	case 208: panic("208 Already Reported -- is invalid");
	case 226: panic("226 IM Used -- is invalid");
	case 300: msg = "Multiple Choice"; break;
	case 301: msg = "Moved Permanently"; break;
	case 302: msg = "Found"; break;
	case 303: msg = "See Other"; break;
	case 304: msg = "Not Modified"; break;
	case 305: msg = "Use Proxy"; break;
	case 307: msg = "Temporary Redirect"; break;
	case 308: msg = "Permanent Redirect"; break;
	case 400: msg = "Bad Request"; break;
	case 401: msg = "Unauthorized"; break;
	case 402: panic("402 Payment Required -- is invalid");
	case 403: msg = "Forbidden"; break;
	case 404: msg = "Not Found"; break;
	case 405: msg = "Method Not Allowed"; break;
	case 406: msg = "Not Acceptable"; break;
	case 407: msg = "Proxy Authentication Required"; break;
	case 408: msg = "Request Timeout"; break;
	case 409: msg = "Conflict"; break;
	case 410: msg = "Gone"; break;
	case 411: msg = "Length Required"; break;
	case 412: msg = "Precondition Failed"; break;
	case 413: msg = "Payload Too Large"; break;
	case 414: msg = "URI Too Long"; break;
	case 415: msg = "Unsupported Media Type"; break;
	case 416: msg = "Range Not Satisfiable"; break;
	case 417: msg = "Expectation Failed"; break;
	case 418: msg = "I'm a teapot"; break;
	case 421: msg = "Misdirected Request"; break;
	case 422: panic("422 Unprocessable Entity -- is invalid");
	case 423: panic("423 Locked -- is invalid");
	case 424: panic("424 Failed Dependency -- is invalid");
	case 425: msg = "Too Early"; break;
	case 426: msg = "Upgrade Required"; break;
	case 428: msg = "Precondition Required"; break;
	case 429: msg = "Too Many Requests"; break;
	case 431: msg = "Request Header Fields Too Large"; break;
	case 451: msg = "Unavailable For Legal Reasons"; break;
	case 500: msg = "Internal Server Error"; break;
	case 501: msg = "Not Implemented"; break;
	case 502: msg = "Bad Gateway"; break;
	case 503: msg = "Service Unavailable"; break;
	case 504: msg = "Gateway Timeout"; break;
	case 505: msg = "HTTP Version Not Supported"; break;
	case 506: msg = "Variant Also Negotiates"; break;
	case 507: panic("507 Insufficient Storage -- is invalid");
	case 508: panic("508 Loop Detected -- is invalid");
	case 510: msg = "Not Extended"; break;
	case 511: msg = "Network Authentication Required"; break;
	default: panic(std::to_string(status) + " (unknown) -- is invalid");
	}

	std::string body = "HTTP/1.1 " + std::to_string(status) + " " + msg + "\r\n";
	using Iterator = hlink::HTTPHeaders::const_iterator;
	for(Iterator it = headers.begin(); it != headers.end(); ++it)
		body += it->first + ": " + it->second + "\r\n";
	body += "\r\n";

	this->send(body);
}

void hlink::HTTPRequestContext::send_chunk(const std::string& data)
{
	panic_assert(this->fd != -1, "tried to send chunk to unbound context");
	char hexbuf[17]; /* max is FFFFFFFFFFFFFFFF which is 16 chars */
	snprintf(hexbuf, sizeof(hexbuf), "%16X", data.size());
	std::string rdata = std::string(hexbuf) + "\r\n" + data;
	this->send(rdata);
}

void hlink::HTTPRequestContext::send(const std::string& data)
{
	panic_assert(this->fd != -1, "tried to send to unbound context");
	::send(this->fd, data.c_str(), data.size(), 0);
}

void hlink::HTTPRequestContext::serve_file(int status, const std::string& fname, HTTPHeaders headers)
{
	FILE *f = fopen(fname.c_str(), "r");
	if(f == nullptr)
	{
		switch(errno)
		{
		case ENOENT: /* 404 not found */
			this->serve_404(fname);
			return;
		default: /* 500 internal server error */
			elog("Failed to open file %s, errno=%i: %s", fname.c_str(), errno, strerror(errno));
			this->serve_500();
			return;
		}
	}

	size_t total, sent = 0;

	fseek(f, 0, SEEK_END);
	total = ftell(f);
	headers["Content-Length"] = std::to_string(total);
	fseek(f, 0, SEEK_SET);
	this->respond(status, headers);

	char buf[4098];
	while(sent != total)
	{
		size_t toSend = fread(buf, 1, sizeof(buf), f);
		::send(this->fd, buf, toSend, 0);
		sent += toSend;
	}

	fclose(f);
}

void hlink::HTTPRequestContext::serve_path(int status, const std::string& path, HTTPHeaders headers)
{
	this->path = path; /* sneaky */
	std::string content;
	this->read_path_content(content);
	this->respond(status, content, headers);
}

void hlink::HTTPRequestContext::read_path_content(std::string& buf)
{
	auto it = file_cache.find(this->path);
	if(it != file_cache.end())
	{
		buf = it->second;
		return;
	}

	FILE *f = fopen((this->server->root + this->path).c_str(), "r");
	if(f == nullptr) return;

	size_t total, i = 0;
	fseek(f, 0, SEEK_END);
	total = ftell(f);
	fseek(f, 0, SEEK_SET);

	char cbuf[4098];
	while(i != total)
	{
		size_t r = fread(cbuf, 1, sizeof(buf), f);
		buf += std::string(cbuf, r);
		i += r;
	}

	fclose(f);
	file_cache[this->path] = buf;
}

void hlink::HTTPRequestContext::serve_plain()
{
	this->serve_file(200, this->server->root + this->path, { });
}

static bool isdir(const std::string& str)
{
	struct stat st;
	if(stat(str.c_str(), &st) != 0)
		return false;
	return S_ISDIR(st.st_mode);
}

HTTPRequestContext::serve_type hlink::HTTPRequestContext::type()
{
#define ROOT this->server->root
	bool ok = false;
	if(isdir(ROOT+this->path))
	{
		if(this->path.back() != '/') this->path.push_back('/');
		else if(access((ROOT+this->path + "index.tpl").c_str(), R_OK) == 0)
		{ this->path += "index.tpl"; ok = true; }
		else if(access((ROOT+this->path + "index.html").c_str(), R_OK) == 0)
		{ this->path += "index.html"; ok = true; }
	}
	else if(access((ROOT+this->path).c_str(), R_OK) == 0)
		ok = true;
	else if(access((ROOT+this->path + ".tpl").c_str(), R_OK) == 0)
	{ this->path += ".tpl"; ok = true; }
	else if(access((ROOT+this->path + ".html").c_str(), R_OK) == 0)
	{ this->path += ".html"; ok = true; }

	if(ok)
	{
		return this->path.rfind(".tpl") == std::string::npos
			? HTTPRequestContext::plain : HTTPRequestContext::templ;
	}
	return HTTPRequestContext::notfound;
#undef ROOT
}

static void realize_offset(HTTPRequestContext& ctx, size_t offset)
{
	memmove(ctx.buf, ctx.buf + offset, ctx.buflen - offset);
	ctx.buflen -= offset;
}

static char *bufstrstr(char *buf, size_t len, const char *str)
{
	size_t lstr = strlen(str);
	if(len >= lstr) len -= lstr - 1;
	else return nullptr;

	for(size_t i = 0; i < len; ++i)
	{
		if(strncmp(buf + i, str, lstr) == 0)
			return buf + i;
	}

	return nullptr;
}

static char *bufstrchr(char *buf, size_t len, char chr)
{
	for(size_t i = 0; i < len; ++i)
		if(buf[i] == chr) return buf + i;
	return nullptr;
}

static char *bufstrnl(char *buf, size_t len)
{
	char *ret;
	if((ret = bufstrstr(buf, len, "\r\n")) != nullptr)
		return ret;
	return bufstrchr(buf, len, '\n');
}

static ssize_t bufstrchrof(char *buf, size_t len, char chr)
{
	char *r = bufstrchr(buf, len, chr);
	if(r == nullptr) return -1;
	return r - buf;
}

static ssize_t bufstrnlof(char *buf, size_t len)
{
	char *r = bufstrnl(buf, len);
	if(r == nullptr) return -1;
	return r - buf;
}

static int bufnllen(char *buf, size_t len)
{
	if(len > 0)
	{
		if(len > 1)
		{
			// check for \r\n
			if(*buf == '\r' && *(buf + 1) == '\n')
				return 2;
		}
		return *buf == '\n' ? 1 : 0;
	}
	return 0;
}

static void normalize_path(std::string& path)
{
	std::string::size_type pos;
	while((pos = path.find("//")) != std::string::npos)
		path.erase(path.begin() + pos);
}

/* returns 0 on finish, 1 if we need more data, 2 = parse success, -1 on error */
static int parse_header(HTTPRequestContext& ctx)
{
	ssize_t of = bufstrnlof(ctx.buf, ctx.buflen);
	if(of == -1) return 1; /* need more data */
	if(of == 0) /* we ended on (\r)\n! */
	{
		realize_offset(ctx, bufnllen(ctx.buf, ctx.buflen));
		return 0; /* finished */
	}

	/* we need to parse an actual header ... */
	char nlchr = ctx.buf[of + 1];
	ctx.buf[of + 1] = '\0'; /* override the \r or \n with a \0 to make a real string */

	char *namesep = strstr(ctx.buf, ": ");
	if(namesep == nullptr) return -1; /* error */
	size_t len = namesep - ctx.buf;
	if(len == 0) return -1; /* error: ": \r\n" */

	std::string name  = std::string(ctx.buf, len);
	std::string value = std::string(ctx.buf + len + 2, of - len - 2);
	lower(name);
	ctx.headers[name] = value;

	vlog("(HTTP) Parsed header |%s|: |%s|", name.c_str(), value.c_str());

	ctx.buf[of + 1] = nlchr;
	realize_offset(ctx, of + bufnllen(ctx.buf + of, ctx.buflen - of));
	return 2; /* success! */
}

static inline void between(std::string& dst, const std::string& src,
	size_t begin, size_t end)
{
	dst = src.substr(begin, end - begin);
}

/* removes ?.*$ from `path' and puts the parsed params in `params' */
static void parse_url_params(std::string& path, hlink::HTTPParameters& params)
{
	std::string::size_type question = path.find("?");
	if(question == std::string::npos) return; /* no params to parse */

	std::string::size_type begin = question + 1; /* ?|a|b=cd&ef=gh */
	do {
		std::string::size_type eq = path.find("=", begin); /* ?ab|=|cd&ef=gh */
		std::string::size_type and_s = path.find("&", begin); /* ?ab=cd|&|ef=gh */
		std::string name;
		between(name, path, begin, eq == std::string::npos ? and_s : eq);
		std::string val = "";
		if(eq != std::string::npos)
			between(val, path, eq + 1, and_s);
		vlog("(HTTP) Parsed parameter |%s|: |%s|", name.c_str(), val.c_str());
		params[name] = val;
		if(and_s == std::string::npos)
			break; /* no more left */
		begin = and_s + 1;
	} while(true);

	path.erase(question, std::string::npos);
}

int hlink::HTTPServer::make_reqctx(HTTPRequestContext& ctx)
{
	panic_assert(this->fd != -1, "Tried to make a request on an unbound context");
	ctx.server = this;
	ctx.iseof = false;
	ctx.buflen = 0;

	ssize_t len;
	memset(&ctx.clientaddr, 0x0, sizeof(ctx.clientaddr));
	socklen_t clientaddr_len = sizeof(ctx.clientaddr);
	if((ctx.fd = accept(this->fd, (struct sockaddr *) &ctx.clientaddr, &clientaddr_len)) < 0)
		return errno;

	if((len = recv(ctx.fd, ctx.buf, sizeof(ctx.buf), 0)) < 0)
	{ ctx.serve_400(); ctx.close(); return errno; }
	ctx.buflen = len;

	ssize_t of;
	if((of = bufstrchrof(ctx.buf, ctx.buflen, ' ')) < 1)
	{ ctx.serve_400(); ctx.close(); return -1; }

	ctx.method = std::string(ctx.buf, of);
	realize_offset(ctx, of + 1);
	lower(ctx.method);

	if((of = bufstrchrof(ctx.buf, ctx.buflen, ' ')) < 1)
	{ ctx.serve_400(); ctx.close(); return -1; }

	ctx.path = std::string(ctx.buf, of);
	realize_offset(ctx, of + 1);
	normalize_path(ctx.path);
	parse_url_params(ctx.path, ctx.params);

	if((of = bufstrnlof(ctx.buf, ctx.buflen)) < 1)
	{ ctx.serve_400(); ctx.close(); return -1; }
	/* we don't care about the version */
	realize_offset(ctx, of + bufnllen(ctx.buf + of, ctx.buflen - of));

	vlog("(HTTP) Parsed request line; method=%s,path=%s", ctx.method.c_str(), ctx.path.c_str());

	/* parse headers */
	int res;
	while(true)
	{
		while((res = parse_header(ctx)) == 2) continue; /* while parse_success */
		if(ctx.iseof) res = -1;
		if(res != 1) break; /* if not need_more_data */ 

		/* refill buffer */
		if((len = recv(ctx.fd, ctx.buf + ctx.buflen, sizeof(ctx.buf) - ctx.buflen, 0)) < 0)
		{ ctx.serve_400(); ctx.close(); return errno; }
		ctx.iseof = len == 0;
		ctx.buflen += len;
	}

	if(res != 0)
	{
		ctx.serve_400();
		ctx.close();
	}
	return res; /* eof before parse_header() = bad */
}

