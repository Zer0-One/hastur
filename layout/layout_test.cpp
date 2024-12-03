// SPDX-FileCopyrightText: 2021-2024 Robin Lindén <dev@robinlinden.eu>
// SPDX-FileCopyrightText: 2022 Mikael Larsson <c.mikael.larsson@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause

#include "layout/layout.h"

#include "layout/layout_box.h"

#include "css/property_id.h"
#include "dom/dom.h"
#include "etest/etest.h"
#include "geom/geom.h"
#include "style/styled_node.h"
#include "type/naive.h"
#include "type/type.h"
#include "util/string.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace std::literals;
using etest::expect;
using etest::expect_eq;
using etest::require;
using etest::require_eq;

namespace {

class NoType : public type::IType {
public:
    std::optional<std::shared_ptr<type::IFont const>> font(std::string_view) const override { return std::nullopt; }
};

// Until we have a nicer tree-creation abstraction for the tests, this needs to
// be called if a test relies on property inheritance.
// NOLINTNEXTLINE(misc-no-recursion)
void set_up_parent_ptrs(style::StyledNode &parent) {
    for (auto &child : parent.children) {
        child.parent = &parent;
        set_up_parent_ptrs(child);
    }
}

// TODO(robinlinden): Remove.
dom::Node create_element_node(std::string_view name, dom::AttrMap attrs, std::vector<dom::Node> children) {
    return dom::Element{std::string{name}, std::move(attrs), std::move(children)};
}

void whitespace_collapsing_tests() {
    etest::test("whitespace collapsing: simple", [] {
        constexpr auto kText = "   hello   "sv;
        constexpr auto kCollapsedText = util::trim(kText);
        constexpr auto kTextWidth = kCollapsedText.length() * 5;

        dom::Element p{.name{"p"}, .children{dom::Text{std::string{kText}}}};
        dom::Node html = dom::Element{.name{"html"}, .children{std::move(p)}};
        dom::Element const &html_element = std::get<dom::Element>(html);

        style::StyledNode p_style{
                .node{html_element.children[0]},
                .properties{{css::PropertyId::Display, "inline"}},
                .children{style::StyledNode{std::get<dom::Element>(html_element.children.at(0)).children.at(0)}},
        };
        style::StyledNode style{
                .node{html},
                .properties{{css::PropertyId::Display, "block"}, {css::PropertyId::FontSize, "10px"}},
                .children{std::move(p_style)},
        };
        set_up_parent_ptrs(style);

        layout::LayoutBox p_layout{
                .node = &style.children.at(0),
                .dimensions{{0, 0, kTextWidth, 10}},
                .children{layout::LayoutBox{
                        .node = &style.children.at(0).children.at(0),
                        .dimensions{{0, 0, kTextWidth, 10}},
                        .layout_text{kCollapsedText},
                }},
        };
        layout::LayoutBox expected_layout{
                .node = &style,
                .dimensions{{0, 0, 1234, 10}},
                .children{layout::LayoutBox{
                        .node = nullptr,
                        .dimensions{{0, 0, 1234, 10}},
                        .children{std::move(p_layout)},
                }},
        };

        auto actual = layout::create_layout(style, 1234);
        expect_eq(actual, expected_layout);
    });

    etest::test("whitespace collapsing: text split across multiple inline elements", [] {
        constexpr auto kFirstText = "   cr     "sv;
        constexpr auto kSecondText = " lf   "sv;
        constexpr auto kCollapsedFirst = "cr "sv;
        constexpr auto kFirstWidth = kCollapsedFirst.length() * 5;
        constexpr auto kCollapsedSecond = "lf"sv;
        constexpr auto kSecondWidth = kCollapsedSecond.length() * 5;

        dom::Element a{.name{"a"}, .children{dom::Text{std::string{kSecondText}}}};
        dom::Element p{.name{"p"}, .children{dom::Text{std::string{kFirstText}}, std::move(a)}};
        dom::Node html = dom::Element{.name{"html"}, .children{std::move(p)}};
        auto const &html_element = std::get<dom::Element>(html);
        auto const &p_element = std::get<dom::Element>(html_element.children.at(0));
        auto const &a_element = std::get<dom::Element>(p_element.children.at(1));

        style::StyledNode a_style{
                .node{a_element},
                .properties{{css::PropertyId::Display, "inline"}},
                .children{style::StyledNode{a_element.children.at(0)}},
        };
        style::StyledNode p_style{
                .node{p_element},
                .properties{{css::PropertyId::Display, "inline"}},
                .children{style::StyledNode{p_element.children.at(0)}, std::move(a_style)},
        };
        style::StyledNode style{
                .node{html},
                .properties{{css::PropertyId::Display, "block"}, {css::PropertyId::FontSize, "10px"}},
                .children{std::move(p_style)},
        };
        set_up_parent_ptrs(style);

        layout::LayoutBox a_layout{
                .node = &style.children.at(0).children.at(1),
                .dimensions{{kFirstWidth, 0, kSecondWidth, 10}},
                .children{layout::LayoutBox{
                        .node = &style.children.at(0).children.at(1).children.at(0),
                        .dimensions{{kFirstWidth, 0, kSecondWidth, 10}},
                        .layout_text{kCollapsedSecond},
                }},
        };
        layout::LayoutBox p_layout{
                .node = &style.children.at(0),
                .dimensions{{0, 0, kFirstWidth + kSecondWidth, 10}},
                .children{
                        layout::LayoutBox{
                                .node = &style.children.at(0).children.at(0),
                                .dimensions{{0, 0, kFirstWidth, 10}},
                                .layout_text{kCollapsedFirst},
                        },
                        std::move(a_layout),
                },
        };
        layout::LayoutBox expected_layout{
                .node = &style,
                .dimensions{{0, 0, 1234, 10}},
                .children{layout::LayoutBox{
                        .node = nullptr,
                        .dimensions{{0, 0, 1234, 10}},
                        .children{std::move(p_layout)},
                }},
        };

        auto actual = layout::create_layout(style, 1234);
        expect_eq(actual, expected_layout);
    });

    etest::test("whitespace collapsing: allocating collapsing", [] {
        constexpr auto kFirstText = "c  r"sv;
        constexpr auto kSecondText = "l\nf"sv;
        auto const collapsed_first = "c r"s;
        auto const first_width = static_cast<int>(collapsed_first.length() * 5);
        auto const collapsed_second = "l f"s;
        auto const second_width = static_cast<int>(collapsed_second.length() * 5);

        dom::Element a{.name{"a"}, .children{dom::Text{std::string{kSecondText}}}};
        dom::Element p{.name{"p"}, .children{dom::Text{std::string{kFirstText}}, std::move(a)}};
        dom::Node html = dom::Element{.name{"html"}, .children{std::move(p)}};
        auto const &html_element = std::get<dom::Element>(html);
        auto const &p_element = std::get<dom::Element>(html_element.children.at(0));
        auto const &a_element = std::get<dom::Element>(p_element.children.at(1));

        style::StyledNode a_style{
                .node{a_element},
                .properties{{css::PropertyId::Display, "inline"}},
                .children{style::StyledNode{a_element.children.at(0)}},
        };
        style::StyledNode p_style{
                .node{p_element},
                .properties{{css::PropertyId::Display, "inline"}},
                .children{style::StyledNode{p_element.children.at(0)}, std::move(a_style)},
        };
        style::StyledNode style{
                .node{html},
                .properties{{css::PropertyId::Display, "block"}, {css::PropertyId::FontSize, "10px"}},
                .children{std::move(p_style)},
        };
        set_up_parent_ptrs(style);

        layout::LayoutBox a_layout{
                .node = &style.children.at(0).children.at(1),
                .dimensions{{first_width, 0, second_width, 10}},
                .children{layout::LayoutBox{
                        .node = &style.children.at(0).children.at(1).children.at(0),
                        .dimensions{{first_width, 0, second_width, 10}},
                        .layout_text{collapsed_second},
                }},
        };
        layout::LayoutBox p_layout{
                .node = &style.children.at(0),
                .dimensions{{0, 0, first_width + second_width, 10}},
                .children{
                        layout::LayoutBox{
                                .node = &style.children.at(0).children.at(0),
                                .dimensions{{0, 0, first_width, 10}},
                                .layout_text{collapsed_first},
                        },
                        std::move(a_layout),
                },
        };
        layout::LayoutBox expected_layout{
                .node = &style,
                .dimensions{{0, 0, 1234, 10}},
                .children{layout::LayoutBox{
                        .node = nullptr,
                        .dimensions{{0, 0, 1234, 10}},
                        .children{std::move(p_layout)},
                }},
        };

        auto actual = layout::create_layout(style, 1234);
        expect_eq(actual, expected_layout);
    });

    etest::test("whitespace collapsing: text separated by a block element", [] {
        constexpr auto kFirstText = "  a  "sv;
        constexpr auto kSecondText = "  b  "sv;
        constexpr auto kCollapsedFirst = util::trim(kFirstText);
        constexpr auto kFirstWidth = kCollapsedFirst.length() * 5;
        constexpr auto kCollapsedSecond = util::trim(kSecondText);
        constexpr auto kSecondWidth = kCollapsedSecond.length() * 5;

        dom::Element first{.name{"p"}, .children{dom::Text{std::string{kFirstText}}}};
        dom::Element block{.name{"div"}};
        dom::Element second{.name{"p"}, .children{dom::Text{std::string{kSecondText}}}};
        dom::Node html = dom::Element{.name{"html"}, .children{std::move(first), std::move(block), std::move(second)}};
        auto const &html_element = std::get<dom::Element>(html);
        auto const &first_text_element = std::get<dom::Element>(html_element.children.at(0));
        auto const &block_element = std::get<dom::Element>(html_element.children.at(1));
        auto const &second_text_element = std::get<dom::Element>(html_element.children.at(2));

        style::StyledNode first_style{
                .node{first_text_element},
                .properties{{css::PropertyId::Display, "inline"}},
                .children{style::StyledNode{first_text_element.children.at(0)}},
        };
        style::StyledNode block_style{.node{block_element}, .properties{{css::PropertyId::Display, "block"}}};
        style::StyledNode second_style{
                .node{second_text_element},
                .properties{{css::PropertyId::Display, "inline"}},
                .children{style::StyledNode{second_text_element.children.at(0)}},
        };
        style::StyledNode style{
                .node{html},
                .properties{{css::PropertyId::Display, "block"}, {css::PropertyId::FontSize, "10px"}},
                .children{std::move(first_style), std::move(block_style), std::move(second_style)},
        };
        set_up_parent_ptrs(style);

        layout::LayoutBox first_layout{
                .node = &style.children.at(0),
                .dimensions{{0, 0, kFirstWidth, 10}},
                .children{layout::LayoutBox{
                        .node = &style.children.at(0).children.at(0),
                        .dimensions{{0, 0, kFirstWidth, 10}},
                        .layout_text{kCollapsedFirst},
                }},
        };
        layout::LayoutBox second_layout{
                .node = &style.children.at(2),
                .dimensions{{0, 10, kSecondWidth, 10}},
                .children{layout::LayoutBox{
                        .node = &style.children.at(2).children.at(0),
                        .dimensions{{0, 10, kSecondWidth, 10}},
                        .layout_text{kCollapsedSecond},
                }},
        };
        layout::LayoutBox expected_layout{
                .node = &style,
                .dimensions{{0, 0, 1234, 20}},
                .children{
                        layout::LayoutBox{
                                .node = nullptr,
                                .dimensions{{0, 0, 1234, 10}},
                                .children{std::move(first_layout)},
                        },
                        layout::LayoutBox{
                                .node = &style.children.at(1),
                                .dimensions{{0, 10, 1234, 0}},
                        },
                        layout::LayoutBox{
                                .node = nullptr,
                                .dimensions{{0, 10, 1234, 10}},
                                .children{std::move(second_layout)},
                        },
                },
        };

        auto actual = layout::create_layout(style, 1234);
        expect_eq(actual, expected_layout);
    });

    etest::test("whitespace collapsing: <span>hello</span>   <span>world</span>", [] {
        constexpr auto kFirstText = "hello"sv;
        constexpr auto kSecondText = "world"sv;
        constexpr auto kFirstWidth = kFirstText.length() * 5;
        constexpr auto kSecondWidth = kSecondText.length() * 5;
        constexpr auto kSpaceWidth = 5; // 1 space after collapsing, 10px font size.

        dom::Element first{.name{"span"}, .children{dom::Text{std::string{kFirstText}}}};
        dom::Text space{"   "};
        dom::Element second{.name{"span"}, .children{dom::Text{std::string{kSecondText}}}};
        dom::Node html = dom::Element{.name{"html"}, .children{std::move(first), std::move(space), std::move(second)}};
        auto const &html_element = std::get<dom::Element>(html);
        auto const &first_text_element = std::get<dom::Element>(html_element.children.at(0));
        auto const &second_text_element = std::get<dom::Element>(html_element.children.at(2));

        style::StyledNode first_style{
                .node{first_text_element},
                .properties{{css::PropertyId::Display, "inline"}},
                .children{style::StyledNode{first_text_element.children.at(0)}},
        };
        style::StyledNode space_style{.node{html_element.children.at(1)}};
        style::StyledNode second_style{
                .node{second_text_element},
                .properties{{css::PropertyId::Display, "inline"}},
                .children{style::StyledNode{second_text_element.children.at(0)}},
        };
        style::StyledNode style{
                .node{html},
                .properties{{css::PropertyId::Display, "block"}, {css::PropertyId::FontSize, "10px"}},
                .children{std::move(first_style), std::move(space_style), std::move(second_style)},
        };
        set_up_parent_ptrs(style);

        layout::LayoutBox first_layout{
                .node = &style.children.at(0),
                .dimensions{{0, 0, kFirstWidth, 10}},
                .children{layout::LayoutBox{
                        .node = &style.children.at(0).children.at(0),
                        .dimensions{{0, 0, kFirstWidth, 10}},
                        .layout_text{kFirstText},
                }},
        };
        layout::LayoutBox space_layout{
                .node = &style.children.at(1),
                .dimensions{{kFirstWidth, 0, kSpaceWidth, 10}},
                .layout_text{std::string{" "}},
        };
        layout::LayoutBox second_layout{
                .node = &style.children.at(2),
                .dimensions{{kFirstWidth + kSpaceWidth, 0, kSecondWidth, 10}},
                .children{layout::LayoutBox{
                        .node = &style.children.at(2).children.at(0),
                        .dimensions{{kFirstWidth + kSpaceWidth, 0, kSecondWidth, 10}},
                        .layout_text{kSecondText},
                }},
        };
        layout::LayoutBox expected_layout{
                .node = &style,
                .dimensions{{0, 0, 1234, 10}},
                .children{
                        layout::LayoutBox{
                                .node = nullptr,
                                .dimensions{{0, 0, 1234, 10}},
                                .children{
                                        std::move(first_layout),
                                        std::move(space_layout),
                                        std::move(second_layout),
                                },
                        },
                },
        };

        auto actual = layout::create_layout(style, 1234);
        expect_eq(actual, expected_layout);
    });

    etest::test("whitespace collapsing: <p>hello</p>   <p>world</p>", [] {
        constexpr auto kFirstText = "hello"sv;
        constexpr auto kSecondText = "world"sv;
        constexpr auto kFirstWidth = kFirstText.length() * 5;
        constexpr auto kSecondWidth = kSecondText.length() * 5;

        dom::Element first{.name{"p"}, .children{dom::Text{std::string{kFirstText}}}};
        dom::Text space{"   "};
        dom::Element second{.name{"p"}, .children{dom::Text{std::string{kSecondText}}}};
        dom::Node html = dom::Element{.name{"html"}, .children{std::move(first), std::move(space), std::move(second)}};
        auto const &html_element = std::get<dom::Element>(html);
        auto const &first_text_element = std::get<dom::Element>(html_element.children.at(0));
        auto const &second_text_element = std::get<dom::Element>(html_element.children.at(2));

        style::StyledNode first_style{
                .node{first_text_element},
                .properties{{css::PropertyId::Display, "block"}},
                .children{style::StyledNode{first_text_element.children.at(0)}},
        };
        style::StyledNode space_style{.node{html_element.children.at(1)}};
        style::StyledNode second_style{
                .node{second_text_element},
                .properties{{css::PropertyId::Display, "block"}},
                .children{style::StyledNode{second_text_element.children.at(0)}},
        };
        style::StyledNode style{
                .node{html},
                .properties{{css::PropertyId::Display, "block"}, {css::PropertyId::FontSize, "10px"}},
                .children{std::move(first_style), std::move(space_style), std::move(second_style)},
        };
        set_up_parent_ptrs(style);

        layout::LayoutBox first_layout{
                .node = &style.children.at(0),
                .dimensions{{0, 0, 1234, 10}},
                .children{
                        layout::LayoutBox{
                                .dimensions{{0, 0, 1234, 10}},
                                .children{
                                        layout::LayoutBox{
                                                .node = &style.children.at(0).children.at(0),
                                                .dimensions{{0, 0, kFirstWidth, 10}},
                                                .layout_text{kFirstText},
                                        },
                                },
                        },
                },
        };
        layout::LayoutBox second_layout{
                .node = &style.children.at(2),
                .dimensions{{0, 10, 1234, 10}},
                .children{
                        layout::LayoutBox{
                                .dimensions{{0, 10, 1234, 10}},
                                .children{
                                        layout::LayoutBox{
                                                .node = &style.children.at(2).children.at(0),
                                                .dimensions{{0, 10, kSecondWidth, 10}},
                                                .layout_text{kSecondText},
                                        },
                                },
                        },
                },
        };
        layout::LayoutBox expected_layout{
                .node = &style,
                .dimensions{{0, 0, 1234, 20}},
                .children{
                        std::move(first_layout),
                        std::move(second_layout),
                },
        };

        auto actual = layout::create_layout(style, 1234);
        expect_eq(actual, expected_layout);
    });
}

void text_transform_tests() {
    etest::test("text-transform: uppercase", [] {
        constexpr auto kText = "hello   goodbye"sv;
        constexpr auto kExpectedText = "HELLO GOODBYE"sv;
        constexpr auto kTextWidth = kExpectedText.length() * 5;

        dom::Element p{.name{"p"}, .children{dom::Text{std::string{kText}}}};
        dom::Node html = dom::Element{.name{"html"}, .children{std::move(p)}};
        dom::Element const &html_element = std::get<dom::Element>(html);

        style::StyledNode p_style{
                .node{html_element.children[0]},
                .properties{{css::PropertyId::Display, "inline"}, {css::PropertyId::TextTransform, "uppercase"}},
                .children{style::StyledNode{std::get<dom::Element>(html_element.children.at(0)).children.at(0)}},
        };
        style::StyledNode style{
                .node{html},
                .properties{{css::PropertyId::Display, "block"}, {css::PropertyId::FontSize, "10px"}},
                .children{std::move(p_style)},
        };
        set_up_parent_ptrs(style);

        layout::LayoutBox p_layout{
                .node = &style.children.at(0),
                .dimensions{{0, 0, kTextWidth, 10}},
                .children{layout::LayoutBox{
                        .node = &style.children.at(0).children.at(0),
                        .dimensions{{0, 0, kTextWidth, 10}},
                        .layout_text{std::string{kExpectedText}},
                }},
        };
        layout::LayoutBox expected_layout{
                .node = &style,
                .dimensions{{0, 0, 1234, 10}},
                .children{layout::LayoutBox{
                        .node = nullptr,
                        .dimensions{{0, 0, 1234, 10}},
                        .children{std::move(p_layout)},
                }},
        };

        auto actual = layout::create_layout(style, 1234);
        expect_eq(actual, expected_layout);
    });

    etest::test("text-transform: lowercase", [] {
        constexpr auto kText = "HELLO   GOODBYE"sv;
        constexpr auto kExpectedText = "hello goodbye"sv;
        constexpr auto kTextWidth = kExpectedText.length() * 5;

        dom::Element p{.name{"p"}, .children{dom::Text{std::string{kText}}}};
        dom::Node html = dom::Element{.name{"html"}, .children{std::move(p)}};
        dom::Element const &html_element = std::get<dom::Element>(html);

        style::StyledNode p_style{
                .node{html_element.children[0]},
                .properties{{css::PropertyId::Display, "inline"}, {css::PropertyId::TextTransform, "lowercase"}},
                .children{style::StyledNode{std::get<dom::Element>(html_element.children.at(0)).children.at(0)}},
        };
        style::StyledNode style{
                .node{html},
                .properties{{css::PropertyId::Display, "block"}, {css::PropertyId::FontSize, "10px"}},
                .children{std::move(p_style)},
        };
        set_up_parent_ptrs(style);

        layout::LayoutBox p_layout{
                .node = &style.children.at(0),
                .dimensions{{0, 0, kTextWidth, 10}},
                .children{layout::LayoutBox{
                        .node = &style.children.at(0).children.at(0),
                        .dimensions{{0, 0, kTextWidth, 10}},
                        .layout_text{std::string{kExpectedText}},
                }},
        };
        layout::LayoutBox expected_layout{
                .node = &style,
                .dimensions{{0, 0, 1234, 10}},
                .children{layout::LayoutBox{
                        .node = nullptr,
                        .dimensions{{0, 0, 1234, 10}},
                        .children{std::move(p_layout)},
                }},
        };

        auto actual = layout::create_layout(style, 1234);
        expect_eq(actual, expected_layout);
    });

    etest::test("text-transform: capitalize", [] {
        constexpr auto kText = "HE?LO   GOODBYE!"sv;
        constexpr auto kExpectedText = "He?Lo Goodbye!"sv;
        constexpr auto kTextWidth = kExpectedText.length() * 5;

        dom::Element p{.name{"p"}, .children{dom::Text{std::string{kText}}}};
        dom::Node html = dom::Element{.name{"html"}, .children{std::move(p)}};
        dom::Element const &html_element = std::get<dom::Element>(html);

        style::StyledNode p_style{
                .node{html_element.children[0]},
                .properties{{css::PropertyId::Display, "inline"}, {css::PropertyId::TextTransform, "capitalize"}},
                .children{style::StyledNode{std::get<dom::Element>(html_element.children.at(0)).children.at(0)}},
        };
        style::StyledNode style{
                .node{html},
                .properties{{css::PropertyId::Display, "block"}, {css::PropertyId::FontSize, "10px"}},
                .children{std::move(p_style)},
        };
        set_up_parent_ptrs(style);

        layout::LayoutBox p_layout{
                .node = &style.children.at(0),
                .dimensions{{0, 0, kTextWidth, 10}},
                .children{layout::LayoutBox{
                        .node = &style.children.at(0).children.at(0),
                        .dimensions{{0, 0, kTextWidth, 10}},
                        .layout_text{std::string{kExpectedText}},
                }},
        };
        layout::LayoutBox expected_layout{
                .node = &style,
                .dimensions{{0, 0, 1234, 10}},
                .children{layout::LayoutBox{
                        .node = nullptr,
                        .dimensions{{0, 0, 1234, 10}},
                        .children{std::move(p_layout)},
                }},
        };

        auto actual = layout::create_layout(style, 1234);
        expect_eq(actual, expected_layout);
    });
}

void img_tests() {
    etest::test("img, no alt or src", [] {
        dom::Node dom = dom::Element{"body", {}, {dom::Element{"img"}}};
        auto const &body = std::get<dom::Element>(dom);
        auto style = style::StyledNode{
                .node = dom,
                .properties{
                        {css::PropertyId::Display, "block"},
                        {css::PropertyId::FontSize, "10px"},
                },
                .children{
                        {body.children.at(0), {{css::PropertyId::Display, "block"}}},
                },
        };
        set_up_parent_ptrs(style);

        auto expected_layout = layout::LayoutBox{
                .node = &style,
                .dimensions{{0, 0, 100, 0}},
                .children{{
                        &style.children[0],
                        {{0, 0, 100, 0}},
                }},
        };

        auto layout_root = layout::create_layout(style, 100);
        expect_eq(expected_layout, layout_root);
    });

    etest::test("img, alt, no src", [] {
        dom::Node dom = dom::Element{"body", {}, {dom::Element{"img", {{"alt", "hello"}}}}};
        auto const &body = std::get<dom::Element>(dom);
        auto style = style::StyledNode{
                .node = dom,
                .properties{
                        {css::PropertyId::Display, "block"},
                        {css::PropertyId::FontSize, "10px"},
                },
                .children{
                        {body.children.at(0), {{css::PropertyId::Display, "block"}}},
                },
        };
        set_up_parent_ptrs(style);

        auto expected_layout = layout::LayoutBox{
                .node = &style,
                .dimensions{{0, 0, 100, 10}},
                .children{{
                        &style.children[0],
                        {{0, 0, 100, 10}},
                        {},
                        "hello"sv,
                }},
        };

        auto layout_root = layout::create_layout(style, 100);
        expect_eq(expected_layout, layout_root);
        expect_eq(expected_layout.children.at(0).text(), "hello");
    });

    // TODO(robinlinden): This test should break when we implement more of image layouting.
    etest::test("img, alt, src", [] {
        dom::Node dom = dom::Element{"body", {}, {dom::Element{"img", {{"alt", "asdf"}, {"src", "hallo"}}}}};
        auto const &body = std::get<dom::Element>(dom);
        auto style = style::StyledNode{
                .node = dom,
                .properties{
                        {css::PropertyId::Display, "block"},
                        {css::PropertyId::FontSize, "10px"},
                },
                .children{
                        {body.children.at(0), {{css::PropertyId::Display, "block"}}},
                },
        };
        set_up_parent_ptrs(style);

        auto expected_layout = layout::LayoutBox{
                .node = &style,
                .dimensions{{0, 0, 100, 0}},
                .children{{
                        &style.children[0], {{0, 0, 100, 0}}, {},
                        // TODO(robinlinden)
                        // {{0, 0, 37, 87}},
                }},
        };

        auto layout_root =
                layout::create_layout(style, 100, type::NaiveType{}, [](auto) { return layout::Size{37, 87}; });
        expect_eq(expected_layout, layout_root);
    });
}

} // namespace

// TODO(robinlinden): clang-format doesn't get along well with how I structured
// the trees in these test cases.

// clang-format off

int main() {
    etest::test("simple tree", [] {
        auto dom_root = create_element_node("html", {}, {
            create_element_node("head", {}, {}),
            create_element_node("body", {}, {
                create_element_node("p", {}, {}),
            }),
        });

        auto const &children = std::get<dom::Element>(dom_root).children;
        auto style_root = style::StyledNode{
            .node = dom_root,
            .properties = {{css::PropertyId::Display, "block"}},
            .children = {
                {children[0], {{css::PropertyId::Display, "block"}}, {}},
                {children[1], {{css::PropertyId::Display, "block"}}, {
                    {std::get<dom::Element>(children[1]).children[0], {{css::PropertyId::Display, "block"}}, {}},
                }},
            },
        };

        auto expected_layout = layout::LayoutBox{
            .node = &style_root,
            .dimensions = {},
            .children = {
                {&style_root.children[0], {}, {}},
                {&style_root.children[1], {}, {
                    {&style_root.children[1].children[0], {}, {}},
                }},
            }
        };

        auto layout_root = layout::create_layout(style_root, 0);
        expect(expected_layout == layout_root);
    });

    etest::test("layouting removes display:none nodes", [] {
        auto dom_root = create_element_node("html", {}, {
            create_element_node("head", {}, {}),
            create_element_node("body", {}, {
                create_element_node("p", {}, {}),
            }),
        });

        auto const &children = std::get<dom::Element>(dom_root).children;
        auto style_root = style::StyledNode{
            .node = dom_root,
            .properties = {{css::PropertyId::Display, "block"}},
            .children = {
                {children[0], {{css::PropertyId::Display, "none"}}, {}},
                {children[1], {{css::PropertyId::Display, "block"}}, {

                    {std::get<dom::Element>(children[1]).children[0], {{css::PropertyId::Display, "block"}}, {}},
                }},
            },
        };

        auto expected_layout = layout::LayoutBox{
            .node = &style_root,
            .dimensions = {},
            .children = {
                {&style_root.children[1], {}, {
                    {&style_root.children[1].children[0], {}, {}},
                }},
            }
        };

        auto layout_root = layout::create_layout(style_root, 0);
        expect(expected_layout == layout_root);
    });

    etest::test("inline nodes get wrapped in anonymous blocks", [] {
        auto dom_root = create_element_node("html", {}, {
            create_element_node("head", {}, {}),
            create_element_node("body", {}, {
                create_element_node("p", {}, {}),
            }),
        });

        auto const &children = std::get<dom::Element>(dom_root).children;
        auto style_root = style::StyledNode{
            .node = dom_root,
            .properties = {{css::PropertyId::Display, "block"}},
            .children = {
                {children[0], {{css::PropertyId::Display, "inline"}}, {}},
                {children[1], {{css::PropertyId::Display, "inline"}}, {
                    {std::get<dom::Element>(children[1]).children[0], {}, {}},
                }},
            },
        };

        auto expected_layout = layout::LayoutBox{
            .node = &style_root,
            .dimensions = {},
            .children = {
                {nullptr, {}, {
                    {&style_root.children[0], {}, {}},
                    {&style_root.children[1], {}, {
                        {&style_root.children[1].children[0], {}, {}},
                    }},
                }},
            }
        };

        auto layout_root = layout::create_layout(style_root, 0);
        expect(expected_layout == layout_root);
    });

    // clang-format on
    etest::test("inline in inline don't get wrapped in anon-blocks", [] {
        auto dom_root = create_element_node("span", {}, {create_element_node("span", {}, {})});

        auto const &children = std::get<dom::Element>(dom_root).children;
        auto style_root = style::StyledNode{
                .node = dom_root,
                .properties = {{css::PropertyId::Display, "inline"}},
                .children = {{children[0], {{css::PropertyId::Display, "inline"}}, {}}},
        };

        auto expected_layout = layout::LayoutBox{
                .node = &style_root,
                .dimensions = {},
                .children = {{&style_root.children[0], {}, {}}},
        };

        auto layout_root = layout::create_layout(style_root, 0);
        expect(expected_layout == layout_root);
    });
    // clang-format off

    etest::test("text", [] {
        auto dom_root = create_element_node("html", {}, {
            create_element_node("body", {}, {
                dom::Text{"hello"},
                dom::Text{"goodbye"},
            }),
        });

        auto const &children = std::get<dom::Element>(dom_root).children;
        auto style_root = style::StyledNode{
            .node = dom_root,
            .properties = {{css::PropertyId::Display, "block"}, {css::PropertyId::FontSize, "10px"}},
            .children = {
                {children[0], {{css::PropertyId::Display, "block"}}, {
                    {std::get<dom::Element>(children[0]).children[0], {}, {}},
                    {std::get<dom::Element>(children[0]).children[1], {}, {}},
                }},
            },
        };
        set_up_parent_ptrs(style_root);

        auto expected_layout = layout::LayoutBox{
            .node = &style_root,
            .dimensions = {{0, 0, 100, 10}},
            .children = {
                {&style_root.children[0], {{0, 0, 100, 10}}, {
                    {nullptr, {{0, 0, 100, 10}}, {
                        {&style_root.children[0].children[0], {{0, 0, 25, 10}}, {}, "hello"sv},
                        {&style_root.children[0].children[1], {{25, 0, 35, 10}}, {}, "goodbye"sv},
                    }},
                }},
            }
        };

        auto layout_root = layout::create_layout(style_root, 100);
        expect(expected_layout == layout_root);

        expect_eq(expected_layout.children.at(0).children.at(0).children.at(0).text(), "hello");
        expect_eq(expected_layout.children.at(0).children.at(0).children.at(1).text(), "goodbye");
    });

    etest::test("simple width", [] {
        auto dom_root = create_element_node("html", {}, {
            create_element_node("body", {}, {
                create_element_node("p", {}, {}),
            }),
        });

        auto const &children = std::get<dom::Element>(dom_root).children;
        auto style_root = style::StyledNode{
            .node = dom_root,
            .properties = {{css::PropertyId::Width, "100px"}, {css::PropertyId::Display, "block"}},
            .children = {
                {children[0], {{css::PropertyId::Display, "block"}}, {
                    {std::get<dom::Element>(children[0]).children[0], {{css::PropertyId::Display, "block"}}, {}},
                }},
            },
        };

        auto expected_layout = layout::LayoutBox{
            .node = &style_root,
            .dimensions = {{0, 0, 100, 0}},
            .children = {
                {&style_root.children[0], {{0, 0, 100, 0}}, {
                    {&style_root.children[0].children[0], {{0, 0, 100, 0}}, {}},
                }},
            }
        };

        expect(layout::create_layout(style_root, 1000) == expected_layout);
    });

    etest::test("min-width", [] {
        auto dom_root = create_element_node("html", {}, {
            create_element_node("body", {}, {
                create_element_node("p", {}, {}),
            }),
        });

        auto const &children = std::get<dom::Element>(dom_root).children;
        auto style_root = style::StyledNode{
            .node = dom_root,
            .properties = {{css::PropertyId::MinWidth, "100px"}, {css::PropertyId::Display, "block"}},
            .children = {
                {children[0], {{css::PropertyId::MinWidth, "50%"}, {css::PropertyId::Display, "block"}}, {
                    {std::get<dom::Element>(children[0]).children[0], {{css::PropertyId::Display, "block"}}, {}},
                }},
            },
        };

        auto expected_layout = layout::LayoutBox{
            .node = &style_root,
            .dimensions = {{0, 0, 100, 0}},
            .children = {
                {&style_root.children[0], {{0, 0, 100, 0}}, {
                    {&style_root.children[0].children[0], {{0, 0, 100, 0}}, {}},
                }},
            }
        };

        expect(layout::create_layout(style_root, 20) == expected_layout);
    });

    etest::test("max-width", [] {
        auto dom_root = create_element_node("html", {}, {
            create_element_node("body", {}, {
                create_element_node("p", {}, {}),
            }),
        });

        auto const &children = std::get<dom::Element>(dom_root).children;
        auto style_root = style::StyledNode{
            .node = dom_root,
            .properties = {{css::PropertyId::MaxWidth, "200px"}, {css::PropertyId::Display, "block"}},
            .children = {
                {children[0], {{css::PropertyId::MaxWidth, "50%"}, {css::PropertyId::Display, "block"}}, {
                    {std::get<dom::Element>(children[0]).children[0], {{css::PropertyId::Display, "block"}}, {}},
                }},
            },
        };

        auto expected_layout = layout::LayoutBox{
            .node = &style_root,
            .dimensions = {{0, 0, 200, 0}},
            .children = {
                {&style_root.children[0], {{0, 0, 100, 0}}, {
                    {&style_root.children[0].children[0], {{0, 0, 100, 0}}, {}},
                }},
            }
        };

        expect(layout::create_layout(style_root, 1000) == expected_layout);
    });

    etest::test("less simple width", [] {
        auto dom_root = create_element_node("html", {}, {
            create_element_node("body", {}, {
                create_element_node("p", {}, {}),
            }),
        });

        auto const &children = std::get<dom::Element>(dom_root).children;
        auto style_root = style::StyledNode{
            .node = dom_root,
            .properties = {{css::PropertyId::Width, "100px"}, {css::PropertyId::Display, "block"}},
            .children = {
                {children[0], {{css::PropertyId::Width, "50px"}, {css::PropertyId::Display, "block"}}, {
                    {std::get<dom::Element>(children[0]).children[0], {{css::PropertyId::Width, "25px"}, {css::PropertyId::Display, "block"}}, {}},
                }},
            },
        };

        auto expected_layout = layout::LayoutBox{
            .node = &style_root,
            .dimensions = {{0, 0, 100, 0}},
            .children = {
                {&style_root.children[0], {{0, 0, 50, 0}}, {
                    {&style_root.children[0].children[0], {{0, 0, 25, 0}}, {}},
                }},
            }
        };

        expect(layout::create_layout(style_root, 1000) == expected_layout);
    });

    etest::test("auto width expands to fill parent", [] {
        auto dom_root = create_element_node("html", {}, {
            create_element_node("body", {}, {
                create_element_node("p", {}, {}),
            }),
        });

        auto const &children = std::get<dom::Element>(dom_root).children;
        auto style_root = style::StyledNode{
            .node = dom_root,
            .properties = {{css::PropertyId::Width, "100px"}, {css::PropertyId::Display, "block"}},
            .children = {
                {children[0], {{css::PropertyId::Display, "block"}}, {
                    {std::get<dom::Element>(children[0]).children[0], {{css::PropertyId::Display, "block"}}, {}},
                }},
            },
        };

        auto expected_layout = layout::LayoutBox{
            .node = &style_root,
            .dimensions = {{0, 0, 100, 0}},
            .children = {
                {&style_root.children[0], {{0, 0, 100, 0}}, {
                    {&style_root.children[0].children[0], {{0, 0, 100, 0}}, {}},
                }},
            }
        };

        expect(layout::create_layout(style_root, 1000) == expected_layout);
    });

    etest::test("height doesn't affect children", [] {
        auto dom_root = create_element_node("html", {}, {
            create_element_node("body", {}, {
                create_element_node("p", {}, {}),
            }),
        });

        auto const &children = std::get<dom::Element>(dom_root).children;
        auto style_root = style::StyledNode{
            .node = dom_root,
            .properties = {{css::PropertyId::Height, "100px"}, {css::PropertyId::Display, "block"}},
            .children = {
                {children[0], {{css::PropertyId::Display, "block"}}, {
                    {std::get<dom::Element>(children[0]).children[0], {{css::PropertyId::Display, "block"}}, {}},
                }},
            },
        };

        auto expected_layout = layout::LayoutBox{
            .node = &style_root,
            .dimensions = {{0, 0, 0, 100}},
            .children = {
                {&style_root.children[0], {{0, 0, 0, 0}}, {
                    {&style_root.children[0].children[0], {{0, 0, 0, 0}}, {}},
                }},
            }
        };

        expect(layout::create_layout(style_root, 0) == expected_layout);
    });

    etest::test("height affects siblings and parents", [] {
        auto dom_root = create_element_node("html", {}, {
            create_element_node("body", {}, {
                create_element_node("p", {}, {}),
                create_element_node("p", {}, {}),
            }),
        });

        auto const &children = std::get<dom::Element>(dom_root).children;
        auto style_root = style::StyledNode{
            .node = dom_root,
            .properties = {{css::PropertyId::Display, "block"}},
            .children = {
                {children[0], {{css::PropertyId::Display, "block"}}, {
                    {std::get<dom::Element>(children[0]).children[0], {{css::PropertyId::Height, "25px"}, {css::PropertyId::Display, "block"}}, {}},
                    {std::get<dom::Element>(children[0]).children[1], {{css::PropertyId::Display, "block"}}, {}},
                }},
            },
        };

        auto expected_layout = layout::LayoutBox{
            .node = &style_root,
            .dimensions = {{0, 0, 0, 25}},
            .children = {
                {&style_root.children[0], {{0, 0, 0, 25}}, {
                    {&style_root.children[0].children[0], {{0, 0, 0, 25}}, {}},
                    {&style_root.children[0].children[1], {{0, 25, 0, 0}}, {}},
                }},
            }
        };

        expect(layout::create_layout(style_root, 0) == expected_layout);
    });

    etest::test("min-height is respected", [] {
        auto dom_root = create_element_node("html", {}, {
            create_element_node("body", {}, {
                create_element_node("p", {}, {}),
                create_element_node("p", {}, {}),
            }),
        });

        auto const &children = std::get<dom::Element>(dom_root).children;
        auto style_root = style::StyledNode{
            .node = dom_root,
            .properties = {{css::PropertyId::MinHeight, "400px"}, {css::PropertyId::Display, "block"}},
            .children = {
                {children[0], {{css::PropertyId::Display, "block"}}, {
                    {std::get<dom::Element>(children[0]).children[0], {{css::PropertyId::Height, "25px"}, {css::PropertyId::Display, "block"}}, {}},
                    {std::get<dom::Element>(children[0]).children[1], {{css::PropertyId::Display, "block"}}, {}},
                }},
            },
        };

        auto expected_layout = layout::LayoutBox{
            .node = &style_root,
            .dimensions = {{0, 0, 0, 400}},
            .children = {
                {&style_root.children[0], {{0, 0, 0, 25}}, {
                    {&style_root.children[0].children[0], {{0, 0, 0, 25}}, {}},
                    {&style_root.children[0].children[1], {{0, 25, 0, 0}}, {}},
                }},
            }
        };

        expect(layout::create_layout(style_root, 0) == expected_layout);
    });

    etest::test("max-height is respected", [] {
        auto dom_root = create_element_node("html", {}, {
            create_element_node("body", {}, {
                create_element_node("p", {}, {}),
                create_element_node("p", {}, {}),
            }),
        });

        auto const &children = std::get<dom::Element>(dom_root).children;
        auto style_root = style::StyledNode{
            .node = dom_root,
            .properties = {{css::PropertyId::MaxHeight, "10px"}, {css::PropertyId::Display, "block"}},
            .children = {
                {children[0], {{css::PropertyId::Display, "block"}}, {
                    {std::get<dom::Element>(children[0]).children[0], {{css::PropertyId::Height, "400px"}, {css::PropertyId::Display, "block"}}, {}},
                    {std::get<dom::Element>(children[0]).children[1], {{css::PropertyId::Display, "block"}}, {}},
                }},
            },
        };

        auto expected_layout = layout::LayoutBox{
            .node = &style_root,
            .dimensions = {{0, 0, 0, 10}},
            .children = {
                {&style_root.children[0], {{0, 0, 0, 400}}, {
                    {&style_root.children[0].children[0], {{0, 0, 0, 400}}, {}},
                    {&style_root.children[0].children[1], {{0, 400, 0, 0}}, {}},
                }},
            }
        };

        expect(layout::create_layout(style_root, 0) == expected_layout);
    });

    etest::test("padding is taken into account", [] {
        auto dom_root = create_element_node("html", {}, {
            create_element_node("body", {}, {
                create_element_node("p", {}, {}),
                create_element_node("p", {}, {}),
            }),
        });

        auto properties = std::vector{
                std::pair{css::PropertyId::Display, "block"s},
                std::pair{css::PropertyId::Height, "100px"s},
                std::pair{css::PropertyId::PaddingTop, "10px"s},
                std::pair{css::PropertyId::PaddingRight, "10px"s},
                std::pair{css::PropertyId::PaddingBottom, "10px"s},
                std::pair{css::PropertyId::PaddingLeft, "10px"s},
        };

        auto const &children = std::get<dom::Element>(dom_root).children;
        auto style_root = style::StyledNode{
            .node = dom_root,
            .properties = {{css::PropertyId::Display, "block"}},
            .children = {
                {children[0], {{css::PropertyId::Display, "block"}}, {
                    {std::get<dom::Element>(children[0]).children[0], std::move(properties), {}},
                    {std::get<dom::Element>(children[0]).children[1], {{css::PropertyId::Display, "block"}}, {}},
                }},
            },
        };

        auto expected_layout = layout::LayoutBox{
            .node = &style_root,
            .dimensions = {{0, 0, 100, 120}},
            .children = {
                {&style_root.children[0], {{0, 0, 100, 120}}, {
                    {&style_root.children[0].children[0], {{10, 10, 80, 100}, {10, 10, 10, 10}, {}, {0, 0, 0, 0}}, {}},
                    {&style_root.children[0].children[1], {{0, 120, 100, 0}}, {}},
                }},
            }
        };

        expect(layout::create_layout(style_root, 100) == expected_layout);
    });

    etest::test("border is taken into account", [] {
        auto dom_root = create_element_node("html", {}, {
            create_element_node("body", {}, {
                create_element_node("p", {}, {}),
                create_element_node("p", {}, {}),
            }),
        });

        auto properties = std::vector{
                std::pair{css::PropertyId::Display, "block"s},
                std::pair{css::PropertyId::Height, "100px"s},
                std::pair{css::PropertyId::BorderLeftStyle, "solid"s},
                std::pair{css::PropertyId::BorderRightStyle, "solid"s},
                std::pair{css::PropertyId::BorderTopStyle, "solid"s},
                std::pair{css::PropertyId::BorderBottomStyle, "solid"s},
                std::pair{css::PropertyId::BorderLeftWidth, "10px"s},
                std::pair{css::PropertyId::BorderRightWidth, "12px"s},
                std::pair{css::PropertyId::BorderTopWidth, "14px"s},
                std::pair{css::PropertyId::BorderBottomWidth, "16px"s},
        };

        auto const &children = std::get<dom::Element>(dom_root).children;
        auto style_root = style::StyledNode{
            .node = dom_root,
            .properties = {{css::PropertyId::Display, "block"}},
            .children = {
                {children[0], {{css::PropertyId::Display, "block"}}, {
                    {std::get<dom::Element>(children[0]).children[0], std::move(properties), {}},
                    {std::get<dom::Element>(children[0]).children[1], {{css::PropertyId::Display, "block"}}, {}},
                }},
            },
        };

        auto expected_layout = layout::LayoutBox{
            .node = &style_root,
            .dimensions = {{0, 0, 100, 130}},
            .children = {
                {&style_root.children[0], {{0, 0, 100, 130}}, {
                    {&style_root.children[0].children[0], {{10, 14, 78, 100}, {}, {10, 12, 14, 16}, {}}, {}},
                    {&style_root.children[0].children[1], {{0, 130, 100, 0}}, {}},
                }},
            }
        };

        expect(layout::create_layout(style_root, 100) == expected_layout);
    });

    etest::test("border is not added if border style is none", [] {
        auto dom_root = create_element_node("html", {}, {
            create_element_node("body", {}, {
                create_element_node("p", {}, {}),
            }),
        });

        auto properties = std::vector{
                std::pair{css::PropertyId::Display, "block"s},
                std::pair{css::PropertyId::Height, "100px"s},
                std::pair{css::PropertyId::BorderLeftWidth, "10px"s},
                std::pair{css::PropertyId::BorderRightWidth, "12px"s},
                std::pair{css::PropertyId::BorderTopWidth, "14px"s},
                std::pair{css::PropertyId::BorderBottomWidth, "16px"s},
        };

        auto const &children = std::get<dom::Element>(dom_root).children;
        auto style_root = style::StyledNode{
            .node = dom_root,
            .properties = {{css::PropertyId::Display, "block"}},
            .children = {
                {children[0], {{css::PropertyId::Display, "block"}}, {
                    {std::get<dom::Element>(children[0]).children[0], std::move(properties), {}},
                }},
            },
        };

        auto expected_layout = layout::LayoutBox{
            .node = &style_root,
            .dimensions = {{0, 0, 100, 100}},
            .children = {
                {&style_root.children[0], {{0, 0, 100, 100}}, {
                    {&style_root.children[0].children[0], {{0, 0, 100, 100}, {}, {}, {}}, {}},
                }},
            }
        };

        expect(layout::create_layout(style_root, 100) == expected_layout);
    });

    etest::test("margin is taken into account", [] {
        auto dom_root = create_element_node("html", {}, {
            create_element_node("body", {}, {
                create_element_node("p", {}, {}),
                create_element_node("p", {}, {}),
            }),
        });

        auto properties = std::vector{
                std::pair{css::PropertyId::Display, "block"s},
                std::pair{css::PropertyId::MarginTop, "10px"s},
                std::pair{css::PropertyId::MarginRight, "10px"s},
                std::pair{css::PropertyId::MarginBottom, "10px"s},
                std::pair{css::PropertyId::MarginLeft, "10px"s},
        };

        auto const &children = std::get<dom::Element>(dom_root).children;
        auto style_root = style::StyledNode{
            .node = dom_root,
            .properties = {{css::PropertyId::Display, "block"}},
            .children = {
                {children[0], {{css::PropertyId::Display, "block"}}, {
                    {std::get<dom::Element>(children[0]).children[0], std::move(properties), {}},
                    {std::get<dom::Element>(children[0]).children[1], {{css::PropertyId::Display, "block"}}, {}},
                }},
            },
        };

        auto expected_layout = layout::LayoutBox{
            .node = &style_root,
            .dimensions = {{0, 0, 100, 20}},
            .children = {
                {&style_root.children[0], {{0, 0, 100, 20}}, {
                    {&style_root.children[0].children[0], {{10, 10, 80, 0}, {}, {}, {10, 10, 10, 10}}, {}},
                    {&style_root.children[0].children[1], {{0, 20, 100, 0}}, {}},
                }},
            }
        };

        expect(layout::create_layout(style_root, 100) == expected_layout);
    });

    etest::test("auto margin is handled", [] {
        auto dom_root = create_element_node("html", {}, {
            create_element_node("body", {}, {
                create_element_node("p", {}, {}),
            }),
        });

        auto properties = std::vector{
                std::pair{css::PropertyId::Display, "block"s},
                std::pair{css::PropertyId::Width, "100px"s},
                std::pair{css::PropertyId::MarginLeft, "auto"s},
                std::pair{css::PropertyId::MarginRight, "auto"s},
        };

        auto const &children = std::get<dom::Element>(dom_root).children;
        auto style_root = style::StyledNode{
            .node = dom_root,
            .properties = {{css::PropertyId::Display, "block"}},
            .children = {
                {children[0], {{css::PropertyId::Display, "block"}}, {
                    {std::get<dom::Element>(children[0]).children[0], std::move(properties), {}},
                }},
            },
        };

        auto expected_layout = layout::LayoutBox{
            .node = &style_root,
            .dimensions = {{0, 0, 200, 0}},
            .children = {
                {&style_root.children[0], {{0, 0, 200, 0}}, {
                    {&style_root.children[0].children[0], {{50, 0, 100, 0}, {}, {}, {50, 50, 0, 0}}, {}},
                }},
            }
        };

        expect(layout::create_layout(style_root, 200) == expected_layout);
    });

    etest::test("auto left margin and fixed right margin is handled", [] {
        auto dom_root = create_element_node("html", {}, {
            create_element_node("body", {}, {
                create_element_node("p", {}, {}),
            }),
        });

        auto properties = std::vector{
                std::pair{css::PropertyId::Display, "block"s},
                std::pair{css::PropertyId::Width, "100px"s},
                std::pair{css::PropertyId::MarginLeft, "auto"s},
                std::pair{css::PropertyId::MarginRight, "20px"s},
        };

        auto const &children = std::get<dom::Element>(dom_root).children;
        auto style_root = style::StyledNode{
            .node = dom_root,
            .properties = {{css::PropertyId::Display, "block"}},
            .children = {
                {children[0], {{css::PropertyId::Display, "block"}}, {
                    {std::get<dom::Element>(children[0]).children[0], std::move(properties), {}},
                }},
            },
        };

        auto expected_layout = layout::LayoutBox{
            .node = &style_root,
            .dimensions = {{0, 0, 200, 0}},
            .children = {
                {&style_root.children[0], {{0, 0, 200, 0}}, {
                    {&style_root.children[0].children[0], {{80, 0, 100, 0}, {}, {}, {80, 20, 0, 0}}, {}},
                }},
            }
        };

        expect(layout::create_layout(style_root, 200) == expected_layout);
    });

    etest::test("fixed left margin and auto right margin is handled", [] {
        auto dom_root = create_element_node("html", {}, {
            create_element_node("body", {}, {
                create_element_node("p", {}, {}),
            }),
        });

        auto properties = std::vector{
                std::pair{css::PropertyId::Display, "block"s},
                std::pair{css::PropertyId::Width, "100px"s},
                std::pair{css::PropertyId::MarginLeft, "75px"s},
                std::pair{css::PropertyId::MarginRight, "auto"s},
        };

        auto const &children = std::get<dom::Element>(dom_root).children;
        auto style_root = style::StyledNode{
            .node = dom_root,
            .properties = {{css::PropertyId::Display, "block"}},
            .children = {
                {children[0], {{css::PropertyId::Display, "block"}}, {
                    {std::get<dom::Element>(children[0]).children[0], std::move(properties), {}},
                }},
            },
        };

        auto expected_layout = layout::LayoutBox{
            .node = &style_root,
            .dimensions = {{0, 0, 200, 0}},
            .children = {
                {&style_root.children[0], {{0, 0, 200, 0}}, {
                    {&style_root.children[0].children[0], {{75, 0, 100, 0}, {}, {}, {75, 25, 0, 0}}, {}},
                }},
            }
        };

        expect(layout::create_layout(style_root, 200) == expected_layout);
    });

    etest::test("em sizes depend on the font-size", [] {
        auto dom_root = create_element_node("html", {}, {});
        {
            auto style_root = style::StyledNode{
                .node = dom_root,
                .properties = {
                        std::pair{css::PropertyId::Display, "block"},
                        std::pair{css::PropertyId::FontSize, "10px"},
                        std::pair{css::PropertyId::Height, "10em"},
                        std::pair{css::PropertyId::Width, "10em"},
                },
                .children = {},
            };

            auto expected_layout = layout::LayoutBox{
                .node = &style_root,
                .dimensions = {{0, 0, 100, 100}},
                .children = {}
            };

            expect(layout::create_layout(style_root, 1000) == expected_layout);
        }

        // Doubling the font-size should double the width/height.
        auto style_root = style::StyledNode{
            .node = dom_root,
            .properties = {
                    std::pair{css::PropertyId::Display, "block"},
                    std::pair{css::PropertyId::FontSize, "20px"},
                    std::pair{css::PropertyId::Height, "10em"},
                    std::pair{css::PropertyId::Width, "10em"},
            },
            .children = {},
        };

        auto expected_layout = layout::LayoutBox{
            .node = &style_root,
            .dimensions = {{0, 0, 200, 200}},
            .children = {}
        };

        expect(layout::create_layout(style_root, 1000) == expected_layout);
    });

    etest::test("px sizes don't depend on the font-size", [] {
        auto dom_root = create_element_node("html", {}, {});
        {
            auto style_root = style::StyledNode{
                .node = dom_root,
                .properties = {
                        std::pair{css::PropertyId::Display, "block"},
                        std::pair{css::PropertyId::FontSize, "10px"},
                        std::pair{css::PropertyId::Height, "10px"},
                        std::pair{css::PropertyId::Width, "10px"},
                },
                .children = {},
            };

            auto expected_layout = layout::LayoutBox{
                .node = &style_root,
                .dimensions = {{0, 0, 10, 10}},
                .children = {}
            };

            expect(layout::create_layout(style_root, 1000) == expected_layout);
        }

        // Doubling the font-size shouldn't change the width/height.
        auto style_root = style::StyledNode{
            .node = dom_root,
            .properties = {
                    std::pair{css::PropertyId::Display, "block"},
                    std::pair{css::PropertyId::FontSize, "20px"},
                    std::pair{css::PropertyId::Height, "10px"},
                    std::pair{css::PropertyId::Width, "10px"},
            },
            .children = {},
        };

        auto expected_layout = layout::LayoutBox{
            .node = &style_root,
            .dimensions = {{0, 0, 10, 10}},
            .children = {}
        };

        expect(layout::create_layout(style_root, 1000) == expected_layout);
    });

    // clang-format on
    etest::test("max-width: none", [] {
        dom::Node dom = dom::Element{.name{"html"}};
        style::StyledNode style{
                .node{dom},
                .properties{{css::PropertyId::Display, "block"},
                        {css::PropertyId::Width, "100px"},
                        {css::PropertyId::MaxWidth, "none"}},
        };
        layout::LayoutBox expected_layout{.node = &style, .dimensions{{0, 0, 100, 0}}};

        auto layout = layout::create_layout(style, 0);
        expect_eq(layout, expected_layout);
    });

    etest::test("max-height: none", [] {
        dom::Node dom = dom::Element{.name{"html"}};
        style::StyledNode style{
                .node{dom},
                .properties{{css::PropertyId::Display, "block"},
                        {css::PropertyId::Height, "100px"},
                        {css::PropertyId::MaxHeight, "none"}},
        };
        layout::LayoutBox expected_layout{.node = &style, .dimensions{{0, 0, 0, 100}}};

        auto layout = layout::create_layout(style, 0);
        expect_eq(layout, expected_layout);
    });

    etest::test("height: auto", [] {
        dom::Node dom = dom::Element{.name{"html"}, .children{dom::Element{.name{"p"}}}};
        style::StyledNode style{
                .node{dom},
                .properties{{css::PropertyId::Display, "block"}, {css::PropertyId::Height, "auto"}},
                .children{
                        style::StyledNode{
                                .node{std::get<dom::Element>(dom).children[0]},
                                .properties{{css::PropertyId::Display, "block"}, {css::PropertyId::Height, "10px"}},
                        },
                },
        };
        layout::LayoutBox expected_layout{
                .node = &style,
                .dimensions{{0, 0, 0, 10}},
                .children{layout::LayoutBox{
                        .node = &style.children[0],
                        .dimensions{{0, 0, 0, 10}},
                }},
        };

        auto layout = layout::create_layout(style, 0);
        expect_eq(layout, expected_layout);
    });

    etest::test("font-size absolute value keywords", [] {
        dom::Node dom = dom::Element{.name{"html"}, .children{dom::Text{"hi"}}};
        style::StyledNode style{
                .node{dom},
                .properties{{css::PropertyId::Display, "block"}, {css::PropertyId::FontSize, "medium"}},
                .children{
                        style::StyledNode{.node{std::get<dom::Element>(dom).children[0]}},
                },
        };
        set_up_parent_ptrs(style);

        auto medium_layout = layout::create_layout(style, 1000).value();
        style.properties = {{css::PropertyId::Display, "block"}, {css::PropertyId::FontSize, "xxx-large"}};
        auto xxxlarge_layout = layout::create_layout(style, 1000).value();

        auto get_text_width = [](layout::LayoutBox const &layout) {
            require_eq(layout.children.size(), std::size_t{1});
            require_eq(layout.children[0].children.size(), std::size_t{1});
            return layout.children[0].children[0].dimensions.content.width;
        };

        auto medium_layout_width = get_text_width(medium_layout);
        auto xxxlarge_layout_width = get_text_width(xxxlarge_layout);
        expect(medium_layout_width > 0);

        // font-size: xxx-large should be 3x font-size: medium.
        // https://drafts.csswg.org/css-fonts-4/#absolute-size-mapping
        expect_eq(medium_layout_width * 3, xxxlarge_layout_width);
    });

    etest::test("invalid size", [] {
        dom::Node dom = dom::Element{.name{"html"}};
        style::StyledNode style{
                .node{dom},
                .properties{{css::PropertyId::Display, "block"}, {css::PropertyId::Height, "no"}},
        };

        layout::LayoutBox expected_layout{
                .node = &style,
                .dimensions{{0, 0, 0, 0}},
        };

        auto layout = layout::create_layout(style, 0);
        expect_eq(layout, expected_layout);
    });

    etest::test("unhandled unit", [] {
        dom::Node dom = dom::Element{.name{"html"}};
        style::StyledNode style{
                .node{dom},
                .properties{{css::PropertyId::Display, "block"}, {css::PropertyId::Height, "0notarealunit"}},
        };

        layout::LayoutBox expected_layout{
                .node = &style,
                .dimensions{{0, 0, 0, 0}},
        };

        auto layout = layout::create_layout(style, 0);
        expect_eq(layout, expected_layout);
    });

    etest::test("border-width keywords", [] {
        dom::Node dom = dom::Element{.name{"html"}};
        style::StyledNode style{
                .node{dom},
                .properties{{css::PropertyId::Display, "block"},
                        {css::PropertyId::BorderLeftStyle, "solid"},
                        {css::PropertyId::BorderLeftWidth, "thin"}},
        };

        auto layout = layout::create_layout(style, 0).value();
        expect_eq(layout.dimensions.border, geom::EdgeSize{.left = 3});
    });

    etest::test("text, bold", [] {
        dom::Node dom = dom::Element{.name{"html"}, .children{dom::Text{"hello"}}};
        style::StyledNode style{
                .node{dom},
                .properties{
                        {css::PropertyId::Display, "inline"},
                        {css::PropertyId::FontSize, "10px"},
                        {css::PropertyId::FontWeight, "bold"},
                },
                .children{style::StyledNode{.node{std::get<dom::Element>(dom).children[0]}}},
        };
        set_up_parent_ptrs(style);

        layout::LayoutBox expected{
                .node = &style,
                .dimensions{{0, 0, 25, 10}},
                .children{layout::LayoutBox{
                        .node = &style.children[0],
                        .dimensions{{0, 0, 25, 10}},
                        .layout_text = "hello"sv,
                }},
        };

        auto l = layout::create_layout(style, 30, NoType{}).value();
        expect_eq(l, expected);
    });

    etest::test("text, no font available", [] {
        dom::Node dom = dom::Element{.name{"html"}, .children{dom::Text{"hello"}}};
        style::StyledNode style{
                .node{dom},
                .properties{{css::PropertyId::Display, "block"}, {css::PropertyId::FontSize, "10px"}},
                .children{style::StyledNode{.node{std::get<dom::Element>(dom).children[0]}}},
        };
        set_up_parent_ptrs(style);

        layout::LayoutBox expected{
                .node = &style,
                .dimensions{{0, 0, 30, 10}},
                .children{layout::LayoutBox{
                        .node = nullptr,
                        .dimensions{{0, 0, 30, 10}},
                        .children{layout::LayoutBox{
                                .node = &style.children[0],
                                .dimensions{{0, 0, 25, 10}},
                                .layout_text = "hello"sv,
                        }},
                }},
        };

        auto l = layout::create_layout(style, 30, NoType{}).value();
        expect_eq(l, expected);
    });

    etest::test("text with newlines in", [] {
        dom::Node dom = dom::Element{.name{"html"}, .children{dom::Text{"hi"}}};
        style::StyledNode style{
                .node{dom},
                .properties{{css::PropertyId::Display, "block"}},
                .children{style::StyledNode{.node{std::get<dom::Element>(dom).children[0]}}},
        };
        set_up_parent_ptrs(style);

        auto get_text_dimensions = [](layout::LayoutBox const &layout) {
            require_eq(layout.children.size(), std::size_t{1});
            require_eq(layout.children[0].children.size(), std::size_t{1});
            auto const &content_dims = layout.children[0].children[0].dimensions.content;
            return std::pair{content_dims.width, content_dims.height};
        };

        auto single_line_layout = layout::create_layout(style, 1000).value();
        auto single_line_layout_dims = get_text_dimensions(single_line_layout);
        require(single_line_layout_dims.second > 0);

        // This will get collapsed to a single line.
        std::get<dom::Text>(std::get<dom::Element>(dom).children[0]).text = "hi\nhi"s;
        auto two_line_layout = layout::create_layout(style, 1000).value();
        auto two_line_layout_dims = get_text_dimensions(two_line_layout);
        expect_eq(std::get<std::string>(two_line_layout.children.at(0).children.at(0).layout_text), "hi hi"sv);

        expect_eq(two_line_layout_dims.second, single_line_layout_dims.second);
        expect(two_line_layout_dims.first >= 2 * single_line_layout_dims.first);
    });

    etest::test("text too long for its container", [] {
        dom::Node dom = dom::Element{.name{"html"}, .children{dom::Text{"hi hello"}}};
        style::StyledNode style{
                .node{dom},
                .properties{{css::PropertyId::Display, "block"}, {css::PropertyId::FontSize, "10px"}},
                .children{style::StyledNode{.node{std::get<dom::Element>(dom).children[0]}}},
        };
        set_up_parent_ptrs(style);

        // TODO(robinlinden): It should be possible for the text here to be
        // views into the dom text.
        layout::LayoutBox expected{
                .node = &style,
                // 2 lines, where the widest one is 5 characters.
                .dimensions{{0, 0, 30, 20}},
                .children{layout::LayoutBox{
                        .node = nullptr,
                        .dimensions{{0, 0, 30, 20}},
                        .children{
                                layout::LayoutBox{
                                        .node = &style.children[0],
                                        .dimensions{{0, 0, 10, 10}},
                                        .layout_text = "hi"s,
                                },
                                layout::LayoutBox{
                                        .node = &style.children[0],
                                        .dimensions{{0, 10, 25, 10}},
                                        .layout_text = "hello"s,
                                },
                        },
                }},
        };

        auto l = layout::create_layout(style, 30).value();
        expect_eq(l, expected);
    });

    etest::test("text too long for its container, better split point later", [] {
        dom::Node dom = dom::Element{.name{"html"}, .children{dom::Text{"oh no !! !"}}};
        style::StyledNode style{
                .node{dom},
                .properties{{css::PropertyId::Display, "block"}, {css::PropertyId::FontSize, "10px"}},
                .children{style::StyledNode{.node{std::get<dom::Element>(dom).children[0]}}},
        };
        set_up_parent_ptrs(style);

        layout::LayoutBox expected{
                .node = &style,
                .dimensions{{0, 0, 30, 20}},
                .children{layout::LayoutBox{
                        .node = nullptr,
                        .dimensions{{0, 0, 30, 20}},
                        .children{
                                layout::LayoutBox{
                                        .node = &style.children[0],
                                        .dimensions{{0, 0, 25, 10}},
                                        .layout_text = "oh no"s,
                                },
                                layout::LayoutBox{
                                        .node = &style.children[0],
                                        .dimensions{{0, 10, 20, 10}},
                                        .layout_text = "!! !"s,
                                },
                        },
                }},
        };

        auto l = layout::create_layout(style, 30).value();
        expect_eq(l, expected);
    });

    etest::test("text too long for its container, but no split point available", [] {
        dom::Node dom = dom::Element{.name{"html"}, .children{dom::Text{"hello"}}};
        style::StyledNode style{
                .node{dom},
                .properties{{css::PropertyId::Display, "block"}, {css::PropertyId::FontSize, "10px"}},
                .children{style::StyledNode{.node{std::get<dom::Element>(dom).children[0]}}},
        };
        set_up_parent_ptrs(style);

        layout::LayoutBox expected{
                .node = &style,
                .dimensions{{0, 0, 15, 10}},
                .children{layout::LayoutBox{
                        .node = nullptr,
                        .dimensions{{0, 0, 15, 10}},
                        .children{layout::LayoutBox{
                                .node = &style.children[0],
                                .dimensions{{0, 0, 25, 10}},
                                .layout_text = "hello"sv,
                        }},
                }},
        };

        auto l = layout::create_layout(style, 15).value();
        expect_eq(l, expected);
    });

    etest::test("br", [] {
        dom::Node dom = dom::Element{
                .name{"html"},
                .children{
                        dom::Text{"hello"},
                        dom::Element{"br"},
                        dom::Text{"world"},
                },
        };
        auto const &children = std::get<dom::Element>(dom).children;

        style::StyledNode style{
                .node{dom},
                .properties{{css::PropertyId::Display, "block"}, {css::PropertyId::FontSize, "10px"}},
                .children{
                        style::StyledNode{.node{children[0]}},
                        style::StyledNode{.node{children[1]}},
                        style::StyledNode{.node{children[2]}},
                },
        };
        set_up_parent_ptrs(style);

        layout::LayoutBox expected{
                .node = &style,
                .dimensions{{0, 0, 25, 20}},
                .children{layout::LayoutBox{
                        .node = nullptr,
                        .dimensions{{0, 0, 25, 20}},
                        .children{
                                layout::LayoutBox{
                                        .node = &style.children[0],
                                        .dimensions{{0, 0, 25, 10}},
                                        .layout_text = "hello"sv,
                                },
                                layout::LayoutBox{
                                        .node = &style.children[1],
                                        .dimensions{{25, 0, 0, 0}},
                                },
                                layout::LayoutBox{
                                        .node = &style.children[2],
                                        .dimensions{{0, 10, 25, 10}},
                                        .layout_text = "world"sv,
                                },
                        },
                }},
        };

        auto l = layout::create_layout(style, 25).value();
        expect_eq(l, expected);
    });

    etest::test("display:none on root node", [] {
        dom::Node dom = dom::Element{.name{"html"}};
        style::StyledNode style{
                .node{dom},
                .properties{{css::PropertyId::Display, "none"}},
        };

        expect_eq(layout::create_layout(style, 0), std::nullopt);
    });

    etest::test("rem units", [] {
        dom::Node dom = dom::Element{"html", {}, {dom::Element{"div"}}};
        auto const &div = std::get<dom::Element>(dom).children[0];
        style::StyledNode style{
                .node{dom},
                .properties{{css::PropertyId::FontSize, "10px"}, {css::PropertyId::Display, "block"}},
                .children{
                        style::StyledNode{
                                .node{div},
                                .properties{{css::PropertyId::Width, "2rem"}, {css::PropertyId::Display, "block"}},
                        },
                },
        };
        set_up_parent_ptrs(style);

        auto layout = layout::create_layout(style, 1000).value();
        expect_eq(layout.children.at(0).dimensions.border_box().width, 20);

        style.properties.at(0).second = "16px";
        layout = layout::create_layout(style, 1000).value();
        expect_eq(layout.children.at(0).dimensions.border_box().width, 32);
    });

    etest::test("% units", [] {
        dom::Node dom = dom::Element{"html", {}, {dom::Element{"div"}}};
        auto const &div = std::get<dom::Element>(dom).children[0];
        style::StyledNode style{
                .node{dom},
                .properties{{css::PropertyId::Width, "500px"}, {css::PropertyId::Display, "block"}},
                .children{
                        style::StyledNode{
                                .node{div},
                                .properties{{css::PropertyId::Width, "50%"}, {css::PropertyId::Display, "block"}},
                        },
                },
        };
        set_up_parent_ptrs(style);

        auto layout = layout::create_layout(style, 1000).value();
        expect_eq(layout.children.at(0).dimensions.border_box().width, 250);

        style.properties.at(0).second = "10%";
        layout = layout::create_layout(style, 1000).value();
        expect_eq(layout.children.at(0).dimensions.border_box().width, 50);
    });

    etest::test("invalid width properties", [] {
        dom::Node dom = dom::Element{"html", {}, {dom::Element{"div"}}};
        auto const &div = std::get<dom::Element>(dom).children[0];
        style::StyledNode style{
                .node{dom},
                .properties{{css::PropertyId::Width, "asdf"}, {css::PropertyId::Display, "block"}},
                .children{
                        style::StyledNode{
                                .node{div},
                                .properties{{css::PropertyId::Width, "100px"}, {css::PropertyId::Display, "block"}},
                        },
                },
        };
        set_up_parent_ptrs(style);

        auto layout = layout::create_layout(style, 1000).value();
        expect_eq(layout.dimensions.border_box().width, 1000);
        expect_eq(layout.children.at(0).dimensions.border_box().width, 100);

        style.properties.emplace_back(css::PropertyId::MaxWidth, "asdf");
        layout = layout::create_layout(style, 1000).value();
        expect_eq(layout.dimensions.border_box().width, 1000);
        expect_eq(layout.children.at(0).dimensions.border_box().width, 100);
    });

    etest::test("the height property is ignored for inline elements", [] {
        dom::Node dom = dom::Element{"html", {}, {dom::Element{"span"}}};
        auto &span = std::get<dom::Element>(dom).children[0];
        style::StyledNode style{
                .node{dom},
                .properties{{css::PropertyId::FontSize, "10px"}, {css::PropertyId::Display, "block"}},
                .children{
                        style::StyledNode{
                                .node{span},
                                .properties{{css::PropertyId::Height, "100px"}, {css::PropertyId::Display, "inline"}},
                        },
                },
        };
        set_up_parent_ptrs(style);

        // 0 due to height being ignored and there being no content.
        auto layout = layout::create_layout(style, 1000).value();
        expect_eq(layout.dimensions.border_box().height, 0);
        expect_eq(layout.children.at(0).dimensions.border_box().height, 0);

        auto &span_elem = std::get<dom::Element>(span);
        span_elem.children.emplace_back(dom::Text{"hello"});
        style.children.at(0).children.emplace_back(style::StyledNode{.node{span_elem.children[0]}});
        set_up_parent_ptrs(style);

        // 10px due to the text content being 10px tall.
        layout = layout::create_layout(style, 1000).value();
        expect_eq(layout.dimensions.border_box().height, 10);
        expect_eq(layout.children.at(0).dimensions.border_box().height, 10);

        // And blocks don't have the height ignored, so 100px.
        style.children.at(0).properties.at(1).second = "block";
        layout = layout::create_layout(style, 1000).value();
        expect_eq(layout.dimensions.border_box().height, 100);
        expect_eq(layout.children.at(0).dimensions.border_box().height, 100);
    });

    whitespace_collapsing_tests();
    text_transform_tests();
    img_tests();

    return etest::run_all_tests();
}
