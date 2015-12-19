#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <boost/asio.hpp>
#include "ws.hpp"

using boost::asio::ip::tcp;

class session_base {
public:
    session_base(tcp::socket socket) : socket_(std::move(socket)) { }
protected:
    tcp::socket socket_;
};

using T = tcp::socket;
class session : public session_base, public ws::session<T> {
public:
    session(tcp::socket socket) :
        session_base(std::move(socket)), ws::session<T>(socket_)
    {
        std::cout << "session()\n";
    }

    ~session() {
        std::cout << "~session()\n";
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
        acceptor_(io_service, endpoint), socket_(io_service)
    {
        accept();
    }

private:
    tcp::acceptor acceptor_;
    tcp::socket socket_;

    void accept() {
        acceptor_.async_accept(socket_,
            [this](const boost::system::error_code &ec)
        {
            if (!ec) {
                std::make_shared<session>(std::move(socket_))->start();
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
