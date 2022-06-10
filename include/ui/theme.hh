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

#ifndef inc_ui_theme_hh
#define inc_ui_theme_hh

#include <unordered_map>
#include <3ds/types.h>
#include <stddef.h>
#include <string>
#include <vector>

#define UI_SLOTS_PROTO_EXTERN(name) \
	extern ui::slot_color_getter *name##__do_not_touch;
#define UI_SLOTS_PROTO(name, count) \
	ui::SlotManager slots = ui::ThemeManager::global()->get_slots(this, id /* id is defined by UI_WIDGET */, count, name##__do_not_touch);
#define UI_SLOTS__3(line, name, ...) \
	static ui::slot_color_getter _slot_data__do_not_touch##line[] = { __VA_ARGS__ }; \
	ui::slot_color_getter *name##__do_not_touch = _slot_data__do_not_touch##line;
#define UI_SLOTS__2(line, name, ...) \
	UI_SLOTS__3(line, name, __VA_ARGS__)
#define UI_SLOTS(name, ...) \
	UI_SLOTS__2(__LINE__, name, __VA_ARGS__)


namespace ui
{
	typedef u32 (*slot_color_getter)();
	class BaseWidget;

	class SlotManager
	{
	public:
		SlotManager(const u32 *colors)
			: colors(colors) { }
		/* no bounds checking! */
		inline u32 get(size_t i) { return this->colors[i]; }
	private:
		const u32 *colors;
	};


	class ThemeManager
	{
	public:
		ui::SlotManager get_slots(BaseWidget *that, const char *id, size_t count, const slot_color_getter *getters);
		void reget(const char *id);
		void reget();

		/* XXX: Is there really not better way to go around this than a pointer vector?
		 *      I feel there must be, but i just don't know how. For now, this will do. */
		void unregister(ui::BaseWidget *w);

		static ui::ThemeManager *global();

		~ThemeManager();


	public:
		struct slot_data {
			const slot_color_getter *getters; /* of size len */
			std::vector<BaseWidget *> slaves;
			u32 *colors; /* of size len */
			size_t len;
		};
		std::unordered_map<std::string, slot_data> slots;


	};
}

#endif

