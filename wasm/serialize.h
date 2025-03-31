// SPDX-FileCopyrightText: 2024 David Zero <zero-one@zer0-one.net>
// SPDX-FileCopyrightText: 2024-2025 Robin Lindén <dev@robinlinden.eu>
//
// SPDX-License-Identifier: BSD-2-Clause

#ifndef WASM_SERIALIZE_H_
#define WASM_SERIALIZE_H_

#include "wasm/instructions.h"
#include "wasm/types.h"

#include <cassert>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>

namespace wasm {

constexpr std::string_view to_string(ValueType vt) {
    switch (vt) {
        case ValueType::Int32:
            return "i32";
        case ValueType::Int64:
            return "i64";
        case ValueType::Float32:
            return "f32";
        case ValueType::Float64:
            return "f64";
        case ValueType::Vector128:
            return "v128";
        case ValueType::FunctionReference:
            return "funcref";
        case ValueType::ExternReference:
            return "externref";
    }

    return "UNHANDLED_VALUE_TYPE";
}

} // namespace wasm

namespace wasm::instructions {

constexpr std::string to_string(BlockType const &bt) {
    std::string out;

    if (auto const *v = std::get_if<ValueType>(&bt.value)) {
        out += "(result ";
        out += wasm::to_string(*v);
        out += ")";
    } else if (auto const *t = std::get_if<TypeIdx>(&bt.value)) {
        out += "(type ";
        out += std::to_string(*t);
        out += ")";
    } else {
        assert(std::holds_alternative<BlockType::Empty>(bt.value));
    }

    return out;
}

constexpr std::string to_string(MemArg const &ma, std::optional<std::uint32_t> natural_alignment = std::nullopt) {
    std::string out;

    // If offset == 0, omit offset
    if (ma.offset != 0) {
        out += "offset=";
        out += std::to_string(ma.offset);
    }

    // Natural alignment, omit "align=" phrase
    if (natural_alignment == ma.align) {
        return out;
    }

    if (ma.offset != 0) {
        out += " ";
    }

    out += "align=";
    out += std::to_string(ma.align);

    return out;
}

std::string to_string(Instruction const &);
std::string to_string(std::span<Instruction const>);

} // namespace wasm::instructions

#endif
