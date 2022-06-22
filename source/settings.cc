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

#include <widgets/indicators.hh>

#include <ui/menuselect.hh>
#include <ui/selector.hh>
#include <ui/confirm.hh>
#include <ui/swkbd.hh>
#include <ui/theme.hh>
#include <ui/list.hh>

#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>

#include <string>

#include "proxy.hh"
#include "util.hh"
#include "log.hh"

#define SETTINGS_LOCATION "/3ds/3hs/settings"
#define THEMES_DIR        "/3ds/3hs/themes/"
#define THEMES_EXT        ".hstx"

static std::vector<ui::Theme> g_avail_themes;
static bool g_loaded = false;
static Settings g_settings;


static lang::type get_lang()
{
	if(!g_loaded) ensure_settings();
	return g_settings.language;
}

bool settings_are_ready()
{ return g_loaded; }

Settings *get_settings()
{ return &g_settings; }

void save_settings()
{
	// Ensures dirs exist
	mkdir("/3ds", 0777);
	mkdir("/3ds/3hs", 0777);

	FILE *settings = fopen(SETTINGS_LOCATION, "w");
	if(settings == NULL) return; // this shouldn't happen, but we'll use an in-mem config

	fwrite(&g_settings, sizeof(Settings), 1, settings);
	fclose(settings);
}

static void write_default_settings()
{
	cfguInit(); /* ensure cfg is loaded */
	g_settings.language = i18n::default_lang();
	cfguExit(); /* remove cfg reference */
	save_settings();
}

void reset_settings()
{
	/* construct defaults */
	g_settings = Settings();
	/* do we want to automatically detect language here? i don't think we should
	 * since it wouldn't be useful incase a user has a system which is set in a
	 * language he doesn't speak, think people buying japanese 3DS' because they're
	 * cheaper when they don't actually speak japanese. */
	save_settings();
	g_loaded = true;
}

void ensure_settings()
{
	if(g_loaded) return;

	// We want the defaults
	if(access(SETTINGS_LOCATION, F_OK) != 0)
		write_default_settings();
	else
	{
		FILE *settings = fopen(SETTINGS_LOCATION, "r");
		Settings nset; fread(&nset, sizeof(Settings), 1, settings);
		fclose(settings);

		if(memcmp(nset.magic, "3HSS", 4) == 0)
			g_settings = nset;
		else write_default_settings();
	}

	g_loaded = true;
}

void cleanup_themes()
{
	for(ui::Theme& theme : g_avail_themes)
		theme.cleanup();
}

void load_themes()
{
	ui::Theme cthem;
	panic_assert(cthem.open("romfs:/light.hstx", nullptr), "failed to parse built-in theme");
	g_avail_themes.push_back(cthem);
	cthem.clear();
	panic_assert(cthem.open("romfs:/dark.hstx", nullptr), "failed to parse built-in theme");
	g_avail_themes.push_back(cthem);

	DIR *d = opendir(THEMES_DIR);
	/* not having a themes directory is fine as well */
	if(d)
	{
		struct dirent *ent;
		constexpr size_t dirname_len = strlen(THEMES_DIR);
		char fname[dirname_len + sizeof(ent->d_name)] = THEMES_DIR;
		while((ent = readdir(d)))
		{
			if(ent->d_type != DT_REG) continue;
			strcpy(fname + dirname_len, ent->d_name);
			if(cthem.open(fname, &g_avail_themes.front()))
				g_avail_themes.push_back(cthem);
		}
		closedir(d);
	}
	cthem.clear();
}

std::vector<ui::Theme>& themes()
{ return g_avail_themes; }

enum SettingsId
{
	ID_LightMode,  // bool
	ID_Resumable,  // bool
	ID_FreeSpace,  // bool
	ID_Battery,    // bool
	ID_Extra,      // bool
	ID_WarnNoBase, // bool
	ID_AllowLED,   // bool
	ID_TimeFmt,    // show as text: enum val
	ID_ProgLoc,    // show as text: enum val
	ID_Language,   // show as text: enum val
	ID_Localemode, // show as text: enum val
	ID_Direction,  // show as text: enum val
	ID_Method,     // show as text: enum val
	ID_Proxy,      // show as text: custom menu
	ID_MaxELogs,   // show as text: custom menu
};

typedef struct SettingInfo
{
	std::string name;
	std::string desc;
	SettingsId ID;
	bool as_text;
} SettingInfo;

static const char *localemode2str(LumaLocaleMode mode)
{
	switch(mode)
	{
	case LumaLocaleMode::manual: return STRING(manual);
	case LumaLocaleMode::automatic: return STRING(automatic);
	case LumaLocaleMode::disabled: return STRING(disabled);
	}
	return STRING(unknown);
}

static const char *localemode2str_en(LumaLocaleMode mode)
{
	switch(mode)
	{
	case LumaLocaleMode::manual: return "manual";
	case LumaLocaleMode::automatic: return "automatic";
	case LumaLocaleMode::disabled: return "disabled";
	}
	return "unknown";
}

static const char *direction2str(SortDirection dir)
{
	switch(dir)
	{
	case SortDirection::asc: return STRING(ascending);
	case SortDirection::desc: return STRING(descending);
	}
	return STRING(unknown);
}

static const char *direction2str_en(SortDirection dir)
{
	switch(dir)
	{
	case SortDirection::asc: return "ascending";
	case SortDirection::desc: return "descending";
	}
	return "unknown";
}

static const char *method2str(SortMethod dir)
{
	switch(dir)
	{
	case SortMethod::alpha: return STRING(alphabetical);
	case SortMethod::tid: return STRING(tid);
	case SortMethod::size: return STRING(size);
	case SortMethod::downloads: return STRING(downloads);
	case SortMethod::id: return STRING(landing_id);
	}
	return "unknown";
}

static const char *method2str_en(SortMethod dir)
{
	switch(dir)
	{
	case SortMethod::alpha: return "alphabetical";
	case SortMethod::tid: return "title ID";
	case SortMethod::size: return "size";
	case SortMethod::downloads: return "downloads";
	case SortMethod::id: return "hShop ID";
	}
	return "unknown";
}

static bool serialize_id_bool(SettingsId ID)
{
	switch(ID)
	{
	case ID_LightMode:
		return g_settings.isLightMode;
	case ID_Resumable:
		return g_settings.resumeDownloads;
	case ID_FreeSpace:
		return g_settings.loadFreeSpace;
	case ID_Battery:
		return g_settings.showBattery;
	case ID_Extra:
		return g_settings.checkForExtraContent;
	case ID_WarnNoBase:
		return g_settings.warnNoBase;
	case ID_AllowLED:
		return g_settings.allowLEDChange;
	case ID_TimeFmt:
	case ID_ProgLoc:
	case ID_Language:
	case ID_Localemode:
	case ID_Proxy:
	case ID_MaxELogs:
	case ID_Method:
	case ID_Direction:
		panic("impossible bool setting switch case reached");
	}

	return false;
}

static std::string serialize_id_text(SettingsId ID)
{
	switch(ID)
	{
	case ID_LightMode:
	case ID_Resumable:
	case ID_FreeSpace:
	case ID_Battery:
	case ID_Extra:
	case ID_WarnNoBase:
	case ID_AllowLED:
		panic("impossible text setting switch case reached");
	case ID_TimeFmt:
		return g_settings.timeFormat == Timefmt::good ? STRING(fmt_24h) : STRING(fmt_12h);
	case ID_ProgLoc:
		return g_settings.progloc == ProgressBarLocation::top ? STRING(top) : STRING(bottom);
	case ID_Language:
		return i18n::langname(g_settings.language);
	case ID_Localemode:
		return localemode2str(g_settings.lumalocalemode);
	case ID_Proxy:
		return proxy::is_set() ? STRING(press_a_to_view) : STRING(none);
	case ID_MaxELogs:
		return std::to_string(g_settings.maxExtraLogs);
	case ID_Direction:
		return direction2str(g_settings.defaultSortDirection);
	case ID_Method:
		return method2str(g_settings.defaultSortMethod);
	}

	return STRING(unknown);
}

template <typename TEnum>
static void read_set_enum(const std::vector<std::string>& keys,
	const std::vector<TEnum>& values, TEnum& val)
{
	ui::RenderQueue queue;

	ui::Selector<TEnum> *sel;
	ui::builder<ui::Selector<TEnum>>(ui::Screen::bottom, keys, values, &val)
		.add_to(&sel, queue);
	sel->search_set_idx(val);

	queue.render_finite();
}

static void clean_proxy_content(std::string& s)
{
	if(s.find("https://") == 0)
		s.replace(0, 8, "");
	if(s.find("http://") == 0)
		s.replace(0, 7, "");
	std::string::size_type pos;
	while((pos = s.find(":")) != std::string::npos)
		s.replace(pos, 1, "");
	while((pos = s.find("\n")) != std::string::npos)
		s.replace(pos, 1, "");
}

static void show_update_proxy()
{
	ui::RenderQueue queue;

	ui::Button *password;
	ui::Button *username;
	ui::Button *host;
	ui::Button *port;

	constexpr float w = ui::screen_width(ui::Screen::bottom) - 10.0f;
	constexpr float h = 20;

#define UPDATE_LABELS() do { \
		port->set_label(proxy::proxy().port == 0 ? "" : std::to_string(proxy::proxy().port)); \
		password->set_label(proxy::proxy().password); \
		username->set_label(proxy::proxy().username); \
		host->set_label(proxy::proxy().host); \
	} while(0)

#define BASIC_CALLBACK(name) \
		[&name]() -> bool { \
			ui::RenderQueue::global()->render_and_then([&name]() -> void { \
				SwkbdButton btn; \
				std::string val = ui::keyboard([](ui::AppletSwkbd *swkbd) -> void { \
					swkbd->hint(STRING(name)); \
				}, &btn); \
			\
				if(btn == SWKBD_BUTTON_CONFIRM) \
				{ \
					clean_proxy_content(val); \
					proxy::proxy().name = val; \
					name->set_label(val); \
				} \
			}); \
			return true; \
		}

	/* host */
	ui::builder<ui::Text>(ui::Screen::bottom, STRING(host))
		.x(ui::layout::left)
		.y(10.0f)
		.add_to(queue);
	ui::builder<ui::Button>(ui::Screen::bottom)
		.connect(ui::Button::click, BASIC_CALLBACK(host))
		.size(w, h)
		.x(ui::layout::center_x)
		.under(queue.back())
		.add_to(&host, queue);
	/* port */
	ui::builder<ui::Text>(ui::Screen::bottom, STRING(port))
		.x(ui::layout::left)
		.under(queue.back())
		.add_to(queue);
	ui::builder<ui::Button>(ui::Screen::bottom)
		.connect(ui::Button::click, [&port]() -> bool {
			ui::RenderQueue::global()->render_and_then([&port]() -> void {
				SwkbdButton btn;
				uint64_t val = ui::numpad([](ui::AppletSwkbd *swkbd) -> void {
					swkbd->hint(STRING(port));
				}, &btn, nullptr, 5 /* max is 65535: 5 digits */);

				if(btn == SWKBD_BUTTON_CONFIRM && val != 0)
				{
					val &= 0xFFFF; /* limit to a max of 0xFFFF */
					proxy::proxy().port = val;
					port->set_label(std::to_string(val));
				}
			});
			return true;
		})
		.size(w, h)
		.x(ui::layout::center_x)
		.under(queue.back())
		.add_to(&port, queue);
	/* username */
	ui::builder<ui::Text>(ui::Screen::bottom, STRING(username))
		.x(ui::layout::left)
		.under(queue.back())
		.add_to(queue);
	ui::builder<ui::Button>(ui::Screen::bottom)
		.connect(ui::Button::click, BASIC_CALLBACK(username))
		.size(w, h)
		.x(ui::layout::center_x)
		.under(queue.back())
		.add_to(&username, queue);
	/* password */
	ui::builder<ui::Text>(ui::Screen::bottom, STRING(password))
		.x(ui::layout::left)
		.under(queue.back())
		.add_to(queue);
	ui::builder<ui::Button>(ui::Screen::bottom)
		.connect(ui::Button::click, BASIC_CALLBACK(password))
		.size(w, h)
		.x(ui::layout::center_x)
		.under(queue.back())
		.add_to(&password, queue);

	ui::builder<ui::Button>(ui::Screen::bottom, STRING(clear))
		.connect(ui::Button::click, [host, port, username, password]() -> bool {
			proxy::clear();
			UPDATE_LABELS();
			return true;
		})
		.connect(ui::Button::nobg)
		.x(10.0f)
		.y(210.0f)
		.wrap()
		.add_to(queue);

	UPDATE_LABELS();
	queue.render_finite_button(KEY_B | KEY_A);
#undef BASIC_CALLBACK
#undef UPDATE_LABELS
}

static void show_elogs()
{
	uint64_t val;
	do {
		SwkbdButton btn;
		val = ui::numpad([](ui::AppletSwkbd *swkbd) -> void {
			swkbd->hint(STRING(elogs_hint));
		}, &btn, nullptr, 3 /* max is 255: 3 digits */);

		if(btn != SWKBD_BUTTON_CONFIRM)
			return;
		if(val <= 0xFF)
			break;
	} while(true);
	g_settings.maxExtraLogs = val & 0xFF;
}


SortMethod settings_sort_switch()
{
	SortMethod ret = g_settings.defaultSortMethod;
	read_set_enum<SortMethod>(
		{ STRING(alphabetical), STRING(tid), STRING(size), STRING(downloads), STRING(landing_id) },
		{ SortMethod::alpha, SortMethod::tid, SortMethod::size, SortMethod::downloads, SortMethod::id },
		ret
	);
	return ret;
}

static void show_langs()
{
	ui::RenderQueue queue;
	ui::MenuSelect *ms;

	ui::builder<ui::MenuSelect>(ui::Screen::bottom)
		.add_to(&ms, queue);

#define ITER(name, id) \
	ms->add_row(name, []() -> bool { g_settings.language = id; return false; });
	I18N_ALL_LANG_ITER(ITER)
#undef ITER

	queue.render_finite_button(KEY_B);
}

static void update_settings_ID(SettingsId ID)
{
	switch(ID)
	{
	// Boolean
	case ID_LightMode:
		g_settings.isLightMode = !g_settings.isLightMode;
		ui::ThemeManager::global()->reget();
		break;
	case ID_Resumable:
		g_settings.resumeDownloads = !g_settings.resumeDownloads;
		break;
	case ID_WarnNoBase:
		g_settings.warnNoBase = !g_settings.warnNoBase;
		break;
	case ID_FreeSpace:
		g_settings.loadFreeSpace = !g_settings.loadFreeSpace;
		// If we switched it from off to on and we've never drawed the widget before
		// It wouldn't draw the widget until another update
		ui::RenderQueue::global()->find_tag<ui::FreeSpaceIndicator>(ui::tag::free_indicator)->update();
		break;
	case ID_Battery:
		g_settings.showBattery = !g_settings.showBattery;
		break;
	case ID_Extra:
		g_settings.checkForExtraContent = !g_settings.checkForExtraContent;
		break;
	case ID_AllowLED:
		g_settings.allowLEDChange = !g_settings.allowLEDChange;
		break;
	// Enums
	case ID_TimeFmt:
		read_set_enum<Timefmt>(
			{ i18n::getstr(str::fmt_24h, get_lang()), i18n::getstr(str::fmt_12h, get_lang()) },
			{ Timefmt::good, Timefmt::bad },
			g_settings.timeFormat
		);
		break;
	case ID_ProgLoc:
		read_set_enum<ProgressBarLocation>(
			{ STRING(top), STRING(bottom) },
			{ ProgressBarLocation::top, ProgressBarLocation::bottom },
			g_settings.progloc
		);
		break;
	case ID_Language:
		show_langs();
		break;
	case ID_Localemode:
		read_set_enum<LumaLocaleMode>(
			{ STRING(automatic), STRING(manual), STRING(disabled) },
			{ LumaLocaleMode::automatic, LumaLocaleMode::manual, LumaLocaleMode::disabled },
			g_settings.lumalocalemode
		);
		break;
	case ID_Direction:
		read_set_enum<SortDirection>(
			{ STRING(ascending), STRING(descending) },
			{ SortDirection::asc, SortDirection::desc },
			g_settings.defaultSortDirection
		);
		break;
	case ID_Method:
		g_settings.defaultSortMethod = settings_sort_switch();
		break;
	// Other
	case ID_Proxy:
		show_update_proxy();
		break;
	case ID_MaxELogs:
		show_elogs();
		break;
	}
}

void log_settings()
{
#define BOOL(n) g_settings.n ? "true" : "false"
	ilog("settings dump: "
		"isLightMode: %s, "
		"resumeDownloads: %s, "
		"loadFreeSpace: %s, "
		"showBattery: %s, "
		"showNet: %s, "
		"timeFormat: %i, "
		"progloc: %s, "
		"language: %s, "
		"lumalocalemode: %s, "
		"checkForExtraContent: %s, "
		"warnNoBase: %s, "
		"maxExtraLogs: %u, "
		"defaultSortMethod: %s, "
		"defaultSortDirection: %s",
			BOOL(isLightMode), BOOL(resumeDownloads), BOOL(loadFreeSpace),
			BOOL(showBattery), BOOL(showNet), (int) g_settings.timeFormat,
			g_settings.progloc == ProgressBarLocation::bottom ? "bottom" : "top",
			i18n::langname(g_settings.language), localemode2str_en(g_settings.lumalocalemode),
			BOOL(checkForExtraContent), BOOL(warnNoBase), g_settings.maxExtraLogs,
			method2str_en(g_settings.defaultSortMethod), direction2str_en(g_settings.defaultSortDirection));
#undef BOOL
}

static void display_setting_value(const SettingInfo& set, ui::Text *value, ui::Toggle *toggle)
{
	if(set.as_text)
	{
		value->set_text(PSTRING(value_x, serialize_id_text(set.ID)));
		value->set_hidden(false);
		toggle->set_hidden(true);
		return;
	}

	value->set_hidden(true);
	toggle->set_hidden(false);
	toggle->set_toggled(serialize_id_bool(set.ID));
}

void show_settings()
{
	std::vector<SettingInfo> settingsInfo =
	{
		{ STRING(light_mode)     , STRING(light_mode_desc)     , ID_LightMode  , false },
		{ STRING(resume_dl)      , STRING(resume_dl_desc)      , ID_Resumable  , false },
		{ STRING(load_space)     , STRING(load_space_desc)     , ID_FreeSpace  , false },
		{ STRING(show_battery)   , STRING(show_battery_desc)   , ID_Battery    , false },
		{ STRING(check_extra)    , STRING(check_extra_desc)    , ID_Extra      , false },
		{ STRING(warn_no_base)   , STRING(warn_no_base_desc)   , ID_WarnNoBase , false },
		{ STRING(allow_led)      , STRING(allow_led_desc)      , ID_AllowLED   , false },
		{ STRING(time_format)    , STRING(time_format_desc)    , ID_TimeFmt    , true  },
		{ STRING(progbar_screen) , STRING(progbar_screen_desc) , ID_ProgLoc    , true  },
		{ STRING(language)       , STRING(language_desc)       , ID_Language   , true  },
		{ STRING(lumalocalemode) , STRING(lumalocalemode)      , ID_Localemode , true  },
		{ STRING(proxy)          , STRING(proxy_desc)          , ID_Proxy      , true  },
		{ STRING(max_elogs)      , STRING(max_elogs_desc)      , ID_MaxELogs   , true  },
		{ STRING(def_sort_meth)  , STRING(def_sort_meth_desc)  , ID_Method     , true  },
		{ STRING(def_sort_dir)   , STRING(def_sort_dir_desc)   , ID_Direction  , true  },
	};

	panic_assert(settingsInfo.size() > 0, "empty settings meta table");

	using list_t = ui::List<SettingInfo>;

	bool focus = set_focus(true);

	SettingsId current_setting = settingsInfo[0].ID;
	ui::RenderQueue queue;
	ui::Toggle *toggle;
	ui::Text *value;
	ui::Text *desc;
	list_t *list;

	ui::builder<ui::Text>(ui::Screen::bottom, settingsInfo[0].desc)
		.x(10.0f)
		.y(20.0f)
		.wrap()
		.add_to(&desc, queue);
	ui::builder<ui::Text>(ui::Screen::bottom)
		.x(10.0f)
		.under(desc, 5.0f)
		.wrap()
		.hide()
		.add_to(&value, queue);
	ui::builder<ui::Toggle>(ui::Screen::bottom, serialize_id_bool(settingsInfo[0].ID),
			[&current_setting]() -> void { update_settings_ID(current_setting); })
		.x(10.0f)
		.under(desc, 5.0f)
		.add_to(&toggle, queue);

	ui::builder<list_t>(ui::Screen::top, &settingsInfo)
		.connect(list_t::to_string, [](const SettingInfo& entry) -> std::string { return entry.name; })
		.connect(list_t::select, [value, toggle](list_t *self, size_t i, u32 kDown) -> bool {
			(void) kDown;
			ui::RenderQueue::global()->render_and_then([self, i, value, toggle]() -> void {
				const SettingInfo& set = self->at(i);
				update_settings_ID(set.ID);
				display_setting_value(set, value, toggle);
			});
			return true;
		})
		.connect(list_t::change, [value, toggle, desc, &current_setting](list_t *self, size_t i) -> void {
			const SettingInfo& set = self->at(i);
			current_setting = set.ID;
			display_setting_value(set, value, toggle);
			desc->set_text(set.desc);
			ui::BaseWidget *display = set.as_text
				? (ui::BaseWidget *) value : (ui::BaseWidget *) toggle;
			display->set_y(ui::under(desc, display, 5.0f));
		})
		.x(5.0f).y(25.0f)
		.add_to(&list, queue);

	auto reset_settings_local = [](u32) -> bool {
		ui::RenderQueue::global()->render_and_then([]() -> void {
			if(ui::Confirm::exec(STRING(sure_reset)))
			{
				reset_settings();
				/* Perhaps this should be intergrated into reset_settings()?
				 * only other place it's used is main() where nothing is loaded yet anyway */
				ui::ThemeManager::global()->reget();
				ui::RenderQueue::global()->find_tag<ui::FreeSpaceIndicator>(ui::tag::free_indicator)->update();
			}
		});
		return true;
	};

	ui::builder<ui::ButtonCallback>(ui::Screen::top, KEY_R)
		.connect(ui::ButtonCallback::kdown, reset_settings_local)
		.connect(ui::ButtonCallback::kheld, reset_settings_local)
		.add_to(queue);

	queue.render_finite_button(KEY_B);
	set_focus(focus);

	log_delete_invalid();
	save_settings();
	proxy::write();
}

void show_theme_menu()
{
	ui::RenderQueue queue;
	ui::MenuSelect *ms;
	ui::Text *author, *name;

	bool focus = set_focus(true);

	ui::builder<ui::Text>(ui::Screen::top, ui::Theme::global()->name)
		.x(ui::layout::center_x)
		.y(40.0f)
		.wrap()
		.add_to(&name, queue);

	ui::builder<ui::Text>(ui::Screen::top, PSTRING(made_by, ui::Theme::global()->author))
		.x(ui::layout::center_x)
		.under(queue.back())
		.size(0.45f, 0.45f)
		.wrap()
		.add_to(&author, queue);

	ui::builder<ui::MenuSelect>(ui::Screen::bottom)
		.connect(ui::MenuSelect::on_select, [&ms]() -> bool {
			ui::Theme::global()->replace_with(g_avail_themes[ms->pos()]);
			ui::ThemeManager::global()->reget();
			return true;
		})
		.connect(ui::MenuSelect::on_move, [&ms, author, name]() -> bool {
			ui::Theme& theme = g_avail_themes[ms->pos()];
			author->set_text(PSTRING(made_by, theme.author));
			name->set_text(theme.name);

			author->set_y(ui::under(name, author));
			return true;
		})
		.add_to(&ms, queue);

	for(ui::Theme& theme : g_avail_themes)
		ms->add_row(theme.name);

	queue.render_finite_button(KEY_B);
	set_focus(focus);
}

