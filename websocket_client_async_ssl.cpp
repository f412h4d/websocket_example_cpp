#include "libs/common/root_certificates.hpp"
#include "libs/common/json.hpp"
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

using tcp = boost::asio::ip::tcp;
namespace ssl = boost::asio::ssl;
namespace websocket = boost::beast::websocket;

class session : public std::enable_shared_from_this<session>
{
    tcp::resolver resolver_;
    websocket::stream<ssl::stream<tcp::socket>> ws_;
    boost::beast::multi_buffer buffer_;
    std::string host_;
    std::string api_key_;
    std::string secret_key_;
    std::string listen_key_;

public:
    explicit session(boost::asio::io_context& ioc, ssl::context& ctx)
        : resolver_(ioc), ws_(ioc, ctx) {}

    void run(const std::string& host, const std::string& port)
    {
        host_ = host;
        resolver_.async_resolve(
            host,
            port,
            std::bind(
                &session::on_resolve,
                shared_from_this(),
                std::placeholders::_1,
                std::placeholders::_2));
    }

    void on_resolve(boost::system::error_code ec, const tcp::resolver::results_type& results)
    {
        if (ec)
            return fail(ec, "resolve");

        boost::asio::async_connect(
            ws_.next_layer().next_layer(),
            results.begin(),
            results.end(),
            std::bind(
                &session::on_connect,
                shared_from_this(),
                std::placeholders::_1));
    }

    void on_connect(boost::system::error_code ec)
    {
        if (ec)
            return fail(ec, "connect");

        ws_.next_layer().async_handshake(
            ssl::stream_base::client,
            std::bind(
                &session::on_ssl_handshake,
                shared_from_this(),
                std::placeholders::_1));
    }

    void on_ssl_handshake(boost::system::error_code ec)
    {
        if (ec)
            return fail(ec, "ssl_handshake");

        ws_.async_handshake(host_, "/ws",
            std::bind(
                &session::on_handshake,
                shared_from_this(),
                std::placeholders::_1));
    }

    void on_handshake(boost::system::error_code ec)
    {
        if (ec)
            return fail(ec, "handshake");

        send_subscribe_message();
    }

    void send_subscribe_message()
    {
        nlohmann::json j;
        j["id"] = "9d32157c-a556-4d27-9866-66760a174b57";
        j["method"] = "SUBSCRIBE";
        j["params"] = {"btcusdt@ticker"};

        ws_.async_write(
            boost::asio::buffer(j.dump()),
            std::bind(
                &session::on_write,
                shared_from_this(),
                std::placeholders::_1,
                std::placeholders::_2));
    }

    void on_write(boost::system::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return fail(ec, "write");

        ws_.async_read(
            buffer_,
            std::bind(
                &session::on_read,
                shared_from_this(),
                std::placeholders::_1,
                std::placeholders::_2));
    }

    void on_read(boost::system::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return fail(ec, "read");

        std::string message = boost::beast::buffers_to_string(buffer_.data());
        std::cout << "Received: " << message << std::endl;

        buffer_.consume(buffer_.size());

        ws_.async_read(
            buffer_,
            std::bind(
                &session::on_read,
                shared_from_this(),
                std::placeholders::_1,
                std::placeholders::_2));
    }

    static void fail(boost::system::error_code ec, char const* what)
    {
        std::cerr << what << ": " << ec.message() << "\n";
    }
};

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cerr <<
            "Usage: websocket-client-async-ssl <host> <port>\n" <<
            "Example:\n" <<
            "    websocket-client-async-ssl fstream.binance.com 443\n";
        return EXIT_FAILURE;
    }
    auto const host = argv[1];
    auto const port = argv[2];

    boost::asio::io_context ioc;
    ssl::context ctx{ssl::context::sslv23_client};

    load_root_certificates(ctx);

    std::make_shared<session>(ioc, ctx)->run(host, port);

    ioc.run();

    return EXIT_SUCCESS;
}

