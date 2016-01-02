#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include "ws.hpp"

using boost::asio::ip::tcp;

class session_base {
public:
    session_base(tcp::socket socket, boost::asio::ssl::context &context) :
        socket_(std::move(socket)), ssl_socket_(socket_, context) { }
protected:
    tcp::socket socket_;
    boost::asio::ssl::stream<tcp::socket&> ssl_socket_;
};

using T = boost::asio::ssl::stream<tcp::socket&>;
class session : public session_base, public ws::session<T> {
public:
    session(tcp::socket socket, boost::asio::ssl::context &context) :
        session_base(std::move(socket), context), ws::session<T>(ssl_socket_)
    {
        std::cout << "session()\n";
    }

    ~session() {
        std::cout << "~session()\n";
    }

    void start() {
        auto self(shared_from_this());

        /* SSL handshake */
        ssl_socket_.async_handshake(boost::asio::ssl::stream_base::server,
            [this, self](const boost::system::error_code &ec)
        {
            if (!ec) {
                ws::session<T>::start();
            }
        });
    }

private:
    void on_open() override {
        std::cout << "WebSocket connection open\n";
    }

    void on_msg(const ws::message &msg) override {
        const std::vector<unsigned char> payload = msg.get_payload();

        std::cout << "WebSocket message received: ";
        if (msg.get_opcode() == ws::message::opcode::text) {
            std::cout << "[opcode: text, length " << payload.size() << "]: "
                << std::string(payload.begin(), payload.end())
                << std::endl;
        } else if (msg.get_opcode() == ws::message::opcode::binary) {
            std::cout << "[opcode: binary, length " << payload.size() << "]: ";
            for (auto &b : payload)
                std::cout << std::hex << std::setfill('0') << std::setw(2)
                    << static_cast<unsigned int>(b) << " ";
            std::cout << std::dec << std::endl;
        }

        write(msg.get_opcode(),
            boost::asio::buffer(payload.data(), payload.size()), [this]()
        {
            read();
        });
    }

    void on_close() override {
        std::cout << "WebSocket connection closed\n";
    }

    void on_error() override {
        std::cout << "WebSocket connection error\n";
    }
};

class server {
public:
    server(boost::asio::io_service& io_service,
        const tcp::endpoint& endpoint) :
        acceptor_(io_service, endpoint), socket_(io_service),
        context_(boost::asio::ssl::context::sslv23)
    {
        context_.set_options(
            boost::asio::ssl::context::default_workarounds
            | boost::asio::ssl::context::no_sslv2
            | boost::asio::ssl::context::single_dh_use);
        context_.use_private_key_file("echo-secure.key",
            boost::asio::ssl::context::pem);
        context_.use_certificate_chain_file("echo-secure.crt");

        accept();
    }

private:
    tcp::acceptor acceptor_;
    tcp::socket socket_;
    boost::asio::ssl::context context_;

    void accept() {
        acceptor_.async_accept(socket_,
            [this](const boost::system::error_code &ec)
        {
            if (!ec) {
                std::make_shared<session>(std::move(socket_), context_)->start();
            }

            accept();
        });
    }
};

int main(int, const char **) {
    const unsigned short PORT = 4567;

    try {
        boost::asio::io_service io_service;
        tcp::endpoint endpoint(tcp::v4(), PORT);
        server server(io_service, endpoint);
        io_service.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return EXIT_SUCCESS;
}
