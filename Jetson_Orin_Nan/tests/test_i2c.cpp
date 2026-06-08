#include <iostream>
#include <iomanip>
#include <vector>
#include "i2c.hpp"

using namespace std;

void testScanBus() {
    cout << "=== I2C Bus Scan Test ===" << endl;
    
    try {
        I2C i2c("/dev/i2c-1");
        
        if(!i2c.open()) {
            cerr << "Failed to open I2C bus" << endl;
            return;
        }
        
        cout << "Scanning I2C bus 1..." << endl;
        cout << "    0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f" << endl;
        
        for(int i = 0; i < 128; i += 16) {
            cout << hex << setw(2) << setfill('0') << i << ": ";
            
            for(int j = 0; j < 16; j++) {
                int addr = i + j;
                if(addr < 8) {
                    cout << "   ";
                    continue;
                }
                
                if(i2c.probeDevice(addr)) {
                    cout << " " << hex << setw(2) << setfill('0') << addr;
                } else {
                    cout << " --";
                }
            }
            cout << dec << endl;
        }
        
        i2c.close();
        
    } catch(const exception& e) {
        cerr << "Error: " << e.what() << endl;
    }
}

void testReadWrite() {
    cout << "\n=== I2C Read/Write Test ===" << endl;
    
    try {
        I2C i2c("/dev/i2c-1", 0x50);  // Common EEPROM address
        
        if(!i2c.open()) {
            cerr << "Failed to open I2C bus" << endl;
            return;
        }
        
        // Test write byte
        if(i2c.writeByte(0x00, 0xAA) >= 0) {
            cout << "Write byte successful" << endl;
            
            // Test read byte
            uint8_t value;
            if(i2c.readByte(0x00, &value) >= 0) {
                cout << "Read byte: 0x" << hex << (int)value << dec << endl;
                
                if(value == 0xAA) {
                    cout << "✓ Read/Write verification passed" << endl;
                }
            }
        } else {
            cout << "No device at address 0x50 (expected for EEPROM)" << endl;
        }
        
        i2c.close();
        
    } catch(const exception& e) {
        cerr << "Error: " << e.what() << endl;
    }
}

void testBMESensor() {
    cout << "\n=== BME280 Sensor Test ===" << endl;
    
    try {
        I2C i2c("/dev/i2c-1", 0x76);
        
        if(!i2c.open()) {
            cerr << "Failed to open I2C bus" << endl;
            return;
        }
        
        // Check if BME280 is present
        uint8_t chip_id;
        if(i2c.readByte(0xD0, &chip_id) < 0) {
            cout << "BME280 not found at address 0x76" << endl;
            i2c.close();
            return;
        }
        
        if(chip_id != 0x60) {
            cout << "Invalid chip ID: 0x" << hex << (int)chip_id << dec << endl;
            i2c.close();
            return;
        }
        
        cout << "BME280 detected" << endl;
        
        // Read calibration data (simplified)
        uint8_t calib[24];
        i2c.readBlock(0x88, calib, 24);
        
        uint16_t dig_T1 = calib[0] | (calib[1] << 8);
        cout << "dig_T1: " << dig_T1 << endl;
        
        i2c.close();
        
    } catch(const exception& e) {
        cerr << "Error: " << e.what() << endl;
    }
}

void testSMBusCommands() {
    cout << "\n=== SMBus Command Test ===" << endl;
    
    try {
        SMBus smbus("/dev/i2c-1", 0x48);  // Common ADC address
        
        if(!smbus.open()) {
            cerr << "Failed to open I2C bus" << endl;
            return;
        }
        
        // Test SMBus commands
        int8_t result = smbus.smbusReceiveByte();
        if(result >= 0) {
            cout << "Receive byte: 0x" << hex << (int)result << dec << endl;
        }
        
        uint8_t config = 0x00;
        if(smbus.smbusWriteByteData(0x01, config)) {
            cout << "Write config register successful" << endl;
        }
        
        int16_t value = smbus.smbusReadWordData(0x00);
        if(value >= 0) {
            cout << "Read conversion: 0x" << hex << value << dec << endl;
        }
        
        smbus.close();
        
    } catch(const exception& e) {
        cerr << "Error: " << e.what() << endl;
    }
}

int main() {
    cout << "I2C Test Suite for Jetson Orin Nano" << endl;
    cout << "===================================" << endl;
    
    testScanBus();
    testReadWrite();
    testBMESensor();
    testSMBusCommands();
    
    cout << "\nTest completed!" << endl;
    return 0;
}
