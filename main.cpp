#include <array>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <boost/asio.hpp>
#include "ws.hpp"

using boost::asio::ip::tcp;

/* Base is required with the current design because it is undefined behaviour
 * to reference and uninitialised member. ws::session<T> would be referencing
 * socket_ in Session which cannot be initialised before ws::session ctor is
 * called. */
class Base {
public:
    Base(tcp::socket socket) : socket_(std::move(socket)) { }
    virtual ~Base() { }
protected:
    tcp::socket socket_;
};

using T = tcp::socket;
class Session : public Base, public ws::session<T> {
public:
    Session(tcp::socket socket) : Base(std::move(socket)), ws::session<T>(socket_) {
        auto endpoint = get_socket().remote_endpoint();
        std::cout << "New connection from " << endpoint.address().to_string()
            << ":" << endpoint.port() << std::endl;
    }

    ~Session() {
        auto endpoint = get_socket().remote_endpoint();
        std::cout << endpoint.address().to_string() << ":"
            << endpoint.port() << " disconnected\n";
    }

    void on_open() override {
        std::cout << "on_open\n";

        // for (auto &kv : headers_)
        //     std::cout << kv.first << ": " << kv.second << std::endl;
        // std::cout << std::endl;
    }

    void on_msg(const ws::message &msg) override {
        std::cout << "on_msg\n";

        const std::vector<unsigned char> &payload = msg.get_payload();

        std::copy(payload.begin(), payload.end(), buffer_.begin());

        for (auto &i : buffer_)
            std::cout << i;
        std::cout << std::endl;

        buffer_[0] = 'h';
        buffer_[1] = 'e';
        buffer_[2] = 'l';
        buffer_[3] = 'l';
        buffer_[4] = 'o';

        write(ws::message::opcode::binary, boost::asio::buffer(buffer_, 5));
    }

    void on_close() override {
        std::cout << "on_close\n";
    }

private:

    /* Buffer for receiving/sending data on top of the web socket protocol */
    std::array<unsigned char, 65536> buffer_;
};


class Server {
public:
    Server(boost::asio::io_service &io_service,
        const tcp::endpoint& endpoint) :
        acceptor_(io_service, endpoint),
        socket_(io_service)
    {
        accept();
    }

    void accept() {
        acceptor_.async_accept(socket_,
            [this](const boost::system::error_code &ec)
        {
            if (!ec) {
                std::make_shared<Session>(std::move(socket_))->start();
            }
            accept();
        });
    }

private:
    tcp::acceptor acceptor_;
    tcp::socket socket_;
};

int main() {
    try {
        boost::asio::io_service io_service;
        Server s(io_service, tcp::endpoint(tcp::v4(), 5678));
        io_service.run();
    } catch (const std::exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        throw e;
    }

    return EXIT_SUCCESS;
}
