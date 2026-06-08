/**
 * UART Echo Example
 * 
 * Demonstrates UART communication with echo back
 * Connect TX to RX (pins 8 and 10) for loopback test
 */

#include <iostream>
#include <cstring>
#include <signal.h>
#include "uart.hpp"

static volatile bool running = true;

void signalHandler(int signum) {
    std::cout << "\nExiting..." << std::endl;
    running = false;
}

int main() {
    signal(SIGINT, signalHandler);
    
    std::cout << "=== UART Echo Example ===" << std::endl;
    std::cout << "Device: /dev/ttyTHS1" << std::endl;
    std::cout << "Baud rate: 115200" << std::endl;
    std::cout << "Connect TX (pin8) to RX (pin10) for loopback test" << std::endl;
    std::cout << "Press Ctrl+C to exit" << std::endl;
    std::cout << std::endl;
    
    try {
        UART uart("/dev/ttyTHS1", 115200);
        
        if(!uart.open()) {
            std::cerr << "Failed to open UART device" << std::endl;
            return 1;
        }
        
        std::cout << "UART opened successfully" << std::endl;
        
        // Send test message
        const char* test_msg = "Hello from Jetson! Echo test...\r\n";
        uart.writeString(test_msg);
        std::cout << "Sent: " << test_msg;
        
        // Read and echo back
        uint8_t buffer[256];
        
        while(running) {
            ssize_t bytes = uart.readTimeout(buffer, sizeof(buffer) - 1, 100);
            
            if(bytes > 0) {
                buffer[bytes] = '\0';
                std::cout << "Received: " << buffer;
                
                // Echo back
                uart.write(buffer, bytes);
            }
        }
        
        uart.close();
        std::cout << "\nUART closed." << std::endl;
        
    } catch(const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
