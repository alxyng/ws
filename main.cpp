#include <array>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <boost/asio.hpp>
#include "ws.hpp"

class Session : public ws::session {
public:
    Session(tcp::socket socket) : ws::session(std::move(socket)) {
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
