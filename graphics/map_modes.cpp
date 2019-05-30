#include "map_modes.hpp"
#include "provinces/province_functions.hpp"
#include "nations/nations_functions.hpp"
#include "economy/economy_functions.hpp"
#include "population/population_functions.h"

namespace map_mode {
	state::state() : legends(std::make_unique<legend_gui>()) {}
	state::~state() {}

	void default_color_province(world_state const& ws, provinces::province_tag p, uint8_t* pcolors, uint8_t* scolors) {
		if(ws.s.province_m.province_container.get<province::is_sea>(p)) {
			pcolors[0] = 80ui8;
			pcolors[1] = 80ui8;
			pcolors[2] = 255ui8;
			scolors[0] = 80ui8;
			scolors[1] = 80ui8;
			scolors[2] = 255ui8;
		} else {
			pcolors[0] = 255ui8;
			pcolors[1] = 255ui8;
			pcolors[2] = 255ui8;
			scolors[0] = 255ui8;
			scolors[1] = 255ui8;
			scolors[2] = 255ui8;
		}
	}

	void update_map_colors(world_state& ws) {
		bool map_change_expected = true;
		if(ws.w.map_view.changed.compare_exchange_strong(map_change_expected, false)) {
			const auto pcolors = ws.w.map.colors.primary_color_data();
			const auto scolors = ws.w.map.colors.secondary_color_data();

			if(pcolors == nullptr || scolors == nullptr) {
				ws.w.map_view.changed.store(false, std::memory_order_release);
				return;
			}

			auto g = ws.w.trade_w.selected_good;
			if(is_valid_index(g) && ws.w.map_view.mode == type::prices) {
				auto price_range = economy::global_price_range(ws, g);

				for(int32_t i = 0; i < ws.w.province_s.province_state_container.size(); ++i) {
					if(auto sid = provinces::province_state(ws, provinces::province_tag(provinces::province_tag::value_base_t(i)));
						bool(sid) && bool(ws.w.nation_s.states.get<::state::owner>(sid))) {


						auto local_prices = economy::state_current_prices(ws, sid);

						auto fraction = (local_prices[g] - price_range.minimum + 0.01) / (price_range.maximum - price_range.minimum + 0.01);
						pcolors[i * 3 + 0] = uint8_t((1.0f - fraction) * 255.0f);
						pcolors[i * 3 + 1] = uint8_t(fraction * 255.0f);
						pcolors[i * 3 + 2] = uint8_t(100);
						scolors[i * 3 + 0] = uint8_t((1.0f - fraction) * 255.0f);
						scolors[i * 3 + 1] = uint8_t(fraction * 255.0f);
						scolors[i * 3 + 2] = uint8_t(100);

						continue;
					}

					default_color_province(ws, provinces::province_tag(provinces::province_tag::value_base_t(i)),
						pcolors + i * 3, scolors + i * 3);
				}

			} else if(is_valid_index(g) && g != economy::gold_good && ws.w.map_view.mode == type::purchasing) {
				if(auto selected_id = ws.w.trade_w.selected_state; ws.w.nation_s.states.is_valid_index(selected_id)) {
					auto purchasing_handle = ws.w.nation_s.state_purchases.get(selected_id, g);
					auto purchases_data_range = get_range(ws.w.economy_s.purchasing_arrays, purchasing_handle);

					if(std::begin(purchases_data_range) != std::end(purchases_data_range)) {
						auto max_purchases = *std::max_element(std::begin(purchases_data_range) + 1, std::end(purchases_data_range));


						for(int32_t i = 0; i < ws.w.province_s.province_state_container.size(); ++i) {
							if(auto sid = provinces::province_state(ws, provinces::province_tag(provinces::province_tag::value_base_t(i)));
								bool(sid) && bool(ws.w.nation_s.states.get<::state::owner>(sid)) && purchases_data_range.first + to_index(sid) + 1 < purchases_data_range.second) {

								auto amount = purchases_data_range.first[to_index(sid) + 1];

								auto fraction = amount / (max_purchases + 0.000001f);
								if(fraction < 0)
									std::abort();
								pcolors[i * 3 + 0] = uint8_t(fraction * 205.0f + 50.0f);
								pcolors[i * 3 + 1] = uint8_t(fraction * 205.0f + 50.0f);
								pcolors[i * 3 + 2] = uint8_t(fraction * 205.0f + 50.0f);
								scolors[i * 3 + 0] = uint8_t(fraction * 205.0f + 50.0f);
								scolors[i * 3 + 1] = uint8_t(fraction * 205.0f + 50.0f);
								scolors[i * 3 + 2] = uint8_t(fraction * 205.0f + 50.0f);

								continue;
							}

							default_color_province(ws, provinces::province_tag(provinces::province_tag::value_base_t(i)),
								pcolors + i * 3, scolors + i * 3);
						}

					}
				}
			} else if(is_valid_index(g) && g != economy::gold_good && ws.w.map_view.mode == type::production) {
				economy::goods_qnty_type max_production = 0.0001;
				ws.w.nation_s.states.for_each([&](nations::state_tag id) {
					max_production = std::max(max_production, economy::state_current_production(ws, id, g));
				});



				for(int32_t i = 0; i < ws.w.province_s.province_state_container.size(); ++i) {
					if(auto sid = provinces::province_state(ws, provinces::province_tag(provinces::province_tag::value_base_t(i)));
						bool(sid) && bool(ws.w.nation_s.states.get<::state::owner>(sid))) {

						auto fraction = economy::state_current_production(ws, sid, g) / max_production;
						if(fraction < 0)
							std::abort();
						pcolors[i * 3 + 0] = uint8_t(fraction * 205.0f + 50.0f);
						pcolors[i * 3 + 1] = uint8_t(fraction * 205.0f + 50.0f);
						pcolors[i * 3 + 2] = uint8_t(fraction * 205.0f + 50.0f);
						scolors[i * 3 + 0] = uint8_t(fraction * 205.0f + 50.0f);
						scolors[i * 3 + 1] = uint8_t(fraction * 205.0f + 50.0f);
						scolors[i * 3 + 2] = uint8_t(fraction * 205.0f + 50.0f);

						continue;
					}

					default_color_province(ws, provinces::province_tag(provinces::province_tag::value_base_t(i)),
						pcolors + i * 3, scolors + i * 3);
				}


			} else if(auto lprov = ws.w.map_view.legends ? ws.w.map_view.legends->current_province : provinces::province_tag();
				is_valid_index(lprov) && ws.w.map_view.mode == type::distance) {

				auto pcount = ws.s.province_m.province_container.size();
				for(int32_t i = 0; i < pcount; ++i) {
					auto distance = ws.w.province_s.province_distance_to[to_index(lprov) * pcount + i];
					pcolors[i * 3 + 0] = uint8_t(std::clamp((distance / 8'000.0f - 1.0f), 0.0f, 1.0f) * 255.0f);
					pcolors[i * 3 + 1] = uint8_t(std::clamp((1.0f - distance / 8'000.0f), 0.0f, 1.0f) * 255.0f);
					pcolors[i * 3 + 2] = uint8_t(100);
					scolors[i * 3 + 0] = uint8_t(std::clamp((distance / 8'000.0f - 1.0f), 0.0f, 1.0f) * 255.0f);
					scolors[i * 3 + 1] = uint8_t(std::clamp((1.0f - distance / 8'000.0f), 0.0f, 1.0f) * 255.0f);
					scolors[i * 3 + 2] = uint8_t(100);
				}
			} else if(ws.w.map_view.mode == type::culture) {
				auto pcount = ws.s.province_m.province_container.size();
				auto const c = ws.w.map_view.legends->current_culture;
				auto const col = c ? ws.s.culture_m.culture_container[c].color : graphics::color_rgb{ 0,0,0 };

				for(int32_t i = 0; i < pcount; ++i) {
					auto const p = provinces::province_tag(provinces::province_tag::value_base_t(i));
					if(is_valid_index(p) && p != provinces::province_tag(0) && to_index(p) < ws.s.province_m.first_sea_province) {
						if(c) {
							auto total_pop = ws.w.province_s.province_state_container.get<province_state::total_population>(p);
							auto culture_pop = ws.w.province_s.province_demographics.get(p, population::to_demo_tag(ws, c));
							auto const frac = culture_pop / (total_pop > 0 ? total_pop : 1.0f);

							pcolors[i * 3 + 0] = uint8_t(std::clamp(float(col.r) * frac, 0.0f, 255.0f));
							pcolors[i * 3 + 1] = uint8_t(std::clamp(float(col.g) * frac, 0.0f, 255.0f));
							pcolors[i * 3 + 2] = uint8_t(std::clamp(float(col.b) * frac, 0.0f, 255.0f));
							scolors[i * 3 + 0] = uint8_t(std::clamp(float(col.r) * frac, 0.0f, 255.0f));
							scolors[i * 3 + 1] = uint8_t(std::clamp(float(col.g) * frac, 0.0f, 255.0f));
							scolors[i * 3 + 2] = uint8_t(std::clamp(float(col.b) * frac, 0.0f, 255.0f));
						} else {
							auto dominant_culture = ws.w.province_s.province_state_container.get<province_state::dominant_culture>(p);
							if(dominant_culture) {
								auto const dcol = ws.s.culture_m.culture_container[dominant_culture].color;

								pcolors[i * 3 + 0] = dcol.r;
								pcolors[i * 3 + 1] = dcol.g;
								pcolors[i * 3 + 2] = dcol.b;
								scolors[i * 3 + 0] = dcol.r;
								scolors[i * 3 + 1] = dcol.g;
								scolors[i * 3 + 2] = dcol.b;
							} else {
								pcolors[i * 3 + 0] = uint8_t(0);
								pcolors[i * 3 + 1] = uint8_t(0);
								pcolors[i * 3 + 2] = uint8_t(0);
								scolors[i * 3 + 0] = uint8_t(0);
								scolors[i * 3 + 1] = uint8_t(0);
								scolors[i * 3 + 2] = uint8_t(0);
							}
						}
					} else {
						default_color_province(ws, provinces::province_tag(provinces::province_tag::value_base_t(i)),
							pcolors + i * 3, scolors + i * 3);
					}
				}
			} else if(ws.w.map_view.mode == type::region) {
				auto pcount = ws.s.province_m.province_container.size();
				int32_t const total_number_of_colors = 100;
				int32_t const rel_prime = find_best_relative_prime(total_number_of_colors);

				for(int32_t i = 0; i < pcount; ++i) {
					auto const p = provinces::province_tag(provinces::province_tag::value_base_t(i));

					if(is_valid_index(p) && p != provinces::province_tag(0) && to_index(p) < ws.s.province_m.first_sea_province) {
						auto const state_id = ws.s.province_m.province_container.get<province::state_id>(p);
						auto const val = (to_index(state_id) * rel_prime) % total_number_of_colors;
						auto const icolor = graphics::hue_to_rgb(float(val) / float(total_number_of_colors));

						pcolors[i * 3 + 0] = icolor.r;
						pcolors[i * 3 + 1] = icolor.g;
						pcolors[i * 3 + 2] = icolor.b;
						scolors[i * 3 + 0] = icolor.r;
						scolors[i * 3 + 1] = icolor.g;
						scolors[i * 3 + 2] = icolor.b;
					} else {
						default_color_province(ws, provinces::province_tag(provinces::province_tag::value_base_t(i)),
							pcolors + i * 3, scolors + i * 3);
					}
				}
			} else if(ws.w.map_view.mode == type::sphere) {
				for(size_t i = 0; i < ws.s.province_m.province_container.size(); ++i) {
					const provinces::province_tag p(static_cast<provinces::province_tag::value_base_t>(i));
					if(is_valid_index(p) && p != provinces::province_tag(0) && to_index(p) < ws.s.province_m.first_sea_province) {

						if(auto owner = provinces::province_owner(ws, p); owner) {
							if(auto sl = ws.w.nation_s.nations.get<nation::sphere_leader>(owner); sl || nations::is_great_power(ws, owner)) {
								auto owner_cc = ws.w.nation_s.nations.get<nation::current_color>(sl ? sl : owner);
								pcolors[i * 3 + 0] = owner_cc.r;
								pcolors[i * 3 + 1] = owner_cc.g;
								pcolors[i * 3 + 2] = owner_cc.b;
								scolors[i * 3 + 0] = owner_cc.r;
								scolors[i * 3 + 1] = owner_cc.g;
								scolors[i * 3 + 2] = owner_cc.b;
							} else {
								pcolors[i * 3 + 0] = 0ui8;
								pcolors[i * 3 + 1] = 0ui8;
								pcolors[i * 3 + 2] = 0ui8;
								scolors[i * 3 + 0] = 0ui8;
								scolors[i * 3 + 1] = 0ui8;
								scolors[i * 3 + 2] = 0ui8;
							}
						} else {
							pcolors[i * 3 + 0] = 0ui8;
							pcolors[i * 3 + 1] = 0ui8;
							pcolors[i * 3 + 2] = 0ui8;
							scolors[i * 3 + 0] = 0ui8;
							scolors[i * 3 + 1] = 0ui8;
							scolors[i * 3 + 2] = 0ui8;
						}
					} else {
						default_color_province(ws, provinces::province_tag(provinces::province_tag::value_base_t(i)),
							pcolors + i * 3, scolors + i * 3);
					}
				}
			} else if(ws.w.map_view.mode == type::population) {
				auto const range = ws.w.map_view.legends->showing_density == false ? 
					population::get_population_range(ws) : population::get_population_density_range(ws);

				for(size_t i = 0; i < ws.s.province_m.province_container.size(); ++i) {
					const provinces::province_tag p(static_cast<provinces::province_tag::value_base_t>(i));
					if(is_valid_index(p) && p != provinces::province_tag(0) && to_index(p) < ws.s.province_m.first_sea_province) {
						auto total_pop = ws.w.province_s.province_state_container.get<province_state::total_population>(p);
						auto area = ws.s.province_m.province_container.get<province::area>(p);
						if(total_pop > 0 && area > 0) {
							auto const val = ws.w.map_view.legends->showing_density ? total_pop / area : total_pop;
							auto const frac = std::sqrt(val - range.min) / std::sqrt(range.max - range.min);

							pcolors[i * 3 + 0] = uint8_t(std::clamp(25.5f * frac, 0.0f, 255.0f));
							pcolors[i * 3 + 1] = uint8_t(std::clamp(255.0f * frac, 0.0f, 255.0f));
							pcolors[i * 3 + 2] = uint8_t(std::clamp(25.5f * frac, 0.0f, 255.0f));
							scolors[i * 3 + 0] = uint8_t(std::clamp(25.5f * frac, 0.0f, 255.0f));
							scolors[i * 3 + 1] = uint8_t(std::clamp(255.0f * frac, 0.0f, 255.0f));
							scolors[i * 3 + 2] = uint8_t(std::clamp(25.5f * frac, 0.0f, 255.0f));
						} else {
							pcolors[i * 3 + 0] = 0ui8;
							pcolors[i * 3 + 1] = 0ui8;
							pcolors[i * 3 + 2] = 0ui8;
							scolors[i * 3 + 0] = 0ui8;
							scolors[i * 3 + 1] = 0ui8;
							scolors[i * 3 + 2] = 0ui8;
						}
					} else {
						default_color_province(ws, p, pcolors + i * 3, scolors + i * 3);
					}
				}
			} else { // default case: color by ownership
				for(size_t i = 0; i < ws.s.province_m.province_container.size(); ++i) {
					const provinces::province_tag this_province(static_cast<provinces::province_tag::value_base_t>(i));

					if(auto owner = provinces::province_owner(ws, this_province); owner) {
						auto owner_cc = ws.w.nation_s.nations.get<nation::current_color>(owner);
						pcolors[i * 3 + 0] = owner_cc.r;
						pcolors[i * 3 + 1] = owner_cc.g;
						pcolors[i * 3 + 2] = owner_cc.b;
						scolors[i * 3 + 0] = owner_cc.r;
						scolors[i * 3 + 1] = owner_cc.g;
						scolors[i * 3 + 2] = owner_cc.b;
					} else {
						default_color_province(ws, provinces::province_tag(provinces::province_tag::value_base_t(i)),
							pcolors + i * 3, scolors + i * 3);
					}
				}
			}

			ws.w.map.colors.update_ready();
		}
	}

	void change_mode(world_state& ws, type new_mode) {
		if(ws.w.map_view.mode != new_mode) {
			ui::hide(*(ws.w.map_view.legends->generic_legend_window.associated_object));
			ui::hide(*(ws.w.map_view.legends->generic_gradient_legend_window.associated_object));
			ui::hide(*(ws.w.map_view.legends->population_legend_window.associated_object));

			ws.w.map_view.mode = new_mode;
			ws.w.map_view.changed.store(true, std::memory_order_release);

			ws.w.bottombar_w.update_bottombar(ws.w.gui_m);

			if(ws.w.map_view.mode == type::political || ws.w.map_view.mode == type::sphere || ws.w.map_view.mode == type::region) {
				ui::make_visible_and_update(ws.w.gui_m, *(ws.w.map_view.legends->generic_legend_window.associated_object));
			} else if(ws.w.map_view.mode == type::culture) {
				ws.w.map_view.legends->current_culture = cultures::culture_tag();
				ui::make_visible_and_update(ws.w.gui_m, *(ws.w.map_view.legends->generic_legend_window.associated_object));
			} else if(ws.w.map_view.mode == type::population) {
				ui::make_visible_and_update(ws.w.gui_m, *(ws.w.map_view.legends->population_legend_window.associated_object));
			}
		}
	}

	void on_selection(world_state& ws, provinces::province_tag p) {
		if(is_valid_index(p) && p != provinces::province_tag(0) && to_index(p) < ws.s.province_m.first_sea_province) {
			if(ws.w.map_view.mode == type::political || ws.w.map_view.mode == type::sphere || ws.w.map_view.mode == type::region
				|| ws.w.map_view.mode == type::population) {
				sound::play_interface_sound(ws, ws.s.sound_m.click_sound);
				ws.w.province_w.show_province_window(ws.w.gui_m, p);
			} else if(ws.w.map_view.mode == type::culture) {
				auto dominant_culture = ws.w.province_s.province_state_container.get<province_state::dominant_culture>(p);
				if(dominant_culture) {
					ws.w.map_view.legends->current_culture = dominant_culture;
					ui::hide(*(ws.w.map_view.legends->generic_legend_window.associated_object));
					ui::make_visible_and_update(ws.w.gui_m, *(ws.w.map_view.legends->generic_gradient_legend_window.associated_object));
				} else {
					ws.w.map_view.legends->current_culture = cultures::culture_tag();
					ui::hide(*(ws.w.map_view.legends->generic_gradient_legend_window.associated_object));
					ui::make_visible_and_update(ws.w.gui_m, *(ws.w.map_view.legends->generic_legend_window.associated_object));
				}
				ws.w.map_view.changed.store(true, std::memory_order_release);
			}
		} else {
			if(ws.w.map_view.mode == type::political || ws.w.map_view.mode == type::sphere || ws.w.map_view.mode == type::region
				|| ws.w.map_view.mode == type::population) {
				ws.w.province_w.hide_province_window(ws.w.gui_m);
			} else if(ws.w.map_view.mode == type::culture) {
				ws.w.map_view.legends->current_culture = cultures::culture_tag();
				ui::hide(*(ws.w.map_view.legends->generic_gradient_legend_window.associated_object));
				ui::make_visible_and_update(ws.w.gui_m, *(ws.w.map_view.legends->generic_legend_window.associated_object));
				ws.w.map_view.changed.store(true, std::memory_order_release);
			}
		}
	}

	void on_mouse_over(world_state& ws, provinces::province_tag p) {
		if(ws.w.map_view.mode == type::political || ws.w.map_view.mode == type::sphere || ws.w.map_view.mode == type::region) {
			if(ws.w.map_view.legends->current_province != p) {
				ws.w.map_view.legends->current_province = p;
				ui::make_visible_and_update(ws.w.gui_m, *(ws.w.map_view.legends->generic_legend_window.associated_object));
			}
		} else if(ws.w.map_view.mode == type::culture) {
			if(ws.w.map_view.legends->current_province != p) {
				ws.w.map_view.legends->current_province = p;

				if(ws.w.map_view.legends->current_culture)
					ui::make_visible_and_update(ws.w.gui_m, *(ws.w.map_view.legends->generic_gradient_legend_window.associated_object));
				else
					ui::make_visible_and_update(ws.w.gui_m, *(ws.w.map_view.legends->generic_legend_window.associated_object));
			}
		} else if(ws.w.map_view.mode == type::population) {
			ws.w.map_view.legends->current_province = p;
			ui::make_visible_and_update(ws.w.gui_m, *(ws.w.map_view.legends->population_legend_window.associated_object));
		}
	}

	void state::init(world_state & ws) {
		ui::create_static_element(ws, std::get<ui::window_tag>(ws.s.gui_m.ui_definitions.name_to_element_map["open_v2_map_legend_generic"]), ui::tagged_gui_object{ ws.w.gui_m.root, ui::gui_object_tag(0) }, ws.w.map_view.legends->generic_legend_window);
		ui::create_static_element(ws, std::get<ui::window_tag>(ws.s.gui_m.ui_definitions.name_to_element_map["open_v2_map_legend_generic_gradient"]), ui::tagged_gui_object{ ws.w.gui_m.root, ui::gui_object_tag(0) }, ws.w.map_view.legends->generic_gradient_legend_window);
		ui::create_static_element(ws, std::get<ui::window_tag>(ws.s.gui_m.ui_definitions.name_to_element_map["open_v2_map_legend_population"]), ui::tagged_gui_object{ ws.w.gui_m.root, ui::gui_object_tag(0) }, ws.w.map_view.legends->population_legend_window);
	}
	void generic_legend_title::update(ui::tagged_gui_object box, ui::text_box_line_manager & lm, ui::text_format & fmt, world_state & ws) {
		if(ws.w.map_view.mode == type::political) {
			ui::add_text(ui::xy_pair{ 0,0 }, scenario::fixed_ui::map_legend_political, fmt, ws, box, lm);
		} else if(ws.w.map_view.mode == type::culture) {
			ui::add_text(ui::xy_pair{ 0,0 }, scenario::fixed_ui::map_legend_culture , fmt, ws, box, lm);
		} else if(ws.w.map_view.mode == type::region) {
			ui::add_text(ui::xy_pair{ 0,0 }, scenario::fixed_ui::map_legend_region, fmt, ws, box, lm);
		} else if(ws.w.map_view.mode == type::sphere) {
			ui::add_text(ui::xy_pair{ 0,0 }, scenario::fixed_ui::map_legend_sphere, fmt, ws, box, lm);
		} else if(ws.w.map_view.mode == type::population) {
			ui::add_text(ui::xy_pair{ 0,0 }, scenario::fixed_ui::map_legend_population, fmt, ws, box, lm);
		}
	}
	void generic_legend_contents::update(ui::tagged_gui_object box, ui::text_box_line_manager & lm, ui::text_format & fmt, world_state & ws) {
		if(ws.w.map_view.mode == type::political) {
			if(auto const p = ws.w.map_view.legends->current_province; is_valid_index(p) && p != provinces::province_tag(0)) {
				auto cursor = ui::add_text(ui::xy_pair{ 0,0 }, ws.w.province_s.province_state_container.get<province_state::name>(p), fmt, ws, box, lm);
				auto owner = ws.w.province_s.province_state_container.get<province_state::owner>(p);
				if(owner) {
					cursor = ui::advance_cursor_to_newline(cursor, ws.s.gui_m, fmt, lm);
					cursor = ui::add_text(cursor, scenario::fixed_ui::owner_label, fmt, ws, box, lm);
					cursor = ui::advance_cursor_by_space(cursor, ws.s.gui_m, fmt);
					cursor = ui::add_text(cursor, ws.w.nation_s.nations.get<nation::name>(owner), fmt, ws, box, lm);
				}
			}
		} else if(ws.w.map_view.mode == type::culture) {
			if(auto const p = ws.w.map_view.legends->current_province; is_valid_index(p) && p != provinces::province_tag(0)) {
				auto cursor = ui::add_text(ui::xy_pair{ 0,0 }, ws.w.province_s.province_state_container.get<province_state::name>(p), fmt, ws, box, lm);
				cursor = ui::advance_cursor_to_newline(cursor, ws.s.gui_m, fmt, lm);

				auto total_pop = ws.w.province_s.province_state_container.get<province_state::total_population>(p);
				if(total_pop > 0) {
					if(auto const c = ws.w.map_view.legends->current_culture; c) {
						auto culture_pop = ws.w.province_s.province_demographics.get(p, population::to_demo_tag(ws, c));
						cursor = ui::add_text(cursor, text_data::percent{ culture_pop / total_pop }, fmt, ws, box, lm);
						cursor = ui::advance_cursor_by_space(cursor, ws.s.gui_m, fmt);
						cursor = ui::add_text(cursor, ws.s.culture_m.culture_container[c].name, fmt, ws, box, lm);
					} else {
						auto dominant_culture = ws.w.province_s.province_state_container.get<province_state::dominant_culture>(p);
						if(dominant_culture) {
							auto culture_pop = ws.w.province_s.province_demographics.get(p, population::to_demo_tag(ws, dominant_culture));
							cursor = ui::add_text(cursor, text_data::percent{ culture_pop / total_pop }, fmt, ws, box, lm);
							cursor = ui::advance_cursor_by_space(cursor, ws.s.gui_m, fmt);
							cursor = ui::add_text(cursor, ws.s.culture_m.culture_container[dominant_culture].name, fmt, ws, box, lm);
						}
					}
				}
			}
		} else if(ws.w.map_view.mode == type::region) {
			if(auto const p = ws.w.map_view.legends->current_province; is_valid_index(p) && p != provinces::province_tag(0)) {
				auto cursor = ui::add_text(ui::xy_pair{ 0,0 }, ws.w.province_s.province_state_container.get<province_state::name>(p), fmt, ws, box, lm);
				auto rgn = ws.s.province_m.province_container.get<province::state_id>(p);
				if(rgn) {
					cursor = ui::advance_cursor_to_newline(cursor, ws.s.gui_m, fmt, lm);
					cursor = ui::add_text(cursor, scenario::fixed_ui::region_label, fmt, ws, box, lm);
					cursor = ui::advance_cursor_by_space(cursor, ws.s.gui_m, fmt);
					cursor = ui::add_text(cursor, ws.s.province_m.state_names[rgn], fmt, ws, box, lm);
				}
			}
		} else if(ws.w.map_view.mode == type::sphere) {
			if(auto const p = ws.w.map_view.legends->current_province; is_valid_index(p) && p != provinces::province_tag(0)) {
				auto owner = ws.w.province_s.province_state_container.get<province_state::owner>(p);
				if(owner) {
					auto cursor = ui::add_text(ui::xy_pair{ 0,0 }, ws.w.nation_s.nations.get<nation::name>(owner), fmt, ws, box, lm);
					cursor = ui::advance_cursor_to_newline(cursor, ws.s.gui_m, fmt, lm);
					cursor = ui::add_text(cursor, scenario::fixed_ui::sphere_label, fmt, ws, box, lm);
					cursor = ui::advance_cursor_by_space(cursor, ws.s.gui_m, fmt);

					if(auto const sl = ws.w.nation_s.nations.get<nation::sphere_leader>(owner); sl)
						cursor = ui::add_text(cursor, ws.w.nation_s.nations.get<nation::name>(sl), fmt, ws, box, lm);
					else
						cursor = ui::add_text(cursor, scenario::fixed_ui::none, fmt, ws, box, lm);
				}
			}
		} else if(ws.w.map_view.mode == type::population) {
			if(auto const p = ws.w.map_view.legends->current_province; is_valid_index(p) && p != provinces::province_tag(0)) {
				auto total_pop = ws.w.province_s.province_state_container.get<province_state::total_population>(p);
		
				auto cursor = ui::add_text(ui::xy_pair{ 0,0 }, ws.w.province_s.province_state_container.get<province_state::name>(p), fmt, ws, box, lm);
				cursor = ui::advance_cursor_to_newline(cursor, ws.s.gui_m, fmt, lm);
				cursor = ui::add_text(cursor, scenario::fixed_ui::population_label, fmt, ws, box, lm);
				cursor = ui::advance_cursor_by_space(cursor, ws.s.gui_m, fmt);
				cursor = ui::add_text(cursor, text_data::integer{total_pop}, fmt, ws, box, lm);
			}
		}
	}
	void gradient_bar::update(ui::tinted_icon<gradient_bar>& self, world_state & ws) {
		if(ws.w.map_view.mode == map_mode::type::culture) {
			if(auto const c = ws.w.map_view.legends->current_culture; c) {
				auto const col = ws.s.culture_m.culture_container[c].color;
				self.set_color(ws.w.gui_m, float(col.r) / 255.0f, float(col.g) / 255.0f, float(col.b) / 255.0f);
			}
		} else if(ws.w.map_view.mode == map_mode::type::population) {
			self.set_color(ws.w.gui_m, 0.1f, 1.0f, 0.1f);
		}
	}
	void generic_legend_min::update(ui::tagged_gui_object box, ui::text_box_line_manager & lm, ui::text_format & fmt, world_state & ws) {
		if(ws.w.map_view.mode == map_mode::type::culture) {
			ui::add_text(ui::xy_pair{ 0,0 }, u"0%", fmt, ws, box, lm);
		} else if(ws.w.map_view.mode == map_mode::type::population) {
			auto const range = ws.w.map_view.legends->showing_density == false ?
				population::get_population_range(ws) : population::get_population_density_range(ws);
			ui::add_text(ui::xy_pair{ 0,0 }, text_data::integer{range.min}, fmt, ws, box, lm);
		}

		
	}
	void generic_legend_max::update(ui::tagged_gui_object box, ui::text_box_line_manager & lm, ui::text_format & fmt, world_state & ws) {
		if(ws.w.map_view.mode == map_mode::type::culture) {
			ui::add_text(ui::xy_pair{ 0,0 }, u"100%", fmt, ws, box, lm);
		} else if(ws.w.map_view.mode == map_mode::type::population) {
			auto const range = ws.w.map_view.legends->showing_density == false ?
				population::get_population_range(ws) : population::get_population_density_range(ws);
			ui::add_text(ui::xy_pair{ 0,0 }, text_data::integer{ range.max }, fmt, ws, box, lm);
		}
	}
	void population_amount_button::update(ui::simple_button<population_amount_button>& self, world_state & ws) {
		if(ws.w.map_view.legends->showing_density)
			self.set_frame(ws.w.gui_m, 0);
		else
			self.set_frame(ws.w.gui_m, 1);
	}
	void population_amount_button::button_function(ui::simple_button<population_amount_button>& self, world_state & ws) {
		ws.w.map_view.legends->showing_density = false;
		ws.w.map_view.changed.store(true, std::memory_order_release);
		ui::make_visible_and_update(ws.w.gui_m, *(ws.w.map_view.legends->population_legend_window.associated_object));
	}
	void population_density_button::update(ui::simple_button<population_density_button>& self, world_state & ws) {
		if(ws.w.map_view.legends->showing_density)
			self.set_frame(ws.w.gui_m, 1);
		else
			self.set_frame(ws.w.gui_m, 0);
	}
	void population_density_button::button_function(ui::simple_button<population_density_button>& self, world_state & ws) {
		ws.w.map_view.legends->showing_density = true;
		ws.w.map_view.changed.store(true, std::memory_order_release);
		ui::make_visible_and_update(ws.w.gui_m, *(ws.w.map_view.legends->population_legend_window.associated_object));
	}
}