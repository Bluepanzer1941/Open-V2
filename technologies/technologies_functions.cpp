#include "common\\common.h"
#include "technologies_functions.h"
#include "nations\\nations.h"
#include "world_state\\world_state.h"
#include "modifiers\\modifier_functions.h"
#include "military\\military_functions.h"

namespace technologies {
	void init_technology_state(world_state& ws) {
		ws.w.technology_s.discovery_count.resize(ws.s.technology_m.technologies_container.size());
		std::fill_n(ws.w.technology_s.discovery_count.data(), ws.s.technology_m.technologies_container.size(), 0);
	}
	void apply_technology(world_state& ws, nations::nation& this_nation, tech_tag tech) {
		technology& t = ws.s.technology_m.technologies_container[tech];
		auto nation_id = this_nation.id;

		ws.w.technology_s.discovery_count[tech] += 1;

		bit_vector_set(ws.w.nation_s.active_technologies.get_row(nation_id), to_index(tech), true);

		this_nation.tech_attributes += t.attributes;

		if(is_valid_index(t.production_adjustment)) {
			const auto production_adjustment_count = ws.s.economy_m.goods_count * uint32_t(production_adjustment::production_adjustment_count);
			Eigen::Map<Eigen::Matrix<float, -1, 1>>(
				ws.w.nation_s.production_adjustments.get_row(nation_id),
				production_adjustment_count) +=
				Eigen::Map<Eigen::Matrix<float, -1, 1>>(
					ws.s.technology_m.production_adjustments.get_row(t.production_adjustment), 
					production_adjustment_count);
		}
		if(is_valid_index(t.rebel_adjustment)) {
			const auto rebel_types_count = int32_t(ws.s.population_m.rebel_types.size());
			Eigen::Map<Eigen::Matrix<float, -1, 1>>(
				ws.w.nation_s.rebel_org_gain.get_row(nation_id),
				rebel_types_count) +=
				Eigen::Map<Eigen::Matrix<float, -1, 1>>(
					ws.s.technology_m.rebel_org_gain.get_row(t.rebel_adjustment),
					rebel_types_count);
		}
		if(is_valid_index(t.unit_adjustment)) {
			for(uint32_t i = to_index(military::naval_unit_base) + 1ui32; i < ws.s.military_m.unit_types_count; ++i) {
				military::unit_type_tag this_unit(static_cast<military::unit_type_tag::value_base_t>(i));
				auto& stats_vector = ws.w.nation_s.unit_stats.get(nation_id, this_unit);
				stats_vector +=
					ws.s.technology_m.unit_type_adjustments.get(t.unit_adjustment, this_unit);
				auto unit_class = ws.s.military_m.unit_types[this_unit].flags & military::unit_type::class_mask;
				if((unit_class == military::unit_type::class_big_ship) |
					(unit_class == military::unit_type::class_light_ship) |
					(unit_class == military::unit_type::class_transport)) {
					//naval
					stats_vector += ws.s.technology_m.unit_type_adjustments.get(t.unit_adjustment, military::naval_unit_base);
				} else {
					//land
					stats_vector += ws.s.technology_m.unit_type_adjustments.get(t.unit_adjustment, military::army_unit_base);
				}
			}
		}

		if(is_valid_index(t.activate_factory)) {
			auto good = ws.s.economy_m.factory_types[t.activate_factory].output_good;
			bit_vector_set(ws.w.nation_s.active_goods.get_row(nation_id), to_index(good), true);
		}
		if(is_valid_index(t.enable_crime)) {
			for(int32_t i = int32_t(ws.s.modifiers_m.crimes.size()); i--; ) {
				if(ws.s.modifiers_m.crimes[uint32_t(i)].modifier == t.enable_crime) {
					bit_vector_set(&(this_nation.enabled_crimes), uint32_t(i), true);
					break;
				}
			}
		}
		if((t.flags & technology::gas_defence) != 0)
			this_nation.flags |= nations::nation::has_gas_defence;
		if((t.flags & technology::gas_attack) != 0)
			this_nation.flags |= nations::nation::has_gas_attack;
	}


	void apply_single_technology(world_state& ws, nations::nation& this_nation, tech_tag tech) {
		if(bit_vector_test(ws.w.nation_s.active_technologies.get_row(this_nation.id), to_index(tech)) == true)
			return;

		apply_technology(ws, this_nation, tech);
		if(is_valid_index(ws.s.technology_m.technologies_container[tech].unit_adjustment))
			military::update_all_unit_attributes(ws, this_nation);
		this_nation.base_prestige += ws.s.technology_m.technologies_container[tech].shared_prestige / float(ws.w.technology_s.discovery_count[tech]);
		
		// to be caught by standard update
		//if(is_valid_index(ws.s.technology_m.technologies_container[tech].modifier))
		//	modifiers::reset_national_modifier(ws, this_nation);
	}

	void reset_technologies(world_state& ws, nations::nation& this_nation) {
		this_nation.tech_attributes = tech_attribute_vector::Zero();

		auto tech_row = ws.w.nation_s.active_technologies.get_row(this_nation.id);
		Eigen::Map<Eigen::Matrix<uint64_t, -1, 1>>(tech_row, ws.w.nation_s.active_technologies.inner_size) =
			Eigen::Matrix<uint64_t, -1, 1>::Zero(ws.w.nation_s.active_technologies.inner_size);

		auto active_goods = ws.w.nation_s.active_goods.get_row(this_nation.id);
		Eigen::Map<Eigen::Matrix<uint64_t, -1, 1>>(active_goods, ws.w.nation_s.active_goods.inner_size) =
			Eigen::Matrix<uint64_t, -1, 1>::Zero(ws.w.nation_s.active_goods.inner_size);

		for(int32_t i = int32_t(ws.s.modifiers_m.crimes.size()); i--; ) {
			if(ws.s.modifiers_m.crimes[uint32_t(i)].default_active)
				bit_vector_set(&(this_nation.enabled_crimes), uint32_t(i), true);
			else
				bit_vector_set(&(this_nation.enabled_crimes), uint32_t(i), false);
		}

		const auto production_adjustment_count = ws.s.economy_m.goods_count * uint32_t(production_adjustment::production_adjustment_count);
		Eigen::Map<Eigen::Matrix<float, -1, 1>>(
			ws.w.nation_s.production_adjustments.get_row(this_nation.id),
			production_adjustment_count) =
			Eigen::Matrix<float, -1, 1>::Zero(production_adjustment_count);

		const auto rebel_types_count = int32_t(ws.s.population_m.rebel_types.size());
		Eigen::Map<Eigen::Matrix<float, -1, 1>>(
			ws.w.nation_s.rebel_org_gain.get_row(this_nation.id),
			rebel_types_count) = Eigen::Matrix<float, -1, 1>::Zero(rebel_types_count);
	}

	void restore_technologies(world_state& ws, nations::nation& this_nation) {
		auto tech_row = ws.w.nation_s.active_technologies.get_row(this_nation.id);
		for(int32_t i = int32_t(ws.s.technology_m.technologies_container.size()); i--; ) {
			if(bit_vector_test(tech_row, uint32_t(i))) {
				apply_technology(ws, this_nation, tech_tag(static_cast<tech_tag::value_base_t>(i)));
			}
		}

		military::update_all_unit_attributes(ws, this_nation);
	}

	float daily_research_points(world_state const& ws, nations::nation const& n) {
		auto id = n.id;
		if(!ws.w.nation_s.nations.is_valid_index(id))
			return 0.0f;

		int64_t total_pop = ws.w.nation_s.nation_demographics.get(id, population::total_population_tag);
		int64_t* pop_by_type = ws.w.nation_s.nation_demographics.get_row(id) + to_index(population::to_demo_tag(ws, population::pop_type_tag(0)));

		float points_by_type = 0.0f;
		if(total_pop != 0)
			for(uint32_t i = 0; i < ws.s.population_m.count_poptypes; ++i) {
				population::pop_type_tag this_tag(static_cast<population::pop_type_tag::value_base_t>(i));
				auto& pt = ws.s.population_m.pop_types[this_tag];
				if(pt.research_points != 0 && pt.research_optimum != 0.0f) {
					points_by_type += float(pt.research_points) * std::min(1.0f, (float(pop_by_type[i]) / float(total_pop)) / ws.s.population_m.pop_types[this_tag].research_optimum);
				}
			}

		return (n.modifier_values[modifiers::national_offsets::research_points] + points_by_type) * (1.0f + n.modifier_values[modifiers::national_offsets::research_points_modifier]);
	}

	float effective_tech_cost(tech_tag t, world_state const& ws, nations::nation const& this_nation) {
		auto& tech = ws.s.technology_m.technologies_container[t];
		auto base_cost = float(tech.cost);
		auto years_after_unlock_adjustment = std::max(0.0f, 1.0f - float(int32_t(tag_to_date(ws.w.current_date).year()) - int32_t(tech.year)) / ws.s.modifiers_m.global_defines.tech_year_span);
		float modifier = [&ws, &tech, &this_nation](){
			if(tech.category == tech_category_type::army)
				return 1.0f + this_nation.modifier_values[modifiers::national_offsets::army_tech_research_bonus];
			if(tech.category == tech_category_type::navy)
				return 1.0f + this_nation.modifier_values[modifiers::national_offsets::navy_tech_research_bonus];
			if(tech.category == tech_category_type::commerce)
				return 1.0f + this_nation.modifier_values[modifiers::national_offsets::commerce_tech_research_bonus];
			if(tech.category == tech_category_type::industry)
				return 1.0f + this_nation.modifier_values[modifiers::national_offsets::industry_tech_research_bonus];
			if(tech.category == tech_category_type::culture)
				return 1.0f + this_nation.modifier_values[modifiers::national_offsets::culture_tech_research_bonus];
			else
				return 1.0f;
		}();
		float vassal_factor = [&ws, t, &this_nation]() {
			if(auto ol = this_nation.overlord; ol) {
				if(auto id = ol->id; ws.w.nation_s.nations.is_valid_index(id)) {
					return bit_vector_test(ws.w.nation_s.active_technologies.get_row(id), to_index(t)) ? ws.s.modifiers_m.global_defines.tech_factor_vassal : 1.0f;
				}
			}
			return 1.0f;
		}();
		return years_after_unlock_adjustment * vassal_factor* base_cost / modifier;
	}

	float get_invention_chance(tech_tag t, world_state& ws, nations::nation& this_nation) {
		return modifiers::test_additive_factor(ws.s.technology_m.technologies_container[t].ai_chance, ws, &this_nation, nullptr, nullptr) / 100.0f;
	}

	bool can_research(tech_tag t, world_state const& ws, nations::nation const& this_nation) {
		auto id = this_nation.id;
		auto id_valid = ws.w.nation_s.nations.is_valid_index(id);
		if(id_valid && bit_vector_test(ws.w.nation_s.active_technologies.get_row(id), to_index(t)))
			return false;
		auto& tech = ws.s.technology_m.technologies_container[t];
		if(tech.year != 0 && tag_to_date(ws.w.current_date).year() < tech.year)
			return false;
		if(is_valid_index(tech.allow) && !triggers::test_trigger(ws.s.trigger_m.trigger_data.data() + to_index(tech.allow),
			const_cast<world_state&>(ws), &const_cast<nations::nation&>(this_nation), &const_cast<nations::nation&>(this_nation), nullptr, nullptr))
			return false;
		if(is_valid_index(tech.preceeding) && id_valid && !bit_vector_test(ws.w.nation_s.active_technologies.get_row(id), to_index(tech.preceeding)))
			return false;

		return true;
	}
}
