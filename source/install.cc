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

#include "settings.hh"
#include "install.hh"
#include "thread.hh"
#include "update.hh" /* includes net constants */
#include "error.hh"
#include "proxy.hh"
#include "panic.hh"
#include "ctr.hh"
#include "log.hh"

#include <3ds.h>

#define BUFSIZE 0x80000

enum class ITC // inter thread communication
{
	normal, exit, timeoutscr
};

enum class ActionType {
	install,
	download,
};

using content_type = std::basic_string<u8>;

typedef struct cia_net_data
{
	union {
		// We write to this handle
		Handle cia;
		// Or write to this basic_string
		content_type *content;
	};
	// At what index are we writing __the cia__ now?
	u32 index = 0;
	// Total cia size
	u32 totalSize = 0;
	// Messages back and forth the UI/Install thread
	ITC itc = ITC::normal;
	// Buffer. allocated on heap for extra storage
	u8 *buffer = nullptr;
	// amount of data in the buffer
	u32 bufferSize = 0;
	// Tells second thread to wake up
	Handle eventHandle;
	// Type of action
	ActionType type;
} cia_net_data;


static Result i_install_net_cia(std::string url, cia_net_data *data, size_t from, httpcContext *pctx)
{
	u32 status = 0, dled = 0, remaining, dlnext, written, rdl = 0;
	Result res = 0;
#define CHECKRET(expr) if(R_FAILED(res = ( expr ) )) goto err

	/* configure */
	if(R_FAILED(res = httpcOpenContext(pctx, HTTPC_METHOD_GET, url.c_str(), 0)))
		return res;
	CHECKRET(httpcSetSSLOpt(pctx, SSLCOPT_DisableVerify));
	CHECKRET(httpcSetKeepAlive(pctx, HTTPC_KEEPALIVE_ENABLED));
	CHECKRET(httpcAddRequestHeaderField(pctx, "Connection", "Keep-Alive"));
	CHECKRET(httpcAddRequestHeaderField(pctx, "User-Agent", USER_AGENT));
	CHECKRET(proxy::apply(pctx));

	if(from != 0)
	{
		CHECKRET(httpcAddRequestHeaderField(pctx, "Range", ("bytes=" + std::to_string(from) + "-").c_str()));
	}

	CHECKRET(httpcBeginRequest(pctx));

	CHECKRET(httpcGetResponseStatusCode(pctx, &status));
	vlog("Download status code: %lu", status);

	// Do we want to redirect?
	if(status / 100 == 3)
	{
		char newurl[2048];
		CHECKRET(httpcGetResponseHeader(pctx, "location", newurl, sizeof(newurl)));
		newurl[sizeof(newurl) - 1] = '\0';
		std::string redir(newurl);

		vlog("Redirected to %s", redir.c_str());
		httpcCancelConnection(pctx);
		httpcCloseContext(pctx);
		return i_install_net_cia(redir, data, from, pctx);
	}

	// Are we resuming and does the server support range?
	if(from != 0)
	{
		/* fuck me
		 * before this if used to be && meaning if from != 0 it would always fail
		 * reason: status != 200 check was added later than range support */
		if(status != 206)
		{
			elog("expected 206 but got %lu", status);
			res = APPERR_NORANGE;
			goto err;
		}
	}
	// Bad status code
	else if(status != 200)
	{
		elog("HTTP status was NOT 200 but instead %lu", status);
		res = APPERR_NON200;
		goto err;
	}

	if(from == 0)
		CHECKRET(httpcGetDownloadSizeState(pctx, nullptr, &data->totalSize));
	/* else data->totalSize is already known */
	if(data->totalSize == 0)
	{
#ifndef RELEASE
		char buffer[0x6000];
		u32 total;
		httpcDownloadData(pctx, (u8 *) buffer, sizeof(buffer), &total);
		buffer[total] = '\0';
		dlog("API data on '%s' (probably json):\n%s", url.c_str(), buffer);
#endif
		res = APPERR_NOSIZE;
		goto err;
	}
	svcSignalEvent(data->eventHandle);

	// Install.
	panic_assert(data->totalSize > from, "invalid download start position");
	remaining = data->totalSize - from - data->bufferSize;
	dlnext = remaining < BUFSIZE ? remaining : BUFSIZE;
	if(dlnext == 0) goto immediate_install;
	written = 0;

	while(data->index != data->totalSize)
	{
		dlog("receiving data, dlnext=%lu, progress is (session:%lu)%lu/%lu", dlnext, dled, data->index, data->totalSize);
		panic_if(dlnext > BUFSIZE, "dlnext is invalid");
		rdl = dled;
		res = httpcReceiveDataTimeout(pctx, &data->buffer[data->bufferSize], dlnext, 30000000000L);
immediate_install:
		vlog("httpcReceiveDataTimeout(): 0x%08lX", res);
		if((R_FAILED(res) && res != (Result) HTTPC_RESULTCODE_DOWNLOADPENDING) || R_FAILED(res = httpcGetDownloadSizeState(pctx, &dled, nullptr)))
		{
			elog("aborted http connection due to error: %08lX.", res);
			goto err;
		}
		rdl = dled - rdl;
		if(rdl != dlnext)
		{
			data->bufferSize = rdl;
			dlnext -= data->bufferSize;
			ilog("only a chunk was downloaded, retrying");
			continue;
		}
		data->bufferSize = 0;

#define CHK_EXIT \
		if(data->itc == ITC::exit) \
		{ \
			dlog("aborted http connection due to ITC::exit"); \
			res = APPERR_CANCELLED; \
			goto err; \
		}
		CHK_EXIT
		dlog("Writing to cia handle, size=%lu,index=%lu,totalSize=%lu", dlnext, data->index, data->totalSize);
		if(data->type == ActionType::install)
		{
			/* we don't need to add the FS_WRITE_FLUSH flag because AM just ignores write flags... */
			if(R_FAILED(res = FSFILE_Write(data->cia, &written, data->index, data->buffer, dlnext, 0)))
				goto err;
		}
		else
			data->content->append(data->buffer, dlnext);
		remaining = data->totalSize - dled - from;
		data->index += dlnext;
		CHK_EXIT
#undef CHK_EXIT

		dlnext = remaining < BUFSIZE ? remaining : BUFSIZE;
		svcSignalEvent(data->eventHandle);
	}

out:
	httpcCancelConnection(pctx);
	httpcCloseContext(pctx);
	if(data->index == data->totalSize)
		data->itc = ITC::exit;
	svcSignalEvent(data->eventHandle);
	return res;
#undef CHECKRET
err:
	goto out;
}

static void i_install_loop_thread_cb(Result& res, get_url_func get_url, cia_net_data& data, httpcContext& hctx)
{
	std::string url;

	if(!ISET_RESUME_DOWNLOADS)
	{
		if((url = get_url(res)) == "")
		{
			elog("failed to fetch url: %08lX", res);
			goto out;
		}
		res = i_install_net_cia(url, &data, 0, &hctx);
		goto out;
	}

	// install loop
	while(data.itc != ITC::exit)
	{
		url = get_url(res);
		if(R_SUCCEEDED(res))
			res = i_install_net_cia(url, &data, data.index + data.bufferSize, &hctx);

		if(R_FAILED(res)) { elog("Failed in install loop. ErrCode=0x%08lX", res); }
		if(R_MODULE(res) == RM_HTTP)
		{
			ilog("timeout.? ui::timeoutscreen() is up.");
			// Does the user want to stop?

			data.itc = ITC::timeoutscr;
			svcSignalEvent(data.eventHandle);
			/* other thread wakes up */
			svcWaitSynchronization(data.eventHandle, U64_MAX);
			data.itc = ITC::normal;
			if(res == APPERR_CANCELLED)
				break; /* finished */
			continue;
		}

		// Installation was a fail or succeeded, so we stop
		break;
	}
out:
	data.itc = ITC::exit;
	svcSignalEvent(data.eventHandle);
}

static Result i_install_resume_loop(get_url_func get_url, prog_func prog, cia_net_data *data)
{
	Result res;
	data->buffer = new u8[BUFSIZE];

	if(R_FAILED(res = svcCreateEvent(&data->eventHandle, RESET_ONESHOT)))
		return res;

	httpcContext hctx;

	// Install thread
	ctr::thread<Result&, get_url_func, cia_net_data&, httpcContext&> th
		(i_install_loop_thread_cb, res, get_url, *data, hctx);

	Handle handles[6];
	handles[0] = data->eventHandle;
	extern Handle hidEvents[5];
	memcpy(&handles[1], hidEvents, sizeof(hidEvents));
	s32 outhandle;

	// UI Loop
	while(data->itc != ITC::exit)
	{
		svcWaitSynchronizationN(&outhandle, handles, 6, false, U64_MAX);
		/* other thread signals state update */
		if(outhandle == 0)
		{
			if(data->itc == ITC::exit)
				break;
			/* we want to actually cancel the handle so lets not exit() here already */
			if(!aptMainLoop()) break;
			if(data->itc == ITC::timeoutscr)
			{
				/* we need to display a timeout screen */
				bool wantsQuit = ui::timeoutscreen(PSTRING(netcon_lost, "0x" + pad8code(res)), 10);
				if(wantsQuit) res = APPERR_CANCELLED;
				prog(data->index, data->totalSize);
				/* signal that other thread can wake up again */
				svcSignalEvent(data->eventHandle);
				if(wantsQuit) break;
			}
			else
				prog(data->index, data->totalSize);
		}
		/* hid event */
		else
		{
			hidScanInput();
			if(!aptMainLoop() || (hidKeysDown() & (KEY_B | KEY_START)))
			{
				res = APPERR_CANCELLED;
				break;
			}
		}
	}

	data->itc = ITC::exit;
	th.join();

	svcCloseHandle(data->eventHandle);
	delete [] data->buffer;
	return res;
}

static const char *dest2str(FS_MediaType dest)
{
	switch(dest)
	{
	case MEDIATYPE_GAME_CARD: return "GAME CARD";
	case MEDIATYPE_NAND: return "NAND";
	case MEDIATYPE_SD: return "SD";
	}
	return "INVALID VALUE";
}

static Result net_cia_impl(get_url_func get_url, hsapi::htid tid, bool reinstallable, prog_func prog, cia_net_data *data)
{
	FS_MediaType dest = ctr::mediatype_of(tid);
	Result ret;
	if(data->type == ActionType::install)
	{
		if(reinstallable)
		{
			// Ask ninty why this stupid restriction is in place
			// Basically reinstalling the CURRENT cia requires you
			// To NOT delete the cia but instead have a higher version
			// and just install like normal
			FS_MediaType selfmt;
			u64 selftid;
			if(R_FAILED(ret = APT_GetAppletInfo((NS_APPID) envGetAptAppId(), &selftid, (u8 *) &selfmt, nullptr, nullptr, nullptr)))
				return ret;
			if(envIsHomebrew() || selftid != tid || dest != selfmt)
			{
				if(R_FAILED(ret = ctr::delete_if_exist(tid, dest)))
					return ret;
			}
		}
		else
		{
			if(ctr::title_exists(tid, dest))
				return APPERR_NOREINSTALL;
		}

		ilog("Installing %016llX to %s", tid, dest2str(dest));
		ret = AM_StartCiaInstall(dest, &data->cia);
		ilog("AM_StartCiaInstall(...): 0x%08lX", ret);
		if(R_FAILED(ret)) return ret;
	}

	aptSetHomeAllowed(false);
	float oldrate = C3D_FrameRate(2.0f);
	ret = i_install_resume_loop(get_url, prog, data);
	C3D_FrameRate(oldrate);
	aptSetHomeAllowed(true);

	if(data->type == ActionType::install)
	{
		if(R_FAILED(ret))
		{
			AM_CancelCIAInstall(data->cia);
			svcCloseHandle(data->cia);
			return ret;
		}

		ilog("Done writing all data to CIA handle, finishing up");
		ret = AM_FinishCiaInstall(data->cia);
		ilog("AM_FinishCiaInstall(...): 0x%08lX", ret);
		svcCloseHandle(data->cia);
	}

	return ret;
}

static Result i_install_hs_cia(const hsapi::FullTitle& meta, prog_func prog, bool reinstallable, cia_net_data *data, bool isKtrHint = false)
{
	ctr::Destination media = ctr::detect_dest(meta.tid);
	u64 freeSpace = 0;
	Result res;

	if(R_FAILED(res = ctr::get_free_space(media, &freeSpace)))
		return res;

	if(meta.size > freeSpace)
		return APPERR_NOSPACE;

	// Check if we are NOT on a n3ds and the game is n3ds exclusive
	bool isNew = false;
	if(R_FAILED(res = APT_CheckNew3DS(&isNew)))
		return res;

	if(!isNew && (isKtrHint || meta.prod.rfind("KTR-", 0) == 0))
		return APPERR_NOSUPPORT;

	return net_cia_impl([meta](Result& res) -> std::string {
		std::string ret;
		if(R_FAILED(res = hsapi::get_download_link(ret, meta)))
			return "";
		return ret;
	}, meta.tid, reinstallable, prog, data);
}

Result install::net_cia(get_url_func get_url, u64 tid, prog_func prog, bool reinstallable)
{
	cia_net_data data;
	data.type = ActionType::install;
	return net_cia_impl(get_url, tid, reinstallable, prog, &data);
}

Result install::hs_cia(const hsapi::FullTitle& meta, prog_func prog, bool reinstallable)
{
	cia_net_data data;
	/* we instead want to use the theme installer installation method */
	if(meta.flags & hsapi::TitleFlag::installer)
	{
		ilog("installing installer content");
		content_type content;
		data.content = &content;
		data.type = ActionType::download;
		Result res;
		if(R_FAILED(res = i_install_hs_cia(meta, prog, reinstallable, &data)))
			return res;
		/* trust me this'll be fine */
		return install_forwarder((u8 *) content.c_str(), content.size());
	}
	ilog("installing normal content");
	data.type = ActionType::install;
	return i_install_hs_cia(meta, prog, reinstallable, &data, meta.flags & hsapi::TitleFlag::is_ktr);
}

// HTTPC

// https://3dbrew.org/wiki/HTTPC:SetProxy
Result httpcSetProxy(httpcContext *context, u16 port, u32 proxylen, const char *proxy,
	u32 usernamelen, const char *username, u32 passwordlen, const char *password)
{
	u32 *cmdbuf = getThreadCommandBuffer();

	cmdbuf[0]  = IPC_MakeHeader(0x000D, 0x5, 0x6); // 0x000D0146
	cmdbuf[1]  = context->httphandle;
	cmdbuf[2]  = proxylen;
	cmdbuf[3]  = port & 0xFFFF;
	cmdbuf[4]  = usernamelen;
	cmdbuf[5]  = passwordlen;
	cmdbuf[6]  = (proxylen << 14) | 0x2;
	cmdbuf[7]  = (u32) proxy;
	cmdbuf[8]  = (usernamelen << 14) | 0x402;
	cmdbuf[9]  = (u32) username;
	cmdbuf[10] = (passwordlen << 14) | 0x802;
	cmdbuf[11] = (u32) password;

	Result ret = 0;
	if(R_FAILED(ret = svcSendSyncRequest(context->servhandle)))
		return ret;

	return cmdbuf[1];
}

