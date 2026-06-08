/**
 * I2C Bus Scanner Example
 * 
 * Scans I2C bus for connected devices and displays addresses
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include "i2c.hpp"

int main() {
    std::cout << "=== I2C Bus Scanner ===" << std::endl;
    std::cout << "Scanning I2C bus 1 (pins 3=SDA, 5=SCL)" << std::endl;
    std::cout << std::endl;
    
    try {
        I2C i2c("/dev/i2c-1");
        
        if(!i2c.open()) {
            std::cerr << "Failed to open I2C bus" << std::endl;
            return 1;
        }
        
        std::vector<uint8_t> devices = i2c.scanBus();
        
        std::cout << "     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f" << std::endl;
        
        for(int i = 0; i < 128; i += 16) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << i << ": ";
            
            for(int j = 0; j < 16; j++) {
                int addr = i + j;
                if(addr < 8) {
                    std::cout << "   ";
                    continue;
                }
                
                bool found = false;
                for(uint8_t dev : devices) {
                    if(dev == addr) {
                        found = true;
                        break;
                    }
                }
                
                if(found) {
                    std::cout << " " << std::hex << std::setw(2) << std::setfill('0') << addr;
                } else {
                    std::cout << " --";
                }
            }
            std::cout << std::dec << std::endl;
        }
        
        std::cout << "\nFound " << devices.size() << " device(s):" << std::endl;
        for(uint8_t addr : devices) {
            std::cout << "  0x" << std::hex << (int)addr << std::dec;
            
            // Common device identification
            switch(addr) {
                case 0x18: std::cout << " - MPU6050 (Accelerometer/Gyro)"; break;
                case 0x40: std::cout << " - Si7021 (Temperature/Humidity)"; break;
                case 0x48: std::cout << " - ADS1115 (ADC)"; break;
                case 0x50 ... 0x57: std::cout << " - EEPROM"; break;
                case 0x68: std::cout << " - DS3231 (RTC)"; break;
                case 0x76: std::cout << " - BME280 (Pressure/Temp/Humidity)"; break;
                case 0x77: std::cout << " - BMP280 (Pressure/Temp)"; break;
                default: std::cout << " - Unknown device";
            }
            std::cout << std::endl;
        }
        
        i2c.close();
        
    } catch(const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

