// SPDX-FileCopyrightText: 2021-2022 Mikael Larsson <c.mikael.larsson@gmail.com>
// SPDX-FileCopyrightText: 2023-2024 Robin Lindén <dev@robinlinden.eu>
//
// SPDX-License-Identifier: BSD-2-Clause

#include "protocol/http.h"

#include "etest/etest2.h"
#include "net/test/fake_socket.h"
#include "protocol/response.h"
#include "uri/uri.h"

#include <cassert>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

using namespace std::string_view_literals;
using net::FakeSocket;

namespace {

uri::Uri create_uri(std::string url = "http://example.com") {
    auto parsed = uri::Uri::parse(std::move(url));
    assert(parsed.has_value());
    return std::move(parsed).value();
}

FakeSocket create_chunked_socket(std::string const &body) {
    FakeSocket socket;
    socket.read_data =
            "HTTP/1.1 200 OK\r\n"
            "Transfer-Encoding: chunked\r\n\r\n"
            + body;
    return socket;
}

} // namespace

int main() {
    etest::Suite s{};

    s.add_test("200 response", [](etest::IActions &a) {
        FakeSocket socket;
        socket.read_data =
                "HTTP/1.1 200 OK\r\n"
                "Content-Encoding: gzip\r\n"
                "Accept-Ranges: bytes\r\n"
                "Age: 367849\r\n"
                "Cache-Control: max-age=604800\r\n"
                "Content-Type: text/html; charset=UTF-8\r\n"
                "Date: Mon, 25 Oct 2021 19:48:04 GMT\r\n"
                "Etag: \"3147526947\"\r\n"
                "Expires: Mon, 01 Nov 2021 19:48:04 GMT\r\n"
                "Last-Modified: Thu, 17 Oct 2019 07:18:26 GMT\r\n"
                "Server: ECS (nyb/1D2A)\r\n"
                "Vary: Accept-Encoding\r\n"
                "X-Cache: HIT\r\n"
                "Content-Length: 123\r\n"
                "\r\n"
                "<!doctype html>\n"
                "<html>\n"
                "<head>\n"
                "<title>Example Domain</title>\n"
                "</head>\n"
                "</html>\n";

        auto response = protocol::Http::get(socket, create_uri(), std::nullopt).value();

        a.require(response.headers.size() == 13);
        a.expect_eq(socket.host, "example.com");
        a.expect_eq(socket.service, "http");
        a.expect_eq(response.status_line.version, "HTTP/1.1");
        a.expect_eq(response.status_line.status_code, 200);
        a.expect_eq(response.status_line.reason, "OK");
        a.expect_eq(response.headers.get("Content-Encoding"sv).value(), "gzip");
        a.expect_eq(response.headers.get("Accept-Ranges"sv).value(), "bytes");
        a.expect_eq(response.headers.get("Age"sv).value(), "367849");
        a.expect_eq(response.headers.get("Cache-Control"sv).value(), "max-age=604800");
        a.expect_eq(response.headers.get("Content-Type"sv).value(), "text/html; charset=UTF-8");
        a.expect_eq(response.headers.get("Date"sv).value(), "Mon, 25 Oct 2021 19:48:04 GMT");
        a.expect_eq(response.headers.get("Etag"sv).value(), R"("3147526947")");
        a.expect_eq(response.headers.get("Expires"sv).value(), "Mon, 01 Nov 2021 19:48:04 GMT");
        a.expect_eq(response.headers.get("Last-Modified"sv).value(), "Thu, 17 Oct 2019 07:18:26 GMT");
        a.expect_eq(response.headers.get("Server"sv).value(), "ECS (nyb/1D2A)");
        a.expect_eq(response.headers.get("Vary"sv).value(), "Accept-Encoding");
        a.expect_eq(response.headers.get("X-Cache"sv).value(), "HIT");
        a.expect_eq(response.headers.get("Content-Length"sv).value(), "123");
        a.expect_eq(response.body,
                "<!doctype html>\n"
                "<html>\n"
                "<head>\n"
                "<title>Example Domain</title>\n"
                "</head>\n"
                "</html>\n");
    });

    s.add_test("google 301", [](etest::IActions &a) {
        FakeSocket socket;
        socket.read_data =
                "HTTP/1.1 301 Moved Permanently\r\n"
                "Location: http://www.google.com/\r\n"
                "Content-Type: text/html; charset=UTF-8\r\n"
                "Date: Sun, 26 Apr 2009 11:11:49 GMT\r\n"
                "Expires: Tue, 26 May 2009 11:11:49 GMT\r\n"
                "Cache-Control: public, max-age=2592000\r\n"
                "Server: gws\r\n"
                "Content-Length: 219\r\n"
                "\r\n"
                "<HTML><HEAD><meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\">\n"
                "<TITLE>301 Moved</TITLE></HEAD><BODY>\n"
                "<H1>301 Moved</H1>\n"
                "The document has moved\n"
                "<A HREF=\"http://www.google.com/\">here</A>.\r\n"
                "</BODY></HTML>\r\n";

        auto response = protocol::Http::get(socket, create_uri("http://google.com"), std::nullopt).value();

        a.require(response.headers.size() == 7);
        a.expect_eq(socket.host, "google.com");
        a.expect_eq(socket.service, "http");
        a.expect_eq(response.status_line.version, "HTTP/1.1");
        a.expect_eq(response.status_line.status_code, 301);
        a.expect_eq(response.status_line.reason, "Moved Permanently");
    });

    s.add_test("transfer-encoding chunked, real body", [](etest::IActions &a) {
        auto socket = create_chunked_socket(
                "7f\r\n"
                "<!DOCTYPE html>\r\n"
                "<html lang=en>\r\n"
                "<head>\r\n"
                "<meta charset='utf-8'>\r\n"
                "<title>Chunked transfer encoding test</title>\r\n"
                "</head>\r\n"
                "<body>\r\n"
                "27\r\n"
                "<h1>Chunked transfer encoding test</h1>\r\n"
                "31\r\n"
                "<h5>This is a chunked response after 100 ms.</h5>\r\n"
                "82\r\n"
                "<h5>This is a chunked response after 1 second. The server should not close the stream before all "
                "chunks are sent to a client.</h5>\r\n"
                "e\r\n"
                "</body></html>\r\n"
                "0\r\n"
                "\r\n");

        auto response = protocol::Http::get(socket, create_uri(), std::nullopt).value();

        a.expect_eq(response.body,
                "<!DOCTYPE html>\r\n"
                "<html lang=en>\r\n"
                "<head>\r\n"
                "<meta charset='utf-8'>\r\n"
                "<title>Chunked transfer encoding test</title>\r\n"
                "</head>\r\n"
                "<body><h1>Chunked transfer encoding test</h1><h5>This is a chunked response after 100 ms.</h5>"
                "<h5>This is a chunked response after 1 second. The server should not close the stream before all "
                "chunks are sent to a client.</h5></body></html>"sv);
    });

    s.add_test("transfer-encoding chunked, space before size", [](etest::IActions &a) {
        auto socket = create_chunked_socket(
                "  5\r\nhello\r\n"
                " 0\r\n\r\n");

        auto response = protocol::Http::get(socket, create_uri(), std::nullopt).value();

        a.expect_eq(response.body, "hello");
    });

    s.add_test("transfer-encoding chunked, space after size", [](etest::IActions &a) {
        auto socket = create_chunked_socket(
                "5  \r\nhello\r\n"
                "0  \r\n\r\n");

        auto response = protocol::Http::get(socket, create_uri(), std::nullopt).value();

        a.expect_eq(response.body, "hello");
    });

    s.add_test("transfer-encoding chunked, invalid size", [](etest::IActions &a) {
        auto socket = create_chunked_socket(
                "8684838388283847263674\r\nhello\r\n"
                "0\r\n\r\n");

        auto response = protocol::Http::get(socket, create_uri(), std::nullopt).error();

        a.expect_eq(response.err, protocol::ErrorCode::InvalidResponse);
    });

    s.add_test("transfer-encoding chunked, no separator between chunk", [](etest::IActions &a) {
        auto socket = create_chunked_socket(
                "5\r\nhello"
                "0\r\n\r\n");

        auto response = protocol::Http::get(socket, create_uri(), std::nullopt).error();

        a.expect_eq(response.err, protocol::ErrorCode::InvalidResponse);
    });

    s.add_test("transfer-encoding chunked, chunk too short", [](etest::IActions &a) {
        auto socket = create_chunked_socket(
                "6\r\nhello\r\n"
                "0\r\n\r\n");

        auto response = protocol::Http::get(socket, create_uri(), std::nullopt).error();

        a.expect_eq(response.err, protocol::ErrorCode::InvalidResponse);
    });

    s.add_test("transfer-encoding chunked, chunk too long", [](etest::IActions &a) {
        auto socket = create_chunked_socket(
                "3\r\nhello\r\n"
                "0\r\n\r\n");

        auto response = protocol::Http::get(socket, create_uri(), std::nullopt).error();

        a.expect_eq(response.err, protocol::ErrorCode::InvalidResponse);
    });

    s.add_test("404 no headers no body", [](etest::IActions &a) {
        FakeSocket socket;
        socket.read_data = "HTTP/1.1 404 Not Found\r\n\r\n";
        auto response = protocol::Http::get(socket, create_uri(), std::nullopt).error();

        a.expect_eq(response.status_line->version, "HTTP/1.1"sv);
        a.expect_eq(response.status_line->status_code, 404);
        a.expect_eq(response.status_line->reason, "Not Found");
    });

    s.add_test("connect failure", [](etest::IActions &a) {
        FakeSocket socket{.connect_result = false};
        auto response = protocol::Http::get(socket, create_uri(), std::nullopt).error();
        a.expect_eq(response, protocol::Error{.err = protocol::ErrorCode::Unresolved});
    });

    s.add_test("empty response", [](etest::IActions &a) {
        FakeSocket socket{};
        auto response = protocol::Http::get(socket, create_uri(), std::nullopt).error();
        a.expect_eq(response, protocol::Error{.err = protocol::ErrorCode::InvalidResponse});
    });

    s.add_test("empty status line", [](etest::IActions &a) {
        FakeSocket socket{.read_data = "\r\n"};
        auto response = protocol::Http::get(socket, create_uri(), std::nullopt).error();
        a.expect_eq(response, protocol::Error{.err = protocol::ErrorCode::InvalidResponse});
    });

    s.add_test("no headers", [](etest::IActions &a) {
        FakeSocket socket{.read_data = "HTTP/1.1 200 OK\r\n \r\n\r\n"};
        auto response = protocol::Http::get(socket, create_uri(), std::nullopt).error();
        a.expect_eq(response,
                protocol::Error{protocol::ErrorCode::InvalidResponse, protocol::StatusLine{"HTTP/1.1", 200, "OK"}});
    });

    s.add_test("mixed valid and invalid headers", [](etest::IActions &a) {
        FakeSocket socket{.read_data = "HTTP/1.1 200 OK\r\none: 1\r\nBAD\r\ntwo:2 \r\n\r\n"};
        auto response = protocol::Http::get(socket, create_uri(), std::nullopt);
        a.expect_eq(response,
                protocol::Response{
                        .status_line{"HTTP/1.1", 200, "OK"},
                        .headers{{"one", "1"}, {"two", "2"}},
                });
    });

    s.add_test("query parameters are included", [](etest::IActions &a) {
        FakeSocket socket{};
        auto uri = uri::Uri{
                .uri = {"http://example.com/hello?target=world"},
                .scheme = "http",
                .authority = {.host = "example.com"},
                .path = "/hello",
                .query = "target=world",
        };
        std::ignore = protocol::Http::get(socket, uri, std::nullopt);
        auto first_request_line = socket.write_data.substr(0, socket.write_data.find("\r\n"));
        a.expect_eq(first_request_line, "GET /hello?target=world HTTP/1.1");
    });

    s.add_test("port is removed for standard ports", [](etest::IActions &a) {
        FakeSocket socket{};
        std::ignore = protocol::Http::get(socket, create_uri("http://example.com:80"), std::nullopt);
        a.expect(socket.write_data.find("Host: example.com\r\n") != std::string::npos);

        socket = FakeSocket{};
        std::ignore = protocol::Http::get(socket, create_uri("http://example.com:79"), std::nullopt);
        a.expect(socket.write_data.find("Host: example.com\r\n") == std::string::npos);
        a.expect(socket.write_data.find("Host: example.com:79\r\n") != std::string::npos);

        socket = FakeSocket{};
        std::ignore = protocol::Http::get(socket, create_uri("http://example.com:443"), std::nullopt);
        a.expect(socket.write_data.find("Host: example.com\r\n") == std::string::npos);
        a.expect(socket.write_data.find("Host: example.com:443\r\n") != std::string::npos);

        socket = FakeSocket{};
        std::ignore = protocol::Http::get(socket, create_uri("https://example.com"), std::nullopt);
        a.expect(socket.write_data.find("Host: example.com\r\n") != std::string::npos);
        a.expect(socket.write_data.find("Host: example.com:443\r\n") == std::string::npos);

        socket = FakeSocket{};
        std::ignore = protocol::Http::get(socket, create_uri("https://example.com:443"), std::nullopt);
        a.expect(socket.write_data.find("Host: example.com\r\n") != std::string::npos);
        a.expect(socket.write_data.find("Host: example.com:443\r\n") == std::string::npos);

        socket = FakeSocket{};
        std::ignore = protocol::Http::get(socket, create_uri("https://example.com:80"), std::nullopt);
        a.expect(socket.write_data.find("Host: example.com\r\n") == std::string::npos);
        a.expect(socket.write_data.find("Host: example.com:80\r\n") != std::string::npos);
    });

    s.add_test("unknown schemes don't have their ports dropped", [](etest::IActions &a) {
        FakeSocket socket{};
        std::ignore = protocol::Http::get(socket, create_uri("ftp://example.com:80"), std::nullopt);
        a.expect(socket.write_data.find("Host: example.com:80\r\n") != std::string::npos);
    });

    s.add_test("user agent is included", [](etest::IActions &a) {
        FakeSocket socket{};
        std::ignore = protocol::Http::get(socket, create_uri(), "test-agent");
        a.expect(socket.write_data.find("User-Agent: test-agent\r\n") != std::string::npos);

        socket = FakeSocket{};
        std::ignore = protocol::Http::get(socket, create_uri(), std::nullopt);
        a.expect(socket.write_data.find("User-Agent: test-agent\r\n") == std::string::npos);
    });

    s.add_test("truncated status line", [](etest::IActions &a) {
        FakeSocket socket{.read_data = "HTTP/1.1 200\r\n"};
        auto response = protocol::Http::get(socket, create_uri(), std::nullopt).error();
        a.expect_eq(response.err, protocol::ErrorCode::InvalidResponse);
    });

    s.add_test("status line w/ invalid status code", [](etest::IActions &a) {
        FakeSocket socket{.read_data = "HTTP/1.1 asdf OK\r\n"};
        auto response = protocol::Http::get(socket, create_uri(), std::nullopt).error();
        a.expect_eq(response.err, protocol::ErrorCode::InvalidResponse);
    });

    return s.run();
}
