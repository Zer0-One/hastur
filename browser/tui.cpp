// SPDX-FileCopyrightText: 2021 Robin Lindén <dev@robinlinden.eu>
//
// SPDX-License-Identifier: BSD-2-Clause

#include "tui/tui.h"
#include "browser/engine.h"
#include "dom/dom.h"

#include <spdlog/cfg/env.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <iostream>

namespace {
char const *const kDefaultUri = "http://www.example.com";
} // namespace

int main(int argc, char **argv) {
    spdlog::set_default_logger(spdlog::stderr_color_mt("hastur"));
    spdlog::cfg::load_env_levels();

    //auto url = argc > 1 ? uri::URL::parse(argv[1]) : uri::URL::parse(kDefaultUri);
    std::optional<uri::URL> url;
    if(argc > 1){
        uri::URLParser parser(argv[1]);
        url = parser.parse();
    }
    else{
        uri::URLParser parser(kDefaultUri);
        url = parser.parse();
    }
    if (!url.has_value()) {
        spdlog::error("Unable to parse uri from {}", argc > 1 ? argv[1] : kDefaultUri);
        return 1;
    }

    browser::Engine engine;
    if (auto err = engine.navigate(url.value()); err != protocol::Error::Ok) {
        spdlog::error("Got error {} from {}", static_cast<int>(err), url.value().url);
        std::exit(1);
    }

    std::cout << dom::to_string(engine.dom());
    spdlog::info("Building TUI");
    std::cout << tui::render(engine.layout()) << '\n';
    spdlog::info("Done");
}
