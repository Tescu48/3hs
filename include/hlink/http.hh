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

#ifndef inc_hlink_http_hh
#define inc_hlink_http_hh

#include "util.hh"

#include <arpa/inet.h>

#include <unordered_map>
#include <string>


namespace hlink
{
	using HTTPParameters = std::unordered_map<std::string, std::string>;
	using HTTPHeaders    = std::unordered_map<std::string, std::string>;

	class HTTPServer; /* forward decl */
	struct HTTPRequestContext
	{
		HTTPServer *server;
		HTTPHeaders headers; /* headers are always lowercased */
		std::string method; /* method is always lowercased */
		struct sockaddr_in clientaddr;
		HTTPParameters params;
		std::string path;
		char buf[4096];
		size_t buflen;
		bool iseof;
		int fd;

		enum serve_type
		{
			plain, templ, notfound
		};

		inline bool is_get() { return this->method == "get"; }
		void serve_file(int status, const std::string& fname, HTTPHeaders headers);
		void serve_path(int status, const std::string& path, HTTPHeaders headers);
		void respond(int status, const std::string& data, HTTPHeaders headers);
		void respond_chunked(int status, HTTPHeaders headers);
		void respond(int status, const HTTPHeaders& headers);
		void redirect(const std::string& location);
		void send_chunk(const std::string& data);
		void send(const std::string& data);
		void serve_plain();
		serve_type type(); /* NOTE: Sets this->path on success */
		void close();

		void read_path_content(std::string& buf);

		void serve_400();
		void serve_403();
		void serve_404(const std::string& fname);
		void serve_404();
		void serve_500();
	};

	class HTTPServer
	{
	public:
		/* finds a new fd to make a context with */
		int make_reqctx(HTTPRequestContext& ctx);
		int make_fd();

		void close();

		int fd = -1;

		static std::string errmsg(int code);

		friend class hlink::HTTPRequestContext;


	private:
		std::string root;


	};
}

#endif

