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

#include "search.hh"

#include <widgets/meta.hh>

#include <ui/menuselect.hh>
#include <ui/loading.hh>
#include <ui/swkbd.hh>
#include <ui/base.hh>

#include "lumalocale.hh"
#include "installgui.hh"
#include "extmeta.hh"
#include "queue.hh"
#include "hsapi.hh"
#include "next.hh"
#include "util.hh"
#include "i18n.hh"

static const char *short_keys[6] = { "i", "e", "sb", "sd", "t", "p" };
#define SHORT_KEYS_SIZE (sizeof(short_keys) / sizeof(const char *))

static const std::unordered_map<std::string, std::string> keys =
{
	{ "include"   , "i"  },
	{ "inc"       , "i"  },
	{ "exclude"   , "e"  },
	{ "ex"        , "e"  },
	{ "direction" , "sd" },
	{ "order"     , "sd" },
	{ "d"         , "sd" },
	{ "o"         , "sd" },
	{ "sort"      , "sb" },
	{ "s"         , "sb" },
	{ "r"         , "sb" },
	{ "titleid"   , "t"  },
	{ "tid"       , "t"  },
	{ "type"      , "p"  },
	{ "typ"       , "p"  },
};

static const std::unordered_map<std::string, std::string> sort_by =
{
	{ "size"        , "size"         },
	{ "blocks"      , "size"         },
	{ "name"        , "name"         },
	{ "category"    , "category"     },
	{ "type"        , "category"     },
	{ "subcategory" , "subcategory"  },
	{ "region"      , "subcategory"  },
	{ "added"       , "added_date"   },
	{ "uploaded"    , "added_date"   },
	{ "updated"     , "updated_date" },
	{ "edited"      , "updated_date" },
	{ "tid"         , "title_id"     },
	{ "id"          , "id"           },
	{ "downloads"   , "downloads"    },
	{ "dls"         , "downloads"    },
};

static const std::unordered_map<std::string, std::string> sort_direction =
{
	{ "ascending"  , "ascending"  },
	{ "asc"        , "ascending"  },
	{ "descending" , "descending" },
	{ "desc"       , "descending" },
};

static bool is_tid(std::string& str)
{
	for(size_t i = 0; i < str.size(); ++i)
		if(!((str[i] >= '0' && str[i] <= '9') || (str[i] >= 'a' && str[i] <= 'z') || (str[i] >= 'A' && str[i] <= 'Z')))
			return false;

	return !str.empty() && str.find("0004") == 0;
}

static bool is_filter_name(std::string& str, bool allow_wildcard)
{
	if(allow_wildcard && str == "*")
		return true;

	for(size_t i = 0; i < str.size(); ++i)
  		if(!((str[i] >= '0' && str[i] <= '9') || (str[i] >= 'a' && str[i] <= 'z') || (str[i] >= 'A' && str[i] <= 'Z') || str[i] == '-'))
    		return false;

	return !str.empty();
}

static void str_split(std::vector<std::string>& ret, const std::string& s, const std::string& p)
{
	size_t find_idx = s.find(p);
	size_t cur_idx = 0;
	if(find_idx == std::string::npos)
	{
		ret.push_back(s);
		return;
	}

	do
	{
		std::string part = s.substr(cur_idx, find_idx - cur_idx);
		if(!part.empty() && part != p) ret.push_back(part);
		cur_idx = find_idx + p.size();
	} while((find_idx = s.find(p, cur_idx)) != std::string::npos);

	if(!s.empty())
		ret.push_back(s.substr(cur_idx));
}

static bool has_key(std::unordered_map<std::string, std::string> map, const std::string& key)
{
	return map.find(key) != map.end();
}

static bool is_short_key(const std::string& key)
{
	for(size_t i = 0; i < SHORT_KEYS_SIZE; ++i)
		if(strcmp(key.c_str(), short_keys[i]) == 0)
			return true;

	return false;
}

static bool contains(const std::vector<std::string>& list, const std::string& s)
{
	for(const std::string& elem : list)
		if(elem == s) return true;

	return false;
}

static bool parse_filters(std::string& ret, std::vector<std::string>& out_categories, const std::string& raw_filters)
{
	std::vector<std::string> out_filters;
	std::vector<std::string> filter_list_raw;

	std::string rf = raw_filters;
	trim(rf, ",.");
	str_split(filter_list_raw, rf, ",");

	if(filter_list_raw.size() == 0 || filter_list_raw.size() > 15)
		return false;

	for(const std::string& filter : filter_list_raw)
	{
		std::vector<std::string> filter_split;
		str_split(filter_split, filter, ".");

		switch(filter_split.size())
		{
		case 0:
			return false;
		case 1:
		{
			if(!is_filter_name(filter_split[0], false))
				return false;
			std::string cur_filter = filter_split[0] + ".*";
			if(contains(out_filters, cur_filter))
				return false;
			out_categories.push_back(filter_split[0]);
			out_filters.push_back(cur_filter);
			break;
		}
		case 2:
		{
			if(!is_filter_name(filter_split[0], false) || !is_filter_name(filter_split[1], true))
				return false;
			std::string cur_filter = filter_split[0] + "." + filter_split[1];
			if(contains(out_filters, cur_filter))
				return false;
			out_categories.push_back(filter_split[0]);
			out_filters.push_back(cur_filter);
			break;
		}
		default:
			return false;
		}
	}

	join(ret, out_filters, ",");
	return true;
}

static void error(const std::string& msg)
{
	ui::RenderQueue queue;

	ui::builder<ui::Text>(ui::Screen::top, msg)
		.x(ui::layout::center_x)
		.y(ui::layout::center_y)
		.wrap()
		.add_to(queue);

	queue.render_finite_button(KEY_A);
}

static bool show_searchbar_search()
{
	SwkbdResult res;
	SwkbdButton btn;

	std::string query = ui::keyboard([](ui::AppletSwkbd *swkbd) -> void {
		swkbd->hint(STRING(search_content_action));
		swkbd->valid(SWKBD_NOTEMPTY_NOTBLANK);
	}, &btn, &res);

	if(btn != SWKBD_BUTTON_CONFIRM)
		return true;

	if(query.size() < 3 || res == SWKBD_INVALID_INPUT || res == SWKBD_OUTOFMEM || res == SWKBD_BANNED_INPUT)
	{
		error(STRING(invalid_query));
		return true;
	}

	std::vector<hsapi::Title> titles;
	Result rres = hsapi::call<std::vector<hsapi::Title>&, const std::unordered_map<std::string, std::string>&>(hsapi::search, titles, { { "q", query } });
	if(R_FAILED(rres)) return true;

	if(titles.size() == 0)
	{
		error(STRING(search_zero_results));
		return true;
	}

	next::maybe_install_gam(titles);
	return true;
}

static bool show_id_search()
{
	SwkbdResult res;
	SwkbdButton btn;

	uint64_t id = ui::numpad([](ui::AppletSwkbd *swkbd) -> void {
		swkbd->hint(STRING(search_id));
	}, &btn, &res, 16);

	if(btn != SWKBD_BUTTON_CONFIRM || res == SWKBD_INVALID_INPUT || res == SWKBD_OUTOFMEM || res == SWKBD_BANNED_INPUT)
		return true;

	hsapi::FullTitle title;
	Result rres = hsapi::call<hsapi::FullTitle&, hsapi::hid>(hsapi::title_meta, title, id);
	if(R_FAILED(rres)) return true;

	if(show_extmeta(title))
		install::gui::hs_cia(title);
	return true;
}

static bool show_tid_search()
{
	SwkbdResult res;
	SwkbdButton btn;

	std::string title_id = ui::keyboard([](ui::AppletSwkbd *swkbd) -> void {
		swkbd->hint(STRING(search_tid));
	}, &btn, &res, 16);

	if(btn != SWKBD_BUTTON_CONFIRM || res == SWKBD_INVALID_INPUT || res == SWKBD_OUTOFMEM || res == SWKBD_BANNED_INPUT)
		return true;

	if(!is_tid(title_id))
	{
		error(STRING(invalid_tid));
		return true;
	}

	if(title_id == "0004000001111100")
	{
		error(STRING(theme_installer_tid_bad));
		return true;
	}

	std::vector<hsapi::Title> titles;
	Result rres = hsapi::call<std::vector<hsapi::Title>&, const std::string&>(hsapi::get_by_title_id, titles, title_id);
	if(R_FAILED(rres))
		return true;

	if(titles.size() == 0)
	{
		error(STRING(search_zero_results));
		return true;
	}

	next::maybe_install_gam(titles);
	return true;
}

static bool legacy_search()
{
	SwkbdResult res;
	SwkbdButton btn;

	std::string input = ui::keyboard([](ui::AppletSwkbd *swkbd) -> void {
		swkbd->hint(STRING(enter_lgy_query));
	}, &btn, &res, 1024);

	if(btn != SWKBD_BUTTON_CONFIRM || res == SWKBD_INVALID_INPUT || res == SWKBD_OUTOFMEM || res == SWKBD_BANNED_INPUT)
		return true;

	std::unordered_map<std::string, std::string> params;
	std::vector<std::string> parts;
	std::string query = "";

	str_split(parts, input, " ");

	for(const std::string& part : parts)
	{
		std::vector<std::string> param_pair;
		str_split(param_pair, part, ":");
		bool short_key = is_short_key(param_pair[0]);
		if(param_pair.size() != 2 || (!short_key && !has_key(keys, param_pair[0])))
		{
			query += part + " ";
			continue;
		}

		params[short_key ? param_pair[0] : keys.at(param_pair[0])] = param_pair[1];
	}

	trim(query, " ");

	if(!query.empty())
		params["q"] = query;

	bool has_tid = has_key(params, "t");

	if(has_tid && params.size() > 1)
	{
		error(STRING(no_other_params_tid));
		return true;
	}

	if(has_tid && !is_tid(params["t"]))
	{
		error(STRING(invalid_tid));
		return true;
	}

	if(!has_tid && query.size() < 3)
	{
		error(STRING(invalid_query));
		return true;
	}

	bool has_sb = has_key(params, "sb");
	bool has_sd = has_key(params, "sd");
	bool has_e = has_key(params, "e");
	bool has_i = has_key(params, "i");

	if((has_sb && !has_sd) || (has_sd && !has_sb))
	{
		error(STRING(both_sd_and_sb));
		return true;
	}

	if(has_sb)
	{
		if(!has_key(sort_by, params["sb"]))
		{
			error(STRING(invalid_sb));
			return true;
		}
		params["sb"] = sort_by.at(params["sb"]);
	}

	if(has_sd)
	{
		if(!has_key(sort_direction, params["sd"]))
		{
			error(STRING(invalid_sd));
			return true;
		}
		params["sd"] = sort_direction.at(params["sd"]);
	}

	if(has_key(params, "p") && (params["p"] != "legit" && params["p"] != "piratelegit" && params["p"] != "standard"))
	{
		error(STRING(invalid_content_type));
		return true;
	}

	std::string includes, excludes;
	std::vector<std::string> include_cats, exclude_cats;

	if(has_i && !parse_filters(includes, include_cats, params["i"]))
	{
		error(STRING(invalid_includes));
		return true;
	}

	if(has_e && !parse_filters(excludes, exclude_cats, params["e"]))
	{
		error(STRING(invalid_excludes));
		return true;
	}

	if(!includes.empty()) params["i"] = includes;
	if(!excludes.empty()) params["e"] = excludes;

	for(const std::string& include_cat : include_cats)
		for(const std::string& exclude_cat : exclude_cats)
			if(include_cat == exclude_cat)
			{
				error(STRING(filter_overlap));
				return true;
			}

	std::vector<hsapi::Title> titles;
	Result rres = hsapi::call<std::vector<hsapi::Title>&, const std::unordered_map<std::string, std::string>&>(hsapi::search, titles, params);
	if(R_FAILED(rres)) return true;

	if(titles.size() == 0)
	{
		error(STRING(search_zero_results));
		return true;
	}

	next::maybe_install_gam(titles);
	return true;
}

void show_search()
{
	static bool in_search = false;

	if(in_search) return;
	in_search = true;

	std::string desc = set_desc(STRING(search_content));
	bool focus = set_focus(true);

	ui::RenderQueue queue;
	ui::builder<ui::MenuSelect>(ui::Screen::bottom)
		.connect(ui::MenuSelect::add, STRING(search_text), show_searchbar_search)
		.connect(ui::MenuSelect::add, STRING(search_id), show_id_search)
		.connect(ui::MenuSelect::add, STRING(search_tid), show_tid_search)
		.connect(ui::MenuSelect::add, STRING(lgy_search), legacy_search)
		.add_to(queue);

	queue.render_finite_button(KEY_B);

	set_focus(focus);
	set_desc(desc);

	in_search = false;
}

