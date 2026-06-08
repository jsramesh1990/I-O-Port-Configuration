/**
 * SPI Loopback Example
 * 
 * Demonstrates SPI communication with loopback test
 * Connect MOSI (pin19) to MISO (pin21) for loopback
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include "spi.hpp"

int main() {
    std::cout << "=== SPI Loopback Example ===" << std::endl;
    std::cout << "Device: /dev/spidev0.0" << std::endl;
    std::cout << "Speed: 1 MHz, Mode: 0" << std::endl;
    std::cout << "Connect MOSI (pin19) to MISO (pin21)" << std::endl;
    std::cout << std::endl;
    
    try {
        SPI spi("/dev/spidev0.0", SPI::Mode::MODE_0, 1000000);
        
        if(!spi.open()) {
            std::cerr << "Failed to open SPI device" << std::endl;
            return 1;
        }
        
        std::cout << "SPI opened successfully" << std::endl;
        
        // Test pattern
        std::vector<uint8_t> tx_data;
        for(int i = 0; i < 256; i++) {
            tx_data.push_back(i);
        }
        
        std::cout << "Sending " << tx_data.size() << " bytes..." << std::endl;
        
        std::vector<uint8_t> rx_data;
        spi.transfer(tx_data, rx_data);
        
        // Verify data
        bool match = true;
        for(size_t i = 0; i < tx_data.size(); i++) {
            if(rx_data[i] != tx_data[i]) {
                match = false;
                break;
            }
        }
        
        if(match) {
            std::cout << "✓ Loopback test PASSED!" << std::endl;
        } else {
            std::cout << "✗ Loopback test FAILED!" << std::endl;
            std::cout << "First 16 bytes:" << std::endl;
            
            std::cout << "TX: ";
            for(int i = 0; i < 16; i++) {
                std::cout << std::hex << std::setw(2) << std::setfill('0') 
                          << (int)tx_data[i] << " ";
            }
            std::cout << std::endl;
            
            std::cout << "RX: ";
            for(int i = 0; i < 16; i++) {
                std::cout << std::hex << std::setw(2) << std::setfill('0') 
                          << (int)rx_data[i] << " ";
            }
            std::cout << std::dec << std::endl;
        }
        
        // Test different speeds
        std::cout << "\nTesting different speeds:" << std::endl;
        
        std::vector<uint32_t> speeds = {100000, 500000, 1000000, 5000000, 10000000};
        
        for(uint32_t speed : speeds) {
            spi.setSpeed(speed);
            
            uint8_t tx = 0x55;
            uint8_t rx = spi.transferByte(tx);
            
            std::cout << "  " << speed/1000 << " kHz: ";
            if(rx == tx) {
                std::cout << "PASS" << std::endl;
            } else {
                std::cout << "FAIL (TX=0x" << std::hex << (int)tx 
                         << ", RX=0x" << (int)rx << std::dec << ")" << std::endl;
            }
        }
        
        spi.close();
        
    } catch(const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
