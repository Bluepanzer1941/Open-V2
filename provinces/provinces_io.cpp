#include "common\\common.h"
#include "provinces\\provinces_io.h"
#include "Parsers\\parsers.hpp"
#include "object_parsing\\object_parsing.hpp"
#include "modifiers\\modifiers_io.h"
#include "soil\\SOIL.h"
#include <Windows.h>

namespace provinces {
	struct parsing_environment {
		text_data::text_sequences& text_lookup;

		province_manager& manager;
		modifiers::modifiers_manager& mod_manager;

		parsed_data climate_file;
		parsed_data terrain_file;

		parsing_environment(text_data::text_sequences& tl, province_manager& m, modifiers::modifiers_manager& mm) :
			text_lookup(tl), manager(m), mod_manager(mm) {}
	};

	parsing_state::parsing_state(text_data::text_sequences& tl, province_manager& m, modifiers::modifiers_manager& mm) :
		impl(std::make_unique<parsing_environment>(tl, m, mm)) {}
	parsing_state::~parsing_state() {}

	parsing_state::parsing_state(parsing_state&& o) noexcept : impl(std::move(o.impl)) {}

	struct empty_type {
		void add_unknown_key(int) {}
	};

	struct sea_starts {
		parsing_environment& env;
		sea_starts(parsing_environment& e) : env(e) {}

		void add_sea_start(uint16_t v) {
			env.manager.province_container[province_tag(v)].flags |= province::sea;
		}
	};

	struct default_map_file {
		parsing_environment& env;
		default_map_file(parsing_environment& e) : env(e) {}

		void discard(int) {}
		void discard_empty(const empty_type&) {}
		void set_province_count(size_t v) {
			env.manager.province_container.resize(v);
			for(uint32_t i = 0; i < v; ++i) {
				auto& p = env.manager.province_container[province_tag(static_cast<uint16_t>(i))];
				p.id = province_tag(static_cast<uint16_t>(i));

				std::string name_temp("PROV");
				name_temp += std::to_string(i);

				p.name = text_data::get_thread_safe_text_handle(env.text_lookup, name_temp.data(), name_temp.data() + name_temp.length());
			}
		}
		void handle_sea_starts(const sea_starts&) {}
	};

	struct terrain_parsing_environment {
		text_data::text_sequences& text_lookup;

		province_manager& manager;
		modifiers::modifiers_manager& mod_manager;
		color_to_terrain_map& terrain_color_map;

		terrain_parsing_environment(text_data::text_sequences& tl, province_manager& m, modifiers::modifiers_manager& mm, color_to_terrain_map& tcm) :
			text_lookup(tl), manager(m), mod_manager(mm), terrain_color_map(tcm) {}
	};

	struct color_builder {
		uint32_t current_color = 0;
		graphics::color_rgb color = { 0,0,0 };

		void add_value(int v) {
			switch(current_color) {
				case 0:
					color.r = static_cast<uint8_t>(v);
					break;
				case 1:
					color.g = static_cast<uint8_t>(v);
					break;
				case 2:
					color.b = static_cast<uint8_t>(v);
					break;
				default:
					break;
			}
			++current_color;
		}
	};

	struct color_list_builder {
		std::vector<uint8_t> colors;
		void add_value(uint8_t v) { colors.push_back(v); }
	};

	struct color_assignment {
		terrain_parsing_environment& env;

		color_list_builder colors;
		modifiers::provincial_modifier_tag tag;

		color_assignment(terrain_parsing_environment& e) : env(e) {}

		void set_type(token_and_type const& t) {
			const auto name = text_data::get_thread_safe_existing_text_handle(env.text_lookup, t.start, t.end);
			tag = tag_from_text(env.mod_manager.named_provincial_modifiers_index, name);
		}
		void discard(int) {}
	};

	inline color_assignment get_color_assignment(token_and_type const &, association_type, color_assignment&& v) {
		return std::move(v);
	}

	struct preparse_terrain_category : public modifiers::modifier_reading_base {
		terrain_parsing_environment& env;

		preparse_terrain_category(terrain_parsing_environment& e) : env(e) {}
		void add_color(const color_builder& ) {}
		void discard(int) {}
	};

	struct preparse_terrain_categories {
		terrain_parsing_environment& env;

		preparse_terrain_categories(terrain_parsing_environment& e) : env(e) {}

		void add_category(std::pair<token_and_type, preparse_terrain_category>&& p) {
			const auto name = text_data::get_thread_safe_text_handle(env.text_lookup, p.first.start, p.first.end);
			modifiers::add_provincial_modifier(name, p.second, env.mod_manager);
		}

	};

	struct preparse_terrain_file {
		terrain_parsing_environment& env;
		preparse_terrain_file(terrain_parsing_environment& e) : env(e) {}

		void add_categories(const preparse_terrain_categories&) {}
		void add_color_assignment(color_assignment const & a) {
			for(auto v : a.colors.colors)
				env.terrain_color_map.data[v] = a.tag;
		}
		void discard(int) {}
	};

	inline int discard_empty_type(const token_and_type&, association_type, const empty_type&) { return 0; }
	inline std::pair<token_and_type, preparse_terrain_category>
		bind_terrain_category(const token_and_type& t, association_type, const preparse_terrain_category& f) {
		return std::pair<token_and_type, preparse_terrain_category>(t, std::move(f));
	}

	struct state_parse {
		parsing_environment& env;
		state_tag tag;

		state_parse(parsing_environment& e) : env(e) {
			tag = e.manager.state_names.emplace_back();
		}

		void add_province(uint16_t v) {
			env.manager.province_container[province_tag(v)].state_id = tag;
			env.manager.states_to_province_index.emplace(tag, province_tag(v));
		}
	};

	struct region_file {
		parsing_environment& env;
		region_file(parsing_environment& e) : env(e) {}
		void add_state(const std::pair<token_and_type, state_tag>& p) {
			const auto name = text_data::get_thread_safe_text_handle(env.text_lookup, p.first.start, p.first.end);
			env.manager.state_names[p.second] = name;
			env.manager.named_states_index.emplace(name, p.second);
		}
	};

	inline std::pair<token_and_type, state_tag>
		bind_state(const token_and_type& t, association_type, const state_parse& f) {

		return std::pair<token_and_type, state_tag>(t, f.tag);
	}

	struct parse_continent : public modifiers::modifier_reading_base {
		parsing_environment& env;
		modifiers::provincial_modifier_tag tag;

		parse_continent(parsing_environment& e) : env(e) {
			tag = e.mod_manager.provincial_modifiers.emplace_back();
			e.mod_manager.provincial_modifiers[tag].id = tag;
		}

		void add_continent_provinces(const std::vector<uint16_t>& v) {
			for(auto i : v) {
				env.manager.province_container[province_tag(i)].continent = tag;
			}
		}
	};

	struct continents_parse_file {
		parsing_environment& env;
		continents_parse_file(parsing_environment& e) : env(e) {}

		void add_continent(const std::pair<token_and_type, modifiers::provincial_modifier_tag>& p) {
			const auto name = text_data::get_thread_safe_text_handle(env.text_lookup, p.first.start, p.first.end);
			env.mod_manager.named_provincial_modifiers_index.emplace(name, p.second);
			env.mod_manager.provincial_modifiers[p.second].name = name;
		}
	};

	inline std::pair<token_and_type, modifiers::provincial_modifier_tag>
		bind_continent(const token_and_type& t, association_type, parse_continent&& f) {

		modifiers::set_provincial_modifier(f.tag, f, f.env.mod_manager);
		return std::pair<token_and_type, modifiers::provincial_modifier_tag>(t, f.tag);
	}

	inline modifiers::provincial_modifier_tag get_or_make_prov_modifier(text_data::text_tag name, modifiers::modifiers_manager& m) {
		const auto existing_tag = m.named_provincial_modifiers_index.find(name);
		if(existing_tag != m.named_provincial_modifiers_index.end()) {
			return existing_tag->second;
		} else {
			const auto ntag = m.provincial_modifiers.emplace_back();
			m.provincial_modifiers[ntag].id = ntag;
			m.provincial_modifiers[ntag].name = name;
			m.named_provincial_modifiers_index.emplace(name, ntag);
			return ntag;
		}
	}
	struct climate_province_values {
		std::vector<uint16_t> values;
		void add_province(uint16_t v) {
			values.push_back(v);
		}
	};

	int add_individual_climate(const token_group* s, const token_group* e, const token_and_type& t, parsing_environment& env);

	struct climate_pre_parse_file {
		parsing_environment& env;
		climate_pre_parse_file(parsing_environment& e) : env(e) {}

		void add_climate(int) {}
	};

	inline std::pair<token_and_type, float> full_to_tf_pair(const token_and_type& t, association_type, const token_and_type& r) {
		return std::pair<token_and_type, float>(t, token_to<float>(r));
	}
}

MEMBER_DEF(provinces::color_assignment, colors, "color");
MEMBER_FDEF(provinces::color_assignment, discard, "discard");
MEMBER_FDEF(provinces::color_assignment, set_type, "type");
MEMBER_FDEF(provinces::color_list_builder, add_value, "value");
MEMBER_FDEF(provinces::preparse_terrain_file, add_color_assignment, "color_assignment");
MEMBER_FDEF(provinces::empty_type, add_unknown_key, "unknown_key");
MEMBER_FDEF(provinces::default_map_file, discard, "unknown_key");
MEMBER_FDEF(provinces::default_map_file, discard_empty, "discard_empty");
MEMBER_FDEF(provinces::default_map_file, set_province_count, "max_provinces");
MEMBER_FDEF(provinces::default_map_file, handle_sea_starts, "sea_starts");
MEMBER_FDEF(provinces::sea_starts, add_sea_start, "add_sea_start");
MEMBER_FDEF(provinces::color_builder, add_value, "color");
MEMBER_FDEF(provinces::preparse_terrain_file, add_categories, "categories");
MEMBER_FDEF(provinces::preparse_terrain_file, discard, "unknown_key");
MEMBER_FDEF(provinces::preparse_terrain_categories, add_category, "category");
MEMBER_FDEF(provinces::preparse_terrain_category, add_color, "color");
MEMBER_DEF(provinces::preparse_terrain_category, icon, "icon");
MEMBER_FDEF(provinces::preparse_terrain_category, discard, "discard");
MEMBER_FDEF(provinces::preparse_terrain_category, add_attribute, "attribute");
MEMBER_FDEF(provinces::region_file, add_state, "state");
MEMBER_FDEF(provinces::state_parse, add_province, "province");
MEMBER_FDEF(provinces::continents_parse_file, add_continent, "continent");
MEMBER_FDEF(provinces::parse_continent, discard, "unknown_key");
MEMBER_FDEF(provinces::parse_continent, add_continent_provinces, "provinces");
MEMBER_DEF(provinces::parse_continent, icon, "icon");
MEMBER_FDEF(provinces::parse_continent, add_attribute, "attribute");
MEMBER_FDEF(provinces::climate_pre_parse_file, add_climate, "climate");
MEMBER_FDEF(provinces::climate_province_values, add_province, "value");


namespace provinces {
	BEGIN_DOMAIN(default_map_domain)
		BEGIN_TYPE(empty_type)
		MEMBER_VARIABLE_ASSOCIATION("unknown_key", accept_all, discard_from_full)
		MEMBER_VARIABLE_TYPE_ASSOCIATION("unknown_key", accept_all, empty_type, discard_empty_type)
		END_TYPE
		BEGIN_TYPE(sea_starts)
		MEMBER_VARIABLE_ASSOCIATION("add_sea_start", accept_all, value_from_lh<uint16_t>)
		END_TYPE
		BEGIN_TYPE(default_map_file)
		MEMBER_ASSOCIATION("max_provinces", "max_provinces", value_from_rh<uint32_t>)
		MEMBER_TYPE_ASSOCIATION("sea_starts", "sea_starts", sea_starts)
		MEMBER_TYPE_ASSOCIATION("discard_empty", "border_heights", empty_type)
		MEMBER_TYPE_ASSOCIATION("discard_empty", "terrain_sheet_heights", empty_type)
		MEMBER_ASSOCIATION("unknown_key", "definitions", discard_from_rh)
		MEMBER_ASSOCIATION("unknown_key", "provinces", discard_from_rh)
		MEMBER_ASSOCIATION("unknown_key", "positions", discard_from_rh)
		MEMBER_ASSOCIATION("unknown_key", "terrain", discard_from_rh)
		MEMBER_ASSOCIATION("unknown_key", "rivers", discard_from_rh)
		MEMBER_ASSOCIATION("unknown_key", "terrain_definition", discard_from_rh)
		MEMBER_ASSOCIATION("unknown_key", "tree_definition", discard_from_rh)
		MEMBER_ASSOCIATION("unknown_key", "continent", discard_from_rh)
		MEMBER_ASSOCIATION("unknown_key", "adjacencies", discard_from_rh)
		MEMBER_ASSOCIATION("unknown_key", "region", discard_from_rh)
		MEMBER_ASSOCIATION("unknown_key", "region_sea", discard_from_rh)
		MEMBER_ASSOCIATION("unknown_key", "province_flag_sprite", discard_from_rh)
		MEMBER_ASSOCIATION("unknown_key", "tree", discard_from_rh)
		MEMBER_ASSOCIATION("unknown_key", "border_cutoff", discard_from_rh)
		END_TYPE
	END_DOMAIN;

	BEGIN_DOMAIN(preparse_terrain_domain)
		BEGIN_TYPE(color_assignment)
		MEMBER_ASSOCIATION("discard", "priority", discard_from_rh)
		MEMBER_ASSOCIATION("discard", "has_texture", discard_from_rh)
		MEMBER_ASSOCIATION("type", "type", token_from_rh)
		MEMBER_TYPE_ASSOCIATION("color", "color", color_list_builder)
		END_TYPE
		BEGIN_TYPE(preparse_terrain_file)
		MEMBER_TYPE_ASSOCIATION("categories", "categories", preparse_terrain_categories)
		MEMBER_VARIABLE_ASSOCIATION("unknown_key", accept_all, discard_from_full)
		MEMBER_VARIABLE_TYPE_ASSOCIATION("color_assignment", accept_all, color_assignment, get_color_assignment)
		END_TYPE
		BEGIN_TYPE(preparse_terrain_categories)
		MEMBER_VARIABLE_TYPE_ASSOCIATION("category", accept_all, preparse_terrain_category, bind_terrain_category)
		END_TYPE
		BEGIN_TYPE(preparse_terrain_category)
		MEMBER_TYPE_ASSOCIATION("color", "color", color_builder)
		MEMBER_ASSOCIATION("icon", "icon", value_from_rh<uint32_t>)
		MEMBER_VARIABLE_ASSOCIATION("attribute", accept_all, full_to_tf_pair)
		MEMBER_ASSOCIATION("discard", "is_water", discard_from_rh)
		END_TYPE
		BEGIN_TYPE(color_builder)
		MEMBER_VARIABLE_ASSOCIATION("color", accept_all, value_from_lh<int>)
		END_TYPE
		BEGIN_TYPE(color_list_builder)
		MEMBER_VARIABLE_ASSOCIATION("value", accept_all, value_from_lh<uint8_t>)
		END_TYPE
		END_DOMAIN;

	BEGIN_DOMAIN(read_states_domain)
		BEGIN_TYPE(region_file)
		MEMBER_VARIABLE_TYPE_ASSOCIATION("state", accept_all, state_parse, bind_state)
		END_TYPE
		BEGIN_TYPE(state_parse)
		MEMBER_VARIABLE_ASSOCIATION("province", accept_all, value_from_lh<uint16_t>)
		END_TYPE
		END_DOMAIN;

	BEGIN_DOMAIN(continents_domain)
		BEGIN_TYPE(continents_parse_file)
		MEMBER_VARIABLE_TYPE_ASSOCIATION("continent", accept_all, parse_continent, bind_continent)
		END_TYPE
		BEGIN_TYPE(parse_continent)
		MEMBER_TYPE_ASSOCIATION("provinces", "provinces", std::vector<uint16_t>)
		MEMBER_ASSOCIATION("icon", "icon", value_from_rh<uint32_t>)
		MEMBER_VARIABLE_ASSOCIATION("attribute", accept_all, full_to_tf_pair)
		END_TYPE
		BEGIN_TYPE(std::vector<uint16_t>)
		MEMBER_VARIABLE_ASSOCIATION("this", accept_all, value_from_lh<uint16_t>)
		END_TYPE
		END_DOMAIN;

	BEGIN_DOMAIN(preparse_climate_domain)
		BEGIN_TYPE(climate_pre_parse_file)
		MEMBER_VARIABLE_TYPE_EXTERN("climate", accept_all, int, add_individual_climate)
		END_TYPE
		BEGIN_TYPE(climate_province_values)
		MEMBER_VARIABLE_ASSOCIATION("value", accept_all, value_from_lh<uint16_t>)
		END_TYPE
		END_DOMAIN;

	int add_individual_climate(const token_group* s, const token_group* e, const token_and_type& t, parsing_environment& env) {
		const auto name = text_data::get_thread_safe_text_handle(env.text_lookup, t.start, t.end);
		const auto fr = env.mod_manager.named_provincial_modifiers_index.find(name);
		if(fr == env.mod_manager.named_provincial_modifiers_index.end()) {
			modifiers::parse_provincial_modifier(name, env.mod_manager, s, e);
		} else {
			const auto vals = parse_object<climate_province_values, preparse_climate_domain>(s, e, env);
			for(auto v : vals.values)
				env.manager.province_container[province_tag(v)].climate = fr->second;
		}
		return 0;
	}

	void read_default_map_file(
		parsing_state& state,
		const directory& source_directory) {

		const auto map_dir = source_directory.get_directory(u"\\map");
		parsed_data main_results;

		const auto fi = map_dir.open_file(u"default.map");

		if(fi) {
			const auto sz = fi->size();
			main_results.parse_data = std::unique_ptr<char[]>(new char[sz]);

			fi->read_to_buffer(main_results.parse_data.get(), sz);
			parse_pdx_file(main_results.parse_results, main_results.parse_data.get(), main_results.parse_data.get() + sz);

			if(main_results.parse_results.size() > 0) {
				parse_object<default_map_file, default_map_domain>(
					&main_results.parse_results[0],
					&main_results.parse_results[0] + main_results.parse_results.size(),
					*state.impl);
			}
		}
	}

	color_to_terrain_map read_terrain(
			parsing_state& state,
			const directory& source_directory) {

		const auto map_dir = source_directory.get_directory(u"\\map");
		auto& main_results = state.impl->terrain_file;
		color_to_terrain_map result_map;

		terrain_parsing_environment tstate(state.impl->text_lookup, state.impl->manager, state.impl->mod_manager, result_map);

		const auto fi = map_dir.open_file(u"terrain.txt");

		if(fi) {
			const auto sz = fi->size();
			main_results.parse_data = std::unique_ptr<char[]>(new char[sz]);

			fi->read_to_buffer(main_results.parse_data.get(), sz);
			parse_pdx_file(main_results.parse_results, main_results.parse_data.get(), main_results.parse_data.get() + sz);

			if(main_results.parse_results.size() > 0) {
				parse_object<preparse_terrain_file, preparse_terrain_domain>(
					&main_results.parse_results[0],
					&main_results.parse_results[0] + main_results.parse_results.size(),
					tstate);
			}
		}

		return result_map;
	}

	void read_states(
		parsing_state& state,
		const directory& source_directory) {

		const auto map_dir = source_directory.get_directory(u"\\map");
		parsed_data main_results;

		const auto fi = map_dir.open_file(u"region.txt");

		if(fi) {
			const auto sz = fi->size();
			main_results.parse_data = std::unique_ptr<char[]>(new char[sz]);

			fi->read_to_buffer(main_results.parse_data.get(), sz);
			parse_pdx_file(main_results.parse_results, main_results.parse_data.get(), main_results.parse_data.get() + sz);

			if(main_results.parse_results.size() > 0) {
				parse_object<region_file, read_states_domain>(
					&main_results.parse_results[0],
					&main_results.parse_results[0] + main_results.parse_results.size(),
					*state.impl);
			}
		}
	}

	void read_continents(
		parsing_state& state,
		const directory& source_directory) {

		const auto map_dir = source_directory.get_directory(u"\\map");
		parsed_data main_results;

		const auto fi = map_dir.open_file(u"continent.txt");

		if(fi) {
			const auto sz = fi->size();
			main_results.parse_data = std::unique_ptr<char[]>(new char[sz]);

			fi->read_to_buffer(main_results.parse_data.get(), sz);
			parse_pdx_file(main_results.parse_results, main_results.parse_data.get(), main_results.parse_data.get() + sz);

			if(main_results.parse_results.size() > 0) {
				parse_object<continents_parse_file, continents_domain>(
					&main_results.parse_results[0],
					&main_results.parse_results[0] + main_results.parse_results.size(),
					*state.impl);
			}
		}
	}

	void read_climates(
		parsing_state& state,
		const directory& source_directory) {

		const auto map_dir = source_directory.get_directory(u"\\map");
		auto& main_results = state.impl->climate_file;

		const auto fi = map_dir.open_file(u"climate.txt");

		if(fi) {
			const auto sz = fi->size();
			main_results.parse_data = std::unique_ptr<char[]>(new char[sz]);

			fi->read_to_buffer(main_results.parse_data.get(), sz);
			parse_pdx_file(main_results.parse_results, main_results.parse_data.get(), main_results.parse_data.get() + sz);

			if(main_results.parse_results.size() > 0) {
				parse_object<climate_pre_parse_file, preparse_climate_domain>(
					&main_results.parse_results[0],
					&main_results.parse_results[0] + main_results.parse_results.size(),
					*state.impl);
			}
		}
	}
	boost::container::flat_map<uint32_t, province_tag> read_province_definition_file(directory const & source_directory) {
		boost::container::flat_map<uint32_t, province_tag> t;

		const auto map_dir = source_directory.get_directory(u"\\map");
		const auto fi = map_dir.open_file(u"definition.csv");

		if(fi) {
			const auto sz = fi->size();
			const auto parse_data = std::unique_ptr<char[]>(new char[sz]);

			char* position = parse_data.get();
			fi->read_to_buffer(position, sz);

			while(position < parse_data.get() + sz) {
				position = parse_fixed_amount_csv_values<4>(position, parse_data.get() + sz, ';',
					[&t](std::pair<char*, char*> const* values) {
					if(is_integer(values[0].first, values[0].second)) {
						t.emplace(rgb_to_prov_index(
							uint8_t(parse_int(values[1].first, values[1].second)),
							uint8_t(parse_int(values[2].first, values[2].second)),
							uint8_t(parse_int(values[3].first, values[3].second))),
							province_tag(province_tag::value_base_t(parse_int(values[0].first, values[0].second))));
					}
				});
			}

		}
		return t;
	}

	tagged_vector<uint8_t, province_tag> load_province_map_data(province_manager& m, directory const& root) {
		const auto map_dir = root.get_directory(u"\\map");
		
		auto map_peek = map_dir.peek_file(u"provinces.bmp");
		if(!map_peek)
			map_peek = map_dir.peek_file(u"provinces.png");
		if(map_peek) {
			auto fi = map_peek->open_file();
			if(fi) {
				const auto sz = fi->size();
				std::unique_ptr<char[]> file_data = std::unique_ptr<char[]>(new char[sz]);
				fi->read_to_buffer(file_data.get(), sz);

				int32_t channels = 3;
				const auto raw_data = SOIL_load_image_from_memory((unsigned char*)(file_data.get()), static_cast<int32_t>(sz), &m.province_map_width, &m.province_map_height, &channels, 3);
				m.province_map_data.resize(static_cast<size_t>(m.province_map_width * m.province_map_height));

				const auto color_mapping = read_province_definition_file(root);

				const auto last = m.province_map_width * m.province_map_height - 1;
				uint32_t previous_color_index = provinces::rgb_to_prov_index(raw_data[last * 3 + 0], raw_data[last * 3 + 1], raw_data[last * 3 + 2]);
				province_tag prev_result = province_tag(0);
				if(auto it = color_mapping.find(previous_color_index); it != color_mapping.end()) {
					prev_result = it->second;
					m.province_map_data[static_cast<size_t>(last)] = to_index(it->second);
				}

				for(int32_t t = m.province_map_width * m.province_map_height - 2; t >= 0; --t) {
					uint32_t color_index = provinces::rgb_to_prov_index(raw_data[t * 3 + 0], raw_data[t * 3 + 1], raw_data[t * 3 + 2]);
					if(color_index == previous_color_index) {
						m.province_map_data[static_cast<size_t>(t)] = to_index(prev_result);
					} else {
						previous_color_index = color_index;
						if(auto it = color_mapping.find(color_index); it != color_mapping.end())
							m.province_map_data[static_cast<size_t>(t)] = to_index(it->second);
						else
							m.province_map_data[static_cast<size_t>(t)] = 0ui16;
						prev_result = m.province_map_data[static_cast<size_t>(t)];
					}
				}


				SOIL_free_image_data(raw_data);
			}
		}

		auto terrain_peek = map_dir.peek_file(u"terrain.bmp");
		if(terrain_peek) {
			auto fi = terrain_peek->open_file();
			if(fi) {
				const auto sz = fi->size();
				std::unique_ptr<char[]> file_data = std::unique_ptr<char[]>(new char[sz]);
				fi->read_to_buffer(file_data.get(), sz);

				BITMAPFILEHEADER header;
				memcpy(&header, file_data.get(), sizeof(BITMAPFILEHEADER));
				BITMAPINFOHEADER info_header;
				memcpy(&info_header, file_data.get() + sizeof(BITMAPFILEHEADER), sizeof(BITMAPINFOHEADER));

				if(info_header.biHeight != m.province_map_height || info_header.biWidth != m.province_map_width)
					std::abort();

				return generate_province_terrain_inverse(m.province_container.size(), m.province_map_data.data(), (uint8_t const*)(file_data.get() + header.bfOffBits), info_header.biHeight, info_header.biWidth);


			}
		} else {
			terrain_peek = map_dir.peek_file(u"terrain.png");
			if(terrain_peek) {
				auto fi = terrain_peek->open_file();
				if(fi) {
					const auto sz = fi->size();
					std::unique_ptr<char[]> file_data = std::unique_ptr<char[]>(new char[sz]);
					fi->read_to_buffer(file_data.get(), sz);

					int32_t terrain_width = 0;
					int32_t terrain_height = 0;
					int32_t channels = 1;
					const auto raw_data = SOIL_load_image_from_memory((unsigned char*)(file_data.get()), static_cast<int32_t>(sz), &terrain_width, &terrain_height, &channels, 1);

					if(terrain_height != m.province_map_height || terrain_width != m.province_map_width)
						std::abort();

					const auto t_vector = generate_province_terrain(m.province_container.size(), m.province_map_data.data(), (uint8_t const*)(raw_data), terrain_height, terrain_width);

					SOIL_free_image_data(raw_data);

					return t_vector;
				}
			}
		}

		return tagged_vector<uint8_t, province_tag>();
	}

	tagged_vector<uint8_t, province_tag> generate_province_terrain_inverse(size_t province_count, uint16_t const* province_map_data, uint8_t const* terrain_color_map_data, int32_t height, int32_t width) {
		tagged_vector<uint8_t, province_tag> terrain_out;
		tagged_vector<int32_t, province_tag> count;

		terrain_out.resize(province_count);
		count.resize(province_count);


		for(int32_t j = height - 1; j >= 0; --j) {
			for(int32_t i = width - 1; i >= 0; --i) {
				auto this_province = province_tag(province_map_data[j * width + i]);
				const auto t_color = terrain_color_map_data[(height - j - 1) * width + i];
				if(count[this_province] == 0) {
					terrain_out[this_province] = t_color;
					count[this_province] = 1;
				} else if(terrain_out[this_province] == t_color) {
					count[this_province] += 1;
				} else {
					count[this_province] -= 1;
				}
			}
		}

		return terrain_out;
	}

	tagged_vector<uint8_t, province_tag> generate_province_terrain(size_t province_count, uint16_t const* province_map_data, uint8_t const* terrain_color_map_data, int32_t height, int32_t width) {
		tagged_vector<uint8_t, province_tag> terrain_out;
		tagged_vector<int32_t, province_tag> count;

		terrain_out.resize(province_count);
		count.resize(province_count);
		
		for(int32_t i = height  * width - 1; i >= 0; --i) {
			auto this_province = province_tag(province_map_data[i]);
			const auto t_color = terrain_color_map_data[i];
			if(count[this_province] == 0) {
				terrain_out[this_province] = t_color;
				count[this_province] = 1;
			} else if(terrain_out[this_province] == t_color) {
				count[this_province] += 1;
			} else {
				count[this_province] -= 1;
			}
			
		}

		return terrain_out;
	}

	void assign_terrain_color(province_manager& m, tagged_vector<uint8_t, province_tag> const & terrain_colors, color_to_terrain_map const & terrain_map) {
		int32_t max_province = static_cast<int32_t>(m.province_container.size()) - 1;
		for(int32_t i = max_province; i >= 0; --i) {
			const auto this_province = province_tag(static_cast<province_tag::value_base_t>(i));
			const auto this_t_color = terrain_colors[this_province];
			m.province_container[this_province].terrain = terrain_map.data[this_t_color];
		}
	}
}
