// SPDX-FileCopyrightText: 2021 David Zero <zero-one@zer0-one.net>
//
// SPDX-License-Identifier: BSD-2-Clause

#include "uri/uri.h"

#include <spdlog/spdlog.h>

#include <regex>
#include <utility>

using namespace std::string_literals;

namespace uri {

std::optional<Uri> Uri::parse(std::string uristr) {
    std::smatch match;

    // Regex taken from RFC 3986.
    std::regex uri_regex("^(([^:/?#]+):)?(//([^/?#]*))?([^?#]*)(\\?([^#]*))?(#(.*))?");
    if (!std::regex_search(uristr, match, uri_regex)) {
        return std::nullopt;
    }

    Authority authority{};

    std::string hostport = match.str(4);
    if (auto userinfo_end = match.str(4).find_first_of('@'); userinfo_end != std::string::npos) {
        // Userinfo present.
        std::string userinfo(match.str(4).substr(0, userinfo_end));
        hostport = match.str(4).substr(userinfo_end + 1, match.str(4).size() - userinfo_end);

        if (auto user_end = userinfo.find_first_of(':'); user_end != std::string::npos) {
            // Password present.
            authority.user = userinfo.substr(0, user_end);
            authority.passwd = userinfo.substr(user_end + 1, userinfo.size() - user_end);
        } else {
            // Password not present.
            authority.user = userinfo;
        }
    }

    if (auto host_end = hostport.find_first_of(':'); host_end != std::string::npos) {
        // Port present.
        authority.host = hostport.substr(0, host_end);
        authority.port = hostport.substr(host_end + 1, hostport.size() - host_end);
    } else {
        // Port not present.
        authority.host = hostport;
    }

    auto uri = Uri{
            .scheme{match.str(2)},
            .authority{std::move(authority)},
            .path{match.str(5)},
            .query{match.str(7)},
            .fragment{match.str(9)},
    };
    uri.uri = std::move(uristr);
    return uri;
}

ParserState state_scheme_start(){ return FAILURE; }
ParserState state_scheme(){ return FAILURE; }
ParserState state_no_scheme(){ return FAILURE; }
ParserState state_special_relative_or_authority(){ return FAILURE; }
ParserState state_path_or_authority(){ return FAILURE; }
ParserState state_relative(){ return FAILURE; }
ParserState state_relative_slash(){ return FAILURE; }
ParserState state_special_authority_slashes(){ return FAILURE; }
ParserState state_special_authority_ignore_slashes(){ return FAILURE; }
ParserState state_authority(){ return FAILURE; }
ParserState state_host(){ return FAILURE; }
ParserState state_hostname(){ return FAILURE; }
ParserState state_port(){ return FAILURE; }
ParserState state_file(){ return FAILURE; }
ParserState state_file_slash(){ return FAILURE; }
ParserState state_file_host(){ return FAILURE; }
ParserState state_path_start(){ return FAILURE; }
ParserState state_path(){ return FAILURE; }
ParserState state_opaque_path(){ return FAILURE; }
ParserState state_query(){ return FAILURE; }
ParserState state_fragment(){ return FAILURE; }

bool URL::parse_basic_url(std::optional<std::string_view> base,
                     std::optional<std::string_view> encoding,
                     std::optional<URL> url,
                     std::optional<ParserState> state_override){

    bool validation_err = false;

    /**
     * If input contains any leading or trailing C0 control or space, validation error.
     * Remove any leading and trailing C0 control or space from input.
     */
    for(size_t i = 0; i != input.length(); i++){
        if(is_c0_or_space(input[i])){
            spdlog::info("Validation error while parsing URL '{}'; contains leading C0 control or space characters", input);

            validation_err = true;

            continue;
        }

        if(validation_err){
            input.erase(0, i);
        }

        break;
    }

    spdlog::info("Trimmed input: {}", input);
    validation_err = false;

    for(size_t i = 0; i != input.length(); i++){
        if(is_c0_or_space(input[input.length() - 1 - i])){
            spdlog::info("Validation error while parsing URL '{}'; contains trailing C0 control or space characters", input);

            validation_err = true;

            continue;
        }

        if(validation_err){
            // this may look wrong, but if validation_err, i > 0
            input.erase(input.length() - i, i);
        }

        break;
    }

    spdlog::info("Trimmed input: {}", input);
    validation_err = false;

    /** 
     * If input contains any ASCII tab or newline, validation error.
     * Remove all ASCII tab or newline from input.
     */
    for(auto i = input.begin(); i != input.end(); i++){
        if(is_tab_or_newline(*i)){
            validation_err = true;
            input.erase(i);
        }
    }

    if(validation_err){
        spdlog::info("Validation error while parsing URL '{}'; contains ASCII tab or newline characters", input);
        validation_err = false;
    }

    /**
     * Let state be state override if given, or scheme start state otherwise. 
     */
    ParserState state = SCHEME_START;

    if(state_override.has_value()){
        state = state_override.value();
    }

    /**
     * Set encoding to the result of getting an output encoding from encoding.
     */
    //Encoding enc = get_output_encoding(encoding);

    /**
     * Let buffer be the empty string.
     */
    std::string buffer = "";

    /**
     * Let atSignSeen, insideBrackets, and passwordTokenSeen be false.
     */
    bool atSignSeen = false;
    bool insideBrackets = false;
    bool passwordTokenSeen = false;

    // Shut up the -Wunused-variable from the compiler while testing
    (void)base;
    (void)encoding;
    (void)state_override;
    (void)url;
    (void)state;
    (void)atSignSeen;
    (void)insideBrackets;
    (void)passwordTokenSeen;

    while(state != FAILURE){
        switch(state){
            case SCHEME_START:
                state = state_scheme_start();
                break;
            case SCHEME:
                state = state_scheme();
                break;
            case NO_SCHEME:
                state = state_no_scheme();
                break;
            case SPECIAL_RELATIVE_OR_AUTHORITY:
                state = state_special_relative_or_authority();
                break;
            case PATH_OR_AUTHORITY:
                state = state_path_or_authority();
                break;
            case RELATIVE:
                state = state_relative();
                break;
            case RELATIVE_SLASH:
                state = state_relative_slash();
                break;
            case SPECIAL_AUTHORITY_SLASHES:
                state = state_special_authority_slashes();
                break;
            case SPECIAL_AUTHORITY_IGNORE_SLASHES:
                state = state_special_authority_ignore_slashes();
                break;
            case AUTHORITY:
                state = state_authority();
                break;
            case HOST:
                state = state_host();
                break;
            case HOSTNAME:
                state = state_hostname();
                break;
            case PORT:
                state = state_port();
                break;
            case FILE:
                state = state_file();
                break;
            case FILE_SLASH:
                state = state_file_slash();
                break;
            case FILE_HOST:
                state = state_file_host();
                break;
            case PATH_START:
                state = state_path_start();
                break;
            case PATH:
                state = state_path();
                break;
            case OPAQUE_PATH:
                state = state_opaque_path();
                break;
            case QUERY:
                state = state_query();
                break;
            case FRAGMENT:
                state = state_fragment();
                break;
            case FAILURE:
                spdlog::warn("Failed to parse URL {}", input);
                return false;
        }
    }

    return true;
}

bool URL::parse(std::optional<std::string_view> base, std::optional<std::string_view> encoding){
    bool ret = parse_basic_url(base, encoding, std::nullopt, std::nullopt);

    if(!ret){
        return false;
    }

    if(scheme != "blob"s){
        return true;
    }

    // Resolve blob URL
    // blob_entry = blob_resolve(url);
    return true;
}

} // namespace uri
