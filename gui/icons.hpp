#pragma once
#include "gui.hpp"

template<typename BASE>
void ui::dynamic_icon<BASE>::update_data(gui_object_tag, world_state& w) {
	if constexpr(ui::detail::has_update<BASE, dynamic_icon<BASE>&, world_state&>) {
		BASE::update(*this, w);
	}
}

template<typename BASE>
void ui::tinted_icon<BASE>::update_data(gui_object_tag, world_state& w) {
	if constexpr(ui::detail::has_update<BASE, dynamic_icon<BASE>&, world_state&>) {
		BASE::update(*this, w);
	}
}

template<typename BASE>
template<typename window_type>
void ui::dynamic_icon<BASE>::windowed_update(window_type& w, world_state& s) {
	if constexpr(ui::detail::has_windowed_update<BASE, dynamic_icon<BASE>&, window_type&, world_state&>) {
		BASE::windowed_update(*this, w, s);
	}
}

template<typename BASE>
template<typename window_type>
void ui::tinted_icon<BASE>::windowed_update(window_type& w, world_state& s) {
	if constexpr(ui::detail::has_windowed_update<BASE, dynamic_icon<BASE>&, window_type&, world_state&>) {
		BASE::windowed_update(*this, w, s);
	}
}

template<typename BASE>
ui::tooltip_behavior ui::dynamic_icon<BASE>::has_tooltip(gui_object_tag, world_state& ws, const mouse_move&) {
	if constexpr(ui::detail::has_has_tooltip<BASE, world_state&>)
		return BASE::has_tooltip(ws) ? tooltip_behavior::tooltip : tooltip_behavior::no_tooltip;
	else
		return tooltip_behavior::no_tooltip;
}

template<typename BASE>
ui::tooltip_behavior ui::tinted_icon<BASE>::has_tooltip(gui_object_tag, world_state& ws, const mouse_move&) {
	if constexpr(ui::detail::has_has_tooltip<BASE, world_state&>)
		return BASE::has_tooltip(ws) ? tooltip_behavior::tooltip : tooltip_behavior::no_tooltip;
	else
		return tooltip_behavior::no_tooltip;
}

template<typename BASE>
void ui::dynamic_icon<BASE>::create_tooltip(gui_object_tag, world_state& ws, const mouse_move&, tagged_gui_object tw) {
	if constexpr(ui::detail::has_has_tooltip<BASE, world_state&>)
		BASE::create_tooltip(ws, tw);
}

template<typename BASE>
void ui::tinted_icon<BASE>::create_tooltip(gui_object_tag, world_state& ws, const mouse_move&, tagged_gui_object tw) {
	if constexpr(ui::detail::has_has_tooltip<BASE, world_state&>)
		BASE::create_tooltip(ws, tw);
}

template<typename BASE>
void ui::dynamic_icon<BASE>::set_frame(gui_manager& m, uint32_t frame_num) {
	if (const auto go = m.graphics_instances.safe_at(graphics_instance_tag(associated_object->type_dependant_handle)); go) {
		go->frame = static_cast<int32_t>(frame_num);
	}
}

template<typename BASE>
void ui::tinted_icon<BASE>::set_color(gui_manager& m, float r, float g, float b) {
	if(const auto go = m.tinted_icon_instances.safe_at(tinted_icon_instance_tag(associated_object->type_dependant_handle)); go) {
		go->r = r;
		go->g = g;
		go->b = b;
	}
}

template<typename BASE>
void ui::dynamic_icon<BASE>::set_visibility(gui_manager& m, bool visible) {
	if (visible)
		ui::make_visible_and_update(m, *associated_object);
	else
		ui::hide(*associated_object);
}

template<typename BASE>
void ui::tinted_icon<BASE>::set_visibility(gui_manager& m, bool visible) {
	if(visible)
		ui::make_visible_and_update(m, *associated_object);
	else
		ui::hide(*associated_object);
}

template<typename B>
ui::tagged_gui_object ui::create_static_element(world_state& ws, icon_tag handle, tagged_gui_object parent, dynamic_icon<B>& b) {
	auto new_obj = ui::detail::create_element_instance(ws.s.gui_m, ws.w.gui_m, handle);

	new_obj.object.associated_behavior = &b;
	b.associated_object = &new_obj.object;

	ui::add_to_back(ws.w.gui_m, parent, new_obj);
	ws.w.gui_m.flag_minimal_update();
	return new_obj;
}

template<typename B>
ui::tagged_gui_object ui::create_static_element(world_state& ws, icon_tag handle, tagged_gui_object parent, tinted_icon<B>& b) {
	const auto new_gobj = manager.gui_objects.emplace();

	const ui::icon_def& icon_def = static_manager.ui_definitions.icons[handle];

	const uint16_t rotation =
		(icon_def.flags & ui::icon_def::rotation_mask) == ui::icon_def::rotation_upright ?
		ui::gui_object::rotation_upright :
		((icon_def.flags & ui::icon_def::rotation_mask) == ui::icon_def::rotation_90_right ? ui::gui_object::rotation_right : ui::gui_object::rotation_left);

	new_gobj.object.position = icon_def.position;
	new_gobj.object.flags.fetch_or(rotation, std::memory_order_acq_rel);
	new_gobj.object.align = alignment_from_definition(icon_def);

	instantiate_graphical_object(static_manager, manager, new_gobj, 0, true);

	new_gobj.object.size.x *= icon_def.scale;
	new_gobj.object.size.y *= icon_def.scale;

	new_gobj.object.associated_behavior = &b;
	b.associated_object = &new_gobj.object;

	ui::add_to_back(ws.w.gui_m, parent, new_gobj);
	ws.w.gui_m.flag_minimal_update();

	return new_gobj;
}

template<typename BASE>
void ui::dynamic_transparent_icon<BASE>::update_data(gui_object_tag, world_state& w) {
	if constexpr(ui::detail::has_update<BASE, dynamic_transparent_icon<BASE>&, world_state&>) {
		BASE::update(*this, w);
	}
}

template<typename BASE>
template<typename window_type>
void ui::dynamic_transparent_icon<BASE>::windowed_update(window_type& w, world_state& s) {
	if constexpr(ui::detail::has_windowed_update<BASE, dynamic_transparent_icon<BASE>&, window_type&, world_state&>) {
		BASE::windowed_update(*this, w, s);
	}
}

template<typename BASE>
void ui::dynamic_transparent_icon<BASE>::set_frame(gui_manager& m, uint32_t frame_num) {
	if(const auto go = m.graphics_instances.safe_at(graphics_instance_tag(associated_object->type_dependant_handle)); go) {
		go->frame = static_cast<int32_t>(frame_num);
	}
}

template<typename BASE>
void ui::dynamic_transparent_icon<BASE>::set_visibility(gui_manager& m, bool visible) {
	if(visible)
		ui::make_visible_and_update(m, *associated_object);
	else
		ui::hide(*associated_object);
}

template<typename B>
ui::tagged_gui_object ui::create_static_element(world_state& ws, icon_tag handle, tagged_gui_object parent, dynamic_transparent_icon<B>& b) {
	auto new_obj = ui::detail::create_element_instance(ws.s.gui_m, ws.w.gui_m, handle);

	new_obj.object.associated_behavior = &b;
	b.associated_object = &new_obj.object;

	ui::add_to_back(ws.w.gui_m, parent, new_obj);
	ws.w.gui_m.flag_minimal_update();
	return new_obj;
}
