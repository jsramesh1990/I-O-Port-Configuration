/**
 * Ethernet TCP Example
 * 
 * Demonstrates TCP socket communication
 * Simple TCP echo server and client
 */

#include <iostream>
#include <cstring>
#include <thread>
#include <signal.h>
#include "ethernet.hpp"

static volatile bool running = true;

void signalHandler(int signum) {
    running = false;
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signalHandler);
    
    std::cout << "=== Ethernet TCP Example ===" << std::endl;
    
    bool is_server = true;
    std::string server_ip = "127.0.0.1";
    
    if(argc > 1 && strcmp(argv[1], "client") == 0) {
        is_server = false;
        if(argc > 2) server_ip = argv[2];
    }
    
    std::cout << "Mode: " << (is_server ? "SERVER" : "CLIENT") << std::endl;
    std::cout << "Port: 9999" << std::endl;
    std::cout << "Press Ctrl+C to exit" << std::endl;
    std::cout << std::endl;
    
    try {
        if(is_server) {
            // TCP Server mode
            std::cout << "Starting TCP server on port 9999..." << std::endl;
            
            int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
            if(listen_fd < 0) {
                std::cerr << "Failed to create socket" << std::endl;
                return 1;
            }
            
            int reuse = 1;
            setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
            
            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = INADDR_ANY;
            addr.sin_port = htons(9999);
            
            if(bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                std::cerr << "Failed to bind" << std::endl;
                close(listen_fd);
                return 1;
            }
            
            listen(listen_fd, 5);
            std::cout << "Waiting for client connection..." << std::endl;
            
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
            
            if(client_fd < 0) {
                std::cerr << "Failed to accept connection" << std::endl;
                close(listen_fd);
                return 1;
            }
            
            char client_ip[16];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
            std::cout << "Client connected from " << client_ip << std::endl;
            
            // Echo data back
            uint8_t buffer[1024];
            while(running) {
                ssize_t bytes = recv(client_fd, buffer, sizeof(buffer), 0);
                if(bytes <= 0) break;
                
                std::cout << "Received " << bytes << " bytes: ";
                std::cout.write((char*)buffer, bytes);
                std::cout << std::endl;
                
                send(client_fd, buffer, bytes, 0);
            }
            
            close(client_fd);
            close(listen_fd);
            
        } else {
            // TCP Client mode
            std::cout << "Connecting to server at " << server_ip << ":9999..." << std::endl;
            
            Ethernet::IPAddress server_addr;
            server_addr.fromString(server_ip);
            
            Ethernet::TCPSocket socket;
            if(!socket.connect(server_addr, 9999)) {
                std::cerr << "Failed to connect to server" << std::endl;
                return 1;
            }
            
            std::cout << "Connected to server!" << std::endl;
            
            // Start receive thread
            std::thread receive_thread([&socket]() {
                uint8_t buffer[1024];
                
                while(running) {
                    ssize_t bytes = socket.receive(buffer, sizeof(buffer), 1000);
                    if(bytes > 0) {
                        std::cout << "Server: ";
                        std::cout.write((char*)buffer, bytes);
                        std::cout << std::endl;
                    }
                }
            });
            
            // Send messages
            while(running) {
                std::cout << "Enter message (or 'quit' to exit): ";
                std::string msg;
                std::getline(std::cin, msg);
                
                if(msg == "quit") {
                    running = false;
                    break;
                }
                
                msg += "\n";
                socket.send((const uint8_t*)msg.c_str(), msg.length());
            }
            
            running = false;
            receive_thread.join();
            socket.disconnect();
        }
        
    } catch(const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "\nExiting..." << std::endl;
    return 0;
}
