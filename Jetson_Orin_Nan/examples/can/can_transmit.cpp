/**
 * CAN Transmit Example
 * 
 * Demonstrates transmitting CAN frames
 * Requires external CAN controller (MCP2515) connected via SPI
 */

#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <signal.h>
#include "can.hpp"

static volatile bool running = true;

void signalHandler(int signum) {
    running = false;
}

int main() {
    signal(SIGINT, signalHandler);
    
    std::cout << "=== CAN Transmit Example ===" << std::endl;
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
        
        CAN::CANFrame frame;
        frame.id = 0x123;
        frame.len = 8;
        frame.extended = false;
        frame.rtr = false;
        
        int counter = 0;
        
        while(running) {
            // Fill data with counter
            for(int i = 0; i < 8; i++) {
                frame.data[i] = (counter + i) & 0xFF;
            }
            
            // Send frame
            if(can.sendFrame(frame)) {
                std::cout << "Sent frame " << counter << ": ";
                std::cout << "ID=0x" << std::hex << frame.id << std::dec
                         << " Data=";
                for(int i = 0; i < frame.len; i++) {
                    std::cout << std::hex << std::setw(2) << std::setfill('0') 
                              << (int)frame.data[i] << " ";
                }
                std::cout << std::dec << std::endl;
            } else {
                std::cout << "Failed to send frame" << std::endl;
            }
            
            counter++;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        
        // Print final statistics
        auto stats = can.getStatistics();
        std::cout << "\n=== Final Statistics ===" << std::endl;
        std::cout << "TX Frames: " << stats.tx_frames << std::endl;
        std::cout << "RX Frames: " << stats.rx_frames << std::endl;
        std::cout << "TX Errors: " << stats.tx_errors << std::endl;
        std::cout << "RX Errors: " << stats.rx_errors << std::endl;
        
        can.close();
        
    } catch(const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
