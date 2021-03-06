#pragma once
#include "map_modes.h"
#include "gui\gui.hpp"
#include "world_state/world_state.h"
#include "military\military_functions.h"

namespace map_mode {
	class army_bg_t : public ui::gui_behavior {
	public:
		std::variant<std::monostate, military::army_tag, military::strategic_hq_tag> unit;

		virtual bool mouse_consumer(ui::xy_pair) final override { return true; }
		virtual bool on_get_focus(ui::gui_object_tag, world_state& ws) final override;
		virtual void on_lose_focus(ui::gui_object_tag, world_state& ws) final override;

		/*
		virtual ui::tooltip_behavior has_tooltip(ui::gui_object_tag, world_state&, const ui::mouse_move&) { return ui::tooltip_behavior::tooltip; }
		virtual void create_tooltip(ui::gui_object_tag, world_state& ws, const ui::mouse_move&, ui::tagged_gui_object tw) {
		}
		*/

		virtual bool on_lclick(ui::gui_object_tag, world_state&, const ui::lbutton_down&) final override { return true; }
		virtual bool on_rclick(ui::gui_object_tag, world_state&, const ui::rbutton_down&) final override { return true; }

		template<typename window>
		void windowed_update(window& win, world_state& ws) {
			auto owner = [&ws, &win]() {
				if(win.army) {
					return ws.get<army::owner>(win.army);
				} else if(win.hq) {
					return ws.get<province_state::owner>(ws.get<strategic_hq::location>(win.hq));
				} else {
					return nations::country_tag();
				}
			}();
			if(win.army) {
				unit = win.army;
			} else if(win.hq) {
				unit = win.hq;
			} else {
				unit = std::monostate{};
			}
			auto c = ws.get<nation::current_color>(owner);
			if(const auto go = ws.w.gui_m.tinted_icon_instances.safe_at(ui::tinted_icon_instance_tag(associated_object->type_dependant_handle)); go) {
				go->r = float(c.r) / 255.0f;
				go->g = float(c.g) / 255.0f;
				go->b = float(c.b) / 255.0f;
			}
		}
	};


	ui::tagged_gui_object create_static_element(world_state& ws, ui::icon_tag handle, ui::tagged_gui_object parent, army_bg_t& b) {
		const auto new_gobj = ws.w.gui_m.gui_objects.emplace();

		const ui::icon_def& icon_def = ws.s.gui_m.ui_definitions.icons[handle];

		const uint16_t rotation =
			(icon_def.flags & ui::icon_def::rotation_mask) == ui::icon_def::rotation_upright ?
			ui::gui_object::rotation_upright :
			((icon_def.flags & ui::icon_def::rotation_mask) == ui::icon_def::rotation_90_right ? ui::gui_object::rotation_right : ui::gui_object::rotation_left);

		new_gobj.object.position = icon_def.position;
		new_gobj.object.flags.fetch_or(rotation | ui::gui_object::interactable, std::memory_order_acq_rel);
		new_gobj.object.align = alignment_from_definition(icon_def);

		ui::detail::instantiate_graphical_object(ws.s.gui_m, ws.w.gui_m, new_gobj, icon_def.graphical_object_handle, 0, true);

		new_gobj.object.size.x = int16_t(float(new_gobj.object.size.x) * icon_def.scale);
		new_gobj.object.size.y = int16_t(float(new_gobj.object.size.y) * icon_def.scale);

		new_gobj.object.associated_behavior = &b;
		b.associated_object = &new_gobj.object;

		ui::add_to_back(ws.w.gui_m, parent, new_gobj);

		ws.w.gui_m.flag_minimal_update();
		return new_gobj;
	}

	class generic_legend_window_base : public ui::window_pane {
	public:
		template<typename W>
		void on_create(W& w, world_state& ws);
	};

	class generic_legend_title {
	public:
		void update(ui::tagged_gui_object box, ui::text_box_line_manager& lm, ui::text_format& fmt, world_state& ws);
	};

	class generic_legend_contents {
	public:
		void update(ui::tagged_gui_object box, ui::text_box_line_manager& lm, ui::text_format& fmt, world_state& ws);
	};

	class generic_legend_min {
	public:
		void update(ui::tagged_gui_object box, ui::text_box_line_manager& lm, ui::text_format& fmt, world_state& ws);
	};

	class generic_legend_max {
	public:
		void update(ui::tagged_gui_object box, ui::text_box_line_manager& lm, ui::text_format& fmt, world_state& ws);
	};

	class gradient_bar {
	public:
		void update(ui::tinted_icon<gradient_bar>& self, world_state& ws);
	};

	class population_amount_button {
	public:
		void update(ui::simple_button<population_amount_button>& self, world_state& ws);
		void button_function(ui::simple_button<population_amount_button>& self, world_state& ws);
	};

	class population_density_button {
	public:
		void update(ui::simple_button<population_density_button>& self, world_state& ws);
		void button_function(ui::simple_button<population_density_button>& self, world_state& ws);
	};

	class external_button {
	public:
		void update(ui::simple_button<external_button>& self, world_state& ws);
		void button_function(ui::simple_button<external_button>& self, world_state& ws);
	};

	class internal_button {
	public:
		void update(ui::simple_button<internal_button>& self, world_state& ws);
		void button_function(ui::simple_button<internal_button>& self, world_state& ws);
	};

	using generic_legend_window_t = ui::gui_window<
		CT_STRING("legend_title"), ui::display_text<generic_legend_title>,
		CT_STRING("legend_contents"), ui::multiline_text<generic_legend_contents>,
		generic_legend_window_base
	>;

	using generic_gradient_legend_window_t = ui::gui_window<
		CT_STRING("legend_title"), ui::display_text<generic_legend_title>,
		CT_STRING("legend_contents"), ui::multiline_text<generic_legend_contents>,
		CT_STRING("gradient_icon"), ui::tinted_icon<gradient_bar>,
		CT_STRING("gradient_min"), ui::display_text<generic_legend_min>,
		CT_STRING("gradient_max"), ui::display_text<generic_legend_max>,
		generic_legend_window_base
	>;

	using population_legend_window_t = ui::gui_window <
		CT_STRING("legend_title"), ui::display_text<generic_legend_title>,
		CT_STRING("amount_cb"), ui::simple_button<population_amount_button>,
		CT_STRING("density_cb"), ui::simple_button<population_density_button>,
		CT_STRING("legend_contents"), ui::multiline_text<generic_legend_contents>,
		CT_STRING("gradient_icon"), ui::tinted_icon<gradient_bar>,
		CT_STRING("gradient_min"), ui::display_text<generic_legend_min>,
		CT_STRING("gradient_max"), ui::display_text<generic_legend_max>,
		generic_legend_window_base
	>;

	using relations_legend_window_t = ui::gui_window<
		CT_STRING("legend_contents"), ui::multiline_text<generic_legend_contents>,
		CT_STRING("gradient_min"), ui::display_text<generic_legend_min>,
		CT_STRING("gradient_max"), ui::display_text<generic_legend_max>,
		generic_legend_window_base
	>;

	using migration_legend_window_t = ui::gui_window <
		CT_STRING("external_cb"), ui::simple_button<external_button>,
		CT_STRING("internal_cb"), ui::simple_button<internal_button>,
		CT_STRING("legend_contents"), ui::multiline_text<generic_legend_contents>,
		CT_STRING("gradient_min"), ui::display_text<generic_legend_min>,
		CT_STRING("gradient_max"), ui::display_text<generic_legend_max>,
		generic_legend_window_base
	>;

	class railroad_color {
	public:
		void update(ui::tinted_icon<railroad_color>& self, world_state& ws);
	};

	class naval_base_color {
	public:
		void update(ui::tinted_icon<naval_base_color>& self, world_state& ws);
	};

	using infrastructure_legend_window_t = ui::gui_window <
		CT_STRING("build_rr_color"), ui::tinted_icon<railroad_color>,
		CT_STRING("build_nb_color"), ui::tinted_icon<naval_base_color>,
		CT_STRING("legend_contents"), ui::multiline_text<generic_legend_contents>,
		CT_STRING("gradient_min"), ui::display_text<generic_legend_min>,
		CT_STRING("gradient_max"), ui::display_text<generic_legend_max>,
		generic_legend_window_base
	>;

	class crime_color {
	public:
		void update(ui::tinted_icon<crime_color>& self, world_state& ws);
	};

	using admin_legend_window_t = ui::gui_window <
		CT_STRING("crime_color"), ui::tinted_icon<crime_color>,
		CT_STRING("legend_contents"), ui::multiline_text<generic_legend_contents>,
		CT_STRING("gradient_icon"), ui::tinted_icon<gradient_bar>,
		CT_STRING("gradient_min"), ui::display_text<generic_legend_min>,
		CT_STRING("gradient_max"), ui::display_text<generic_legend_max>,
		generic_legend_window_base
	>;

	class rgo_item_base : public ui::visible_region {
	public:
		economy::goods_tag tag;
		void set_value(economy::goods_tag t) {
			tag = t;
		}
	};

	class rgo_item_button {
	public:
		economy::goods_tag tag;

		template<typename window_type>
		void windowed_update(ui::simple_button<rgo_item_button>& self, window_type& win, world_state& ws);
		void button_function(ui::simple_button<rgo_item_button>& self, world_state& ws);
	};

	class rgo_item_color {
	public:
		template<typename window_type>
		void windowed_update(ui::tinted_icon<rgo_item_color>& self, window_type& win, world_state& ws);
	};

	class rgo_item_icon {
	public:
		template<typename window_type>
		void windowed_update(ui::dynamic_icon<rgo_item_icon>& self, window_type& win, world_state& ws);
	};

	class rgo_item_name {
	public:
		template<typename window_type>
		void windowed_update(window_type& win, ui::tagged_gui_object box, ui::text_box_line_manager& lm, ui::text_format& fmt, world_state& ws);
	};

	using rgo_item = ui::gui_window <
		CT_STRING("background"), ui::simple_button<rgo_item_button>,
		CT_STRING("resource_color"), ui::tinted_icon<rgo_item_color>,
		CT_STRING("resource_icon"), ui::dynamic_icon<rgo_item_icon>,
		CT_STRING("resource_name"), ui::display_text<rgo_item_name>,
		rgo_item_base
	>;

	class rgo_items_lb {
	public:
		template<typename lb_type>
		void populate_list(lb_type& lb, world_state& ws);
		ui::window_tag element_tag(ui::gui_static& m);
	};

	using rgo_legend_window_t = ui::gui_window <
		CT_STRING("legend_contents"), ui::multiline_text<generic_legend_contents>,
		CT_STRING("rgo_items"), ui::discrete_listbox<rgo_items_lb, rgo_item, economy::goods_tag>,
		CT_STRING("gradient_icon"), ui::tinted_icon<gradient_bar>,
		CT_STRING("gradient_min"), ui::display_text<generic_legend_min>,
		CT_STRING("gradient_max"), ui::display_text<generic_legend_max>,
		generic_legend_window_base
	>;

	class resource_item_button {
	public:
		economy::goods_tag tag;

		template<typename window_type>
		void windowed_update(ui::simple_button<resource_item_button>& self, window_type& win, world_state& ws);
		void button_function(ui::simple_button<resource_item_button>& self, world_state& ws);
	};

	using resource_item = ui::gui_window <
		CT_STRING("background"), ui::simple_button<resource_item_button>,
		CT_STRING("resource_icon"), ui::dynamic_icon<rgo_item_icon>,
		CT_STRING("resource_name"), ui::display_text<rgo_item_name>,
		rgo_item_base
	>;

	class resource_items_lb {
	public:
		template<typename lb_type>
		void populate_list(lb_type& lb, world_state& ws);
		ui::window_tag element_tag(ui::gui_static& m);
	};

	using resource_legend_window_t = ui::gui_window <
		CT_STRING("legend_title"), ui::display_text<generic_legend_title>,
		CT_STRING("legend_contents"), ui::multiline_text<generic_legend_contents>,
		CT_STRING("resource_items"), ui::discrete_listbox<resource_items_lb, resource_item, economy::goods_tag>,
		CT_STRING("gradient_min"), ui::display_text<generic_legend_min>,
		CT_STRING("gradient_max"), ui::display_text<generic_legend_max>,
		generic_legend_window_base
	>;

	class show_netural_cb {
	public:
		void update(ui::simple_button<show_netural_cb>& self,world_state& ws);
		void button_function(ui::simple_button<show_netural_cb>& self, world_state& ws);
	};

	class self_color_icon {
	public:
		void update(ui::tinted_icon<self_color_icon>& self, world_state& ws);
	};

	class friendly_color_icon {
	public:
		void update(ui::tinted_icon<friendly_color_icon>& self, world_state& ws);
	};

	class hostile_color_icon {
	public:
		void update(ui::tinted_icon<hostile_color_icon>& self, world_state& ws);
	};

	class friendly_occupation_icon {
	public:
		void update(ui::tinted_icon<friendly_occupation_icon>& self, world_state& ws);
	};

	class hostile_occupation_icon {
	public:
		void update(ui::tinted_icon<hostile_occupation_icon>& self, world_state& ws);
	};

	using army_legend_window_t = ui::gui_window <
		CT_STRING("legend_title"), ui::display_text<generic_legend_title>,
		CT_STRING("legend_contents"), ui::multiline_text<generic_legend_contents>,
		CT_STRING("show_neutral_check_box"), ui::simple_button<show_netural_cb>,
		CT_STRING("self_color"), ui::tinted_icon<self_color_icon>,
		CT_STRING("friendly_color"), ui::tinted_icon<friendly_color_icon>,
		CT_STRING("hostile_color"), ui::tinted_icon<hostile_color_icon>,
		CT_STRING("friendly_occupation_color"), ui::tinted_icon<friendly_occupation_icon>,
		CT_STRING("hostile_occupation_color"), ui::tinted_icon<hostile_occupation_icon>,
		generic_legend_window_base
	>;

	class ideology_item_base : public ui::visible_region {
	public:
		ideologies::ideology_tag tag;
		void set_value(ideologies::ideology_tag t) {
			tag = t;
		}
	};

	class ideology_item_button {
	public:
		ideologies::ideology_tag tag;

		template<typename window_type>
		void windowed_update(ui::simple_button<ideology_item_button>& self, window_type& win, world_state& ws);
		void button_function(ui::simple_button<ideology_item_button>& self, world_state& ws);
	};

	class ideology_item_color {
	public:
		template<typename window_type>
		void windowed_update(ui::tinted_icon<ideology_item_color>& self, window_type& win, world_state& ws);
	};

	class ideology_item_name {
	public:
		template<typename window_type>
		void windowed_update(window_type& win, ui::tagged_gui_object box, ui::text_box_line_manager& lm, ui::text_format& fmt, world_state& ws);
	};

	using ideology_item = ui::gui_window <
		CT_STRING("background"), ui::simple_button<ideology_item_button>,
		CT_STRING("ideology_color"), ui::tinted_icon<ideology_item_color>,
		CT_STRING("ideology_name"), ui::display_text<ideology_item_name>,
		ideology_item_base
	>;

	class ideology_items_lb {
	public:
		template<typename lb_type>
		void populate_list(lb_type& lb, world_state& ws);
		ui::window_tag element_tag(ui::gui_static& m);
	};

	using voting_legend_window_t = ui::gui_window <
		CT_STRING("legend_contents"), ui::multiline_text<generic_legend_contents>,
		CT_STRING("ideologies"), ui::discrete_listbox<ideology_items_lb, ideology_item, ideologies::ideology_tag>,
		generic_legend_window_base
	>;

	class map_icon_base : public ui::gui_behavior {
	public:
		provinces::province_tag tag;
	};

	class map_port_icon {
	public:
		template<typename window_type>
		void windowed_update(ui::dynamic_icon<map_port_icon>& self, window_type& win, world_state& ws);
	};

	class map_port_text {
	public:
		template<typename window_type>
		void windowed_update(window_type& win, ui::tagged_gui_object box, ui::text_box_line_manager& lm, ui::text_format& fmt, world_state& ws);
	};

	using infrastructure_map_icon_window_t = ui::gui_window <
		CT_STRING("port_icon"), ui::dynamic_icon<map_port_icon>,
		CT_STRING("port_level"), ui::display_text<map_port_text>,
		map_icon_base
	>;

	class unit_icon_list_base : public ui::gui_behavior {
	public:
		provinces::province_tag tag;
	};

	class unit_icon_lb {
	public:
		template<typename lb_type, typename window_type>
		void windowed_update(lb_type& lb, window_type& win, world_state& ws);
		ui::window_tag element_tag(ui::gui_static& m);
	};

	class unit_icon_base : public ui::gui_behavior {
	public:
		military::army_tag army;
		military::strategic_hq_tag hq;

		void set_value(military::army_tag a, military::strategic_hq_tag h) {
			army = a;
			hq = h;
		}
	};

	class unit_type {
	public:
		template<typename window_type>
		void windowed_update(ui::dynamic_icon<unit_type>& self, window_type& win, world_state& ws);
	};

	class unit_left_status {
	public:
		template<typename window_type>
		void windowed_update(ui::dynamic_icon<unit_left_status>& self, window_type& win, world_state& ws);
	};

	class unit_right_status {
	public:
		template<typename window_type>
		void windowed_update(ui::dynamic_icon<unit_right_status>& self, window_type& win, world_state& ws);
	};

	class unit_frame {
	public:
		template<typename window_type>
		void windowed_update(ui::tinted_icon<unit_frame>& self, window_type& win, world_state& ws);
	};

	class unit_amount {
	public:
		template<typename window_type>
		void windowed_update(window_type& win, ui::tagged_gui_object box, ui::text_box_line_manager& lm, ui::text_format& fmt, world_state& ws);
	};

	class unit_background {
	public:
		template<typename window_type>
		void windowed_update(ui::tinted_icon<unit_background>& self, window_type& win, world_state& ws);
	};

	using unit_icon = ui::gui_window <
		CT_STRING("frame"), ui::tinted_icon<unit_frame>,
		CT_STRING("background"), army_bg_t,
		CT_STRING("type"), ui::dynamic_icon<unit_type>,
		CT_STRING("amount"), ui::display_text<unit_amount>,
		CT_STRING("left"), ui::dynamic_icon<unit_left_status>,
		CT_STRING("right"), ui::dynamic_icon<unit_right_status>,
		unit_icon_base
	>;

	using unit_icon_list_window_t = ui::gui_window <
		CT_STRING("unit_list"), ui::overlap_box<unit_icon_lb, ui::window_tag, unit_icon, 0, true>,
		unit_icon_list_base
	>;

	class rgo_map_icon {
	public:
		provinces::province_tag tag;
		void update(ui::dynamic_icon<rgo_map_icon>& self, world_state& ws);
	};

	class crime_map_icon {
	public:
		provinces::province_tag tag;
		void update(ui::dynamic_icon<crime_map_icon>& self, world_state& ws);
	};

	class legend_gui {
	public:
		generic_legend_window_t generic_legend_window;
		generic_gradient_legend_window_t generic_gradient_legend_window;
		population_legend_window_t population_legend_window;
		relations_legend_window_t relations_legend_window;
		migration_legend_window_t migration_legend_window;
		infrastructure_legend_window_t infrastructure_legend_window;
		rgo_legend_window_t rgo_legend_window;
		resource_legend_window_t resource_legend_window;
		voting_legend_window_t voting_legend_window;
		admin_legend_window_t admin_legend_window;
		army_legend_window_t army_legend_window;

		bool showing_density = false;
		bool showing_internal_migration = false;
		bool showing_neutral_units = false;

		std::vector< infrastructure_map_icon_window_t> infrastructure_icons_windows;
		std::vector<ui::dynamic_icon<rgo_map_icon>> rgo_map_icons_objects;
		std::vector<ui::dynamic_icon<crime_map_icon>> admin_map_icons_objects;
		std::vector<unit_icon_list_window_t> army_map_windows;

		ideologies::ideology_tag current_ideology;
		provinces::province_tag current_province;
		nations::country_tag current_nation;
		cultures::culture_tag current_culture;
		economy::goods_tag current_good;

		military::army_tag selected_army;
		military::strategic_hq_tag selected_hq;
	};

	template<typename W>
	void generic_legend_window_base::on_create(W & w, world_state & ws) {
		ui::hide(*associated_object);
	}
	template<typename window_type>
	void map_port_icon::windowed_update(ui::dynamic_icon<map_port_icon>& self, window_type & win, world_state & ws) {
		auto const port_level = ws.w.province_s.province_state_container.get<province_state::naval_base_level>(win.tag);
		self.set_visibility(ws.w.gui_m, port_level > 0);
	}
	template<typename window_type>
	void map_port_text::windowed_update(window_type & win, ui::tagged_gui_object box, ui::text_box_line_manager & lm, ui::text_format & fmt, world_state & ws) {
		auto const port_level = ws.w.province_s.province_state_container.get<province_state::naval_base_level>(win.tag);
		if(port_level > 0) {
			ui::add_text(ui::xy_pair{ 0,0 }, text_data::integer{ port_level },
				ui::text_format{ui::text_color::white, fmt.font_handle, fmt.font_size}, ws, box, lm);
		}
	}
	template<typename window_type>
	void rgo_item_button::windowed_update(ui::simple_button<rgo_item_button>& self, window_type & win, world_state & ws) {
		tag = win.tag;
		if(ws.w.map_view.legends->current_good == tag)
			self.set_frame(ws.w.gui_m, 0);
		else
			self.set_frame(ws.w.gui_m, 1);
	}
	template<typename window_type>
	void rgo_item_color::windowed_update(ui::tinted_icon<rgo_item_color>& self, window_type & win, world_state & ws) {
		auto clr = win.tag ? ws.s.economy_m.goods[win.tag].color : graphics::color_rgb{ 255ui8, 255ui8, 255ui8 };
		self.set_color(ws.w.gui_m, float(clr.r) / 255.0f, float(clr.g) / 255.0f, float(clr.b) / 255.0f);
	}
	template<typename window_type>
	void rgo_item_icon::windowed_update(ui::dynamic_icon<rgo_item_icon>& self, window_type & win, world_state & ws) {
		if(win.tag)
			self.set_frame(ws.w.gui_m, ws.s.economy_m.goods[win.tag].icon);
		else
			self.set_frame(ws.w.gui_m, 0);
	}
	template<typename window_type>
	void rgo_item_name::windowed_update(window_type & win, ui::tagged_gui_object box, ui::text_box_line_manager & lm, ui::text_format & fmt, world_state & ws) {
		if(win.tag)
			ui::add_text(ui::xy_pair{ 0,0 }, ws.s.economy_m.goods[win.tag].name, fmt, ws, box, lm);
		else
			ui::add_text(ui::xy_pair{ 0,0 }, scenario::fixed_ui::all, fmt, ws, box, lm);
	}
	template<typename lb_type>
	void rgo_items_lb::populate_list(lb_type & lb, world_state & ws) {
		boost::container::small_vector<economy::goods_tag, 32, concurrent_allocator<economy::goods_tag>> data;
		data.push_back(economy::goods_tag());
		ws.s.economy_m.for_each_good([&ws, &data](economy::goods_tag g) {
			if((ws.s.economy_m.goods[g].flags & economy::good_definition::has_rgo) != 0)
				data.push_back(g);
		});
		lb.new_list(data.begin(), data.end());
	}
	template<typename window_type>
	void resource_item_button::windowed_update(ui::simple_button<resource_item_button>& self, window_type & win, world_state & ws) {
		tag = win.tag;
		if(ws.w.map_view.legends->current_good == tag)
			self.set_frame(ws.w.gui_m, 0);
		else
			self.set_frame(ws.w.gui_m, 1);
	}
	template<typename lb_type>
	void resource_items_lb::populate_list(lb_type & lb, world_state & ws) {
		boost::container::small_vector<economy::goods_tag, 32, concurrent_allocator<economy::goods_tag>> data;
		ws.s.economy_m.for_each_good([&ws, &data](economy::goods_tag g) {
			if(g != economy::gold_good)
				data.push_back(g);
		});
		lb.new_list(data.begin(), data.end());
	}
	template<typename lb_type>
	void ideology_items_lb::populate_list(lb_type & lb, world_state & ws) {
		boost::container::small_vector<ideologies::ideology_tag, 32, concurrent_allocator<ideologies::ideology_tag>> data;
		data.push_back(ideologies::ideology_tag());
		ws.s.ideologies_m.for_each_ideology([&data](ideologies::ideology_tag g) {
			data.push_back(g);
		});
		lb.new_list(data.begin(), data.end());
	}
	template<typename window_type>
	void ideology_item_name::windowed_update(window_type & win, ui::tagged_gui_object box, ui::text_box_line_manager & lm, ui::text_format & fmt, world_state & ws) {
		if(win.tag)
			ui::add_text(ui::xy_pair{ 0,0 }, ws.s.ideologies_m.ideology_container[win.tag].name, fmt, ws, box, lm);
		else
			ui::add_text(ui::xy_pair{ 0,0 }, scenario::fixed_ui::all, fmt, ws, box, lm);
	}
	template<typename window_type>
	void ideology_item_color::windowed_update(ui::tinted_icon<ideology_item_color>& self, window_type & win, world_state & ws) {
		auto clr = win.tag ? ws.s.ideologies_m.ideology_container[win.tag].color : graphics::color_rgb{ 255ui8, 255ui8, 255ui8 };
		self.set_color(ws.w.gui_m, float(clr.r) / 255.0f, float(clr.g) / 255.0f, float(clr.b) / 255.0f);
	}
	template<typename window_type>
	void ideology_item_button::windowed_update(ui::simple_button<ideology_item_button>& self, window_type & win, world_state & ws) {
		tag = win.tag;
		if(ws.w.map_view.legends->current_ideology == tag)
			self.set_frame(ws.w.gui_m, 0);
		else
			self.set_frame(ws.w.gui_m, 1);
	}
	template<typename window_type>
	void unit_type::windowed_update(ui::dynamic_icon<unit_type>& self, window_type & win, world_state & ws) {
		self.set_frame(ws.w.gui_m, bool(win.army) ? 0ui32 : 1ui32);
	}
	template<typename window_type>
	void unit_left_status::windowed_update(ui::dynamic_icon<unit_left_status>& self, window_type & win, world_state & ws) {
		if(win.army) {
			auto frac = ws.get<army::readiness>(win.army) * ws.get<army::supply>(win.army);
			if(frac > 0.75f)
				self.set_frame(ws.w.gui_m, 3ui32);
			else if(frac > 0.5f)
				self.set_frame(ws.w.gui_m, 2ui32);
			else if(frac > 0.25f)
				self.set_frame(ws.w.gui_m, 1ui32);
			else
				self.set_frame(ws.w.gui_m, 0ui32);
		} else if(win.hq) {
			auto mob = ws.get<strategic_hq::mobilization_level>(win.hq);
			self.set_frame(ws.w.gui_m, 3ui32 - uint32_t(mob));
		}
	}
	template<typename window_type>
	void unit_right_status::windowed_update(ui::dynamic_icon<unit_right_status>& self, window_type & win, world_state & ws) {
		if(win.army) {
			self.set_frame(ws.w.gui_m, 4ui32);
		} else if(win.hq) {
			auto mob = ws.get<strategic_hq::mobilization_level>(win.hq);
			self.set_frame(ws.w.gui_m, 3ui32 - uint32_t(mob));
		}
	}
	template<typename window_type>
	void unit_frame::windowed_update(ui::tinted_icon<unit_frame>& self, window_type & win, world_state & ws) {
		auto owner = [&ws, &win]() {
			if(win.army) {
				return ws.get<army::owner>(win.army);
			} else if(win.hq) {
				return ws.get<province_state::owner>(ws.get<strategic_hq::location>(win.hq));
			} else {
				return nations::country_tag();
			}
		}();
		if(win.army && ws.w.map_view.legends->selected_army == win.army) {
			self.set_color(ws.w.gui_m, 0.157f, 0.784f, 0.784f);
			ui::make_visible_immediate(*self.associated_object);
		} else if(win.hq && ws.w.map_view.legends->selected_hq == win.hq) {
			self.set_color(ws.w.gui_m, 0.157f, 0.784f, 0.784f);
			ui::make_visible_immediate(*self.associated_object);
		} else if(win.hq && ws.w.map_view.legends->selected_army && ws.get<army::hq>(ws.w.map_view.legends->selected_army) == win.hq) {
			self.set_color(ws.w.gui_m, 0.157f, 0.784f, 0.784f);
			ui::make_visible_immediate(*self.associated_object);
		} else if(military::in_war_against(ws, owner, ws.w.local_player_nation)) {
			self.set_color(ws.w.gui_m, 0.9f, 0.2f, 0.2f);
			ui::make_visible_immediate(*self.associated_object);
		} else if(owner == ws.w.local_player_nation || ws.get<nation::overlord>(owner) == ws.w.local_player_nation) {
			self.set_color(ws.w.gui_m, 0.2f, 0.9f, 0.2f);
			ui::make_visible_immediate(*self.associated_object);
		} else if(military::in_war_with(ws, owner, ws.w.local_player_nation)) {
			self.set_color(ws.w.gui_m, 0.05f, 0.75f, 0.05f);
			ui::make_visible_immediate(*self.associated_object);
		} else {
			self.set_color(ws.w.gui_m, 0.75f, 0.75f, 0.75f);
			ui::hide(*self.associated_object);
		}
	}
	template<typename window_type>
	void unit_amount::windowed_update(window_type & win, ui::tagged_gui_object box, ui::text_box_line_manager & lm, ui::text_format & fmt, world_state & ws) {
		auto amount = [&ws, &win]() {
			if(win.army) {
				return ws.get<army::current_soldiers>(win.army);
			} else if(win.hq) {
				return ws.get<strategic_hq::reserve_soldiers>(win.hq);
			} else {
				return 0.0f;
			}
		}();

		ui::add_text(ui::xy_pair{ 0,0 }, text_data::integer{ amount }, fmt, ws, box, lm);
	}
	template<typename lb_type, typename window_type>
	void unit_icon_lb::windowed_update(lb_type & lb, window_type & win, world_state & ws) {
		if(win.tag) {
			auto parmies = ws.get_range(ws.get<province_state::armies>(win.tag));
			for(auto a : parmies) {
				if(ws.w.map_view.legends->showing_neutral_units) {
					lb.add_item(ws, a, military::strategic_hq_tag());
				} else {
					auto owner = ws.get<army::owner>(a);
					if(owner == ws.w.local_player_nation
						|| ws.get<nation::overlord>(owner) == ws.w.local_player_nation
						|| military::in_war_against(ws, owner, ws.w.local_player_nation)
						|| military::in_war_with(ws, owner, ws.w.local_player_nation)) {

						lb.add_item(ws, a, military::strategic_hq_tag());
					}
				}
			}
			if(auto h = ws.get<province_state::strat_hq>(win.tag); h && ws.get<strategic_hq::location>(h) == win.tag) {
				if(ws.w.map_view.legends->showing_neutral_units) {
					lb.add_item(ws, military::army_tag(), h);
				} else {
					auto owner = ws.get<province_state::owner>(win.tag);
					if(owner == ws.w.local_player_nation
						|| ws.get<nation::overlord>(owner) == ws.w.local_player_nation
						|| military::in_war_against(ws, owner, ws.w.local_player_nation)
						|| military::in_war_with(ws, owner, ws.w.local_player_nation)) {

						lb.add_item(ws, military::army_tag(), h);
					}
				}
			}
		}
	}
	template<typename window_type>
	void unit_background::windowed_update(ui::tinted_icon<unit_background>& self, window_type & win, world_state & ws) {
		auto owner = [&ws, &win]() {
			if(win.army) {
				return ws.get<army::owner>(win.army);
			} else if(win.hq) {
				return ws.get<province_state::owner>(ws.get<strategic_hq::location>(win.hq));
			} else {
				return nations::country_tag();
			}
		}();
		assert(is_valid_index(owner));
		auto c = ws.get<nation::current_color>(owner);
		self.set_color(ws.w.gui_m, float(c.r) / 255.0f, float(c.g) / 255.0f, float(c.b) / 255.0f);
	}
}
