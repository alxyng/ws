#ifndef WS_SESSION_HPP
#define WS_SESSION_HPP

#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <regex>
#include <unordered_map>
#include <boost/asio.hpp>
#include <boost/detail/endian.hpp>
#include <boost/endian/conversion.hpp>
#include "base64.hpp"
#include "message.hpp"
#include "sha1.hpp"

using boost::asio::ip::tcp;

namespace ws {

template <typename T>
class session : public std::enable_shared_from_this<session<T>> {
public:
    using std::enable_shared_from_this<session<T>>::shared_from_this;

    enum class state {
        connecting,
        open,
        closing,
        closed
    };

    session(T& socket_ref) :
        socket_ref_(socket_ref), state_(state::connecting),
        in_stream_(&in_buffer_), out_stream_(&out_buffer_) { }
    virtual ~session() { }
    void start() {
        read_handshake();
    }

protected:
    std::unordered_map<std::string, std::string> headers_;

    virtual void on_open() = 0;
    virtual void on_msg(const ws::message &msg) = 0;
    virtual void on_close() = 0;
    virtual void on_error() = 0;

    void read() {
        auto self(shared_from_this());
        boost::asio::async_read(socket_ref_, in_buffer_, boost::asio::transfer_exactly(header_.size()),
            [this, self](const boost::system::error_code &ec, std::size_t)
        {
            if (!ec) {
                in_stream_.read(reinterpret_cast<char *>(header_.data()), header_.size());

                /* Only handle fin messages (right now at least) */
                if (!(header_[0] >> 7))
                    return;

                /* If reserved bits are not 0, return */
                if ((header_[0] & 0x40) | (header_[0] & 0x20) | (header_[0] & 0x10))
                    return;

                /* If the message is not masked, return */
                if (!(header_[1] >> 7))
                    return;

                std::size_t payload_length = header_[1] & 0x7f;
                if (payload_length < 126) {
                    /* Read message mask and payload */
                    read_mask_and_payload(payload_length);
                } else if (payload_length == 126) {
                    /* read 2 more bytes and assign this uint16_t to payload_length */
                    boost::asio::async_read(socket_ref_, in_buffer_, boost::asio::transfer_exactly(2),
                        [this, self](const boost::system::error_code &ec, std::size_t)
                    {
                        if (!ec) {
                            uint16_t u16;
                            in_stream_.read(reinterpret_cast<char *>(&u16), 2);
#ifndef BOOST_BIG_ENDIAN
                            u16 = boost::endian::endian_reverse(u16);
#endif /* BOOST_BIG_ENDIAN */
                            std::size_t payload_length = u16;
                            read_mask_and_payload(payload_length);
                        }
                    });
                } else {
                    /* read 8 more bytes and assign this uint64_t to payload_length */
                    boost::asio::async_read(socket_ref_, in_buffer_, boost::asio::transfer_exactly(8),
                        [this, self](const boost::system::error_code &ec, std::size_t)
                    {
                        if (!ec) {
                            uint64_t u64;
                            in_stream_.read(reinterpret_cast<char *>(&u64), 8);
#ifndef BOOST_BIG_ENDIAN
                            u64 = boost::endian::endian_reverse(u64);
#endif /* BOOST_BIG_ENDIAN */
                            std::size_t payload_length = u64;
                            read_mask_and_payload(payload_length);
                        }
                    });
                }
            }
        });
    }

    /* Async write data out */
    void write(message::opcode opcode, const boost::asio::const_buffer &buffer, std::function<void()> cb) {
        auto self(shared_from_this());

        std::size_t payload_length = boost::asio::buffer_size(buffer);
        const char *payload = boost::asio::buffer_cast<const char*>(buffer);

        std::array<char, 10> header;

        /* FIN bit and opcode */
        header[0] = 0x80 | static_cast<unsigned char>(opcode);

        out_stream_ << *reinterpret_cast<char *>(&header[0]);

        /* Payload length (mask bit is always 0) */
        if (payload_length < 126) {
            header[1] = payload_length;
            out_stream_ << header[1];
        } else if (payload_length < 65536) {
            header[1] = 126;
            header[2] = (payload_length >> 8) & 0xff;
            header[3] = payload_length & 0xff;
            out_stream_ << header[1] << header[2] << header[3];
        } else {
            header[1] = 127;
            header[2] = (payload_length >> 54);
            header[3] = (payload_length >> 48) & 0xff;
            header[4] = (payload_length >> 40) & 0xff;
            header[5] = (payload_length >> 32) & 0xff;
            header[6] = (payload_length >> 24) & 0xff;
            header[7] = (payload_length >> 16) & 0xff;
            header[8] = (payload_length >> 8) & 0xff;
            header[9] = payload_length & 0xff;
            out_stream_ << header[1] << header[2] << header[3] << header[4]
                << header[5] << header[6] << header[7] << header[8]
                << header[9] << header[10];
        }

        /* Payload */
        out_stream_.write(payload, payload_length);

        boost::asio::async_write(socket_ref_, out_buffer_,
            [this, self, cb](const boost::system::error_code &ec, std::size_t)
        {
            if (!ec) {
                if (cb) {
                    cb();
                }
            }
        });
    };

    /* Close connection (initiates closing handshake) */
    void close() {
        write(message::opcode::connection_close, {}, [this]() {
            if (state_ == state::closing) {
                /* Client initiated close */
                state_ = state::closed;
                on_close();
            } else {
                /* We initiated close - closing from a timer is not supported
                 * using this method because read() gets called twice, if a
                 * timer is to be used, a flag can be queried to check if
                 * a read is already being waited on. */
                state_ = state::closing;
                read();
            }
        });
    }

    const T &get_socket() {
        return socket_ref_;
    }

private:
    T &socket_ref_;
    state state_;
    boost::asio::streambuf in_buffer_;
    boost::asio::streambuf out_buffer_;
    std::istream in_stream_;
    std::ostream out_stream_;
    std::array<unsigned char, 2> header_;

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
                    state_ = state::open;
                    on_open();
                    read();
                }
            }
        });
    }

    void read_mask_and_payload(std::size_t payload_length) {
        auto self(shared_from_this());
        boost::asio::async_read(socket_ref_, in_buffer_,
            boost::asio::transfer_exactly(4 + payload_length),
            [this, self, payload_length](const boost::system::error_code &ec, std::size_t)
        {
            if (!ec) {
                std::array<unsigned char, 4> mask;
                std::vector<unsigned char> payload(payload_length);
                in_stream_.read((char *)mask.data(), mask.size());
                in_stream_.read((char *)payload.data(), payload.size());
                unmask_data(mask, payload);

                message msg(static_cast<message::opcode>(header_[0] & 0x0f),
                    std::move(payload));
                switch (msg.get_opcode()) {
                    case message::opcode::text:
                    case message::opcode::binary:
                        on_msg(msg);
                        break;
                    case message::opcode::connection_close:
                        if (state_ == state::closing) {
                            /* We initiated close */
                            state_ = state::closed;
                            on_close();
                        } else {
                            /* Client initiated close */
                            state_ = state::closing;
                            close();
                        }
                        break;
                    default:
                        break;
                }
            }
        });
    }

    void unmask_data(const std::array<unsigned char, 4> &mask,
        std::vector<unsigned char> &payload)
    {
        for (std::size_t i = 0; i < payload.size(); ++i)
            payload[i] = payload[i] ^ mask[i % 4];
    }
};

} /* namespace ws */

#endif /* WS_SESSION_HPP */
