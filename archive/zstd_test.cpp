// SPDX-FileCopyrightText: 2024 David Zero <zero-one@zer0-one.net>
//
// SPDX-License-Identifier: BSD-2-Clause

#include "archive/zstd.h"

#include "etest/etest2.h"

#include <tl/expected.hpp>

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

int main() {
    etest::Suite s{"zstd"};

    using namespace archive;

    s.add_test("trivial decode", [](etest::IActions &a) {
        constexpr auto kCompress = std::to_array<std::uint8_t>({0x28,
                0xb5,
                0x2f,
                0xfd,
                0x04,
                0x00,
                0xb1,
                0x00,
                0x00,
                0x54,
                0x68,
                0x69,
                0x73,
                0x20,
                0x69,
                0x73,
                0x20,
                0x61,
                0x20,
                0x74,
                0x65,
                0x73,
                0x74,
                0x20,
                0x73,
                0x74,
                0x72,
                0x69,
                0x6e,
                0x67,
                0x0a,
                0xd8,
                0x6a,
                0x8c,
                0x62});

        tl::expected<std::vector<std::uint8_t>, ZstdError> ret = zstd_decode(kCompress);

        a.expect(ret.has_value());
        a.expect_eq(std::string(ret->begin(), ret->end()), "This is a test string\n");
    });

    s.add_test("empty input", [](etest::IActions &a) {
        tl::expected<std::vector<std::uint8_t>, ZstdError> ret = zstd_decode({});

        a.expect(!ret.has_value());
        a.expect_eq(ret.error(), ZstdError::InputEmpty);
    });

    s.add_test("junk input", [](etest::IActions &a) {
        constexpr auto kCompress = std::to_array<std::uint8_t>({0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00});

        tl::expected<std::vector<std::uint8_t>, ZstdError> ret = zstd_decode(kCompress);

        a.expect(!ret.has_value());
        a.expect_eq(ret.error(), ZstdError::ZstdInternalError);
    });

    s.add_test("truncated zstd stream", [](etest::IActions &a) {
        constexpr auto kCompress = std::to_array<std::uint8_t>({0x28,
                0xb5,
                0x2f,
                0xfd,
                0x04,
                0x00,
                0xb1,
                0x00,
                0x00,
                0x54,
                0x68,
                0x69,
                0x73,
                0x20,
                0x69,
                0x73,
                0x20,
                0x61,
                0x20,
                0x74,
                0x65,
                0x73,
                0x74,
                0x20,
                0x73,
                0x74,
                0x72,
                0x69});

        tl::expected<std::vector<std::uint8_t>, ZstdError> ret = zstd_decode(kCompress);

        a.expect(!ret.has_value());
        a.expect_eq(ret.error(), ZstdError::DecodeEarlyTermination);
    });

    return s.run();
}
