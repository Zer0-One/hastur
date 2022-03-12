// SPDX-FileCopyrightText: 2021 Robin Lindén <dev@robinlinden.eu>
//
// SPDX-License-Identifier: BSD-2-Clause

#ifndef UTIL_BASE_PARSER_H_
#define UTIL_BASE_PARSER_H_

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <string_view>

namespace util {

template<typename T>
concept Predicate = std::predicate<T, char>;

class BaseParser {
public:
    constexpr BaseParser(std::string_view input) : input_{input} {}

    constexpr char peek() const { return input_[pos_]; }

    constexpr std::string_view peek(std::size_t chars) const { return input_.substr(pos_, chars); }

    constexpr bool starts_with(std::string_view prefix) const { return peek(prefix.size()) == prefix; }

    constexpr bool is_eof() const { return pos_ >= input_.size(); }

    constexpr char consume_char() { return input_[pos_++]; }

    constexpr void advance(std::size_t n) { pos_ += n; }

    constexpr void reset() { pos_ = 0; }

    constexpr void reset(std::string_view input) { input_ = input; pos_ = 0; }

    template<Predicate T>
    constexpr std::string_view consume_while(T const &pred) {
        std::size_t start = pos_;
        while (pred(input_[pos_])) {
            ++pos_;
        }
        return input_.substr(start, pos_ - start);
    }

    constexpr void skip_whitespace() {
        while (!is_eof() && is_space(peek())) {
            advance(1);
        }
    }

    static constexpr bool is_c0(char c) {
        return c >= 0x00 && c <= 0x1f;
        //return std::find(begin(c0_chars), end(c0_chars), c) != end(space_chars);
    }

    static constexpr bool is_c0_or_space(char c) {
        return is_c0(c) || c == 0x20;
    }

    static constexpr bool is_tab_or_newline(char c) {
        return c == 0x09 || c == 0x0a || c == 0x0d;
    }

private:
    static constexpr auto space_chars = std::array{' ', '\f', '\n', '\r', '\t', '\v'};

    constexpr static bool is_space(char c) {
        return std::find(begin(space_chars), end(space_chars), c) != end(space_chars);
    }

    std::string_view input_;
    std::size_t pos_{0};
};

} // namespace util

#endif
