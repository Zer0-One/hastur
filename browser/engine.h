// SPDX-FileCopyrightText: 2021 Robin Lindén <dev@robinlinden.eu>
//
// SPDX-License-Identifier: BSD-2-Clause

#ifndef BROWSER_ENGINE_H_
#define BROWSER_ENGINE_H_

#include "dom/dom.h"
#include "layout/layout.h"
#include "protocol/get.h"
#include "style/styled_node.h"
#include "uri/uri.h"

#include <functional>
#include <optional>
#include <utility>

namespace browser {

class Engine {
public:
    protocol::Error navigate(uri::URL url);

    void set_layout_width(int width);

    void set_on_navigation_failure(auto cb) { on_navigation_failure_ = std::move(cb); }
    void set_on_page_loaded(auto cb) { on_page_loaded_ = std::move(cb); }
    void set_on_layout_updated(auto cb) { on_layout_update_ = std::move(cb); }

    uri::URL const &url() const { return url_; }
    protocol::Response const &response() const { return response_; }
    dom::Document const &dom() const { return dom_; }
    layout::LayoutBox const &layout() const { return *layout_; }

private:
    std::function<void(protocol::Error)> on_navigation_failure_{[](protocol::Error) {
    }};
    std::function<void()> on_page_loaded_{[] {
    }};
    std::function<void()> on_layout_update_{[] {
    }};

    int layout_width_{};

    uri::URL url_{};
    protocol::Response response_{};
    dom::Document dom_{};
    std::optional<style::StyledNode> styled_{};
    std::optional<layout::LayoutBox> layout_{};

    void on_navigation_success();
};

} // namespace browser

#endif
