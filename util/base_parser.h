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
    BaseParser(std::string input) : input_{input} {}

    constexpr char peek() const { return input_[pos_]; }

    std::string peek(std::size_t chars) const {
        if (chars == 0){
            return input_.substr(pos_);
        }

        return input_.substr(pos_, chars);
    }

    bool starts_with(std::string_view prefix) const { return peek(prefix.size()) == prefix; }

    constexpr bool is_eof() const { return pos_ >= input_.size(); }

    constexpr char consume_char() { return input_[pos_++]; }

    constexpr void advance(std::size_t n) { pos_ += n; }

    constexpr void reset() { pos_ = 0; }

    void reset(std::string input) { input_ = input; pos_ = 0; }

    template<Predicate T>
    std::string consume_while(T const &pred) {
        std::size_t start = pos_;
        while (pred(input_[pos_])) {
            ++pos_;
        }
        return input_.substr(start, pos_ - start);
    }

    constexpr void skip_whitespace() {
        while (!is_eof() && is_whitespace(peek())) {
            advance(1);
        }
    }

    constexpr void trim_leading(bool (*predicate)(char)) {
        //for(auto i = input_.begin(); i != input_.end(); i++){
        //    if(predicate(*i)){
        //        input_.erase(i);

        //        continue;
        //    }

        //    break;
        //}
        auto it = std::find_if(input_.cbegin(), input_.cend(), [predicate](char ch) { return !predicate(ch); });
        input_.erase(input_.cbegin(), it);
    }

    constexpr void trim_trailing(bool (*predicate)(char)) {
        //for(auto i = input.rbegin(); i != input_.rend(); i++){
        //    if(predicate(*i)){
        //        
        //    }
        //}
        auto it = std::find_if(input_.crbegin(), input_.crend(), [predicate](char ch) { return !predicate(ch); });
        input_.erase((it+1).base(), input_.cend());// <-- doesn't work, because erase() doesn't handle reverse iterators
    }

    constexpr bool is_hexdigit(char c) const {
        return (c >= 0x30 && c <= 0x39) ||
               (c >= 0x41 && c <= 0x46) ||
               (c >= 0x61 && c <= 0x66);
    }

    constexpr static bool is_c0(char c) {
        return c >= 0x00 && c <= 0x1f;
        //return std::find(begin(c0_chars), end(c0_chars), c) != end(space_chars);
    }

    constexpr static bool is_c0_or_space(char c) {
        return is_c0(c) || c == 0x20;
    }

    constexpr static bool is_ctrl(char c){
        return is_c0(c) || (c >= 0x7f); //&& c <= 0x9f); // Codepoints > 0x7f don't fit in a char. 
                                                         // Use unsigned char?
    }

    constexpr static bool is_whitespace(char c) {
        return std::find(begin(space_chars), end(space_chars), c) != end(space_chars);
    }

    //static constexpr auto c0_chars = std::array{ '\x00', '\x01', '\x02', '\x03', '\x04', '\x05', '\x06', '\x07', '\x08',
    //                                             '\x09', '\x0a', '\x0b', '\x0c', '\x0d', '\x0e', '\x0f', '\x10', '\x11',
    //                                             '\x12', '\x13', '\x14', '\x15', '\x16', '\x17', '\x18', '\x19', '\x1a',
    //                                             '\x1b', '\x1c', '\x1d', '\x1e', '\x1f' };


private:
    static constexpr auto space_chars = std::array{' ', '\f', '\n', '\r', '\t', '\v'};

    std::string input_;
    std::size_t pos_{0};
};

} // namespace util

#endif
