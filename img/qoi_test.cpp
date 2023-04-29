// SPDX-FileCopyrightText: 2023 Robin Lindén <dev@robinlinden.eu>
//
// SPDX-License-Identifier: BSD-2-Clause

#include "img/qoi.h"

#include "etest/etest.h"

#include <tl/expected.hpp>

#include <sstream>
#include <string>

using etest::expect_eq;
using img::Qoi;
using img::QoiError;

using namespace std::literals;

int main() {
    etest::test("abrupt eof before magic", [] {
        expect_eq(Qoi::from(std::stringstream{"qoi"s}), tl::unexpected{QoiError::AbruptEof}); //
    });

    etest::test("invalid magic", [] {
        expect_eq(Qoi::from(std::stringstream{"qoib"s}), tl::unexpected{QoiError::InvalidMagic}); //
    });

    etest::test("abrupt eof before width", [] {
        expect_eq(Qoi::from(std::stringstream{"qoif\1\0\0"s}), tl::unexpected{QoiError::AbruptEof}); //
    });

    etest::test("abrupt eof before height", [] {
        expect_eq(Qoi::from(std::stringstream{"qoif\1\0\0\0\1\0\0"s}), tl::unexpected{QoiError::AbruptEof}); //
    });

    etest::test("channels error handling", [] {
        expect_eq(Qoi::from(std::stringstream{"qoif\1\0\0\0\1\0\0\0"s}), tl::unexpected{QoiError::AbruptEof});
        expect_eq(Qoi::from(std::stringstream{"qoif\1\0\0\0\1\0\0\0\5"s}), tl::unexpected{QoiError::InvalidChannels});
    });

    etest::test("colorspace error handling", [] {
        expect_eq(Qoi::from(std::stringstream{"qoif\1\0\0\0\1\0\0\0\3"s}), tl::unexpected{QoiError::AbruptEof});
        expect_eq(
                Qoi::from(std::stringstream{"qoif\1\0\0\0\1\0\0\0\3\2"s}), tl::unexpected{QoiError::InvalidColorspace});
    });

    etest::test("it works", [] {
        expect_eq(Qoi::from(std::stringstream{"qoif\0\0\0\1\0\0\0\2\3\1"s}), Qoi{.width = 1, .height = 2}); //
    });

    return etest::run_all_tests();
}
