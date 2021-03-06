#pragma once
#include <memory>
#include <functional>
#include <mutex>
#include "texture.h"
#include "text.h"
#include "lines.h"

namespace graphics {
	class _open_gl_wrapper;

	class window_base;

	struct color {
		float r = 0.0f;
		float g = 0.0f;
		float b = 0.0f;
	};

	enum class rotation {
		upright,
		left,
		right,
		upright_vertical_flipped,
		left_vertical_flipped,
		right_vertical_flipped
	};

	class scissor_rect {
	private:
		int32_t oldrect[4];
		int32_t* current_rect = nullptr;
	public:
		scissor_rect(open_gl_wrapper& ogl, int32_t x, int32_t y, int32_t width, int32_t height);
		~scissor_rect();
	};

	class open_gl_wrapper {
	private:
		int32_t current_scissor_rect[4] = { 0,0,0,0 };

		void set_render_thread(const std::function<void()>&);
		bool is_running();
		void setup_context(void* hwnd);
	public:
		std::unique_ptr<_open_gl_wrapper> impl;

		open_gl_wrapper();
		~open_gl_wrapper();

		template<typename T>
		void setup(void* hwnd, T& base);
		void destory(void* hwnd);
		void bind_to_thread();
		void bind_to_ui_thread();
		void bind_to_shadows_thread();
		void clear();
		void display();
		void use_default_program();
		void set_viewport(uint32_t width, uint32_t height);
		void render_piechart(color_modification enabled, float x, float y, float size, data_texture& t);
		void render_textured_rect(color_modification enabled, float x, float y, float width, float height, texture& t, rotation r = rotation::upright);
		void render_barchart(color_modification enabled, float x, float y, float width, float height, data_texture& t, rotation r = rotation::upright);
		void render_linegraph(color_modification enabled, float x, float y, float width, float height, lines& l);
		void render_subsprite(color_modification enabled, int frame, int total_frames, float x, float y, float width, float height, texture& t, rotation r = rotation::upright);
		void render_masked_rect(color_modification enabled, float x, float y, float width, float height, texture& t, texture& mask, rotation r = rotation::upright);
		void render_progress_bar(color_modification enabled, float progress, float x, float y, float width, float height, texture& left, texture& right, rotation r = rotation::upright);
		void render_bordered_rect(color_modification enabled, float border_size, float x, float y, float width, float height, texture& t, rotation r = rotation::upright);
		void render_character(char16_t codepoint, color_modification enabled, float x, float y, float size, font& f);
		void render_text(const char16_t* codepoints, uint32_t count, color_modification enabled, float x, float baseline_y, float size, const color& c, font& f);
		void render_outlined_text(const char16_t* codepoints, uint32_t count, color_modification enabled, float x, float baseline_y, float size, const color& c, font& f);
		void render_tinted_textured_rect(float x, float y, float width, float height, float r, float g, float b, texture& t, rotation rot = rotation::upright);
		void render_textured_rect_direct(float x, float y, float width, float height, uint32_t handle);

		friend scissor_rect;
	};
}
