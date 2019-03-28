#include "world_map.h"
#include "concurrency_tools\\concurrency_tools.hpp"
#define GLEW_STATIC

#include "glew.h"
#include "wglew.h"
#include <Windows.h>
#undef min
#undef max
#include <map>
#include <deque>

#include "provinces\\provinces.h"
#include "soil\\SOIL.h"
#include <thread>
#include "world_state\\world_state.h"
#include "economy\\economy_functions.h"
#include "concurrency_tools\\concurrency_tools.hpp"
#include "provinces\\province_functions.h"

namespace graphics {

	enum class subpixel_edge {
		a = 0,
		b = 1,
		c = 2,
		d = 3,
		e = 4,
		f = 5,
		g = 6,
		h = 7,
		no_edge = 8
	};
	enum class subpixel_point {
		one = 0,
		two = 1,
		three = 2,
		four = 3,
		five = 4,
		six = 5,
		seven = 6,
		eight = 7,
	};
	struct pt_xy {
		int32_t x;
		int32_t y;

		pt_xy() : x(0), y(0) {}
		pt_xy(int32_t a, int32_t b) : x(a), y(b) {}
		pt_xy operator+(pt_xy const& o) const { return pt_xy(x + o.x, y + o.y); }
		void operator+=(pt_xy const& o) { x += o.x;  y += o.y; }
		void operator=(pt_xy const& o) { x = o.x; y = o.y; }
	};
	pt_xy subpixel_other_side(subpixel_edge v) {
		switch(v) {
			case subpixel_edge::a: return pt_xy(0, -1);
			case subpixel_edge::b: return pt_xy(0, -1);
			case subpixel_edge::c: return pt_xy(1, 0);
			case subpixel_edge::d: return pt_xy(1, 0);
			case subpixel_edge::e: return pt_xy(-1, -1);
			case subpixel_edge::f: return pt_xy(1, -1);
			case subpixel_edge::g: return pt_xy(-1, -1);
			case subpixel_edge::h: return pt_xy(0, -1);
		}
		assert(false);
	}
	subpixel_point pick_different(subpixel_point value, subpixel_point a, subpixel_point b) {
		return value == a ? b : a;
	}
	subpixel_point matched_subpixel_point(subpixel_edge v, subpixel_point p) {
		switch(v) {
			case subpixel_edge::a: return pick_different(p, subpixel_point::three, subpixel_point::two);
			case subpixel_edge::b: return pick_different(p, subpixel_point::three, subpixel_point::four);
			case subpixel_edge::c: return pick_different(p, subpixel_point::four, subpixel_point::five);
			case subpixel_edge::d: return pick_different(p, subpixel_point::five, subpixel_point::six);
			case subpixel_edge::e: return pick_different(p, subpixel_point::one, subpixel_point::three);
			case subpixel_edge::f: return pick_different(p, subpixel_point::three, subpixel_point::five);
			case subpixel_edge::g: return pick_different(p, subpixel_point::one, subpixel_point::seven);
			case subpixel_edge::h: return pick_different(p, subpixel_point::seven, subpixel_point::five);
		}
		assert(false);
	}
	bool edges_are_straight(subpixel_edge a, subpixel_edge b) {
		return (a == subpixel_edge::a && b == subpixel_edge::b)
			|| (a == subpixel_edge::b && b == subpixel_edge::a)
			|| (a == subpixel_edge::c && b == subpixel_edge::d)
			|| (a == subpixel_edge::d && b == subpixel_edge::c)
			|| (a == subpixel_edge::g && b == subpixel_edge::f)
			|| (a == subpixel_edge::f && b == subpixel_edge::g)
			|| (a == subpixel_edge::h && b == subpixel_edge::e)
			|| (a == subpixel_edge::e && b == subpixel_edge::h);
	}

	bool is_corner_point(subpixel_point p) {
		if(p == subpixel_point::two || p == subpixel_point::four || p == subpixel_point::six || p == subpixel_point::eight)
			return true;
		else
			return false;
	}
	template<typename FUNC>
	void enumerate_edges_at_point(subpixel_point p, FUNC&& f) {
		switch(p) {
			case subpixel_point::one:
				f(pt_xy(-1, 0), subpixel_edge::h, subpixel_point::five);
				f(pt_xy(0, 0), subpixel_edge::g, subpixel_point::one);
				f(pt_xy(-1, 0), subpixel_edge::f, subpixel_point::five);
				f(pt_xy(0, 0), subpixel_edge::e, subpixel_point::one);
				f(pt_xy(-1, 0), subpixel_edge::d, subpixel_point::five);
				f(pt_xy(-1, 0), subpixel_edge::c, subpixel_point::five);
				break;
			case subpixel_point::two:
				f(pt_xy(-1, -1), subpixel_edge::d, subpixel_point::six);
				f(pt_xy(-1, 0), subpixel_edge::c, subpixel_point::four);
				f(pt_xy(-1, 0), subpixel_edge::b, subpixel_point::four);
				f(pt_xy(0, 0), subpixel_edge::a, subpixel_point::two);
				break;
			case subpixel_point::three:
				f(pt_xy(0, -1), subpixel_edge::h, subpixel_point::seven);
				f(pt_xy(0, -1), subpixel_edge::g, subpixel_point::seven);
				f(pt_xy(0, 0), subpixel_edge::f, subpixel_point::three);
				f(pt_xy(0, 0), subpixel_edge::e, subpixel_point::three);
				f(pt_xy(0, 0), subpixel_edge::b, subpixel_point::three);
				f(pt_xy(0, 0), subpixel_edge::a, subpixel_point::three);
				break;
			case subpixel_point::four:
				f(pt_xy(0, -1), subpixel_edge::d, subpixel_point::six);
				f(pt_xy(0, 0), subpixel_edge::c, subpixel_point::four);
				f(pt_xy(0, 0), subpixel_edge::b, subpixel_point::four);
				f(pt_xy(1, 0), subpixel_edge::a, subpixel_point::two);
				break;
			case subpixel_point::five:
				f(pt_xy(0, 0), subpixel_edge::h, subpixel_point::five);
				f(pt_xy(1, 0), subpixel_edge::g, subpixel_point::one);
				f(pt_xy(0, 0), subpixel_edge::f, subpixel_point::five);
				f(pt_xy(1, 0), subpixel_edge::e, subpixel_point::one);
				f(pt_xy(0, 0), subpixel_edge::d, subpixel_point::five);
				f(pt_xy(0, 0), subpixel_edge::c, subpixel_point::five);
				break;
			case subpixel_point::six:
				f(pt_xy(0, 0), subpixel_edge::d, subpixel_point::six);
				f(pt_xy(0, 1), subpixel_edge::c, subpixel_point::four);
				f(pt_xy(0, 1), subpixel_edge::b, subpixel_point::four);
				f(pt_xy(1, 0), subpixel_edge::a, subpixel_point::two);
				break;
			case subpixel_point::seven:
				f(pt_xy(0, 0), subpixel_edge::h, subpixel_point::seven);
				f(pt_xy(0, 0), subpixel_edge::g, subpixel_point::seven);
				f(pt_xy(0, 1), subpixel_edge::f, subpixel_point::three);
				f(pt_xy(0, 1), subpixel_edge::e, subpixel_point::three);
				f(pt_xy(0, 1), subpixel_edge::b, subpixel_point::three);
				f(pt_xy(0, 1), subpixel_edge::a, subpixel_point::three);
				break;
			case subpixel_point::eight:
				f(pt_xy(-1, 0), subpixel_edge::d, subpixel_point::six);
				f(pt_xy(-1, 1), subpixel_edge::c, subpixel_point::four);
				f(pt_xy(-1, 1), subpixel_edge::b, subpixel_point::four);
				f(pt_xy(0, 1), subpixel_edge::a, subpixel_point::two);
				break;
		}
	}

	constexpr int32_t block_size = 64;

	struct block_view {
		uint16_t const* base_data;
		int32_t x_size;
		int32_t y_size;
		int32_t x_offset;
		int32_t y_offset;

		block_view(uint16_t const* d, int32_t sz_x, int32_t sz_y, int32_t x, int32_t y) :
			base_data(d), x_size(sz_x), y_size(sz_y), x_offset(x), y_offset(y) {}

		uint16_t operator()(pt_xy v) const {
			auto adjusted_position = pt_xy(x_offset, y_offset) + v;

			if(adjusted_position.y < 0 || adjusted_position.y >= y_size)
				return 0;

			if(adjusted_position.x < 0)
				adjusted_position.x += x_size;
			if(adjusted_position.x == x_size)
				adjusted_position.x = 0;
			if(adjusted_position.x > x_size)
				return 0;

			return base_data[adjusted_position.y * x_size + adjusted_position.x];
		}

		std::pair<float, float> to_map_point(pt_xy v, subpixel_point sub_pt) const {
			std::pair<float, float> p(x_offset + v.x, y_offset + v.y);
			switch(sub_pt) {
				case subpixel_point::one:
					p.second + 0.5f;
					break;
				case subpixel_point::two:
					break;
				case subpixel_point::three:
					p.first + 0.5f;
					break;
				case subpixel_point::four:
					p.first + 1.0f;
					break;
				case subpixel_point::five:
					p.first + 1.0f;
					p.second + 0.5f;
					break;
				case subpixel_point::six:
					p.first + 1.0f;
					p.second + 1.0f;
					break;
				case subpixel_point::seven:
					p.first + 0.5f;
					p.second + 1.0f;
					break;
				case subpixel_point::eight:
					p.second + 1.0f;
					break;
			}
			return p;
		}
	};

	struct segment_data {
		uint8_t value = 0;

		void set(subpixel_edge e, bool v) {
			uint32_t bit = 1 << int32_t(e);

			if(v)
				value |= uint8_t(bit);
			else
				value &= uint8_t(~bit);
		}
		bool get(subpixel_edge e) {
			uint32_t bit = 1 << int32_t(e);
			return (value & bit) != 0;
		}
	};

	struct ordered_id_pair {
		uint16_t a;
		uint16_t b;
			
		ordered_id_pair(uint16_t x, uint16_t y) : a(std::min(x,y)), b(std::max(x,y)) {}
		ordered_id_pair(uint32_t v) : a(uint16_t(v >> 16)), b(uint16_t(v)) {};

		operator uint32_t() {
			return (uint32_t(a) << 16ui32) | uint32_t(b);
		}
	};

	struct borders_set {
		std::vector<std::pair<float, float>> points;
		std::unordered_map<uint32_t, std::vector<int32_t>> ordered_pair_to_indices;
	};

	void consume_single_border(borders_set& output, block_view const& block, segment_data* segments, pt_xy point, subpixel_edge edge, subpixel_point sub_point) {
		ordered_id_pair origin_pair(block(point), block(point + subpixel_other_side(edge)));

		std::vector<int32_t>& destination_indices = output.ordered_pair_to_indices[uint32_t(origin_pair)];
		if(destination_indices.size() != 0)
			destination_indices.push_back(-1);

		destination_indices.push_back(int32_t(output.points.size()));
		output.points.push_back(block.to_map_point(point, sub_point));

		edge = subpixel_edge::no_edge;
		bool found_next_point = false;

		do {
			found_next_point = false;
			subpixel_edge old_edge = edge;

			enumerate_edges_at_point(
				sub_point,
				[old_point = point, edge_id = uint32_t(origin_pair), &block, &found_next_point, segments, &point, &edge, &sub_point](
				pt_xy direction,
				subpixel_edge inner_new_edge,
				subpixel_point inner_new_sub_point) {
				
				pt_xy temp_new_point = old_point + direction;

				// bounds test
				if(temp_new_point.x >= 0 && temp_new_point.x < block_size &&
					temp_new_point.y >= 0 && temp_new_point.y < block_size) {

					// is edge present?
					if(segments[temp_new_point.x + temp_new_point.y * block_size].get(inner_new_edge)) {
						ordered_id_pair new_edge_id(block(temp_new_point), block(temp_new_point + subpixel_other_side(inner_new_edge)));

						// is part of same border?
						if(uint32_t(new_edge_id) == edge_id) {
							point = temp_new_point;
							edge = inner_new_edge;
							sub_point = inner_new_sub_point;
							found_next_point = true;
						}
					}
				}
			});

			if(found_next_point) {
				segments[point.x + point.y * block_size].set(edge, false);

				if(edges_are_straight(old_edge, edge)) {
					output.points.pop_back();
					output.points.push_back(block.to_map_point(point, sub_point));
				} else {
					destination_indices.push_back(int32_t(output.points.size()));
					output.points.push_back(block.to_map_point(point, sub_point));
				}
			}
		} while(found_next_point);
	}
	
	borders_set block_to_borders(block_view const& block, int32_t first_sea_province) {
		borders_set result;
		segment_data segments[block_size * block_size] = { segment_data() };

		// populate border segments
		for(int32_t i = 0; i < block_size; ++i) {
			for(int32_t j = 0; j < block_size; ++j) {
				pt_xy t(i, j);

				if(block(t) != 0) {
					for(int32_t k = 0; k < 4; ++k) {
						if(block(t) < first_sea_province || block(t + subpixel_other_side(subpixel_edge(k))) < first_sea_province)
							segments[i + j * block_size].set(subpixel_edge(k), block(t) != block(t + subpixel_other_side(subpixel_edge(k))));
					}
					if(block(t + pt_xy(-1, -1)) == block(t + pt_xy(-1, 0)) &&
						block(t + pt_xy(-1, -1)) == block(t + pt_xy(0, -1))) {

						if(block(t) < first_sea_province || block(t + pt_xy(-1, -1)) < first_sea_province)
							segments[i + j * block_size].set(subpixel_edge::e, block(t) != block(t + pt_xy(-1, -1)));
					}
					if(block(t + pt_xy(1, -1)) == block(t + pt_xy(1, 0)) &&
						block(t + pt_xy(1, -1)) == block(t + pt_xy(0, -1))) {

						if(block(t) < first_sea_province || block(t + pt_xy(1, -1)) < first_sea_province)
							segments[i + j * block_size].set(subpixel_edge::f, block(t) != block(t + pt_xy(1, -1)));
					}
					if(block(t + pt_xy(1, 1)) == block(t + pt_xy(1, 0)) &&
						block(t + pt_xy(1, 1)) == block(t + pt_xy(0, 1))) {

						if(block(t) < < first_sea_province || block(t + pt_xy(1, 1)) < < first_sea_province)
							segments[i + j * block_size].set(subpixel_edge::h, block(t) != block(t + pt_xy(1, 1)));
					}
					if(block(t + pt_xy(-1, 1)) == block(t + pt_xy(-1, 0)) &&
						block(t + pt_xy(-1, 1)) == block(t + pt_xy(0, 1))) {

						if(block(t) < < first_sea_province || block(t + pt_xy(-1, 1)) < < first_sea_province)
							segments[i + j * block_size].set(subpixel_edge::g, block(t) != block(t + pt_xy(-1, 1)));
					}
				}
			}
		}

		// consume border segments
		for(int32_t i = 0; i < block_size; ++i) {
			for(int32_t j = 0; j < block_size; ++j) {
				pt_xy t(i, j);

				while(segments[i + j * block_size].value != 0) {
					for(int32_t k = 0; k < 8; ++k) {
						if(segments[i + j * block_size].get(subpixel_edge(k))) {
							// least point = 
							auto sub_pt_a = matched_subpixel_point(subpixel_edge(k), subpixel_point::one);
							auto sub_pt_b = matched_subpixel_point(subpixel_edge(k), sub_pt_a);
							auto least_sub_pt = subpixel_point(std::min(int32_t(sub_pt_a), int32_t(sub_pt_b)));
							consume_single_border(result, block, segments, t, subpixel_edge(k), least_sub_pt);
						}
					}
				}
			}
		}

		return result;
	}

	struct map_vertex_data_3d_b {
		float x;
		float y;
		float z;
		float tx_value;
		float ty_value;
	};

	template<typename FUNC>
	void globe_mesh::for_each_visible_block(int32_t width, int32_t height, FUNC && f) {
		const auto wblocks = (width + block_size - 1) / block_size;
		const auto hblocks = (height + block_size - 1) / block_size;

		for(int32_t j = 0; j < hblocks; ++j) {
			for(int32_t i = 0; i < wblocks; ++i) {
				int32_t const index_a = (i + j * (wblocks + 1)) * 2;
				std::pair<float, float> top_left(transformed_buffer[index_a], transformed_buffer[index_a + 1]);

				int32_t const index_b = (i + 1 + j * (wblocks + 1)) * 2;
				std::pair<float, float> top_right(transformed_buffer[index_b], transformed_buffer[index_b + 1]);

				int32_t const index_c = (i + (j + 1) * (wblocks + 1)) * 2;
				std::pair<float, float> bottom_left(transformed_buffer[index_c], transformed_buffer[index_c + 1]);

				int32_t const index_d = (i + 1 + (j + 1) * (wblocks + 1)) * 2;
				std::pair<float, float> bottom_right(transformed_buffer[index_c], transformed_buffer[index_d + 1]);

				bool const out_of_view =
					(top_left.first < -0.5f && top_right.first < -0.5f && bottom_left.first < -0.5f && bottom_right.first < -0.5f)
					|| (top_left.first > 0.5f && top_right.first > 0.5f && bottom_left.first > 0.5f && bottom_right.first > 0.5f)
					|| (top_left.second < -0.5f && top_right.second < -0.5f && bottom_left.second < -0.5f && bottom_right.second < -0.5f)
					|| (top_left.second > 0.5f && top_right.second > 0.5f && bottom_left.second > 0.5f && bottom_right.second > 0.5f)
					;

				bool const positive_area = ((top_right.first - top_left.first) * (top_right.second - top_left.second) +
					(bottom_right.first - top_right.first) * (bottom_right.second - top_right.second)) > 0.0f;

				if(!out_of_view && positive_area) {
					float x_matrix[4] = { top_left.first, bottom_left.first, top_right.first, bottom_right.first };
					float y_matrix[4] = { top_left.second, bottom_left.second, top_right.second, bottom_right.second };

					f(x_matrix, y_matrix);
				}
			}
		}
	}

	uint16_t get_value_from_data(int32_t i, int32_t j, const uint16_t* data, int32_t width, int32_t height);
	uint16_t get_value_from_data_h(int32_t i, int32_t j, const uint16_t* data, int32_t width);

	uint16_t get_value_from_data(int32_t i, int32_t j, const uint16_t* data, int32_t width, int32_t height) {
		if((j < 0) | (j >= height)) {
			return 0ui16;
		}
		if(i < 0)
			i += width;
		if(i >= width)
			i -= width;
		return data[i + j * width];
	}

	uint16_t get_value_from_data_h(int32_t i, int32_t j, const uint16_t* data, int32_t width) {
		if(i >= width)
			i -= width;
		return data[i + j * width];
	}

	namespace map_parameters {
		constexpr GLuint aspect = 0;
		constexpr GLuint scale = 1;
		constexpr GLuint division_count = 2;
		constexpr GLuint inner_step = 3;
		constexpr GLuint rotation_matrix = 4;
	}

	namespace map_parameters_b {
		constexpr GLuint aspect = 0;
		constexpr GLuint scale = 1;
		constexpr GLuint rotation_matrix = 2;
	}

	Eigen::Vector3f projected_a_to_unrotated(float x, float y, float scale, float aspect);
	Eigen::Vector3f projected_b_to_unrotated(float x, float y, float scale, float aspect);
	std::pair<float, float> move_vector_to_vector(const Eigen::Vector3f &source, const Eigen::Vector3f &target);

	Eigen::Vector3f projected_a_to_unrotated(float x, float y, float scale, float aspect) {
		const float projected_y = y * 3.14159265358f / (scale * aspect);
		const float rdiv = projected_y / 3.14159265358f;
		const float projected_x = x * 3.14159265358f / (scale * sqrt(std::max(0.0001f, 1.0f - (3.0f*rdiv*rdiv))));
		const float sin_y = sin(projected_y);
		const float cos_y = cos(projected_y);
		Eigen::Vector3f r(cos(projected_x) * cos_y, sin(projected_x) * cos_y, sin_y);
		r.normalize();
		return r;
	}

	Eigen::Vector3f projected_b_to_unrotated(float x, float y, float scale, float aspect) {
		const float unproj_y = x / scale;
		const float unproj_z = y / (scale * aspect);
		const float uproj_x = sqrt(std::max(0.0f, 1.0f - unproj_y * unproj_y + unproj_z * unproj_z));
		Eigen::Vector3f r(uproj_x, unproj_y, unproj_z);
		r.normalize();
		return r;
	}

	inline float minimal_rotation(float start, float dest) {
		if(dest < start) {
			const float neg_rotation = dest - start;
			const float pos_rotation = neg_rotation + 3.14159265358f * 2.0f;
			return abs(neg_rotation) < abs(pos_rotation) ? neg_rotation : pos_rotation;
		} else {
			const float pos_rotation = dest - start;
			const float neg_rotation = pos_rotation - 3.14159265358f * 2.0f;
			return abs(neg_rotation) < abs(pos_rotation) ? neg_rotation : pos_rotation;
		}
	}

	std::pair<float, float> move_vector_to_vector(const Eigen::Vector3f &source, const Eigen::Vector3f &target) {
		const auto vertical_radius_squared = target.x() * target.x() + target.z() * target.z();

		const auto intermediate_x_squared = std::max(0.0f, vertical_radius_squared - source.z() * source.z());
		const auto intermediate_x = sqrt(intermediate_x_squared);

		const auto first_angle_a = atan2(intermediate_x, target.y());
		const auto first_angle_b = atan2(-intermediate_x, target.y());
		const auto first_angle_initial = atan2(source.x(), source.y());

		const auto first_rotation_a = minimal_rotation(first_angle_initial, first_angle_a);
		const auto first_rotation_b = minimal_rotation(first_angle_initial, first_angle_b);

		const auto second_angle_initial_a = atan2(source.z(), intermediate_x);
		const auto second_angle_initial_b = atan2(source.z(), -intermediate_x);

		const auto second_angle_target = atan2(target.z(), target.x());

		const auto second_rotation_a = minimal_rotation(second_angle_initial_a, second_angle_target);
		const auto second_rotation_b = minimal_rotation(second_angle_initial_b, second_angle_target);

		if(abs(second_rotation_a) < abs(second_rotation_b)) {
			return std::make_pair(-first_rotation_a, -second_rotation_a);
		} else {
			return std::make_pair(-first_rotation_b, -second_rotation_b);
		}
	}

	map_state::map_state() {
		rotate(0.0f, 0.0f);
	}
	void map_state::resize(int32_t x, int32_t y) {
		_aspect = static_cast<float>(x) / static_cast<float>(y);
		globe.update_transformed_buffer(_aspect, scale, _rotation);
	}
	void map_state::rescale_by(float multiplier) {
		scale = std::clamp(scale * multiplier, 1.0f, 18.0f);
		globe.update_transformed_buffer(_aspect, scale, _rotation);
	}
	void map_state::rotate(float longr, float latr) {
		_rotation = Eigen::AngleAxisf(latr, Eigen::Vector3f::UnitY()) * Eigen::AngleAxisf(longr, Eigen::Vector3f::UnitZ());
		inverse_rotation = Eigen::AngleAxisf(-longr, Eigen::Vector3f::UnitZ()) * Eigen::AngleAxisf(-latr, Eigen::Vector3f::UnitY());
		globe.update_transformed_buffer(_aspect, scale, _rotation);
	}
	std::pair<float, float> map_state::normalize_screen_coordinates(int32_t x, int32_t y, int32_t width, int32_t height) const {
		return std::make_pair(float(x * 2) / float(width) - 1.0f, float(-y * 2) / float(height) + 1.0f);
	}
	void map_state::move_vector_to(const Eigen::Vector3f& start, const Eigen::Vector3f& destination) {
		const auto[z_r, y_r] = move_vector_to_vector(start, destination);
		rotate(z_r, y_r);
	}
	Eigen::Vector3f map_state::get_vector_for(const std::pair<float, float>& in) const {
		switch(projection) {
			case projection_type::standard_map:
				return inverse_rotation * projected_a_to_unrotated(in.first, in.second, scale, _aspect);
			case projection_type::spherical:
				return inverse_rotation * projected_b_to_unrotated(in.first, in.second, scale, _aspect);
			default:
				return inverse_rotation * projected_a_to_unrotated(in.first, in.second, scale, _aspect);
		}
	}
	Eigen::Vector3f map_state::get_unrotated_vector_for(const std::pair<float, float>& in) const {
		switch(projection) {
			case projection_type::standard_map:
				return  projected_a_to_unrotated(in.first, in.second, scale, _aspect);
			case projection_type::spherical:
				return  projected_b_to_unrotated(in.first, in.second, scale, _aspect);
			default:
				return  projected_a_to_unrotated(in.first, in.second, scale, _aspect);
		}
	}


	std::pair<int32_t, int32_t> map_display::map_coordinates_from_screen(std::pair<float, float> const& normalized_screen_coordinates) const {
		auto vector = state.get_vector_for(normalized_screen_coordinates);

		const auto vy_pos = asin(vector[2]);
		const auto vx_pos = atan2(vector[1], vector[0]);
		const auto x_off = (vx_pos - left_long) / long_step;
		const auto y_off = (vy_pos - top_lat) / lat_step;

		const int32_t final_yoff = std::clamp(int32_t(y_off), 0, map_height - 1);
		const int32_t final_xoff = std::clamp(x_off < 0.0f ? int32_t(x_off) + map_width : int32_t(x_off), 0, map_width - 1);

		return std::make_pair(final_xoff, final_yoff);
	}

	std::pair<float, float> normalized_coordinates_from_base(float aspect, float scale, Eigen::Matrix3f const& rotation, float x, float y, float z) {
		Eigen::Vector3f rotated = rotation * Eigen::Vector3f(x, y, z).transpose();
		float base_projection_x = atan2(rotated[1], rotated[0]);
		float base_projection_y = asin(rotated[3]);
		float rdiv = base_projection_y / 3.14159265358f;
		return std::pair<float, float>(scale * base_projection_x * sqrt(1.0f - (3.0f*rdiv*rdiv)) / 3.14159265358f,
			base_projection_y * scale * aspect / 3.14159265358f);
	}

	static const char* map_vertex_shader_b =
		"#version 430 core\n"
		"layout (location = 0) in vec3 vertex_position;\n"
		"layout (location = 1) in vec2 value;\n"
		"\n"
		"out vec2 t_value;\n"
		"\n"
		"layout(location = 0) uniform float aspect;\n" // = w/h
		"layout(location = 1) uniform float scale;\n"
		"layout(location = 2) uniform mat3 rotation_matrix;\n"
		"\n"
		"const float hpi = 1.5707963267f;\n"
		"\n"
		"void main() {\n"
		"   vec3 rotated_position = rotation_matrix * vertex_position;\n"
		"   vec2 base_projection = vec2(atan(rotated_position.y, rotated_position.x), asin(rotated_position.z));\n"
		"   float rdiv = base_projection.y / 3.14159265358f;\n"
		"	gl_Position = vec4(scale * base_projection.x * sqrt(1.0f - (3.0f*rdiv*rdiv)) / 3.14159265358f, base_projection.y * scale * aspect / 3.14159265358f, "
		"        (abs(base_projection.x))/16.0f + 0.5f, 1.0f);\n"
		//"	gl_Position = vec4(scale * rotated_position.y, rotated_position.z * scale * aspect, "
		//"        -rotated_position.x/16.0f + 0.5f, 1.0f);\n"
		"	t_value = value;\n"
		"}\n";

	static const char* map_fragment_shader_b =
		"#version 430 core\n"
		"\n"
		"in vec2 t_value;\n"
		"layout (location = 0) out vec4 frag_color;\n"
		"\n"
		"layout (binding = 0) uniform usampler2D data_texture;\n"
		"layout (binding = 1) uniform sampler1D primary_colors;\n"
		"layout (binding = 2) uniform sampler1D secondary_colors;\n"
		"layout (binding = 3) uniform isampler2D corner_texture;\n"
		"layout (binding = 4) uniform sampler2DRect map_shadows;\n"
		"\n"
		"void main() {\n"
		"   const float d_value = abs(fract(t_value.x) - 0.5f) + abs(fract(t_value.y) - 0.5f);\n"
		"   frag_color = texture(map_shadows, t_value) * texelFetch(primary_colors, d_value <= 0.5f ? "
		"       int(texelFetch(data_texture, ivec2(int(t_value.x), int(t_value.y)), 0).r) : "
		"       int(texelFetch(data_texture, "
		"           ivec2(int(t_value.x) + texelFetch(corner_texture, ivec2(int(t_value.x * 2.0f), int(t_value.y * 2.0f)), 0).r,"
		"           int(t_value.y)), 0).r), 0);\n"
		"}\n";

	static const char* map_border_vertex_shader =
		"#version 430 core\n"
		"layout (location = 0) in vec2 vertex_position;\n"
		"\n"
		"layout (location = 0) uniform mat2 x_scale_matrix;\n"
		"layout (location = 1) uniform mat2 y_scale_matrix;\n"
		"\n"
		"void main() {\n"
		"	vec2 vx = vec2(1.0f - vertex_position.x, vertex_position.x);\n"
		"	vec2 vy = vec2(1.0f - vertex_position.x, vertex_position.x);\n"

		"	gl_Position = vec4(dot(vx, x_scale_matrix * vy)), dot(vx, y_scale_matrix * vy)), 0.5f, 1.0f);\n"
		"}\n";

	static const char* map_border_fragment_shader =
		"#version 430 core\n"
		"\n"
		"layout (location = 0) out vec4 frag_color;\n"
		"\n"
		"layout (binding = 0) uniform vec4 border_color;\n"
		"\n"
		"void main() {\n"
		"   frag_color = border_color;\n"
		"}\n";

	inline auto generate_rotation_matrix(float long_rotation, float lat_rotation) {
		Eigen::Matrix3f m;
		m = Eigen::AngleAxisf(lat_rotation, Eigen::Vector3f::UnitX()) * Eigen::AngleAxisf(long_rotation, Eigen::Vector3f::UnitZ());
		return m;
	}

	inline map_vertex_data_3d_b generate_3d_map_point_b(int32_t x_off, int32_t y_off, float base_lat, float lat_step, float long_step) {
		const float vx_pos = x_off * long_step;
		const float vy_pos = y_off * lat_step + base_lat;
		const float cos_vy = cos(vy_pos);
		const float sin_vy = sin(vy_pos);
		//return map_vertex_data_3d{ cos(vx_pos) * cos_vy , sin(vx_pos) * cos_vy, sin_vy, x_off, y_off };
		return map_vertex_data_3d_b{ cos(vx_pos) * cos_vy , sin(vx_pos) * cos_vy, sin_vy, float(x_off), float(y_off) };
	}

	void globe_mesh::update_transformed_buffer(float aspect, float scale, Eigen::Matrix3f const & rotation) {
		int32_t index = 0;
		for(auto& v : vertices) {
			const auto result = normalized_coordinates_from_base(aspect, scale, rotation, v.x, v.y, v.z);
			transformed_buffer[index] = result.first;
			transformed_buffer[index + 1] = result.second;
			index += 2;
		}
	}

	borders_manager::border_block borders_to_graphics_data(borders_set const& b_set, scenario::scenario_manager const& s) {
		borders_manager::border_block result;

		glGenBuffers(1, &result.vertices_handle);
		glBindBuffer(GL_ARRAY_BUFFER, result.vertices_handle);
		glBufferData(GL_ARRAY_BUFFER, static_cast<int64_t>(b_set.points.size() * 8), b_set.points.data(), GL_STATIC_DRAW);

		std::vector<int32_t> temp_indices_buffer;

		for(auto const& indices_set : b_set.ordered_pair_to_indices) {
			ordered_id_pair const ids(indices_set.first);
			if(ids.a >= s.province_m.first_sea_province || ids.b >= s.province_m.first_sea_province) {
				temp_indices_buffer.push_back(-1);
				temp_indices_buffer.insert(temp_indices_buffer.end(), indices_set.second.begin(), indices_set.second.end());
			}
		}

		result.coastal_borders_size = temp_indices_buffer.size();

		for(auto const& indices_set : b_set.ordered_pair_to_indices) {
			ordered_id_pair const ids(indices_set.first);
			provinces::province_tag const a(ids.a);
			provinces::province_tag const b(ids.b);
			if(ids.a < s.province_m.first_sea_province && ids.b < s.province_m.first_sea_province
				&& (s.province_m.province_container.get<province::state_id>(a) != s.province_m.province_container.get<province::state_id>(b))) {
				result.province_borders.push_back(
					borders_manager::province_border_info{
						indices_set.second.size(),
						temp_indices_buffer.size() + 1,
						a,
						b
					});
				temp_indices_buffer.push_back(-1);
				temp_indices_buffer.insert(temp_indices_buffer.end(), indices_set.second.begin(), indices_set.second.end());
			}
		}

		result.state_borders_size = temp_indices_buffer.size() - result.coastal_borders_size;

		for(auto const& indices_set : b_set.ordered_pair_to_indices) {
			ordered_id_pair const ids(indices_set.first);
			provinces::province_tag const a(ids.a);
			provinces::province_tag const b(ids.b);
			if(ids.a < s.province_m.first_sea_province && ids.b < s.province_m.first_sea_province
				&& (s.province_m.province_container.get<province::state_id>(a) == s.province_m.province_container.get<province::state_id>(b))) {
				result.province_borders.push_back(
					borders_manager::province_border_info{
						indices_set.second.size(),
						temp_indices_buffer.size() + 1,
						a,
						b
					});
				temp_indices_buffer.push_back(-1);
				temp_indices_buffer.insert(temp_indices_buffer.end(), indices_set.second.begin(), indices_set.second.end());
			}
		}

		result.province_borders_size = temp_indices_buffer.size() - (result.coastal_borders_size + result.state_borders_size);

		glGenBuffers(1, &result.indices_handle);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, result.indices_handle);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<int64_t>(temp_indices_buffer.size() * sizeof(int32_t)), temp_indices_buffer.data(), GL_STATIC_DRAW);

		return result;
	}

	void render_coastal_borders(borders_manager::border_block const& block) {

	}
	void render_state_borders(borders_manager::border_block const& block) {

	}
	void render_province_borders(borders_manager::border_block const& block) {
		glLineWidth(2.0f);
	}

	void map_display::populate_borders(scenario::scenario_manager const& s, uint16_t const* map_data, int32_t width, int32_t height) {
		const auto wblocks = (width + block_size - 1) / block_size;
		const auto hblocks = (height + block_size - 1) / block_size;

		borders.borders.resize(wblocks * hblocks);

		concurrency::parallel_for(0, hblocks, 1, [_this = this, wblocks, &s](int32_t j){
			for(int32_t i = 0; i < wblocks; ++i) {
				_this->borders.borders[i + j * wblocks] = borders_to_graphics_data(
					block_to_borders(
						block_view(map_data, width, height, block_size * i, block_size * j),
						s.province_m.first_sea_province),
					s);
			}
		});

	}

	std::tuple<uint32_t, uint32_t, uint32_t> create_patches_buffers_b(int32_t width, int32_t height, globe_mesh& mesh, float base_lat, float lat_step, float long_step) {
		std::vector<uint32_t> indices;
		std::vector<map_vertex_data_3d_b> vertices;

		const auto wblocks = (width + block_size - 1) / block_size;
		const auto hblocks = (height + block_size - 1) / block_size;

		for (int32_t j = 0; j <= hblocks; ++j) {
			for (int32_t i = 0; i <= wblocks; ++i) {
				auto const pt = generate_3d_map_point_b(std::min(i * block_size, width), std::min(j * block_size, height), base_lat, lat_step, long_step);
				vertices.emplace_back(pt);
				mesh.vertices.push_back(globe_mesh::vertex{pt.x, pt.y, pt.z});
			}
		}
		mesh.transformed_buffer.resize(mesh.vertices.size() * 2);

		for (int32_t j = 0; j < hblocks; ++j) {
			for (int32_t i = 0; i <= wblocks; ++i) {
				indices.push_back(static_cast<uint32_t>(i + j * (wblocks + 1)));
				indices.push_back(static_cast<uint32_t>(i + (j + 1) * (wblocks + 1)));
			}
			indices.push_back(static_cast<uint32_t>(-1));
		}
		uint32_t element_buffer = 0;
		uint32_t vertex_buffer = 0;

		glGenBuffers(1, &element_buffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_buffer);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<int64_t>((indices.size() - 1) * sizeof(uint32_t)), indices.data(), GL_STATIC_DRAW);

		glGenBuffers(1, &vertex_buffer);
		glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
		glBufferData(GL_ARRAY_BUFFER, static_cast<int64_t>(vertices.size() * sizeof(map_vertex_data_3d_b)), vertices.data(), GL_STATIC_DRAW);

		return std::make_tuple(vertex_buffer, element_buffer, static_cast<uint32_t>(indices.size() - 1));
	}
	//.....................


	map_data_textures create_data_textures(uint16_t const* map_data, int32_t width, int32_t height) {
		map_data_textures result;
		result.width = width;
		result.height = height;
		result.primary_data = map_data;


		glGenTextures(1, &result.handle);
		glBindTexture(GL_TEXTURE_2D, result.handle);

		glTexStorage2D(GL_TEXTURE_2D, 1, GL_R16UI, width, height);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,  GL_RED_INTEGER, GL_UNSIGNED_SHORT, result.primary_data);

		int8_t* corners = new int8_t[static_cast<size_t>(width * height * 4)];
		memset(corners, 0, static_cast<size_t>(width * height * 4));

		for (int32_t j = 0; j < height - 1; ++j) {
			for (int32_t i = 0; i < width - 1; ++i) {
				const auto ul = result.primary_data[i + j * width];
				const auto ur = result.primary_data[i+1 + j * width];
				const auto bl = result.primary_data[i + (j+1) * width];
				const auto br = result.primary_data[i+1 + (j+1) * width];

				if (ul == ur && bl == ul)
					corners[(i + 1) * 2 + (j + 1) * 2 * width * 2] = -1i8;
				if (ul == ur && br == ur)
					corners[(i) * 2 + 1 + (j + 1) * 2 * width * 2] = 1i8;
				if (br == bl && bl == ul)
					corners[(i + 1) * 2 + ((j) * 2 + 1) * width * 2] = -1i8;
				if (br == bl && br == ur)
					corners[(i) * 2 + 1 + ((j) * 2 + 1) * width * 2] = 1i8;
			}
		}

		glGenTextures(1, &result.corner_handle);
		glBindTexture(GL_TEXTURE_2D, result.corner_handle);

		glTexStorage2D(GL_TEXTURE_2D, 1, GL_R8I, width * 2, height * 2);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width * 2, height * 2, GL_RED_INTEGER, GL_BYTE, corners);


		delete[] corners;

		return result;
	}

	void color_maps::bind_colors() {
		bool expected = true;
		if (update_pending.compare_exchange_strong(expected, false, std::memory_order_release, std::memory_order_acquire)) {
			glBindTexture(GL_TEXTURE_1D, primary_handle);
			glTexSubImage1D(GL_TEXTURE_1D, 0, 0, size, GL_RGB, GL_UNSIGNED_BYTE, primary_colors);
			glBindTexture(GL_TEXTURE_1D, secondary_handle);
			glTexSubImage1D(GL_TEXTURE_1D, 0, 0, size, GL_RGB, GL_UNSIGNED_BYTE, secondary_colors);
		}

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_1D, primary_handle);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_1D, secondary_handle);
	}

	void color_maps::create_color_textures() {
		glGenTextures(1, &primary_handle);
		glBindTexture(GL_TEXTURE_1D, primary_handle);

		glTexStorage1D(GL_TEXTURE_1D, 1, GL_RGB8, size);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

		glGenTextures(1, &secondary_handle);
		glBindTexture(GL_TEXTURE_1D, secondary_handle);

		glTexStorage1D(GL_TEXTURE_1D, 1, GL_RGB8, size);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	}

	uint32_t compile_program_b() {
		GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);

#ifdef _DEBUG
		if(vertex_shader == 0) {
			MessageBox(nullptr, L"vertex shader creation failed", L"OpenGL error", MB_OK);
			//std::abort();
		}
#endif

		GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

#ifdef _DEBUG
		if (fragment_shader == 0) {
			MessageBox(nullptr, L"fragment_shader shader creation failed", L"OpenGL error", MB_OK);
			//std::abort();
		}
#endif
		const GLchar* array_a[] = { map_vertex_shader_b };

		glShaderSource(vertex_shader, 1, array_a, nullptr);
		glCompileShader(vertex_shader);

#ifdef _DEBUG
		GLint result;
		glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &result);
		if (result == GL_FALSE) {
			MessageBox(nullptr, L"vertex shader compilation failed", L"OpenGL error", MB_OK);

			GLint logLen;
			glGetShaderiv(vertex_shader, GL_INFO_LOG_LENGTH, &logLen);

			char * log = new char[static_cast<size_t>(logLen)];
			GLsizei written;
			glGetShaderInfoLog(vertex_shader, logLen, &written, log);

			MessageBoxA(nullptr, log, "OpenGL error", MB_OK);
			delete[] log;

			std::abort();
		}
#endif


		const GLchar* array_b[] = { map_fragment_shader_b };

		glShaderSource(fragment_shader, 1, array_b, nullptr);
		glCompileShader(fragment_shader);

#ifdef _DEBUG
		glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &result);
		if (result == GL_FALSE) {
			MessageBox(nullptr, L"fragment shader compilation failed", L"OpenGL error", MB_OK);

			GLint logLen;
			glGetShaderiv(fragment_shader, GL_INFO_LOG_LENGTH, &logLen);

			char * log = new char[static_cast<size_t>(logLen)];
			GLsizei written;
			glGetShaderInfoLog(fragment_shader, logLen, &written, log);

			MessageBoxA(nullptr, log, "OpenGL error", MB_OK);
			delete[] log;

			std::abort();
		}
#endif

		uint32_t shader_handle = glCreateProgram();

#ifdef _DEBUG
		if (shader_handle == 0) {
			MessageBox(nullptr, L"shader program creation failed", L"OpenGL error", MB_OK);
			//std::abort();
		}
#endif

		glAttachShader(shader_handle, vertex_shader);
		glAttachShader(shader_handle, fragment_shader);
		glLinkProgram(shader_handle);

#ifdef _DEBUG
		glGetProgramiv(shader_handle, GL_LINK_STATUS, &result);
		if (result == GL_FALSE) {
			MessageBox(nullptr, L"shader program linking failed", L"OpenGL error", MB_OK);

			GLint logLen;
			glGetProgramiv(shader_handle, GL_INFO_LOG_LENGTH, &logLen);

			char * log = new char[static_cast<size_t>(logLen)];
			GLsizei written;
			glGetProgramInfoLog(shader_handle, logLen, &written, log);
			MessageBoxA(nullptr, log, "OpenGL error", MB_OK);

			delete[] log;
			std::abort();
		}
#endif
		glDeleteShader(vertex_shader);
		glDeleteShader(fragment_shader);

		return shader_handle;
	}

	uint32_t setup_vao_b(uint32_t vertex_buffer) {
		uint32_t vao;

		glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		glEnableVertexAttribArray(0); //position
		glEnableVertexAttribArray(1); //layer_value

		glBindVertexBuffer(0, vertex_buffer, 0, sizeof(map_vertex_data_3d_b));

		glVertexAttribFormat(0, 3, GL_FLOAT, GL_FALSE, 0); //position
		glVertexAttribFormat(1, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 3); //layer_value
		glVertexAttribBinding(0, 0); //position -> to array zero
		glVertexAttribBinding(1, 0); //layer_value -> to array zero 

		return vao;
	}

	void map_display::initialize(open_gl_wrapper& ogl, std::string shadows_file, uint16_t const* map_data, int32_t width, int32_t height, float left_longitude, float top_latitude, float bottom_latitude) {

		glProvokingVertex(GL_FIRST_VERTEX_CONVENTION);
		glEnable(GL_DEPTH_TEST);

		map_height = height;
		map_width = width;

		shader_handle = compile_program_b();

		lat_step = (bottom_latitude - top_latitude) / static_cast<float>(height);
		top_lat = top_latitude;
		long_step = 6.28318530718f / static_cast<float>(width);
		left_long = left_longitude;

		data_textures = create_data_textures(map_data, width, height);
		colors.create_color_textures();

		const auto [v, e, c] = create_patches_buffers_b(width, height, top_lat, lat_step, long_step);
		vertex_buffer = v;
		element_buffer = e;
		triangle_vertex_count = static_cast<int32_t>(c);

		vao = setup_vao_b(vertex_buffer);

		unsigned char pixel[3] = { 255, 255, 255 };

		glGenTextures(1, &data_textures.shadows_handle);
		glBindTexture(GL_TEXTURE_RECTANGLE, data_textures.shadows_handle);
		glTexStorage2D(GL_TEXTURE_RECTANGLE, 1, GL_RGB8, 1, 1);
		glTexSubImage2D(GL_TEXTURE_RECTANGLE, 0, 0, 0, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, pixel);
		glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

		if(shadows_file.length() != 0) {
			std::thread st([&ogl, shadows_file, ptr_to_sh = &data_textures.shadows_handle]() {
				ogl.bind_to_shadows_thread();

				int32_t swidth = 0;
				int32_t sheight = 0;
				int32_t shchannel = 3;
				unsigned char* shadows_data = SOIL_load_image(shadows_file.c_str(), &swidth, &sheight, &shchannel, 3);

				uint32_t temp_handle;
				glGenTextures(1, &temp_handle);
				glBindTexture(GL_TEXTURE_RECTANGLE, temp_handle);

				glTexStorage2D(GL_TEXTURE_RECTANGLE, 1, GL_RGB8, swidth, sheight);
				glTexSubImage2D(GL_TEXTURE_RECTANGLE, 0, 0, 0, swidth, sheight, GL_RGB, GL_UNSIGNED_BYTE, shadows_data);

				glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

				*ptr_to_sh = temp_handle;

				SOIL_free_image_data(shadows_data);
			});
			st.detach();
		}
		

		glDisable(GL_DEPTH_TEST);

		ready = true;
	}

	void inner_render_b(float scale, const Eigen::Matrix3f& rotation, float aspect, int32_t vertex_count) {
		glUniform1f(map_parameters_b::aspect, aspect);
		glUniform1f(map_parameters_b::scale, scale);


		glUniformMatrix3fv(map_parameters_b::rotation_matrix, 1, GL_FALSE, rotation.data());

		glProvokingVertex(GL_FIRST_VERTEX_CONVENTION);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_PRIMITIVE_RESTART);
		glPrimitiveRestartIndex(uint32_t(-1));
		glDrawElements(GL_TRIANGLE_STRIP, vertex_count, GL_UNSIGNED_INT, nullptr);
		glDisable(GL_PRIMITIVE_RESTART);
		glDisable(GL_DEPTH_TEST);
	}

	void map_display::render(open_gl_wrapper &) {
		if (ready) {
			glUseProgram(shader_handle);


			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, data_textures.handle);
			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_2D, data_textures.corner_handle);
			glActiveTexture(GL_TEXTURE4);
			glBindTexture(GL_TEXTURE_RECTANGLE, data_textures.shadows_handle);

			colors.bind_colors();

			glBindVertexArray(vao);
			glBindVertexBuffer(0, vertex_buffer, 0, sizeof(map_vertex_data_3d_b));
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_buffer);


			inner_render_b(state.scale, state.rotation(), state.aspect(), triangle_vertex_count);
		}
	}

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

	void update_map_colors(graphics::map_display& map, world_state& ws) {
		bool map_change_expected = true;
		if(ws.w.map_view.changed.compare_exchange_strong(map_change_expected, false)) {
			const auto pcolors = map.colors.primary_color_data();
			const auto scolors = map.colors.secondary_color_data();

			if(pcolors == nullptr || scolors == nullptr) {
				ws.w.map_view.changed.store(false, std::memory_order_release);
				return;
			}

			auto g = ws.w.map_view.selected_good;
			if(is_valid_index(g) && ws.w.map_view.mode == current_state::map_mode::prices) {
				auto price_range = economy::global_price_range(ws, g);

				for(int32_t i = 0; i < ws.w.province_s.province_state_container.size(); ++i) {
					if(auto sid = provinces::province_state(ws, provinces::province_tag(provinces::province_tag::value_base_t(i)));
						bool(sid) && bool(ws.w.nation_s.states.get<state::owner>(sid))) {

						
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

			} else if(is_valid_index(g) && g != economy::gold_good && ws.w.map_view.mode == current_state::map_mode::purchasing) {
				if(auto selected_id = ws.w.map_view.selected_state; ws.w.nation_s.states.is_valid_index(selected_id)) {
					auto purchasing_handle = ws.w.nation_s.state_purchases.get(selected_id, g);
					auto purchases_data_range = get_range(ws.w.economy_s.purchasing_arrays, purchasing_handle);

					if(std::begin(purchases_data_range) != std::end(purchases_data_range)) {
						auto max_purchases = *std::max_element(std::begin(purchases_data_range) + 1, std::end(purchases_data_range));

						
						for(int32_t i = 0; i < ws.w.province_s.province_state_container.size(); ++i) {
							if(auto sid = provinces::province_state(ws, provinces::province_tag(provinces::province_tag::value_base_t(i)));
								bool(sid) && bool(ws.w.nation_s.states.get<state::owner>(sid)) && purchases_data_range.first + to_index(sid) + 1 < purchases_data_range.second) {

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
			} else if(is_valid_index(g) && g != economy::gold_good && ws.w.map_view.mode == current_state::map_mode::production) {
				economy::goods_qnty_type max_production = 0.0001;
				ws.w.nation_s.states.for_each([&](nations::state_tag id) {
					max_production = std::max(max_production, economy::state_current_production(ws, id, g));
				});

				

				for(int32_t i = 0; i < ws.w.province_s.province_state_container.size(); ++i) {
					if(auto sid = provinces::province_state(ws, provinces::province_tag(provinces::province_tag::value_base_t(i)));
						bool(sid) && bool(ws.w.nation_s.states.get<state::owner>(sid))) {

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


			} else if(auto p = ws.w.map_view.selected_province; is_valid_index(p) && ws.w.map_view.mode == current_state::map_mode::distance) {
				
				auto pcount = ws.s.province_m.province_container.size();
				for(int32_t i = 0; i < pcount; ++i) {
					auto distance = ws.w.province_s.province_distance_to[to_index(p) * pcount + i];
					pcolors[i * 3 + 0] = uint8_t(std::clamp((distance / 8'000.0f - 1.0f), 0.0f, 1.0f) * 255.0f);
					pcolors[i * 3 + 1] = uint8_t(std::clamp((1.0f - distance / 8'000.0f), 0.0f, 1.0f) * 255.0f);
					pcolors[i * 3 + 2] = uint8_t(100);
					scolors[i * 3 + 0] = uint8_t(std::clamp((distance / 8'000.0f - 1.0f), 0.0f, 1.0f) * 255.0f);
					scolors[i * 3 + 1] = uint8_t(std::clamp((1.0f - distance / 8'000.0f), 0.0f, 1.0f) * 255.0f);
					scolors[i * 3 + 2] = uint8_t(100);
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

			map.colors.update_ready();
		}
	}
}
