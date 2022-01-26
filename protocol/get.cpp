// SPDX-FileCopyrightText: 2021 Robin Lindén <dev@robinlinden.eu>
// SPDX-FileCopyrightText: 2021 Mikael Larsson <c.mikael.larsson@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause

#include "protocol/get.h"

#include "net/socket.h"

#include <filesystem>
#include <fstream>
#include <string_view>

using namespace std::string_view_literals;

namespace protocol {

Response get(uri::URL const &url) {
    if (url.scheme == "http"sv) {
        return Http::get(net::Socket(), url);
    }

    if (url.scheme == "https"sv) {
        return Http::get(net::SecureSocket(), url);
    }

    if (url.scheme == "file"sv) {
        auto path = std::filesystem::path(url.path);
        if (!exists(path)) {
            return {Error::Unresolved};
        }

        if (!is_regular_file(path)) {
            return {Error::InvalidResponse};
        }

        auto file = std::ifstream(path, std::ios::in | std::ios::binary);
        auto size = file_size(path);
        auto content = std::string(size, '\0');
        file.read(content.data(), size);
        return {Error::Ok, {}, {}, std::move(content)};
    }

    return {Error::Unhandled};
}

} // namespace protocol
