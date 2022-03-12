// SPDX-FileCopyrightText: 2021 David Zero <zero-one@zer0-one.net>
//
// SPDX-License-Identifier: BSD-2-Clause

#ifndef URI_URI_H_
#define URI_URI_H_

#include <optional>
#include <string>
#include <string_view>

#include "util/base_parser.h"

namespace uri {

struct Authority {
    std::string user;
    std::string passwd;
    std::string host;
    std::string port;

    [[nodiscard]] bool operator==(Authority const &) const = default;
};

struct Uri {
    static std::optional<Uri> parse(std::string uri);

    std::string uri;
    std::string scheme;
    Authority authority;
    std::string path;
    std::string query;
    std::string fragment;

    [[nodiscard]] bool operator==(Uri const &) const = default;
};

struct EnvironmentSettings {
    std::string id;
    std::string curl;
    std::optional<std::string> tl_curl;
    std::optional<std::string> tl_origin;
    std::optional<std::string> tl_browsing_context;
    std::optional<std::string> svc_worker;
    bool exec_ready;
};

struct MediaSource {
    std::string placeholder;
};

struct Blob {
    std::string placeholder;
};

struct BlobEntry {
    //union {
        Blob blob;
        MediaSource ms;
    //};

    EnvironmentSettings environment;
};

enum ParserState {
    SCHEME_START,
    SCHEME,
    NO_SCHEME,
    SPECIAL_RELATIVE_OR_AUTHORITY,
    PATH_OR_AUTHORITY,
    RELATIVE,
    RELATIVE_SLASH,
    SPECIAL_AUTHORITY_SLASHES,
    SPECIAL_AUTHORITY_IGNORE_SLASHES,
    AUTHORITY,
    HOST,
    HOSTNAME,
    PORT,
    FILE,
    FILE_SLASH,
    FILE_HOST,
    PATH_START,
    PATH,
    OPAQUE_PATH,
    QUERY,
    FRAGMENT,
    FAILURE
};

class URL final : util::BaseParser {
public:
    URL() : BaseParser{""} {};
    URL(std::optional<std::string_view> input, std::optional<std::string_view> base = std::nullopt) : BaseParser{input.value()} {
        if(input.has_value()){
            this->input = std::string(input.value());

            parse(base, std::nullopt);
        }
    }

    std::string url_full;
    std::string scheme;
    std::string user;
    std::string passwd;
    std::string host;
    std::string port;
    std::string path;
    std::string query;
    std::string fragment;
    std::optional<BlobEntry> blob_entry;

private:
    bool parse(std::optional<std::string_view> base, std::optional<std::string_view> encoding);
    bool parse_basic_url(std::optional<std::string_view> base,
                         std::optional<std::string_view> encoding,
                         std::optional<URL> url,
                         std::optional<ParserState> state_override);

    std::string input;
};

} // namespace uri

#endif
