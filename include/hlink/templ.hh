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

#ifndef inc_hlink_templ_hh
#define inc_hlink_templ_hh

#include "hlink/http.hh"

#include <functional>
#include <string>
#include <vector>


namespace hlink
{
	class TemplRen;
	struct TemplCtx
	{
		void abort();

		TemplRen *ren;
	};

	using TemplArgs     = std::vector<std::string>;
	using TemplStrFunc  = std::function<std::string(TemplCtx&, TemplArgs&)>;
	using TemplBoolFunc = std::function<bool       (TemplCtx&, TemplArgs&)>;

	class TemplRen
	{
	public:
		~TemplRen();

		inline HTTPHeaders& headers() { return this->hctx.headers; }

		void use(const std::string& sym, const std::vector<std::string>& val); /* constant array */
		void use(const std::string& sym, const std::string& val);              /* constant string */
		void use(const std::string& sym, TemplBoolFunc func);                  /* boolean function */
		void use(const std::string& sym, TemplStrFunc  func);                  /* string function */

		void use_default();

		enum class result
		{
			ok = 0, aborted, unterminated, not_found, invalid
		};

		result finish(const std::string& src, std::string& res);
		std::string strerr(result res);

		enum class templ_sym_type { vs, ss, sf, bf };
		struct templ_sym {
			templ_sym_type type;
			union {
				std::vector<std::string> *vs;
				TemplBoolFunc *bf;
				TemplStrFunc *sf;
				std::string *ss;
			};
		};
		std::unordered_map<std::string, templ_sym> syms;
		HTTPRequestContext hctx;


	private:
		bool abortBit; /* initialized when render is started */

		result finish_until_keyword(const std::string& src, std::string& res, hlink::TemplCtx& ctx, size_t& i,
			const char *keywords[], size_t klen);
		result finish_until_end_kw(const std::string& src, std::string& res, hlink::TemplCtx& ctx, size_t& i);
		void skip_to_next_if_like_block(const std::string& src, size_t& i);

		friend class TemplCtx;


	};
}

#endif

