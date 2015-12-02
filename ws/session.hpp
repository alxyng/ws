#ifndef WS_SESSION_HPP
#define WS_SESSION_HPP

#include <array>
#include <iostream>
#include <memory>
#include <regex>
#include <unordered_map>
#include <boost/asio.hpp>
#include "base64.hpp"
#include "message.hpp"
#include "sha1.hpp"

using boost::asio::ip::tcp;

namespace ws {

template <typename T>
class session : public std::enable_shared_from_this<session<T>> {
public:
    using std::enable_shared_from_this<session<T>>::shared_from_this;
    session(T& socket_ref) : socket_ref_(socket_ref) { }
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

        // TODO prepare header then write the header
        std::size_t payload_length = boost::asio::buffer_size(buffer);
        const char *data =
            boost::asio::buffer_cast<const char*>(buffer);

        std::ostream out(&out_buffer_);

        out << '\x82' << '\x05';
        out.write(data, payload_length);

        boost::asio::async_write(socket_ref_, out_buffer_,
            [this, self](const boost::system::error_code &ec, std::size_t)
        {
            if (!ec) {
                //
            }
        });

    };

    /* Close connection (initiates closing handshake) */
    void close() {
        //
    }

    const T &get_socket() {
        return socket_ref_;
    }

private:
    T &socket_ref_;
    boost::asio::streambuf in_buffer_;
    boost::asio::streambuf out_buffer_;
    message msg_;

    std::string generate_accept(const std::string &key) const {
        const std::string GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        std::string formed = key + GUID;

        /* SHA1 hash the formed string */
        std::array<char, 20> hash;
        sha1hash(formed, hash);

        /* Base64 encode the SHA1 hash */
        return base64encode(hash.data(), hash.size());
    }

    void read_handshake() {
        auto self(shared_from_this());
        boost::asio::async_read_until(socket_ref_, in_buffer_, "\r\n\r\n",
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
        std::string accept = generate_accept(it->second);

        response << "HTTP/1.1 101 Switching Protocols\r\n"
            << "Upgrade: websocket\r\n"
            << "Connection: Upgrade\r\n"
            << "Sec-WebSocket-Accept: " << accept << "\r\n"
            << "\r\n";

        write_handshake(false);
    }

    void write_handshake(bool error) {
        auto self(shared_from_this());
        boost::asio::async_write(socket_ref_, out_buffer_,
            [this, self, error](const boost::system::error_code &ec, std::size_t)
        {
            if (!ec) {
                if (!error) {
                    on_open();
                    read();
                }
            }
        });
    }

    void read() {
        auto self(shared_from_this());
        boost::asio::async_read(socket_ref_, in_buffer_, boost::asio::transfer_exactly(2),
            [this, self](const boost::system::error_code &ec, std::size_t)
        {
            if (!ec) {
                // TODO: parse parse parse

                std::istream in(&in_buffer_);
                std::array<unsigned char, 2> buffer;
                in.read((char *)buffer.data(), buffer.size());

                struct {
                    unsigned int fin :1;
                    unsigned int res1 :1;
                    unsigned int res2 :1;
                    unsigned int res3 :1;
                    unsigned int opcode :4;
                    unsigned int mask :1;
                    unsigned int payload_length :7;
                } header;

                header = {
                    static_cast<unsigned int>(buffer[0] >> 7),
                    static_cast<unsigned int>(buffer[0] & 0x40),
                    static_cast<unsigned int>(buffer[0] & 0x20),
                    static_cast<unsigned int>(buffer[0] & 0x10),
                    static_cast<unsigned int>(buffer[0] & 0x0f),
                    static_cast<unsigned int>(buffer[1] & 0x80),
                    static_cast<unsigned int>(buffer[1] & 0x7f)
                };

                // std::cout << header.fin << std::endl;
                // std::cout << header.res1 << std::endl;
                // std::cout << header.res2 << std::endl;
                // std::cout << header.res3 << std::endl;
                // std::cout << header.opcode << std::endl;
                // std::cout << header.mask << std::endl;
                // std::cout << header.payload_length << std::endl;

                message msg;
                on_msg(msg);
            }
        });
    }
};

} /* namespace ws */

#endif /* WS_SESSION_HPP */
