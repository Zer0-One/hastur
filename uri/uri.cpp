// SPDX-FileCopyrightText: 2021 David Zero <zero-one@zer0-one.net>
//
// SPDX-License-Identifier: BSD-2-Clause

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>

#include "uri.h"

namespace uri {

using namespace std::string_literals;

std::string URLParser::percent_decode(){
    std::string output = "";

    while(!is_eof()){
        if(peek() == '%'){
            if(is_hexdigit(peek(3)[1]) && is_hexdigit(peek(3)[2])){
                advance(1);
                output += std::stoi(std::string(peek(2)));
                advance(2);

                continue;
            }
        }

        output += consume_char();
    }

    reset();

    return output;
}

std::optional<URL> URLParser::parse_basic_url(std::optional<std::string> base,
                                              std::optional<std::string> encoding,
                                              std::optional<URL> url,
                                              std::optional<std::string> state_override){

    if(!url.has_value()){
        //URL new_url;
        //url.emplace(new_url); <-- Doesn't work, no fucking clue

        std::string url_copy = peek(0);

        // Trim leading C0 control or space
        trim_leading(is_c0_or_space);
        //for(auto i = url_copy.begin(); i != url_copy.end(); i++){
        //    if(is_c0(*i) || *i == 0x20){
        //        //validation error

        //        url_copy.erase(i);

        //        continue;
        //    }
        //    
        //    break;
        //}

        // Trim trailing C0 control or space
        trim_trailing(is_c0_or_space);
        //for(auto i = url_copy.rbegin(); i != url_copy.rend(); i++){
        //    if(is_c0(*i) || *i == 0x20){
        //        //validation error

        //        url_copy.erase(i);

        //        continue;
        //    }
        //    
        //    break;
        //}

        reset(url_copy);
    }

    std::string url_copy = peek(0);

    // Remove ASCII tab or newline from input
    for(auto i = url_copy.begin(); i != url_copy.end(); i++){
        if(*i == '\t' || *i == '\n' || *i == '\r'){
            //validation error
            url_copy.erase(i);
        }
    }

    reset(url_copy);

    //std::string state("scheme_start_state");
    //std::string buffer("");

    //bool atSignSeen = false;
    //bool insideBrackets = false;
    //bool passwordTokenSeen = false;

    if(!scheme_start_state()){
        return std::nullopt;
    }

    auto foo = base;
    auto bar = encoding;
    auto baz = state_override;

    return std::nullopt;
}

bool URLParser::scheme_start_state(){
    return true;
}

bool URLParser::blob_resolve(){
    return true;
}

std::optional<URL> URLParser::parse(std::optional<std::string> base, std::optional<std::string> encoding){
    std::optional<URL> url = parse_basic_url(base, encoding, std::nullopt, std::nullopt);

    if(!url.has_value()){
        return std::nullopt;
    }

    if(url.value().scheme != "blob"s){
        //return url;
    }

    if(!blob_resolve()){
      return std::nullopt;
    }

    return url;
}

//std::optional<URL> parse(std::string_view url){
//    URLParser p(url);
//    std::string url_pd =  p.percent_decode(url);
//
//    //To-Do(zero-one): import icu and perform IDNA/punycode steps
//
//    
//}

//std::string_view URL::url(){
//    return std::string_view(url_);
//}
//
//std::string_view URL::scheme(){
//    return std::string_view(scheme_);
//}
//
//std::string_view URL::user(){
//    return std::string_view(user_);
//}
//
//std::string_view URL::passwd(){
//    return std::string_view(passwd_);
//}
//
//std::string_view URL::host(){
//    return std::string_view(host_);
//}
//
//std::string_view URL::port(){
//    return std::string_view(port_);
//}
//
//std::string_view URL::path(){
//    return std::string_view(path_);
//}
//
//std::string_view URL::query(){
//    return std::string_view(query_);
//}
//
//std::string_view URL::fragment(){
//    return std::string_view(fragment_);
//}

}
