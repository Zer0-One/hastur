// SPDX-FileCopyrightText: 2021-2025 Robin Lindén <dev@robinlinden.eu>
//
// SPDX-License-Identifier: BSD-2-Clause

#ifndef GFX_OPENGL_CANVAS_H_
#define GFX_OPENGL_CANVAS_H_

#include "gfx/opengl_shader.h"

#include "gfx/color.h"
#include "gfx/font.h"
#include "gfx/icanvas.h"

#include "geom/geom.h"

#include <cstdint>
#include <span>
#include <string_view>

namespace gfx {

class OpenGLCanvas final : public ICanvas {
public:
    OpenGLCanvas();

    void set_viewport_size(int width, int height) override;
    constexpr void set_scale(int scale) override { scale_ = scale; }

    constexpr void add_translation(int dx, int dy) override {
        translation_x_ += dx;
        translation_y_ += dy;
    }

    void clear(Color) override;
    void draw_rect(geom::Rect const &, Color const &, Borders const &, Corners const &) override;
    void draw_text(geom::Position, std::string_view, std::span<Font const>, FontSize, FontStyle, Color) override {}
    void draw_text(geom::Position, std::string_view, Font, FontSize, FontStyle, Color) override {}
    void draw_pixels(geom::Rect const &, std::span<std::uint8_t const>) override {}

private:
    OpenGLShader border_shader_;

    int size_x_{};
    int size_y_{};
    int translation_x_{};
    int translation_y_{};
    int scale_{1};
};

} // namespace gfx

#endif
