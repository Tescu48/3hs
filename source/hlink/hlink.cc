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

#include "hlink/hlink.hh"
#include "hlink/templ.hh"
#include "hlink/http.hh"

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <poll.h>

#include <string.h>
#include <panic.hh>
#include <errno.h>

#include <unordered_map>

#include "install.hh"
#include "thread.hh"
#include "queue.hh"
#include "hsapi.hh"
#include "i18n.hh"
#include "ctr.hh"
#include "log.hh"

#define SLEEP_AMOUNT 5
#define SLEEP_AMOUNT_S "5"
#define SLEEP_AMOUNT_S_PLUS_ONE "6"

#ifdef RELEASE
	#define TIMER_START(label)
	#define TIMER_END(label)
#else
	#define TIMER_START(label) u64 timer_##label = osGetTime();
	#define TIMER_END(label) dlog("Completing " #label " took %llums", osGetTime() - timer_##label);
#endif

// All network byte order (BE)

typedef struct iTransactionHeader
{
	char magic[hlink::transaction_magic_len];
	hlink::action action;
	uint32_t size;
} __attribute__((__packed__)) iTransactionHeader;

typedef struct iTransactionResponse
{
	char magic[hlink::transaction_magic_len];
	hlink::response resp;
	uint32_t size;
} __attribute__((__packed__)) iTransactionResponse;

static bool g_lock = false; // is a hlink transaction going on?

using trust_store_t = std::unordered_map<in_addr_t, bool>;

static uint64_t ntohll(uint64_t n)
{ return __builtin_bswap64(n); }

static const char *action2string(hlink::action action)
{
#define MKS(n) case hlink::action::n: return #n
	switch(action)
	{
		MKS(add_queue);
		MKS(install_id);
		MKS(install_url);
		MKS(install_data);
		MKS(nothing);
		MKS(launch);
		MKS(sleep);
		default: return STRING(invalid);
	}
#undef MKS
}

static void send_response(int clientfd, hlink::response resp, const std::string& body)
{
	iTransactionResponse respb;
	strncpy(respb.magic, hlink::transaction_magic, hlink::transaction_magic_len + 1);
	respb.size = htonl(body.size());
	respb.resp = resp;

	send(clientfd, &respb, sizeof(iTransactionResponse), 0);
	send(clientfd, body.c_str(), body.size(), 0);
}

static void send_response(int clientfd, hlink::response resp)
{
	iTransactionResponse respb;
	strncpy(respb.magic, hlink::transaction_magic, hlink::transaction_magic_len + 1);
	respb.resp = resp;
	respb.size = 0;

	send(clientfd, &respb, sizeof(iTransactionResponse), 0);
}

static int read_whole_body(int clientfd, std::string& ret, iTransactionHeader header)
{
	ret.reserve(header.size);

	struct pollfd clientpoll;
	clientpoll.fd = clientfd;
	clientpoll.events = POLLIN;

	char buf[1024];
	int seqbad = 0;

	do
	{
		if(seqbad > hlink::max_timeouts) break;
		if(poll(&clientpoll, 1, hlink::poll_timeout_body) == 0)
		{
			++seqbad;
			continue;
		}
		else seqbad = 0;

		ssize_t recvd = recv(clientfd, buf, 1024, 0);
		if(recvd < 0) break;
		ret += std::string(buf, recvd);
	} while(ret.size() != header.size);

	if(ret.size() < header.size) return errno;
	return 0;
}

static void handle_add_queue(int clientfd, iTransactionHeader header)
{
	std::string body;
	if(read_whole_body(clientfd, body, header) != 0)
		return;

	if(body.size() % sizeof(u64) != 0)
		return send_response(clientfd, hlink::response::error, "body.size() % sizeof(u64) != 0");

	for(size_t i = 0; i < body.size() / sizeof(hsapi::hid); ++i)
	{
		hsapi::hid id = ntohll(((const hsapi::hid *) body.data())[i]);

		hsapi::FullTitle meta;
		if(R_FAILED(hsapi::title_meta(meta, id)))
			continue;
		queue_add(meta);
	}

	send_response(clientfd, hlink::response::success);
}

static bool handle_launch(int clientfd, int server, hlink::HTTPServer& serv, iTransactionHeader header, std::function<void(const std::string&)> disp_error)
{
	if(header.size != sizeof(uint64_t))
	{
		send_response(clientfd, hlink::response::error, "body.size() != sizeof(uint64_t)");
		return false;
	}

	std::string body;
	if(read_whole_body(clientfd, body, header) != 0)
		return false;

	uint64_t tid = ntohll(* (uint64_t *) body.data());
	FS_MediaType media = ctr::mediatype_of(tid);

	if(!ctr::title_exists(tid, media))
	{
		disp_error(PSTRING(title_doesnt_exist, ctr::tid_to_str(tid)));
		send_response(clientfd, hlink::response::notfound);
		return false;
	}

	send_response(clientfd, hlink::response::success);

	close(clientfd);
	close(server);
	serv.close();
	g_lock = false;

	APT_PrepareToDoApplicationJump(0, tid, media);

	u8 parambuf[0x300];
	u8 hmacbuf[0x20];
	APT_DoApplicationJump(parambuf, 0x300, hmacbuf);

	return true; // reachable only if APT_DoApplicationJump fails
}

static bool handle_request(int clientfd, int serverfd, hlink::HTTPServer serv, const char *clientaddr,
	std::function<void(const std::string&)> disp_req,
	std::function<void(const std::string&)> disp_error)
{
	iTransactionHeader header;
	ssize_t recvd = recv(clientfd, &header, sizeof(header), 0);
	if(recvd < 0) // error
		goto cleanup;

	header.size = ntohll(header.size);
	if(memcmp(header.magic, hlink::transaction_magic, hlink::transaction_magic_len) != 0)
	{
		send_response(clientfd, hlink::response::error, "invalid magic");
		goto cleanup;
	}

	disp_req(std::string(clientaddr) + "\n" + action2string(header.action));

	switch(header.action)
	{
	case hlink::action::add_queue:
		handle_add_queue(clientfd, header);
		break;
	case hlink::action::install_id:
	case hlink::action::install_url:
	case hlink::action::install_data:
		send_response(clientfd, hlink::response::error, "stub");
		break;
	case hlink::action::nothing:
		send_response(clientfd, hlink::response::accept);
		break;
	case hlink::action::launch:
		if(handle_launch(clientfd, serverfd, serv, header, disp_error))
			return true;
		break;
	case hlink::action::sleep:
		send_response(clientfd, hlink::response::success);
		close(clientfd);
		sleep(SLEEP_AMOUNT);
		goto no_close;
	default:
		send_response(clientfd, hlink::response::error, "invalid action");
		break;
	}

cleanup:
	close(clientfd);
no_close:
	g_lock = false;
	return false;
}

static void finish_ctx(hlink::HTTPRequestContext& ctx, hlink::TemplRen& ren, size_t status)
{
	hlink::TemplRen::result code;
	std::string res;

	std::string src;
	ctx.read_path_content(src);
	if((code = ren.finish(src, res)) != hlink::TemplRen::result::ok)
	{
		ctx.respond(500, "<!DOCTYPE html><html><body>Failed to render due to a template error. Code = " + std::to_string((int) code)
			+ ". If you do not know what this code means <a href=\"/doc/3hs-template-language.html\">try reading the documentation</a>."
			"<p>If your 3DS showed an ARM11 message/crashed this is a bug.</p>"
			"<p><a href=\"/index.html\">Back to home</a></p></body></html>",
			{ { "Content-Type", "text/html" } });
	}
	else ctx.respond(status, res, { { "Content-Type", "text/html" } });
	ctx.close();
}

static bool handle_http_request(hlink::HTTPRequestContext ctx, int serverfd, char *clientaddr, std::function<void(const std::string&)> disp_req,
	std::function<void(const std::string&)> disp_error)
{
	disp_req(std::string(clientaddr) + "\n" + ctx.path);
	((void) disp_error);
	((void) serverfd);

	/* TODO: Fix concurrency issue: hlink+http blocks? after that segv? */
	hlink::HTTPRequestContext::serve_type type = ctx.type();
	switch(type)
	{
	case hlink::HTTPRequestContext::notfound:
		/* 404 not found */
		ctx.serve_404();
		break;
	case hlink::HTTPRequestContext::plain:
		/* plain serve */
		ctx.serve_plain();
		break;
	case hlink::HTTPRequestContext::templ:
	{
		/* template serve */
		hlink::TemplRen ren;
		size_t status = 500;

		ren.use("is-success?()", [&status](hlink::TemplCtx&, hlink::TemplArgs&) -> bool { return status == 200; });
		ren.use_default();

		if(ctx.path == "/add-queue.tpl")
		{
			if(ctx.params.count("id") == 0)
			{
				status = 400;
				ren.use("error-message", "failed to find an \"id\" parameter");
				goto begin_render;
			}
			char *end;
			const char *str = ctx.params["id"].c_str();
			hsapi::hid id = strtoull(str, &end, 10);
			if(str == end) /* failed to parse int */
			{
				status = 400;
				ren.use("error-message", "failed to parse int");
				goto begin_render;
			}

			hsapi::FullTitle meta;
			if(R_FAILED(hsapi::title_meta(meta, id)))
			{
				ren.use("error-message", "failed to add title to queue");
			}
			else
			{
				status = 200;
				queue_add(meta);
				ren.use("title-name", meta.name);
				ren.use("title-hshop-id", std::to_string(meta.id));
			}
		}

		else if(ctx.path == "/launch.tpl")
		{
			if(ctx.params.count("tid") == 0)
			{
				status = 400;
				ren.use("error-message", "failed to find an \"tid\" parameter");
				goto begin_render;
			}
			char *end;
			const char *str = ctx.params["tid"].c_str();
			hsapi::htid tid = strtoull(str, &end, 16);
			if(str == end)
			{
				status = 400;
				ren.use("error-message", "invalid title id");
				goto begin_render;
			}

			FS_MediaType media = ctr::mediatype_of(tid);
			if(!ctr::title_exists(tid, media))
			{
				status = 400;
				ren.use("error-message", "title doesn't exist");
				goto begin_render;
			}

			ctr::TitleSMDH *smdh = ctr::smdh::get(tid);
			if(!smdh)
			{
				status = 500;
				ren.use("error-message", "failed to fetch SMDH");
				goto begin_render;
			}
			ren.use("title-name", ctr::smdh::u16conv(
				ctr::smdh::get_native_title(smdh)->descShort, 0x40));
			delete smdh;

			status = 200;
			finish_ctx(ctx, ren, status);

			close(serverfd);
			ctx.server->close();
			g_lock = false;

			APT_PrepareToDoApplicationJump(0, tid, media);

			u8 parambuf[0x300];
			u8 hmacbuf[0x20];
			APT_DoApplicationJump(parambuf, 0x300, hmacbuf);

			// TODO: Fix SEGV

			return true;
		}

		else if(ctx.path == "/sleep.tpl")
		{
			status = 200;
			ren.use("sleep-amount-2", SLEEP_AMOUNT_S_PLUS_ONE);
			ren.use("sleep-amount", SLEEP_AMOUNT_S);
			finish_ctx(ctx, ren, status);
			sleep(SLEEP_AMOUNT);
			goto end_render_no_close;
		}

		else
		{
			ctx.respond(500, "<!DOCTYPE html><html><body>this shouldn't happen (path=" + ctx.path + ")</body></html>", { { "Content-Type", "text/html" } });
			break;
		}

begin_render:
		finish_ctx(ctx, ren, status);
		goto end_render_no_close;
	}
	}

	ctx.close();
end_render_no_close:
	g_lock = false;
	return false;
}

static bool isallowedtrust(struct sockaddr_in clientaddr, trust_store_t& truststore, std::function<bool(const std::string&)> on_requester)
{
	if(truststore.count(clientaddr.sin_addr.s_addr) != 0 && !truststore[clientaddr.sin_addr.s_addr])
		return false;

	else if(truststore.count(clientaddr.sin_addr.s_addr) == 0)
	{
		const char *clientipaddr = inet_ntoa(clientaddr.sin_addr);
		bool trusted = on_requester(clientipaddr);

		truststore[clientaddr.sin_addr.s_addr] = trusted;
		if(!trusted)
		{
			ilog("Adding %s as untrusted", clientipaddr);
			return false;
		}
		ilog("Adding %s as trusted", clientipaddr);
	}

	return true;
}

static void handle_http(hlink::HTTPServer& serv, trust_store_t& truststore, int serverfd, ctr::reuse_thread<>& handleThread, bool& keepOpenSignal,
	std::function<bool(const std::string&)> on_requester, std::function<void(const std::string&)> disp_req, std::function<void(const std::string&)> disp_error)
{
	hlink::HTTPRequestContext ctx;
	if(serv.make_reqctx(ctx) != 0)
		return;

	if(g_lock)
	{
		ctx.serve_path(429, "/busy.html", { });
		ctx.close();
		return;
	}

	g_lock = true;

	if(!isallowedtrust(ctx.clientaddr, truststore, on_requester))
	{
		ctx.serve_403();
		ctx.close();
		return;
	}

	/* we need to make a copy of ctx because else stack corruption */
	handleThread.run([&ctx, serverfd, disp_req, disp_error, &keepOpenSignal]() -> void {
		TIMER_START(http_request)
		if(handle_http_request(ctx, serverfd, inet_ntoa(ctx.clientaddr.sin_addr), disp_req, disp_error))
			keepOpenSignal = false;
		TIMER_END(http_request)
	});
}

static void handle_hlink(int serverfd, trust_store_t& truststore, hlink::HTTPServer& serv, ctr::reuse_thread<>& handleThread, bool& keepOpenSignal,
	std::function<void(const std::string&)> disp_error, std::function<void(const std::string&)> disp_req, std::function<bool(const std::string&)> on_requester)
{
	struct sockaddr_in clientaddr;
	socklen_t clientaddrlen = sizeof(clientaddr);
	memset(&clientaddr, 0x0, sizeof(clientaddr));

	// NOW we need to accept() our connection
	int clientfd = accept(serverfd, (struct sockaddr *) &clientaddr, &clientaddrlen);

	if(clientfd < 0)
	{
		disp_error("accept(): " + std::string(strerror(errno)));
		return;
	}

	if(g_lock) // we can't have more than 2 connections, so we just say bAi
	{
		send_response(clientfd, hlink::response::busy);
		close(clientfd);
		return;
	}

	if(!isallowedtrust(clientaddr, truststore, on_requester))
	{
		send_response(clientfd, hlink::response::untrusted);
		close(clientfd);
		return;
	}

	g_lock = true;
	handleThread.run([clientfd, serverfd, &serv, clientaddr, disp_req, disp_error, &keepOpenSignal]() -> void {
		TIMER_START(hlink_request)
		if(handle_request(clientfd, serverfd, serv, inet_ntoa(clientaddr.sin_addr), disp_req, disp_error))
			keepOpenSignal = false;
		TIMER_END(hlink_request)
	});
}

void hlink::create_server(
		std::function<bool(const std::string&)> on_requester,
		std::function<void(const std::string&)> disp_error,
		std::function<void(const std::string&)> on_server_create,
		std::function<bool()> on_poll_exit,
		std::function<void(const std::string&)> disp_req
	)
{
	int serverfd = -1;
	if((serverfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
	{
		disp_error("socket(): " + std::string(strerror(errno)));
		return;
	}

	struct sockaddr_in servaddr;
	memset(&servaddr, 0x0, sizeof(servaddr));
	servaddr.sin_family = AF_INET; // IPv4 only (3ds doesn't support IPv6)
	servaddr.sin_addr.s_addr = gethostid();
	servaddr.sin_port = htons(hlink::port);

	if(bind(serverfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0)
	{
		disp_error("bind(): " + std::string(strerror(errno)));
		close(serverfd);
		return;
	}

	if(listen(serverfd, hlink::backlog) < 0)
	{
		disp_error("listen(): " + std::string(strerror(errno)));
		close(serverfd);
		return;
	}

	hlink::HTTPServer httpserv;
	int res;
	if((res = httpserv.make_fd()) != 0)
	{
		disp_error("httpserv.make_fd(): " + hlink::HTTPServer::errmsg(res));
		close(serverfd);
		return;
	}

	// Now we keep polling
	constexpr size_t polls_len = 2;
	struct pollfd serverpolls[polls_len];
	serverpolls[0].fd = serverfd;
	serverpolls[0].events = POLLIN;
	serverpolls[1].fd = httpserv.fd;
	serverpolls[1].events = POLLIN;

	ctr::reuse_thread<> handleThread;
	bool keepOpenSignal = true;

	trust_store_t truststore;

begin_loop:
	/* if g_lock is set that thread may want to write already */
	bool haveDispedServ = false;
	if(!g_lock)
	{
		const char *ipaddr = inet_ntoa(servaddr.sin_addr);
		on_server_create(ipaddr); // We might need to redraw the screen
		haveDispedServ = true;
	}

	while(keepOpenSignal && (g_lock || on_poll_exit()))
	{
		if(!g_lock && !haveDispedServ)
		{
			const char *ipaddr = inet_ntoa(servaddr.sin_addr);
			on_server_create(ipaddr); // We might need to redraw the screen
			haveDispedServ = true;
		}

		if(poll(serverpolls, polls_len, 1000) == 0)
			continue; // no events; we do nothing
		haveDispedServ = false;
		for(size_t i = 0; i < polls_len; ++i)
		{
			if(!(serverpolls[i].revents & POLLIN))
				continue; /* this one doesn't have anything */
			if(serverpolls[i].fd == httpserv.fd)
				handle_http(httpserv, truststore, serverfd, handleThread, keepOpenSignal, on_requester, disp_req, disp_error);
			else if(serverpolls[i].fd == serverfd)
				handle_hlink(serverfd, truststore, httpserv, handleThread, keepOpenSignal, disp_error, disp_req, on_requester);
			goto begin_loop; /* if we made it here we got a poll that needs updating */
		}
	}

	httpserv.close();
	close(serverfd);
	g_lock = false;
	return;
}

