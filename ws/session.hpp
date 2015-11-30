#ifndef WS_SESSION_HPP
#define WS_SESSION_HPP

#include <iostream>
#include <memory>
#include <regex>
#include <unordered_map>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

namespace ws {

class session : public std::enable_shared_from_this<session> {
public:
    session(tcp::socket socket) :
        socket_(std::move(socket)) { }
    virtual ~session() { }
    void start() {
        read_handshake();
    }

protected:
    virtual void on_open() = 0;
    virtual void on_msg() = 0;
    virtual void on_close() = 0;
    std::unordered_map<std::string, std::string> headers_;

private:
    tcp::socket socket_;
    boost::asio::streambuf in_buffer_;
    boost::asio::streambuf out_buffer_;

    void generate_accept(std::string &key) const {
        const std::string GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    }

    void read_handshake() {
        auto self(shared_from_this());
        boost::asio::async_read_until(socket_, in_buffer_, "\r\n\r\n",
            [this, self](const boost::system::error_code &ec, std::size_t)
        {
            if (!ec) {
                process_handshake();
            }
        });
    }

    /* Successfully received request, process it */
    void process_handshake() {
        std::istream request(&in_buffer_);
        std::ostream response(&out_buffer_);

        /* Parse HTTP header key-value pairs */
        std::string header;
        while (std::getline(request, header) && header != "\r") {
            std::regex expr("(.*): (.*)");
            auto it = std::sregex_iterator(header.begin(), header.end(), expr);
            if (it != std::sregex_iterator())
                headers_[it->str(1)] = it->str(2);
        }

        /* Extract the Sec-WebSocket-Key from the header if it exists */
        auto it = headers_.find("Sec-WebSocket-Key");
        if (it == headers_.end()) {
            throw 400;
        }
        std::string key = it->second;
        generate_accept(key);

        response << "HTTP/1.1 101 Switching Protocols\r\n"
            << "Upgrade: websocket\r\n"
            << "Connection: Upgrade\r\n"
            << "Sec-WebSocket-Accept: " << key << "\r\n"
            << "\r\n";

        write_handshake(false);
    }

    void write_handshake(bool error) {
        auto self(shared_from_this());
        boost::asio::async_write(socket_, out_buffer_,
            [this, self, error](const boost::system::error_code &ec, std::size_t)
        {
            if (!ec) {
                if (!error) {
                    on_open();
                }
            }
        });
    }
};

} /* namespace ws */

#endif /* WS_SESSION_HPP */
