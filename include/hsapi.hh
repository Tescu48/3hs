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

#ifndef inc_hsapi_hh
#define inc_hsapi_hh

#include <unordered_map>
#include <string>
#include <vector>

#include <3ds.h>

#include <ui/confirm.hh>
#include <ui/loading.hh>
#include <ui/base.hh>

#include "panic.hh"
#include "error.hh"
#include "util.hh"
#include "i18n.hh"


namespace hsapi
{
	using htimestamp = uint64_t; /* unix timestamp type. unsigned because there will be no negative timestamps on hshop */
	using hsize      = uint64_t; /* size type */
	using hiver      = uint16_t; /* integer version type */
	using htid       = uint64_t; /* title id type */
	using hid        = int64_t;  /* landing id type */
	using hprio      = uint32_t; /* priority */
	using hflags     = uint32_t; /* flag */

	namespace TitleFlag
	{
		enum TitleFlag {
			is_ktr           = 0x1,
			locale_emulation = 0x2,
			installer        = 0x4,
		};
	}

	namespace impl
	{
		typedef struct BaseCategory
		{
			std::string disp; /* display name; use this */
			std::string name; /* internal short name */
			std::string desc; /* category description */
			hsize titles; /* title count */
			hsize size; /* total size */
		} BaseCategory;
	}

	typedef struct Subcategory : public impl::BaseCategory
	{
		std::string cat; /* parent category */
	} Subcategory;

	typedef struct Category : public impl::BaseCategory
	{
		std::vector<Subcategory> subcategories; /* subcategories in this category */
		hprio prio;

		friend bool operator < (const Category& a, const Category& b)
		{ return a.prio < b.prio; }

		/* find a subcategory by name */
		Subcategory *find(const std::string& name);
	} Category;

	typedef struct Index
	{
		std::vector<Category> categories; /* categories on hShop */
		hsize titles; /* total titles on hShop */
		hsize size; /* total size of hShop */

		/* find a category by name */
		Category *find(const std::string& name);
	} Index;

	typedef struct Title
	{
		std::string subcat; /* subcategory this title belongs to */
		std::string name; /* name of the title on hShop */
		std::string cat; /* category this title belongs to */
		hsize dlCount; /* amount of title downloads */
		hsize size; /* filesize */
		htid tid; /* title id of the title */
		hid id; /* hShop id */

		friend bool operator == (const Title& lhs, const Title& rhs)
		{ return lhs.id == rhs.id; }

		friend bool operator == (const Title& lhs, hsapi::hid rhs)
		{ return lhs.id == rhs; }
	} Title;

	typedef struct FullTitle : public Title
	{
		std::string prod; /* product code */
		std::string desc; /* "" if none */
//		htimestamp updated; /* last updated timestamp */
//		htimestamp added; /* added on timestamp */
		hiver version; /* version int */
		hflags flags; /* title flags */
	} FullTitle;

	typedef struct Related
	{
		std::vector<FullTitle> updates;
		std::vector<FullTitle> dlc;
	} Related;
	using BatchRelated = std::unordered_map<htid, Related>;


	void global_deinit();
	bool global_init();

	Result get_by_title_id(std::vector<Title>& ret, const std::string& title_id);
	Result titles_in(std::vector<Title>& ret, const std::string& cat, const std::string& scat);
	Result batch_related(BatchRelated& ret, const std::vector<htid>& tids);
	Result upload_log(const char *contents, u32 size, std::string& logid);
	Result search(std::vector<Title>& ret, const std::unordered_map<std::string, std::string>& params);
	Result get_download_link(std::string& ret, const Title& title);
	Result get_latest_version_string(std::string& ret);
	Result title_meta(FullTitle& ret, hid id);
	Result random(FullTitle& ret);
	Result fetch_index();

	std::string update_location(const std::string& ver);
	std::string parse_vstring(hiver ver);
	Index *get_index();

	// Silent call. ui::loading() is not called and it will stop after 3 tries
	template <typename ... Ts>
	Result scall(Result (*func)(Ts...), Ts&& ... args)
	{
		Result res = 0;
		int tries = 0;

		do {
			res = (*func)(args...);
			++tries;
		} while(R_FAILED(res) && tries < 3);
		return res;
	}

	// NOTE: You have to std::move() primitives (hid, hiver, htid, ...)
	template <typename ... Ts>
	Result call(Result (*func)(Ts...), Ts&& ... args)
	{
		std::string desc = set_desc(STRING(loading));
		bool focus = set_focus(false);
		Result res;
		do {
			ui::loading([&res, func, &args...]() -> void {
				res = (*func)(args...);
			});

			if(R_FAILED(res)) // Ask if we want to retry
			{
				error_container err = get_error(res);
				report_error(err);
				handle_error(err);

				ui::RenderQueue queue; bool cont = true;
				ui::builder<ui::Confirm>(ui::Screen::bottom, STRING(retry_req), cont)
					.y(ui::layout::center_y)
					.add_to(queue);
				queue.render_finite();

				if(!cont) break;
			}
		} while(R_FAILED(res));
		set_focus(focus);
		set_desc(desc);
		return res;
	}
}

#endif

