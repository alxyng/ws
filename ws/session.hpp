#ifndef WS_SESSION_HPP
#define WS_SESSION_HPP

#include <array>
#include <iostream>
#include <memory>
#include <regex>
#include <unordered_map>
#include <boost/asio.hpp>
#include "message.hpp"

#include "base64encode.h"
#include "base64encode.c"
#include "sha1.h"
#include "sha1.c"

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
    std::unordered_map<std::string, std::string> headers_;

    virtual void on_open() = 0;
    virtual void on_msg(const ws::message &msg) = 0;
    virtual void on_close() = 0;

    /* Async write data out */
    void write(message::opcode opcode, const boost::asio::const_buffer &buffer) {
        auto self(shared_from_this());

        std::size_t payload_length = boost::asio::buffer_size(buffer);
        // TODO prepare header then write the header

        boost::asio::async_write(socket_, out_buffer_,
            boost::asio::transfer_exactly(0),
                [this, self, &buffer](const boost::system::error_code &ec, std::size_t)
        {
            if (!ec) {
                boost::asio::async_write(socket_, boost::asio::buffer(buffer),
                    [this, self](const boost::system::error_code &ec, std::size_t)
                {
                    if (!ec) {
                        //read();
                    }
                });

            }
        });

    };

    /* Close connection (initiates closing handshake) */
    void close() {
        //
    }

    const tcp::socket &get_socket() {
        return socket_;
    }

private:
    tcp::socket socket_;
    boost::asio::streambuf in_buffer_;
    boost::asio::streambuf out_buffer_;
    message msg_;

    void generate_accept(std::string &key) const {
        const std::string GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        key += GUID;

        /* SHA1 hash the key-GUID */
        std::array<unsigned char, 20> hash;
        SHA1((char *)hash.data(), key.data(), key.length());

        /* Base64 encode the SHA1 hash */
        char b64[32];
        std::ptrdiff_t diff = base64_encode(hash.data(), hash.size(), b64);
        std::cout << diff << std::endl;
        key = std::string(b64, diff);
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
                    // read();
                }
            }
        });
    }

    // void read() {
    //     auto self(shared_from_this());
    //     boost::asio::async_read(socket_, in_buffer_,
    //         boost::asio::transfer_exactly(2),
    //             [this, self](const boost::system::error_code &ec, std::size_t)
    //     {
    //         if (!ec) {
    //             // TODO: parse parse parse
    //             //message msg;
    //             //on_msg(msg);
    //         }
    //     });
    // }
};

} /* namespace ws */

#endif /* WS_SESSION_HPP */
