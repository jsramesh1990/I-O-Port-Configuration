/**
 * CAN Receive Example
 * 
 * Demonstrates receiving CAN frames
 * Requires external CAN controller (MCP2515) connected via SPI
 */

#include <iostream>
#include <iomanip>
#include <signal.h>
#include "can.hpp"

static volatile bool running = true;

void signalHandler(int signum) {
    running = false;
}

void frameCallback(const CAN::CANFrame& frame) {
    std::cout << "Received CAN Frame:" << std::endl;
    std::cout << "  ID: 0x" << std::hex << frame.id << std::dec;
    if(frame.extended) std::cout << " (Extended)";
    if(frame.rtr) std::cout << " (RTR)";
    std::cout << std::endl;
    
    std::cout << "  Length: " << (int)frame.len << std::endl;
    
    if(!frame.rtr && frame.len > 0) {
        std::cout << "  Data: ";
        for(int i = 0; i < frame.len; i++) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') 
                      << (int)frame.data[i] << " ";
        }
        std::cout << std::dec << std::endl;
    }
    
    std::cout << std::endl;
}

int main() {
    signal(SIGINT, signalHandler);
    
    std::cout << "=== CAN Receive Example ===" << std::endl;
    std::cout << "Interface: can0" << std::endl;
    std::cout << "Press Ctrl+C to exit" << std::endl;
    std::cout << std::endl;
    
    try {
        CAN can("can0");
        
        if(!can.open()) {
            std::cerr << "Failed to open CAN interface" << std::endl;
            std::cerr << "Make sure CAN interface is configured:" << std::endl;
            std::cerr << "  sudo ip link set can0 up type can bitrate 500000" << std::endl;
            return 1;
        }
        
        std::cout << "CAN interface opened successfully" << std::endl;
        
        // Set filter to receive all frames
        CAN::Filter filter;
        filter.id = 0;
        filter.mask = 0;
        filter.extended = false;
        can.setFilter(filter);
        
        // Start async receive
        can.startAsyncReceive(frameCallback);
        
        // Print statistics periodically
        while(running) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            
            auto stats = can.getStatistics();
            std::cout << "=== Statistics ===" << std::endl;
            std::cout << "TX Frames: " << stats.tx_frames << std::endl;
            std::cout << "RX Frames: " << stats.rx_frames << std::endl;
            std::cout << "TX Errors: " << stats.tx_errors << std::endl;
            std::cout << "RX Errors: " << stats.rx_errors << std::endl;
            std::cout << std::endl;
        }
        
        can.stopAsyncReceive();
        can.close();
        
    } catch(const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
