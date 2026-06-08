#include <iostream>
#include <vector>
#include <iomanip>
#include "spi.hpp"

using namespace std;

void testBasicTransfer() {
    cout << "=== SPI Basic Transfer Test ===" << endl;
    
    try {
        SPI spi("/dev/spidev0.0", SPI::Mode::MODE_0, 1000000);
        
        if(!spi.open()) {
            cerr << "Failed to open SPI device" << endl;
            return;
        }
        
        cout << "SPI opened successfully" << endl;
        cout << "Speed: 1 MHz, Mode: 0" << endl;
        
        // Test single byte transfer
        uint8_t tx_byte = 0xAA;
        uint8_t rx_byte = spi.transferByte(tx_byte);
        
        cout << "Single byte: TX=0x" << hex << (int)tx_byte 
             << ", RX=0x" << (int)rx_byte << dec << endl;
        
        // Test multi-byte transfer
        vector<uint8_t> tx = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
        vector<uint8_t> rx;
        
        spi.transfer(tx, rx);
        
        cout << "Multi-byte transfer (" << tx.size() << " bytes):" << endl;
        cout << "  TX: ";
        for(auto b : tx) cout << hex << setw(2) << setfill('0') << (int)b << " ";
        cout << endl << "  RX: ";
        for(auto b : rx) cout << hex << setw(2) << setfill('0') << (int)b << " ";
        cout << dec << endl;
        
        spi.close();
        
    } catch(const exception& e) {
        cerr << "Error: " << e.what() << endl;
    }
}

void testSPIModes() {
    cout << "\n=== SPI Mode Test ===" << endl;
    
    vector<SPI::Mode> modes = {SPI::Mode::MODE_0, SPI::Mode::MODE_1, 
                                SPI::Mode::MODE_2, SPI::Mode::MODE_3};
    
    for(auto mode : modes) {
        try {
            SPI spi("/dev/spidev0.0", mode, 1000000);
            
            if(!spi.open()) {
                cerr << "Failed to open SPI for mode " << (int)mode << endl;
                continue;
            }
            
            cout << "Testing Mode " << (int)mode << "... ";
            
            uint8_t tx = 0x55;
            uint8_t rx = spi.transferByte(tx);
            
            cout << "OK (RX=0x" << hex << (int)rx << dec << ")" << endl;
            
            spi.close();
            
        } catch(const exception& e) {
            cerr << "FAIL: " << e.what() << endl;
        }
    }
}

void testSPISpeed() {
    cout << "\n=== SPI Speed Test ===" << endl;
    
    vector<uint32_t> speeds = {100000, 500000, 1000000, 5000000, 10000000, 20000000};
    
    for(uint32_t speed : speeds) {
        try {
            SPI spi("/dev/spidev0.0", SPI::Mode::MODE_0, speed);
            
            if(!spi.open()) {
                cerr << "Failed to open SPI at " << speed << " Hz" << endl;
                continue;
            }
            
            cout << "Testing " << speed/1000000.0 << " MHz... ";
            
            // Test transfer
            auto start = chrono::high_resolution_clock::now();
            
            const int iterations = 10000;
            for(int i = 0; i < iterations; i++) {
                spi.transferByte(0x55);
            }
            
            auto end = chrono::high_resolution_clock::now();
            auto duration = chrono::duration_cast<chrono::microseconds>(end - start);
            
            double actual_speed = (iterations * 8) / (duration.count() / 1e6);
            cout << "OK (Actual: " << actual_speed/1e6 << " Mbps)" << endl;
            
            spi.close();
            
        } catch(const exception& e) {
            cerr << "FAIL: " << e.what() << endl;
        }
    }
}

void testADCSensor() {
    cout << "\n=== SPI ADC Sensor Test (MCP3008) ===" << endl;
    
    try {
        SPI spi("/dev/spidev0.0", SPI::Mode::MODE_0, 1000000);
        
        if(!spi.open()) {
            cerr << "Failed to open SPI" << endl;
            return;
        }
        
        cout << "Reading MCP3008 ADC channels..." << endl;
        
        for(int channel = 0; channel < 8; channel++) {
            uint8_t tx[] = {0x01, 0x80 | (channel << 4), 0x00};
            uint8_t rx[3];
            
            spi.transfer(tx, rx, 3);
            
            uint16_t value = ((rx[1] & 0x03) << 8) | rx[2];
            float voltage = (value * 3.3) / 1023.0;
            
            cout << "Channel " << channel << ": " << value 
                 << " (" << voltage << " V)" << endl;
        }
        
        spi.close();
        
    } catch(const exception& e) {
        cerr << "Error: " << e.what() << endl;
        cout << "Make sure MCP3008 is connected to SPI" << endl;
    }
}

int main() {
    cout << "SPI Test Suite for Jetson Orin Nano" << endl;
    cout << "====================================" << endl;
    cout << "Note: Connect MOSI to MISO for loopback tests" << endl;
    
    testBasicTransfer();
    testSPIModes();
    testSPISpeed();
    testADCSensor();
    
    cout << "\nTest completed!" << endl;
    return 0;
}
