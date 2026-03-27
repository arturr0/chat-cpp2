#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/http.hpp>
#include <iostream>
#include <fstream>
#include <set>
#include <mutex>
#include <thread>

namespace asio = boost::asio;
namespace http = boost::beast::http;
namespace websocket = boost::beast::websocket;
using tcp = boost::asio::ip::tcp;

std::mutex g_mutex;
std::set<std::shared_ptr<websocket::stream<tcp::socket>>> clients;

std::string load_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if(!file) return "";
    return std::string((std::istreambuf_iterator<char>(file)), {});
}

std::string mime(const std::string& path) {
    if(path.find(".html") != std::string::npos) return "text/html";
    if(path.find(".js") != std::string::npos) return "application/javascript";
    if(path.find(".css") != std::string::npos) return "text/css";
    return "text/plain";
}

void session(tcp::socket socket) {
    try {
        boost::beast::flat_buffer buffer;
        http::request<http::string_body> req;
        http::read(socket, buffer, req);

        // WebSocket
        if(websocket::is_upgrade(req)) {
            auto ws = std::make_shared<websocket::stream<tcp::socket>>(std::move(socket));
            ws->accept(req);

            {
                std::lock_guard<std::mutex> lock(g_mutex);
                clients.insert(ws);
            }

            while(true) {
                boost::beast::flat_buffer buf;
                ws->read(buf);
                std::string msg = boost::beast::buffers_to_string(buf.data());

                std::lock_guard<std::mutex> lock(g_mutex);
                for(auto& c : clients) {
                    if(c != ws && c->next_layer().is_open())
                        c->write(asio::buffer(msg));
                }
            }
        }

        // HTTP (frontend)
        std::string target = req.target().to_string();
        if(target == "/") target = "/index.html";

        std::string body = load_file("frontend" + target);

        if(body.empty()) {
            http::response<http::string_body> res{http::status::not_found, 11};
            res.body() = "File not found";
            res.prepare_payload();
            http::write(socket, res);
            return;
        }

        http::response<http::string_body> res{http::status::ok, 11};
        res.set(http::field::content_type, mime(target));
        res.body() = body;
        res.prepare_payload();
        http::write(socket, res);

    } catch(...) {}
}

int main() {
    asio::io_context io;

    const char* port_env = std::getenv("PORT");
    int port = port_env ? std::stoi(port_env) : 9001;

    tcp::acceptor acceptor(io, {tcp::v4(), (unsigned short)port});

    std::cout << "Server running on port " << port << "\n";

    while(true) {
        tcp::socket socket(io);
        acceptor.accept(socket);
        std::thread(session, std::move(socket)).detach();
    }
}