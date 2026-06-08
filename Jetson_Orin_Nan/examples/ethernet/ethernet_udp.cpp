/**
 * Ethernet UDP Example
 * 
 * Demonstrates UDP socket communication
 * Sends and receives UDP packets
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

int main() {
    signal(SIGINT, signalHandler);
    
    std::cout << "=== Ethernet UDP Example ===" << std::endl;
    std::cout << "UDP Echo Server/Client" << std::endl;
    std::cout << "Port: 8888" << std::endl;
    std::cout << "Press Ctrl+C to exit" << std::endl;
    std::cout << std::endl;
    
    try {
        Ethernet::UDPSocket socket;
        
        // Bind to port 8888
        Ethernet::IPAddress any;
        any.addr = INADDR_ANY;
        
        if(!socket.bind(8888, any)) {
            std::cerr << "Failed to bind UDP socket" << std::endl;
            return 1;
        }
        
        std::cout << "UDP socket bound to port 8888" << std::endl;
        
        // Start receive thread
        std::thread receive_thread([&socket]() {
            uint8_t buffer[2048];
            
            while(running) {
                Ethernet::IPAddress sender_ip;
                uint16_t sender_port;
                
                ssize_t bytes = socket.receiveFrom(buffer, sizeof(buffer) - 1, 
                                                   &sender_ip, &sender_port);
                
                if(bytes > 0) {
                    buffer[bytes] = '\0';
                    std::cout << "Received from " << sender_ip.toString() 
                              << ":" << sender_port << ": " << buffer;
                    
                    // Echo back
                    socket.sendTo(buffer, bytes, sender_ip, sender_port);
                }
            }
        });
        
        // Send test message
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        Ethernet::IPAddress localhost;
        localhost.fromString("127.0.0.1");
        
        const char* test_msg = "Hello from Jetson!\n";
        socket.sendTo((const uint8_t*)test_msg, strlen(test_msg), localhost, 8888);
        
        std::cout << "Sent test message to localhost" << std::endl;
        
        // Main loop
        while(running) {
            std::cout << "Enter message to broadcast (or 'quit' to exit): ";
            std::string msg;
            std::getline(std::cin, msg);
            
            if(msg == "quit") {
                running = false;
                break;
            }
            
            msg += "\n";
            socket.sendTo((const uint8_t*)msg.c_str(), msg.length(), localhost, 8888);
        }
        
        running = false;
        receive_thread.join();
        
    } catch(const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
