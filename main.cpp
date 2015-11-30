#include <array>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <boost/asio.hpp>
#include "ws.hpp"

class Session : public ws::session {
public:
    Session(tcp::socket socket) : ws::session(std::move(socket)) {
        std::cout << "conn\n";
    }
    ~Session() { std::cout << "disconn\n"; }
    void on_open() override { std::cout << "on_open\n"; }
    void on_msg() override { std::cout << "on_msg\n"; }
    void on_close() override { std::cout << "on_close\n"; }
private:
    std::array<unsigned char, 65536> buffer_;
};

using T = Session;
class Server : public ws::server<T> {
public:
    Server(boost::asio::io_service &io_service, const tcp::endpoint& endpoint) :
        ws::server<T>(io_service, endpoint) { }
    ~Server() { }
private:
    //
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