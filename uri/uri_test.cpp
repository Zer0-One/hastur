// SPDX-FileCopyrightText: 2021 David Zero <zero-one@zer0-one.net>
//
// SPDX-License-Identifier: BSD-2-Clause

#include <string>

#include "uri.h"

#include "etest/etest.h"

using etest::expect;
using uri::URL;
using uri::URLParser;

int main() {
    etest::test("percent-decoding", [] {
        URLParser p("https://hastur.wtf/%28%2b%2B-%29");
        auto u = p.parse();
        if(u.has_value()){
            expect(u.value().url == "https://hastur.wtf/(++-)");
        }

        expect(2 + 2 == 5);
    });

    return etest::run_all_tests();
}
