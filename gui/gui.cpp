#include "gui.hpp"
#include "graphics_objects\\graphics_objects.h"
#include "graphics\\open_gl_wrapper.h"
#include <algorithm>
#include "boost\\container\\small_vector.hpp"

#ifdef _DEBUG
#include "Windows.h"
#undef min
#undef max
#endif

ui::gui_manager::~gui_manager() {};

void ui::gui_manager::destroy(gui_object & g) {
	if (&(gui_objects.at(tooltip)) == &g) {
		hide_tooltip();
		tooltip = gui_object_tag();
	}
	if (&(gui_objects.at(tooltip)) == &g)
		focus = gui_object_tag();

	if (g.associated_behavior) {
		if ((g.flags.load(std::memory_order_relaxed) & gui_object::dynamic_behavior) != 0) {
			g.associated_behavior->~gui_behavior();
			concurrent_allocator<gui_behavior>().deallocate(g.associated_behavior, 1);
		}
		g.associated_behavior = nullptr;
	}
	const auto type_handle = g.type_dependant_handle.load(std::memory_order_relaxed);
	if (type_handle != 0) {
		const auto flags = g.flags.load(std::memory_order_relaxed);
		if ((flags & gui_object::type_mask) == gui_object::type_text_instance) {
			text_instances.free(text_instance_tag(type_handle));
		} else if ((flags & gui_object::type_mask) == gui_object::type_graphics_object) {
			graphics_instances.free(graphics_instance_tag(type_handle));
		} else if (((flags & gui_object::type_mask) == gui_object::type_masked_flag) |
			((flags & gui_object::type_mask) == gui_object::type_progress_bar)) {
			multi_texture_instances.free(multi_texture_instance_tag(type_handle));
		}
	}

	gui_object_tag child = g.first_child;
	g.first_child = gui_object_tag();

	while (child != gui_object_tag()) {
		gui_object_tag next = gui_objects.at(child).right_sibling;
		gui_objects.free(child, *this);
		child = next;
	}

	g.parent = gui_object_tag();
	g.left_sibling = gui_object_tag();
	g.right_sibling = gui_object_tag();
	g.flags.store(0, std::memory_order_release);
	g.type_dependant_handle.store(0, std::memory_order_release);
}

namespace ui {
	namespace detail {
		enum class sub_alignment {
			base, center, extreme
		};

		int32_t position_from_subalignment(int32_t container, int32_t base_position, sub_alignment align);
		sub_alignment vertical_subalign(ui::alignment align);
		sub_alignment horizontal_subalign(ui::alignment align);

		int32_t position_from_subalignment(int32_t container, int32_t base_position, sub_alignment align) {
			switch (align) {
				case sub_alignment::base:
					return base_position;
				case sub_alignment::center:
					return container / 2 + base_position;
				case sub_alignment::extreme:
					return container + base_position;
			}
		}

		sub_alignment vertical_subalign(ui::alignment align) {
			switch (align) {
				case alignment::top_left:
				case alignment::top_center:
				case alignment::top_right:
					return sub_alignment::base;
				case alignment::left:
				case alignment::center:
				case alignment::right:
					return sub_alignment::center;
				case alignment::bottom_left:
				case alignment::bottom_center:
				case alignment::bottom_right:
					return sub_alignment::extreme;
			}
		}

		sub_alignment horizontal_subalign(ui::alignment align) {
			switch (align) {
				case alignment::top_left:
				case alignment::left:
				case alignment::bottom_left:
					return sub_alignment::base;
				case alignment::top_center:
				case alignment::center:
				case alignment::bottom_center:
					return sub_alignment::center;
				case alignment::top_right:
				case alignment::right:
				case alignment::bottom_right:
					return sub_alignment::extreme;
			}
		}
	}
	gui_behavior::~gui_behavior() {
		if (associated_object) associated_object->associated_behavior = nullptr;
		associated_object = nullptr;
	}
}

ui::xy_pair ui::detail::position_with_alignment(ui::xy_pair container_size, ui::xy_pair raw_position, ui::alignment align) {
	return ui::xy_pair{
		(int16_t)position_from_subalignment(static_cast<int32_t>(container_size.x), static_cast<int32_t>(raw_position.x), ui::detail::horizontal_subalign(align)),
		(int16_t)position_from_subalignment(static_cast<int32_t>(container_size.y), static_cast<int32_t>(raw_position.y), ui::detail::vertical_subalign(align))
	};
}

namespace ui {
	text_data::alignment text_aligment_from_button_definition(const button_def& def) {
		switch (def.flags & button_def::format_mask) {
			case button_def::format_center:
				return text_data::alignment::center;
			case button_def::format_left:
				return text_data::alignment::left;
			default:
				return text_data::alignment::center;
		}
	}
	text_data::alignment text_aligment_from_text_definition(const text_def& def) {
		switch (def.flags & text_def::format_mask) {
			case text_def::format_center:
				return text_data::alignment::top_center;
			case text_def::format_left:
				return text_data::alignment::top_left;
			case text_def::format_right:
				return text_data::alignment::top_right;
			case text_def::format_justified:
				return text_data::alignment::top_left;
			default:
				return text_data::alignment::top_left;
		}
	}
	ui::text_color text_color_to_ui_text_color(text_data::text_color c) {
		switch (c) {
			case text_data::text_color::black:
				return ui::text_color::black;
			case text_data::text_color::green:
				return ui::text_color::green;
			case text_data::text_color::red:
				return ui::text_color::red;
			case text_data::text_color::unspecified:
				return ui::text_color::black;
			case text_data::text_color::white:
				return ui::text_color::white;
			case text_data::text_color::yellow:
				return ui::text_color::yellow;
		}
	}

	void unmanaged_region_scollbar::on_position(int32_t pos) {
		contents_frame.position.y = static_cast<int16_t>(-pos);
	}

	bool line_manager::exceeds_extent(int32_t w) const { return w > max_line_extent; }

	void line_manager::add_object(gui_object * o) {
		current_line.push_back(o);
	}

	void line_manager::finish_current_line() {
		if ((align == text_data::alignment::bottom_left) | (align == text_data::alignment::top_left) | (align == text_data::alignment::left))
			return;

		int32_t total_element_width = 0;
		for (auto p : current_line)
			total_element_width += p->size.x;

		const int32_t adjustment = (max_line_extent - total_element_width) /
			((align == text_data::alignment::bottom_right) | (align == text_data::alignment::top_right) | (align == text_data::alignment::right) ? 1 : 2);
		for (auto p : current_line)
			p->position.x += adjustment;

		current_line.clear();
	}

	bool draggable_region::on_drag(gui_object_tag , gui_manager &, const mouse_drag &m) {
		associated_object->position.x = static_cast<int16_t>(base_position.x + m.x);
		associated_object->position.y = static_cast<int16_t>(base_position.y + m.y);
		return true;
	}

	bool draggable_region::on_get_focus(gui_object_tag t, gui_manager& m) {
		base_position = associated_object->position;
		ui::move_to_front(m, ui::tagged_gui_object{ *associated_object, t });
		return true;
	}
}


tagged_object<ui::text_instance, ui::text_instance_tag> ui::create_text_instance(ui::gui_manager &container, tagged_gui_object new_gobj, const text_format& fmt) {
	const auto new_text_instance = container.text_instances.emplace();

	new_gobj.object.flags.store(gui_object::type_text_instance | gui_object::visible | gui_object::enabled, std::memory_order_release);
	new_gobj.object.type_dependant_handle.store(to_index(new_text_instance.id), std::memory_order_release);

	new_text_instance.object.color = fmt.color;
	new_text_instance.object.font_handle = fmt.font_handle;
	new_text_instance.object.size = static_cast<uint8_t>(fmt.font_size / 2);
	new_text_instance.object.length = 0;

	return new_text_instance;
}


float ui::text_component_width(text_data::text_component& c, const std::vector<char16_t>& text_data, graphics::font& this_font, uint32_t font_size) {
	if (std::holds_alternative<text_data::text_chunk>(c)) {
		const auto chunk = std::get<text_data::text_chunk>(c);
		return this_font.metrics_text_extent(text_data.data() + chunk.offset, chunk.length, ui::detail::font_size_to_render_size(this_font, static_cast<int32_t>(font_size)), false);
	} else {
		return 0.0f;
	}
}

void ui::shorten_text_instance_to_space(ui::text_instance& txt) {
	int32_t i = int32_t(txt.length) - 2;
	for (; i >= 0; --i) {
		if (txt.text[i] == u' ')
			break;
	}
	if (i >= 0)
		txt.length = static_cast<uint8_t>(i + 1);
}
void ui::detail::create_multiline_text(gui_manager& manager, tagged_gui_object container, text_data::text_tag text_handle, text_data::alignment align, const text_format& fmt, const text_data::replacement* candidates, uint32_t count) {
	graphics::font& this_font = manager.fonts.at(fmt.font_handle);
	const auto& components = manager.text_data_sequences.all_sequences[text_handle];

	const auto components_start = manager.text_data_sequences.all_components.data() + components.starting_component;
	const auto components_end = components_start + components.component_count;

	ui::xy_pair position{ 0, 0 };
	ui::text_color current_color = fmt.color;

	ui::line_manager lm(align, container.object.size.x);
	
	for (auto component_i = components_start; component_i != components_end; ++component_i) {
		if (std::holds_alternative<text_data::color_change>(*component_i)) {
			current_color = text_color_to_ui_text_color(std::get<text_data::color_change>(*component_i).color);
		} else if (std::holds_alternative<text_data::line_break>(*component_i)) {
			lm.finish_current_line();
			position.x = 0;
			position.y += this_font.line_height(ui::detail::font_size_to_render_size(this_font, static_cast<int32_t>(fmt.font_size))) + 0.5f;
		} else if (std::holds_alternative<text_data::value_placeholder>(*component_i)) {
			const auto rep = text_data::find_replacement(std::get<text_data::value_placeholder>(*component_i), candidates, count);

			const auto replacement_text = rep ? std::get<1>(*rep) : vector_backed_string<char16_t>(text_data::name_from_value_type(std::get<text_data::value_placeholder>(*component_i).value));

			const text_format format{ current_color, fmt.font_handle, fmt.font_size };
			if (rep) 
				position = text_chunk_to_instances(manager, replacement_text, container, position, format, lm, std::get<2>(*rep));
			else
				position = text_chunk_to_instances(manager, replacement_text, container, position, format, lm);
		} else if (std::holds_alternative<text_data::text_chunk>(*component_i)) {
			const auto chunk = std::get<text_data::text_chunk>(*component_i);
			const text_format format{ current_color, fmt.font_handle, fmt.font_size };

			position = text_chunk_to_instances(manager, chunk, container, position, format, lm);
		}
	}

	lm.finish_current_line();
	container.object.size.y = position.y + static_cast<int16_t>(this_font.line_height(ui::detail::font_size_to_render_size(this_font, static_cast<int32_t>(fmt.font_size))) + 0.5f);
}

void ui::detail::create_linear_text(gui_manager& manager, tagged_gui_object container, text_data::text_tag text_handle, text_data::alignment align, const text_format& fmt, const text_data::replacement* candidates, uint32_t count) {
	const auto& components = manager.text_data_sequences.all_sequences[text_handle];
	graphics::font& this_font = manager.fonts.at(fmt.font_handle);

	const auto components_start = manager.text_data_sequences.all_components.data() + components.starting_component;
	const auto components_end = components_start + components.component_count;

	ui::xy_pair position{ 0, 0 };
	ui::text_color current_color = fmt.color;

	ui::single_line_manager lm;

	float x_extent = 0.0f;
	for (auto component_i = components_start; component_i != components_end; ++component_i) {
		x_extent += ui::text_component_width(*component_i, manager.text_data_sequences.text_data, this_font, fmt.font_size);
	}

	std::tie(position.x, position.y) = align_in_bounds(align, int32_t(x_extent + 0.5f), int32_t(this_font.line_height(ui::detail::font_size_to_render_size(this_font, static_cast<int32_t>(fmt.font_size))) + 0.5f), container.object.size.x, container.object.size.y);

	for (auto component_i = components_start; component_i != components_end; ++component_i) {
		if (std::holds_alternative<text_data::color_change>(*component_i)) {
			current_color = text_color_to_ui_text_color(std::get<text_data::color_change>(*component_i).color);
		} else if (std::holds_alternative<text_data::value_placeholder>(*component_i)) {
			const auto rep = text_data::find_replacement(std::get<text_data::value_placeholder>(*component_i), candidates, count);

			const auto replacement_text = rep ? std::get<1>(*rep) : vector_backed_string<char16_t>(text_data::name_from_value_type(std::get<text_data::value_placeholder>(*component_i).value));

			const text_format format{ current_color, fmt.font_handle, fmt.font_size };

			if (rep) 
				position = text_chunk_to_instances(manager, replacement_text, container, position, format, lm, std::get<2>(*rep));
			else 
				position = text_chunk_to_instances(manager, replacement_text, container, position, format, lm);

		} else if (std::holds_alternative<text_data::text_chunk>(*component_i)) {
			const auto chunk = std::get<text_data::text_chunk>(*component_i);
			const text_format format{ current_color, fmt.font_handle, fmt.font_size };

			position = text_chunk_to_instances(manager, chunk, container, position, format, lm);
		}
	}
}

void ui::detail::instantiate_graphical_object(ui::gui_manager& manager, ui::tagged_gui_object container, graphics::obj_definition_tag gtag, int32_t frame) {
	auto& graphic_object_def = manager.graphics_object_definitions.definitions[gtag];

	if (container.object.size == ui::xy_pair{ 0,0 })
		container.object.size = ui::xy_pair{ graphic_object_def.size.x, graphic_object_def.size.y };

	switch (graphics::object_type(graphic_object_def.flags & graphics::object::type_mask)) {
		case graphics::object_type::bordered_rect:
		case graphics::object_type::text_sprite:
		case graphics::object_type::tile_sprite:
		case graphics::object_type::generic_sprite:
			if (is_valid_index(graphic_object_def.primary_texture_handle)) {
				container.object.flags.fetch_or(ui::gui_object::type_graphics_object, std::memory_order_acq_rel);

				const auto icon_graphic = manager.graphics_instances.emplace();

				icon_graphic.object.frame = frame;
				icon_graphic.object.graphics_object = &graphic_object_def;
				icon_graphic.object.t = &(manager.textures.retrieve_by_key(graphic_object_def.primary_texture_handle));

				if (((int32_t)container.object.size.y | (int32_t)container.object.size.x) == 0) {
					icon_graphic.object.t->load_filedata();
					container.object.size.y = static_cast<int16_t>(icon_graphic.object.t->get_height());
					container.object.size.x = static_cast<int16_t>(icon_graphic.object.t->get_width() / ((graphic_object_def.number_of_frames != 0) ? graphic_object_def.number_of_frames : 1));
				}

				container.object.type_dependant_handle.store(to_index(icon_graphic.id), std::memory_order_release);
			}
			break;
		case graphics::object_type::horizontal_progress_bar:
			container.object.flags.fetch_or(ui::gui_object::type_progress_bar, std::memory_order_acq_rel);
			//TODO
			break;
		case graphics::object_type::vertical_progress_bar:
			container.object.flags.fetch_or(ui::gui_object::type_progress_bar | ui::gui_object::rotation_left, std::memory_order_acq_rel);
			//TODO
			break;
		case graphics::object_type::flag_mask:
			container.object.flags.fetch_or(ui::gui_object::type_masked_flag, std::memory_order_acq_rel);
			//TODO
			break;
		case graphics::object_type::barchart:
			if (is_valid_index(graphic_object_def.primary_texture_handle) & is_valid_index(graphics::texture_tag(graphic_object_def.type_dependant))) {
				container.object.flags.fetch_or(ui::gui_object::type_barchart, std::memory_order_acq_rel);

				const auto flag_graphic = manager.multi_texture_instances.emplace();

				flag_graphic.object.flag = nullptr;
				flag_graphic.object.mask_or_primary = &(manager.textures.retrieve_by_key(graphic_object_def.primary_texture_handle));
				flag_graphic.object.overlay_or_secondary = &(manager.textures.retrieve_by_key(graphics::texture_tag(graphic_object_def.type_dependant)));

				if (((int32_t)container.object.size.y | (int32_t)container.object.size.x) == 0) {
					flag_graphic.object.overlay_or_secondary->load_filedata();
					container.object.size.y = static_cast<int16_t>(flag_graphic.object.overlay_or_secondary->get_height());
					container.object.size.x = static_cast<int16_t>(flag_graphic.object.overlay_or_secondary->get_width());
				}

				container.object.type_dependant_handle.store(to_index(flag_graphic.id), std::memory_order_release);
			}
			break;
		case graphics::object_type::piechart:
		{
			container.object.flags.fetch_or(ui::gui_object::type_piechart, std::memory_order_acq_rel);

			container.object.size.x = graphic_object_def.size.x * 2;
			container.object.size.y = graphic_object_def.size.y * 2;

			container.object.position.x -= graphic_object_def.size.x;

			const auto new_dt = manager.data_textures.emplace(ui::piechart_resolution, 3ui16);

			container.object.type_dependant_handle.store(to_index(new_dt.id), std::memory_order_release);
		}
			break;
		case graphics::object_type::linegraph:
			container.object.flags.fetch_or(ui::gui_object::type_linegraph, std::memory_order_acq_rel);
			//TODO
			break;
	}
}

ui::tagged_gui_object ui::detail::create_element_instance(gui_manager& manager, button_tag handle) {
	const button_def& def = manager.ui_definitions.buttons[handle];
	const auto new_gobj = manager.gui_objects.emplace();

	const uint16_t rotation =
		(def.flags & button_def::rotation_mask) == button_def::rotation_90_left ?
		gui_object::rotation_left :
		gui_object::rotation_upright;

	new_gobj.object.flags.store(gui_object::visible | gui_object::enabled | rotation, std::memory_order_release);
	new_gobj.object.position = def.position;
	new_gobj.object.size = def.size;
	new_gobj.object.align = alignment_from_definition(def);

	instantiate_graphical_object(manager, new_gobj, def.graphical_object_handle);

	if (is_valid_index(def.text_handle)) {
		const auto[font_h, is_black, int_font_size] = graphics::unpack_font_handle(def.font_handle);
		detail::create_linear_text(manager, new_gobj, def.text_handle, text_aligment_from_button_definition(def), text_format{ is_black ? ui::text_color::black : ui::text_color::white, font_h, int_font_size });
	}

	return new_gobj;
}


ui::tagged_gui_object ui::detail::create_element_instance(gui_manager& manager, icon_tag handle) {
	const ui::icon_def& icon_def = manager.ui_definitions.icons[handle];
	const auto new_gobj = manager.gui_objects.emplace();

	const uint16_t rotation =
		(icon_def.flags & ui::icon_def::rotation_mask) == ui::icon_def::rotation_upright?
		ui::gui_object::rotation_upright :
		((icon_def.flags & ui::icon_def::rotation_mask) == ui::icon_def::rotation_90_right ? ui::gui_object::rotation_right : ui::gui_object::rotation_left);

	new_gobj.object.position = icon_def.position;
	new_gobj.object.flags.fetch_or(rotation, std::memory_order_acq_rel);
	new_gobj.object.align = alignment_from_definition(icon_def);

	instantiate_graphical_object(manager, new_gobj, icon_def.graphical_object_handle, icon_def.frame);

	new_gobj.object.size.x *= icon_def.scale;
	new_gobj.object.size.y *= icon_def.scale;

	return new_gobj;
}

ui::tagged_gui_object ui::detail::create_element_instance(gui_manager& manager, ui::text_tag handle, const text_data::replacement* candidates, uint32_t count) {
	const ui::text_def& text_def = manager.ui_definitions.text[handle];
	const auto new_gobj = manager.gui_objects.emplace();
	
	new_gobj.object.position = text_def.position;
	new_gobj.object.size = ui::xy_pair{ static_cast<int16_t>(text_def.max_width), 0 };
	new_gobj.object.align = alignment_from_definition(text_def);

	new_gobj.object.size.x -= text_def.border_size.x * 2;

	if (is_valid_index(text_def.text_handle)) {
		const auto[font_h, is_black, int_font_size] = graphics::unpack_font_handle(text_def.font_handle);
		detail::create_multiline_text(manager, new_gobj, text_def.text_handle, text_aligment_from_text_definition(text_def), text_format{ is_black ? ui::text_color::black : ui::text_color::white, font_h, int_font_size }, candidates, count);
	}

	for_each_child(manager, new_gobj, [adjust = text_def.border_size](tagged_gui_object c) {
		c.object.position += adjust;
	});

	new_gobj.object.size.x += text_def.border_size.x * 2;
	new_gobj.object.size.y += text_def.border_size.y * 2;

	return new_gobj;
}

void ui::detail::render_object_type(const gui_manager& manager, graphics::open_gl_wrapper& ogl, const gui_object& root_obj, const screen_position& position, uint32_t type, bool currently_enabled) {
	const auto current_rotation = root_obj.get_rotation();

	switch (type) {
		case ui::gui_object::type_barchart:
		{
			auto dt = manager.data_textures.safe_at(data_texture_tag(root_obj.type_dependant_handle.load(std::memory_order_acquire)));
			if (dt) {
				ogl.render_barchart(
					currently_enabled,
					position.effective_position_x,
					position.effective_position_y,
					position.effective_width,
					position.effective_height,
					*dt,
					current_rotation);
			}
			break;
		}
		case ui::gui_object::type_graphics_object:
		{
			auto gi = manager.graphics_instances.safe_at(graphics_instance_tag(root_obj.type_dependant_handle.load(std::memory_order_acquire)));
			if (gi) {
				const auto t = gi->t;
				if (t) {
					if ((gi->graphics_object->flags & graphics::object::type_mask) == (uint8_t)graphics::object_type::bordered_rect) {
						ogl.render_bordered_rect(
							currently_enabled,
							gi->graphics_object->type_dependant,
							position.effective_position_x,
							position.effective_position_y,
							position.effective_width,
							position.effective_height,
							*t,
							current_rotation);
					} else if (gi->graphics_object->number_of_frames > 1) {
						ogl.render_subsprite(
							currently_enabled,
							gi->frame,
							gi->graphics_object->number_of_frames,
							position.effective_position_x,
							position.effective_position_y,
							position.effective_width,
							position.effective_height,
							*t,
							current_rotation);
					} else {
						ogl.render_textured_rect(
							currently_enabled,
							position.effective_position_x,
							position.effective_position_y,
							position.effective_width,
							position.effective_height,
							*t,
							current_rotation);
					}

				}
			}
			break;
		}
		case ui::gui_object::type_linegraph:
		{
			auto l = manager.lines_set.safe_at(lines_tag(root_obj.type_dependant_handle.load(std::memory_order_acquire)));
			if (l) {
				ogl.render_linegraph(
					currently_enabled,
					position.effective_position_x,
					position.effective_position_y,
					position.effective_width,
					position.effective_height,
					*l);
			}
			break;
		}
		case ui::gui_object::type_masked_flag:
		{
			auto m = manager.multi_texture_instances.safe_at(multi_texture_instance_tag(root_obj.type_dependant_handle.load(std::memory_order_acquire)));
			if (m) {
				const auto flag = m->flag;
				const auto mask = m->mask_or_primary;
				const auto overlay = m->overlay_or_secondary;
				if (flag && mask) {
					ogl.render_masked_rect(
						currently_enabled,
						position.effective_position_x,
						position.effective_position_y,
						position.effective_width,
						position.effective_height,
						*flag,
						*mask,
						current_rotation);
				}
				if (overlay) {
					ogl.render_textured_rect(
						currently_enabled,
						position.effective_position_x,
						position.effective_position_y,
						position.effective_width,
						position.effective_height,
						*overlay,
						current_rotation);
				}
			}
			break;
		}
		case ui::gui_object::type_progress_bar:
		{
			auto m = manager.multi_texture_instances.safe_at(multi_texture_instance_tag(root_obj.type_dependant_handle.load(std::memory_order_acquire)));
			if (m) {
				const auto primary = m->mask_or_primary;
				const auto secondary = m->overlay_or_secondary;
				if (primary && secondary) {
					ogl.render_progress_bar(
						currently_enabled,
						m->progress,
						position.effective_position_x,
						position.effective_position_y,
						position.effective_width,
						position.effective_height,
						*primary,
						*secondary,
						current_rotation);
				}
			}
			break;
		}
		case ui::gui_object::type_piechart:
		{
			auto dt = manager.data_textures.safe_at(data_texture_tag(root_obj.type_dependant_handle.load(std::memory_order_acquire)));
			if (dt) {
				ogl.render_piechart(
					currently_enabled,
					position.effective_position_x,
					position.effective_position_y,
					position.effective_width,
					*dt);
			}
			break;
		}
		case ui::gui_object::type_text_instance:
		{
			auto ti = manager.text_instances.safe_at(text_instance_tag(root_obj.type_dependant_handle.load(std::memory_order_acquire)));
			if (ti) {
				auto& fnt = manager.fonts.at(ti->font_handle);

				switch (ti->color) {
					case ui::text_color::black:
						ogl.render_text(ti->text, ti->length, currently_enabled, position.effective_position_x, position.effective_position_y, ui::detail::font_size_to_render_size(fnt, ti->size * 2) * manager.scale(), graphics::color{ 0.0f, 0.0f, 0.0f }, fnt);
						break;
					case ui::text_color::green:
						ogl.render_text(ti->text, ti->length, currently_enabled, position.effective_position_x, position.effective_position_y, ui::detail::font_size_to_render_size(fnt, ti->size * 2) * manager.scale(), graphics::color{ 0.0f, 0.623f, 0.01f }, fnt);
						break;
					case ui::text_color::outlined_black:
						ogl.render_outlined_text(ti->text, ti->length, currently_enabled, position.effective_position_x, position.effective_position_y, ui::detail::font_size_to_render_size(fnt, ti->size * 2) * manager.scale(), graphics::color{ 0.0f, 0.0f, 0.0f }, fnt);
						break;
					case ui::text_color::outlined_white:
						ogl.render_outlined_text(ti->text, ti->length, currently_enabled, position.effective_position_x, position.effective_position_y, ui::detail::font_size_to_render_size(fnt, ti->size * 2) * manager.scale(), graphics::color{ 1.0f, 1.0f, 1.0f }, fnt);
						break;
					case ui::text_color::red:
						ogl.render_text(ti->text, ti->length, currently_enabled, position.effective_position_x, position.effective_position_y, ui::detail::font_size_to_render_size(fnt, ti->size * 2) * manager.scale(), graphics::color{ 1.0f, 0.2f, 0.2f }, fnt);
						break;
					case ui::text_color::white:
						ogl.render_text(ti->text, ti->length, currently_enabled, position.effective_position_x, position.effective_position_y, ui::detail::font_size_to_render_size(fnt, ti->size * 2) * manager.scale(), graphics::color{ 1.0f, 1.0f, 1.0f }, fnt);
						break;
					case ui::text_color::yellow:
						ogl.render_text(ti->text, ti->length, currently_enabled, position.effective_position_x, position.effective_position_y, ui::detail::font_size_to_render_size(fnt, ti->size * 2) * manager.scale(), graphics::color{ 1.0f, 0.75f, 1.0f }, fnt);
						break;
				}
			}
			break;
		}
	}
}

void ui::detail::render(const gui_manager& manager, graphics::open_gl_wrapper &ogl, const gui_object &root_obj, ui::xy_pair position, ui::xy_pair container_size, bool parent_enabled) {
	const auto flags = root_obj.flags.load(std::memory_order_acquire);
	if ((flags & ui::gui_object::visible) == 0)
		return;

	const uint16_t type = flags & ui::gui_object::type_mask;

	const auto root_position = position + ui::detail::position_with_alignment(container_size, root_obj.position, root_obj.align);

	screen_position screen_pos{
		static_cast<float>(root_position.x) * manager.scale(),
		static_cast<float>(root_position.y) * manager.scale(),
		static_cast<float>(root_obj.size.x) * manager.scale(),
		static_cast<float>(root_obj.size.y) * manager.scale()
	};

	const bool currently_enabled = parent_enabled && ((root_obj.flags.load(std::memory_order_acquire) & ui::gui_object::enabled) != 0);

	if ((flags & ui::gui_object::dont_clip_children) == 0) {
		graphics::scissor_rect clip(std::lround(screen_pos.effective_position_x), manager.height() - std::lround(screen_pos.effective_position_y + screen_pos.effective_height), std::lround(screen_pos.effective_width), std::lround(screen_pos.effective_height));

		detail::render_object_type(manager, ogl, root_obj, screen_pos, type, currently_enabled);

		gui_object_tag current_child = root_obj.first_child;
		while (is_valid_index(current_child)) {
			const auto& child_object = manager.gui_objects.at(current_child);
			detail::render(manager, ogl, child_object, root_position, root_obj.size, currently_enabled);
			current_child = child_object.right_sibling;
		}
	} else {
		detail::render_object_type(manager, ogl, root_obj, screen_pos, type, currently_enabled);

		gui_object_tag current_child = root_obj.first_child;
		while (is_valid_index(current_child)) {
			const auto& child_object = manager.gui_objects.at(current_child);
			detail::render(manager, ogl, child_object, root_position, root_obj.size, currently_enabled);
			current_child = child_object.right_sibling;
		}
	}
}

bool ui::gui_manager::set_focus(tagged_gui_object g) {
	
	if (g.object.associated_behavior && g.object.associated_behavior->on_get_focus(g.id, *this)) {
		if (focus != g.id) {
			clear_focus();
			focus = g.id;
		}
		return true;
	}
	return false;
}

void ui::gui_manager::clear_focus() {
	if (is_valid_index(focus)) {
		auto& with_focus = gui_objects.at(focus);
		if (with_focus.associated_behavior) {
			with_focus.associated_behavior->on_lose_focus(focus, *this);
			focus = gui_object_tag();
		}
	}
}

void ui::make_visible_and_update(gui_manager& manager, gui_object& g) {
	g.flags.fetch_or(ui::gui_object::visible_after_update, std::memory_order_acq_rel);
	manager.flag_minimal_update();
}

void ui::hide(gui_object& g) {
	g.flags.fetch_and((uint16_t)~ui::gui_object::visible, std::memory_order_acq_rel);
}

void ui::set_enabled(gui_object& g, bool enabled) {
	if (enabled)
		g.flags.fetch_or(ui::gui_object::enabled, std::memory_order_acq_rel);
	else
		g.flags.fetch_and((uint16_t)~ui::gui_object::enabled, std::memory_order_acq_rel);
}

void ui::gui_manager::hide_tooltip() {
	hide(tagged_gui_object{ tooltip_window, gui_object_tag(3) });
}

void ui::clear_children(gui_manager& manager, tagged_gui_object g) {
	gui_object_tag current_child = g.object.first_child;
	g.object.first_child = gui_object_tag();

	while (is_valid_index(current_child)) {
		auto& child_object = manager.gui_objects.at(current_child);
		const gui_object_tag next_child = child_object.right_sibling;

		manager.gui_objects.free(current_child, manager);
		current_child = next_child;
	}
}

void ui::remove_object(gui_manager& manager, tagged_gui_object g) {
	gui_object_tag parent_id = g.object.parent;
	auto& parent_object = manager.gui_objects.at(parent_id);

	const gui_object_tag left_sibling = g.object.left_sibling;
	const gui_object_tag right_sibling = g.object.right_sibling;

	if (!is_valid_index(left_sibling))
		parent_object.first_child = right_sibling;
	else
		manager.gui_objects.at(left_sibling).right_sibling = right_sibling;
	
	if (is_valid_index(right_sibling))
		manager.gui_objects.at(right_sibling).left_sibling = left_sibling;

	manager.gui_objects.free(g.id, manager);
}

ui::tagged_gui_object ui::detail::last_sibling_of(const gui_manager& manager, tagged_gui_object g) {
	gui_object_tag sib_child_id = g.id;
	gui_object* current = &g.object;
	while (true) {
		const gui_object_tag next_id = current->right_sibling;
		if (!is_valid_index(next_id))
			return tagged_gui_object{*current, sib_child_id };
		sib_child_id = next_id;
		current = &(manager.gui_objects.at(next_id));
	}
}

void ui::add_to_front(const gui_manager& manager, tagged_gui_object parent, tagged_gui_object child) {
	child.object.parent = parent.id;
	const gui_object_tag first_child_id = parent.object.first_child;

	if (!is_valid_index(first_child_id)) {
		parent.object.first_child = child.id;
	} else {
		const auto last_in_list = detail::last_sibling_of(manager, tagged_gui_object{ manager.gui_objects.at(first_child_id), first_child_id });

		child.object.left_sibling = last_in_list.id;
		last_in_list.object.right_sibling = child.id;
	}
	
}

ui::xy_pair ui::absolute_position(gui_manager& manager, tagged_gui_object g) {
	ui::xy_pair sum{ 0, 0 };
	ui::xy_pair child_position = g.object.position;
	auto child_alignment = g.object.align;
	gui_object_tag parent = g.object.parent;

	while (is_valid_index(parent)) {
		const auto& parent_object = manager.gui_objects.at(parent);

		sum += ui::detail::position_with_alignment(parent_object.size, child_position, child_alignment);
		child_position = parent_object.position;
		child_alignment = parent_object.align;

		parent = parent_object.parent;
	}
	sum += child_position;

	return sum;
}

void ui::add_to_back(const gui_manager& manager, tagged_gui_object parent, tagged_gui_object child) {
	child.object.parent = parent.id;
	const gui_object_tag first_child_id = parent.object.first_child;
	if (is_valid_index(first_child_id)) {
		child.object.right_sibling = first_child_id;
		auto& first_child_object = manager.gui_objects.at(first_child_id);
		first_child_object.left_sibling = child.id;
	}
	parent.object.first_child = child.id;
}

void ui::move_to_front(const gui_manager& manager, tagged_gui_object g) {
	const gui_object_tag left_sibling = g.object.left_sibling;
	const gui_object_tag right_sibling = g.object.right_sibling;

	if (!is_valid_index(right_sibling))
		return;

	const gui_object_tag parent_id = g.object.parent;
	auto& parent_object = manager.gui_objects.at(parent_id);

	const auto last_in_list = ui::detail::last_sibling_of(manager, g);

	if (!is_valid_index(left_sibling))
		parent_object.first_child = right_sibling;
	else
		manager.gui_objects.at(left_sibling).right_sibling = right_sibling;

	manager.gui_objects.at(right_sibling).left_sibling = left_sibling;

	g.object.left_sibling = last_in_list.id;
	g.object.right_sibling = gui_object_tag();
	last_in_list.object.right_sibling = g.id;
}

void ui::move_to_back(const gui_manager& manager, tagged_gui_object g) {
	const gui_object_tag left_sibling = g.object.left_sibling;
	const gui_object_tag right_sibling = g.object.right_sibling;

	if (!is_valid_index(left_sibling))
		return;

	const gui_object_tag parent_id = g.object.parent;
	auto& parent_object = manager.gui_objects.at(parent_id);

	manager.gui_objects.at(left_sibling).right_sibling = right_sibling;
	if (is_valid_index(right_sibling))
		manager.gui_objects.at(right_sibling).left_sibling = left_sibling;

	g.object.left_sibling = gui_object_tag();
	g.object.right_sibling = parent_object.first_child;
	parent_object.first_child = g.id;
}

bool ui::gui_manager::on_lbutton_down(const lbutton_down& ld) {
	if (false == detail::dispatch_message(*this, [_this = this](ui::tagged_gui_object obj, const lbutton_down& ) {
		return _this->set_focus(obj);
	}, tagged_gui_object{ root, gui_object_tag(0) }, root.size, ui::rescale_message(ld, _scale))) {
		focus = gui_object_tag();
	}

	return detail::dispatch_message(*this, [_this = this](ui::tagged_gui_object obj, const lbutton_down& l) {
		if (obj.object.associated_behavior)
			return obj.object.associated_behavior->on_lclick(obj.id, *_this, l);
		return false;
	}, tagged_gui_object{ root, gui_object_tag(0) }, root.size, ui::rescale_message(ld, _scale));
}

bool ui::gui_manager::on_rbutton_down(const rbutton_down& rd) {
	return detail::dispatch_message(*this, [_this = this](ui::tagged_gui_object obj, const rbutton_down& r) {
		if (obj.object.associated_behavior)
			return obj.object.associated_behavior->on_rclick(obj.id, *_this, r);
		return false;
	}, tagged_gui_object{ root,gui_object_tag(0) }, root.size, ui::rescale_message(rd, _scale));
}

bool ui::gui_manager::on_mouse_move(const mouse_move& mm) {
	const bool found_tooltip = detail::dispatch_message(*this, [_this = this](ui::tagged_gui_object obj, const mouse_move& m) {
		if (obj.object.associated_behavior) {
			const auto tt_behavior = obj.object.associated_behavior->has_tooltip(obj.id, *_this, m);
			if (tt_behavior == tooltip_behavior::transparent)
				return false;
			if (tt_behavior == tooltip_behavior::no_tooltip) {
				ui::hide(ui::tagged_gui_object{ _this->tooltip_window, gui_object_tag(3) });
				_this->tooltip = obj.id;
				return true;
			}
			if ((_this->tooltip != obj.id) | (tt_behavior == tooltip_behavior::variable_tooltip)) {
				_this->tooltip = obj.id;
				clear_children(*_this, ui::tagged_gui_object{ _this->tooltip_window, gui_object_tag(3) });
				obj.object.associated_behavior->create_tooltip(obj.id, *_this, m, ui::tagged_gui_object{ _this->tooltip_window, gui_object_tag(3) });
				ui::shrink_to_children(*_this, ui::tagged_gui_object{ _this->tooltip_window, gui_object_tag(3) }, 16);

				_this->tooltip_window.position = ui::absolute_position(*_this, obj);
				if(_this->tooltip_window.position.y + obj.object.size.y + _this->tooltip_window.size.y <= _this->height())
					_this->tooltip_window.position.y += obj.object.size.y;
				else
					_this->tooltip_window.position.y -= _this->tooltip_window.size.y;
				_this->tooltip_window.position.x += obj.object.size.x / 2;
				_this->tooltip_window.position.x -= _this->tooltip_window.size.x / 2;

				_this->tooltip_window.flags.fetch_or(ui::gui_object::visible, std::memory_order_acq_rel);
			}
			return true;
		}
		return false;
	}, tagged_gui_object{ root,gui_object_tag(0) }, root.size, ui::rescale_message(mm, _scale));

	if (!found_tooltip && is_valid_index(tooltip)) {
		tooltip = gui_object_tag();
		hide(ui::tagged_gui_object{ tooltip_window, gui_object_tag(3) });
	}

	return found_tooltip;
}

bool ui::gui_manager::on_mouse_drag(const mouse_drag& md) {
	if (is_valid_index(focus)) {
		if (gui_objects.at(focus).associated_behavior) {
			return gui_objects.at(focus).associated_behavior->on_drag(focus, *this, ui::rescale_message(md, _scale));
		} else
			return false;
	}
	return false;
}

bool ui::gui_manager::on_keydown(const key_down& kd) {
	return detail::dispatch_message(*this, [_this = this](ui::tagged_gui_object obj, const key_down& k) {
		if (obj.object.associated_behavior)
			return obj.object.associated_behavior->on_keydown(obj.id, *_this, k);
		return false;
	}, tagged_gui_object{ root,gui_object_tag(0) }, root.size, ui::rescale_message(kd, _scale));
}

bool ui::gui_manager::on_scroll(const scroll& se) {
	return detail::dispatch_message(*this, [_this = this](ui::tagged_gui_object obj, const scroll& s) {
		if (obj.object.associated_behavior)
			return obj.object.associated_behavior->on_scroll(obj.id, *_this, s);
		return false;
	}, tagged_gui_object{ root,gui_object_tag(0) }, root.size, ui::rescale_message(se, _scale));
}

bool ui::gui_manager::on_text(const text_event &te) {
	if (is_valid_index(focus)) {
		if (gui_objects.at(focus).associated_behavior)
			return gui_objects.at(focus).associated_behavior->on_text(focus, *this, te);
		else
			return false;
	}
	return false;
}

void ui::detail::update(gui_manager& manager, tagged_gui_object obj, world_state& w) {
	const auto object_flags = obj.object.flags.load(std::memory_order_acquire);

	if ((object_flags & ui::gui_object::visible_after_update) != 0) {
		obj.object.flags.fetch_and((uint16_t)~ui::gui_object::visible_after_update, std::memory_order_acq_rel);
		if ((obj.object.associated_behavior != nullptr) & ((object_flags & ui::gui_object::visible) == 0)) {
			obj.object.associated_behavior->on_visible(obj.id, manager, w);
			obj.object.flags.fetch_or(ui::gui_object::visible, std::memory_order_acq_rel);
		}
	} else if ((object_flags & ui::gui_object::visible) == 0) {
		return;
	}

	if (obj.object.associated_behavior)
		obj.object.associated_behavior->update_data(obj.id, manager, w);

	ui::for_each_child(manager, obj, [&manager, &w](ui::tagged_gui_object child) {
		update(manager, tagged_gui_object{ child, child }, w);
	});
}

void ui::detail::minimal_update(gui_manager& manager, tagged_gui_object obj, world_state& w) {
	const auto object_flags = obj.object.flags.load(std::memory_order_acquire);

	if ((object_flags & ui::gui_object::visible_after_update) != 0) {
		obj.object.flags.fetch_and((uint16_t)~ui::gui_object::visible_after_update, std::memory_order_acq_rel);
		
		if ((obj.object.associated_behavior != nullptr) & ((object_flags & ui::gui_object::visible) == 0)) {
			obj.object.associated_behavior->on_visible(obj.id, manager, w);
			obj.object.flags.fetch_or(ui::gui_object::visible, std::memory_order_acq_rel);
		}

		if (obj.object.associated_behavior)
			obj.object.associated_behavior->update_data(obj.id, manager, w);

		ui::for_each_child(manager, obj, [&manager, &w](ui::tagged_gui_object child) {
			minimal_update(manager, tagged_gui_object{ child, child }, w);
		});
	} else if ((object_flags & ui::gui_object::visible) != 0) {
		ui::for_each_child(manager, obj, [&manager, &w](ui::tagged_gui_object child) {
			minimal_update(manager, tagged_gui_object{ child, child }, w);
		});
	}
}

void ui::minimal_update(gui_manager& manager, world_state& w) {
	detail::minimal_update(manager, tagged_gui_object{ manager.root, gui_object_tag(0) }, w);
	detail::minimal_update(manager, tagged_gui_object{ manager.background, gui_object_tag(1) }, w);
	detail::minimal_update(manager, tagged_gui_object{ manager.foreground, gui_object_tag(2) }, w);
}

void ui::update(gui_manager& manager, world_state& w) {
	manager.check_and_clear_minimal_update();
	detail::update(manager, tagged_gui_object{ manager.root, gui_object_tag(0) }, w);
	detail::update(manager, tagged_gui_object{ manager.background, gui_object_tag(1) }, w);
	detail::update(manager, tagged_gui_object{ manager.foreground, gui_object_tag(2) }, w);
}

ui::gui_manager::gui_manager(int32_t width, int32_t height) :
	_width(width), _height(height),
	root(gui_objects.emplace_at(gui_object_tag(0))),
	background(gui_objects.emplace_at(gui_object_tag(1))),
	foreground(gui_objects.emplace_at(gui_object_tag(2))),
	tooltip_window(gui_objects.emplace_at(gui_object_tag(3))) {

	on_resize(resize{ static_cast<uint32_t>(width > 0 ? width : 0) , static_cast<uint32_t>(height > 0 ? height : 0) });
	
	hide(tagged_gui_object{ tooltip_window, gui_object_tag(3) });
	add_to_back(*this, tagged_gui_object{ foreground, gui_object_tag(2) }, tagged_gui_object{ tooltip_window, gui_object_tag(3) });
}

void ui::gui_manager::on_resize(const resize& r) {
	ui::xy_pair new_size{
		static_cast<int16_t>(static_cast<float>(r.width) / scale()),
		static_cast<int16_t>(static_cast<float>(r.height) / scale()) };

	_width = static_cast<int32_t>(r.width);
	_height = static_cast<int32_t>(r.height);
	root.size = new_size;
	foreground.size = new_size;
	background.size = new_size;
}

void ui::gui_manager::rescale(float new_scale) {
	const resize new_size{ static_cast<uint32_t>(static_cast<float>(root.size.x) * scale()), static_cast<uint32_t>(static_cast<float>(root.size.y) * scale()) };
	_scale = new_scale;
	on_resize(new_size);
}

void ui::render(const gui_manager& manager, graphics::open_gl_wrapper& ogl) {
	detail::render(manager, ogl, manager.background, ui::xy_pair{ 0, 0 }, manager.background.size, true);
	detail::render(manager, ogl, manager.root, ui::xy_pair{ 0, 0 }, manager.root.size, true);
	detail::render(manager, ogl, manager.foreground, ui::xy_pair{ 0, 0 }, manager.foreground.size, true);
}

graphics::rotation ui::gui_object::get_rotation() const {
	const auto rotation_bits = flags.load(std::memory_order_acquire) & ui::gui_object::rotation_mask;
	if (rotation_bits == ui::gui_object::rotation_left)
		return graphics::rotation::left;
	else if (rotation_bits == ui::gui_object::rotation_right)
		return graphics::rotation::right;
	else
		return graphics::rotation::upright;
}


void ui::load_gui_from_directory(const directory& source_directory, gui_manager& manager) {
	auto fonts_directory = source_directory.get_directory(u"\\gfx\\fonts");
	manager.fonts.load_standard_fonts(fonts_directory);

	manager.fonts.load_metrics_fonts();

	manager.textures.load_standard_textures(source_directory);

	auto localisation_directory = source_directory.get_directory(u"\\localisation");
	load_text_sequences_from_directory(localisation_directory, manager.text_data_sequences);

	auto interface_directory = source_directory.get_directory(u"\\interface");

	ui::definitions defs;
	std::vector<std::pair<std::string, ui::errors>> errors_generated;

	graphics::name_maps gobj_nmaps;
	std::vector<std::pair<std::string, graphics::errors>> gobj_errors_generated;

	ui::load_ui_definitions_from_directory(
		interface_directory, manager.nmaps, manager.ui_definitions, errors_generated,
		[&manager](const char* a, const char* b) { return text_data::get_text_handle(manager.text_data_sequences, a, b); },
		[&manager](const char* a, const char* b) { return graphics::pack_font_handle(manager.fonts.find_font(a, b), manager.fonts.is_black(a,b), manager.fonts.find_font_size(a, b)); },
		[&gobj_nmaps](const char* a, const char* b) { return graphics::reserve_graphics_object(gobj_nmaps, a, b); });

#ifdef _DEBUG
	for (auto& e : errors_generated) {
		OutputDebugStringA(e.first.c_str());
		OutputDebugStringA(": ");
		OutputDebugStringA(ui::format_error(e.second));
		OutputDebugStringA("\n");
	}
#endif

	graphics::load_graphics_object_definitions_from_directory(
		interface_directory,
		gobj_nmaps,
		manager.graphics_object_definitions,
		gobj_errors_generated,
		[&manager, &source_directory](const char* a, const char* b) { return manager.textures.retrieve_by_name(source_directory, a, b); });

#ifdef _DEBUG
	for (auto& e : gobj_errors_generated) {
		OutputDebugStringA(e.first.c_str());
		OutputDebugStringA(": ");
		OutputDebugStringA(graphics::format_error(e.second));
		OutputDebugStringA("\n");
	}
#endif

	manager.tooltip_window.flags.fetch_or(ui::gui_object::type_graphics_object, std::memory_order_acq_rel);
	const auto bg_graphic = manager.graphics_instances.emplace();
	bg_graphic.object.frame = 0;
	bg_graphic.object.graphics_object = &(manager.graphics_object_definitions.definitions[manager.graphics_object_definitions.standard_text_background]);
	bg_graphic.object.t = &(manager.textures.retrieve_by_key(manager.textures.standard_tiles_dialog));
	manager.tooltip_window.type_dependant_handle.store(to_index(bg_graphic.id), std::memory_order_release);
}

ui::tagged_gui_object ui::create_scrollable_text_block(gui_manager& manager, ui::text_tag handle, tagged_gui_object parent, const text_data::replacement* candidates, uint32_t count) {
	const ui::text_def& text_def = manager.ui_definitions.text[handle];
	const auto[font_h, is_black, int_font_size] = graphics::unpack_font_handle(text_def.font_handle);
	const auto& this_font = manager.fonts.at(font_h);

	auto res = create_scrollable_region(
		manager,
		parent,
		text_def.position,
		text_def.max_height,
		(int32_t)this_font.line_height(ui::detail::font_size_to_render_size(this_font, static_cast<int32_t>(int_font_size))),
		graphics::obj_definition_tag(),
		[handle, candidates, count](gui_manager& m) {
		return detail::create_element_instance(m, handle, candidates, count);
	});
	res.object.align = alignment_from_definition(text_def);

	const auto background = text_def.flags & text_def.background_mask;
	if ((background != ui::text_def::background_none_specified) & (background != ui::text_def::background_transparency_tga)) {

		res.object.flags.fetch_or(ui::gui_object::type_graphics_object, std::memory_order_acq_rel);

		const auto bg_graphic = manager.graphics_instances.emplace();
		const auto texture =
			(background == ui::text_def::background_small_tiles_dialog_tga) ?
			manager.textures.standard_small_tiles_dialog :
			manager.textures.standard_tiles_dialog;
		bg_graphic.object.frame = 0;
		bg_graphic.object.graphics_object = &(manager.graphics_object_definitions.definitions[manager.graphics_object_definitions.standard_text_background]);
		bg_graphic.object.t = &(manager.textures.retrieve_by_key(texture));

		res.object.type_dependant_handle.store(to_index(bg_graphic.id), std::memory_order_release);
	}

	return res;
}

ui::tagged_gui_object ui::create_scrollable_text_block(gui_manager& manager, ui::text_tag handle, text_data::text_tag contents, tagged_gui_object parent, const text_data::replacement* candidates, uint32_t count) {
	const ui::text_def& text_def = manager.ui_definitions.text[handle];
	const auto[font_h, is_black, int_font_size] = graphics::unpack_font_handle(text_def.font_handle);
	const auto& this_font = manager.fonts.at(font_h);

	auto res = create_scrollable_region(
		manager,
		parent,
		text_def.position,
		text_def.max_height,
		(int32_t)this_font.line_height(ui::detail::font_size_to_render_size(this_font, static_cast<int32_t>(int_font_size))),
		graphics::obj_definition_tag(),
		[handle, contents, candidates, count, &text_def](gui_manager& m) {
		const auto new_gobj = m.gui_objects.emplace();

		new_gobj.object.position = ui::xy_pair{ 0, 0 };
		new_gobj.object.size = ui::xy_pair{ static_cast<int16_t>(text_def.max_width), 0 };

		new_gobj.object.size.x -= text_def.border_size.x * 2;

		if (is_valid_index(contents)) {
			const auto[font_h, is_black, int_font_size] = graphics::unpack_font_handle(text_def.font_handle);
			detail::create_multiline_text(m, new_gobj, contents, text_aligment_from_text_definition(text_def), text_format{ is_black ? ui::text_color::black : ui::text_color::white, font_h, int_font_size }, candidates, count);
		}

		for_each_child(m, new_gobj, [adjust = text_def.border_size](tagged_gui_object c) {
			c.object.position += adjust;
		});

		new_gobj.object.size.x += text_def.border_size.x * 2;
		new_gobj.object.size.y += text_def.border_size.y * 2;

		return new_gobj;
	});
	res.object.align = alignment_from_definition(text_def);

	const auto background = text_def.flags & text_def.background_mask;
	if ((background != ui::text_def::background_none_specified) & (background != ui::text_def::background_transparency_tga)) {

		res.object.flags.fetch_or(ui::gui_object::type_graphics_object, std::memory_order_acq_rel);

		const auto bg_graphic = manager.graphics_instances.emplace();
		const auto texture =
			(background == ui::text_def::background_small_tiles_dialog_tga) ?
			manager.textures.standard_small_tiles_dialog :
			manager.textures.standard_tiles_dialog;
		bg_graphic.object.frame = 0;
		bg_graphic.object.graphics_object = &(manager.graphics_object_definitions.definitions[manager.graphics_object_definitions.standard_text_background]);
		bg_graphic.object.t = &(manager.textures.retrieve_by_key(texture));

		res.object.type_dependant_handle.store(to_index(bg_graphic.id), std::memory_order_release);
	}

	return res;
}

void ui::shrink_to_children(gui_manager& manager, tagged_gui_object g) {
	ui::shrink_to_children(manager, g, 0);
}

void ui::shrink_to_children(gui_manager& manager, tagged_gui_object g, int32_t border) {
	g.object.size = ui::xy_pair{ 0,0 };
	ui::xy_pair minimum_child{ std::numeric_limits<decltype(g.object.size.x)>::max(), std::numeric_limits<decltype(g.object.size.y)>::max() };

	for_each_child(manager, g, [g, &minimum_child](tagged_gui_object child) {
		g.object.size.x = static_cast<int16_t>(std::max(int32_t(g.object.size.x), child.object.size.x + child.object.position.x));
		g.object.size.y = static_cast<int16_t>(std::max(int32_t(g.object.size.y), child.object.size.y + child.object.position.y));
		minimum_child.x = std::min(minimum_child.x, child.object.position.x);
		minimum_child.y = std::min(minimum_child.y, child.object.position.y);
	});

	for_each_child(manager, g, [g, minimum_child, border](tagged_gui_object child) {
		child.object.position.x += border - minimum_child.x;
		child.object.position.y += border - minimum_child.y;
	});

	g.object.size.x = static_cast<int16_t>(border * 2 + std::max(g.object.size.x - minimum_child.x, 0));
	g.object.size.y = static_cast<int16_t>(border * 2 + std::max(g.object.size.y - minimum_child.y, 0));
}

ui::tagged_gui_object ui::create_dynamic_window(gui_manager& manager, window_tag t, tagged_gui_object parent) {
	const auto& definition = manager.ui_definitions.windows[t];

	if (is_valid_index(definition.background_handle)) {
		const auto window = ((definition.flags & (window_def::is_moveable | window_def::is_dialog)) != 0) ?
			create_dynamic_element<draggable_region>(manager, definition.background_handle, parent) :
			create_dynamic_element<fixed_region>(manager, definition.background_handle, parent);

		window.object.size = definition.size;
		window.object.position = definition.position;
		window.object.align = alignment_from_definition(definition);

		return window;
	} else {
		const auto window = manager.gui_objects.emplace();

		window.object.flags.fetch_or(ui::gui_object::type_graphics_object, std::memory_order_acq_rel);

		const auto bg_graphic = manager.graphics_instances.emplace();
		bg_graphic.object.frame = 0;
		bg_graphic.object.graphics_object = &(manager.graphics_object_definitions.definitions[manager.graphics_object_definitions.standard_text_background]);
		bg_graphic.object.t = &(manager.textures.retrieve_by_key(manager.textures.standard_tiles_dialog));

		window.object.type_dependant_handle.store(to_index(bg_graphic.id), std::memory_order_release);

		if ((definition.flags & (window_def::is_moveable | window_def::is_dialog)) != 0) {
			window.object.associated_behavior = concurrent_allocator<draggable_region>().allocate(1);
			new (window.object.associated_behavior)draggable_region();
		} else {
			window.object.associated_behavior = concurrent_allocator<fixed_region>().allocate(1);
			new (window.object.associated_behavior)fixed_region();
		}

		window.object.flags.fetch_or(ui::gui_object::dynamic_behavior, std::memory_order_acq_rel);
		window.object.associated_behavior->associated_object = &window.object;

		window.object.size = definition.size;
		window.object.position = definition.position;
		window.object.align = alignment_from_definition(definition);

		add_to_back(manager, parent, window);
		return window;
	}
}

ui::alignment ui::alignment_from_definition(const button_def& d) {
	switch (d.flags & button_def::orientation_mask) {
		case button_def::orientation_lower_left:
			return alignment::bottom_left;
		case button_def::orientation_lower_right:
			return alignment::bottom_right;
		case button_def::orientation_center:
			return alignment::center;
		case button_def::orientation_upper_right:
			return alignment::top_right;
	}
	return alignment::top_left;
}

ui::alignment ui::alignment_from_definition(const icon_def& d) {
	switch (d.flags & icon_def::orientation_mask) {
		case icon_def::orientation_lower_left:
			return alignment::bottom_left;
		case icon_def::orientation_lower_right:
			return alignment::bottom_right;
		case icon_def::orientation_center:
			return alignment::center;
		case icon_def::orientation_upper_right:
			return alignment::top_right;
		case icon_def::orientation_center_down:
			return alignment::bottom_center;
		case icon_def::orientation_center_up:
			return alignment::top_center;
	}
	return alignment::top_left;
}

ui::alignment ui::alignment_from_definition(const text_def& d) {
	switch (d.flags & text_def::orientation_mask) {
		case text_def::orientation_lower_left:
			return alignment::bottom_left;
		case text_def::orientation_lower_right:
			return alignment::bottom_right;
		case text_def::orientation_center:
			return alignment::center;
		case text_def::orientation_upper_right:
			return alignment::top_right;
		case text_def::orientation_center_down:
			return alignment::bottom_center;
		case text_def::orientation_center_up:
			return alignment::top_center;
	}
	return alignment::top_left;
}

ui::alignment ui::alignment_from_definition(const overlapping_region_def& d) {
	switch (d.flags & overlapping_region_def::orientation_mask) {
		case overlapping_region_def::orientation_center:
			return alignment::center;
		case overlapping_region_def::orientation_upper_right:
			return alignment::top_right;
	}
	return alignment::top_left;
}

ui::alignment ui::alignment_from_definition(const listbox_def& d) {
	switch (d.flags & listbox_def::orientation_mask) {
		case listbox_def::orientation_center:
			return alignment::center;
		case listbox_def::orientation_upper_right:
			return alignment::top_right;
		case listbox_def::orientation_center_down:
			return alignment::bottom_center;
	}
	return alignment::top_left;
}

ui::alignment ui::alignment_from_definition(const scrollbar_def&) {
	return alignment::top_left;
}

ui::alignment ui::alignment_from_definition(const window_def& d) {
	switch (d.flags & window_def::orientation_mask) {
		case window_def::orientation_center:
			return alignment::center;
		case window_def::orientation_upper_right:
			return alignment::top_right;
		case window_def::orientation_lower_left:
			return alignment::bottom_left;
		case window_def::orientation_lower_right:
			return alignment::bottom_right;
	}
	return alignment::top_left;
}

float ui::detail::font_size_to_render_size(const graphics::font& f, int32_t sz) {
	const auto ft64_sz = f.line_height(64.0f);
	return static_cast<float>(sz) * 64.0f / ft64_sz;
}
