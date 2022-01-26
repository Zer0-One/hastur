// SPDX-FileCopyrightText: 2021 David Zero <zero-one@zer0-one.net>
//
// SPDX-License-Identifier: BSD-2-Clause

#ifndef URI_H
#define URI_H

#include <optional>
#include <string>
#include <string_view>

#include "util/base_parser.h"

namespace uri {

struct EnvironmentSettings{
    std::string id;
    std::string curl;
    std::optional<std::string> tl_curl;
    std::optional<std::string> tl_origin;
    std::optional<std::string> browsing_context;
    std::optional<std::string> svc_worker;
    bool exec_ready;
};

struct MediaSource{
    std::string placeholder;
};

struct Blob{
    std::string placeholder;
};

struct BlobEntry{
    union{
        Blob blob;
        MediaSource ms;
    };

    EnvironmentSettings environment;
};

struct URL {
    std::string url;
    std::string scheme;
    std::string user;
    std::string passwd;
    std::string host;
    std::string port;
    std::string path;
    std::string query;
    std::string fragment;
    //std::optional<BlobEntry> blob_entry;
};

class URLParser final : util::BaseParser {
public:
    URLParser(std::string input) : BaseParser{input} {}
    std::optional<URL> parse(std::optional<std::string> base = std::nullopt, std::optional<std::string> encoding = std::nullopt);

private:
    std::optional<URL> parse_basic_url(std::optional<std::string> base,
                                       std::optional<std::string> encoding,
                                       std::optional<URL> url,
                                       std::optional<std::string> state_override);
    //basic parser states
    bool scheme_start_state();
    bool scheme_state();
    bool no_scheme_state();
    bool special_relative_or_authority_state();
    bool path_or_authority_state();
    bool relative_state();
    bool relative_slash_state();
    bool special_authority_slashes_state();
    bool special_authority_ignore_slashes_state();
    bool authority_state();
    bool host_state();
    bool port_state();
    bool file_state();
    bool file_slash_state();
    bool file_host_state();
    bool path_start_state();
    bool path_state();
    bool opaque_path_state();
    bool query_state();
    bool fragment_state();

    //misc
    bool blob_resolve();
    std::string percent_decode();
};

} //namespace uri

#endif
