#include <iostream>
#include <string>
#include <sstream>
#include <cstring>
#include <vector>
#include <atomic>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "reasoner.h"

static const int SOCKET_PORT = 3002;
static std::atomic<bool> search_interrupted{false};

static std::string read_line(int fd) {
    std::string line;
    char ch;
    while (read(fd, &ch, 1) > 0) {
        if (ch == '\n') break;
        line += ch;
    }
    return line;
}

static void write_line(int fd, const std::string& s) {
    std::string out = s + "\n";
    write(fd, out.c_str(), out.size());
}

int main(int argc, char* argv[]) {
    bool serve = false;
    std::string output_path = "output.json";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--serve") serve = true;
        else output_path = arg;
    }

    Reasoner reasoner;
    reasoner.load_axioms("axioms.txt");

    if (serve) {
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0) {
            std::cerr << "socket failed" << std::endl;
            return 1;
        }

        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(SOCKET_PORT);

        if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
            std::cerr << "bind failed" << std::endl;
            return 1;
        }
        if (listen(server_fd, 1) < 0) {
            std::cerr << "listen failed" << std::endl;
            return 1;
        }

        std::cerr << "[engine] listening on port " << SOCKET_PORT << std::endl;

        fd_set master_set, read_set;
        FD_ZERO(&master_set);
        FD_SET(server_fd, &master_set);
        int max_fd = server_fd;
        std::vector<int> clients;

        while (true) {
            read_set = master_set;
            int ready = select(max_fd + 1, &read_set, nullptr, nullptr, nullptr);
            if (ready < 0) continue;

            if (FD_ISSET(server_fd, &read_set)) {
                int client_fd = accept(server_fd, nullptr, nullptr);
                if (client_fd >= 0) {
                    if (clients.size() >= 50) {
                        write_line(client_fd, R"({"status":"error","message":"too many clients"})");
                        close(client_fd);
                        std::cerr << "[engine] rejected client — 50 limit" << std::endl;
                    } else {
                        FD_SET(client_fd, &master_set);
                        if (client_fd > max_fd) max_fd = client_fd;
                        clients.push_back(client_fd);
                        std::cerr << "[engine] client connected (fd=" << client_fd << ")" << std::endl;
                    }
                }
                ready--;
            }

            for (size_t i = 0; i < clients.size() && ready > 0; ) {
                int fd = clients[i];
                if (FD_ISSET(fd, &read_set)) {
                    std::string line = read_line(fd);
                    if (line.empty()) {
                        std::cerr << "[engine] client disconnected (fd=" << fd << ")" << std::endl;
                        close(fd);
                        FD_CLR(fd, &master_set);
                        clients.erase(clients.begin() + i);
                    } else {
                        search_interrupted = false;
                        reasoner.run(search_interrupted);
                        std::ostringstream oss;
                        oss << "{\"status\":\"ok\",\"input\":\"" << line << "\"}";
                        write_line(fd, oss.str());
                        i++;
                    }
                    ready--;
                } else {
                    i++;
                }
            }
        }

        close(server_fd);
    } else {
        reasoner.run();
        reasoner.save(output_path);
        std::cout << "Output saved to " << output_path << std::endl;
    }
    return 0;
}
