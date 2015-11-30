#ifndef WS_SESSION_HPP
#define WS_SESSION_HPP

#include <memory>
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


private:
    tcp::socket socket_;
    boost::asio::streambuf in_buffer_;
    boost::asio::streambuf out_buffer_;
    std::unordered_map<std::string, std::string> headers_;

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

    void process_handshake() {
        //

        write_handshake(false);
    }

    void write_handshake(bool error) {
        auto self(shared_from_this());
        boost::asio::async_write(socket_, out_buffer_,
            [this, self, error](const boost::system::error_code &ec, std::size_t)
        {
            if (!error) {
                on_open();
            }
        });
    }
};

} /* namespace ws */

#endif /* WS_SESSION_HPP */
