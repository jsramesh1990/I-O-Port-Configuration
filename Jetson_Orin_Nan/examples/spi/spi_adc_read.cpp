/**
 * SPI ADC Read Example
 * 
 * Demonstrates reading from MCP3008 8-channel ADC over SPI
 * Connect MCP3008 to SPI1 and analog sensors to channels
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <thread>
#include "spi.hpp"

int main() {
    std::cout << "=== SPI ADC Read Example ===" << std::endl;
    std::cout << "Device: /dev/spidev0.0" << std::endl;
    std::cout << "ADC: MCP3008" << std::endl;
    std::cout << std::endl;
    
    try {
        SPI spi("/dev/spidev0.0", SPI::Mode::MODE_0, 1000000);
        
        if(!spi.open()) {
            std::cerr << "Failed to open SPI device" << std::endl;
            return 1;
        }
        
        std::cout << "Reading all 8 ADC channels..." << std::endl;
        std::cout << std::endl;
        
        for(int iteration = 0; iteration < 10; iteration++) {
            std::cout << "Iteration " << iteration + 1 << ":" << std::endl;
            
            for(int channel = 0; channel < 8; channel++) {
                // MCP3008 command: start bit (1), single-ended (1), channel (3 bits)
                uint8_t tx[] = {
                    0x01,                           // Start bit
                    0x80 | (channel << 4),          // Single-ended + channel
                    0x00                            // Don't care
                };
                uint8_t rx[3];
                
                spi.transfer(tx, rx, 3);
                
                // Combine 10-bit result from last 10 bits of rx[1] and rx[2]
                uint16_t value = ((rx[1] & 0x03) << 8) | rx[2];
                float voltage = (value * 3.3) / 1023.0;
                
                std::cout << "  Channel " << channel << ": "
                          << std::setw(4) << value << " ("
                          << std::fixed << std::setprecision(2) 
                          << voltage << " V)" << std::endl;
            }
            
            std::cout << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        spi.close();
        
    } catch(const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
