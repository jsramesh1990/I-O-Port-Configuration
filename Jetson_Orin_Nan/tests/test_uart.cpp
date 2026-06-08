#include <iostream>
#include <cstring>
#include <thread>
#include "uart.hpp"

using namespace std;

void testBasicCommunication() {
    cout << "=== UART Basic Communication Test ===" << endl;
    
    try {
        UART uart("/dev/ttyTHS1", 115200);
        
        if(!uart.open()) {
            cerr << "Failed to open UART device" << endl;
            return;
        }
        
        cout << "UART opened successfully" << endl;
        
        // Send test data
        const char* test_msg = "Hello from Jetson!\r\n";
        ssize_t sent = uart.writeString(test_msg);
        cout << "Sent " << sent << " bytes: " << test_msg;
        
        // Try to read (requires echo/loopback)
        uint8_t buffer[256];
        ssize_t received = uart.readTimeout(buffer, sizeof(buffer), 100);
        
        if(received > 0) {
            cout << "Received " << received << " bytes: ";
            cout.write((char*)buffer, received);
            cout << endl;
        } else {
            cout << "No data received (connect TX to RX for loopback test)" << endl;
        }
        
        uart.close();
        
    } catch(const exception& e) {
        cerr << "Error: " << e.what() << endl;
    }
}

void testBaudRates() {
    cout << "\n=== UART Baud Rate Test ===" << endl;
    
    vector<int> baudrates = {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};
    
    for(int baud : baudrates) {
        try {
            UART uart("/dev/ttyTHS1", baud);
            
            if(!uart.open()) {
                cerr << "Failed to open at " << baud << " baud" << endl;
                continue;
            }
            
            cout << "Testing " << baud << " baud... ";
            
            // Send test pattern
            uint8_t pattern[] = {0x55, 0xAA, 0x00, 0xFF, 0x55, 0xAA};
            ssize_t sent = uart.write(pattern, sizeof(pattern));
            
            if(sent == sizeof(pattern)) {
                cout << "OK" << endl;
            } else {
                cout << "FAIL" << endl;
            }
            
            uart.close();
            
        } catch(const exception& e) {
            cerr << "Error at " << baud << ": " << e.what() << endl;
        }
    }
}

void testAsyncRead() {
    cout << "\n=== UART Async Read Test ===" << endl;
    
    try {
        UART uart("/dev/ttyTHS1", 115200);
        
        if(!uart.open()) {
            cerr << "Failed to open UART" << endl;
            return;
        }
        
        atomic<bool> running(true);
        
        uart.startAsyncRead([&running](const uint8_t* data, size_t len) {
            cout << "Async data (" << len << " bytes): ";
            cout.write((char*)data, len);
            cout << endl;
        });
        
        cout << "Async reading for 5 seconds..." << endl;
        cout << "Send data to " << uart.getDevice() << endl;
        
        this_thread::sleep_for(chrono::seconds(5));
        
        uart.stopAsyncRead();
        uart.close();
        
    } catch(const exception& e) {
        cerr << "Error: " << e.what() << endl;
    }
}

void testHardwareFlowControl() {
    cout << "\n=== UART Hardware Flow Control Test ===" << endl;
    
    try {
        UART::Config config;
        config.baudrate = 115200;
        config.hardware_flow = true;
        
        UART uart("/dev/ttyTHS1", 115200);
        uart.setConfig(config);
        
        if(!uart.open()) {
            cerr << "Failed to open UART with flow control" << endl;
            return;
        }
        
        cout << "Hardware flow control enabled" << endl;
        
        // Test RTS/CTS (requires external connection)
        cout << "Check RTS/CTS pins (36=RTS, 38=CTS)" << endl;
        
        uart.close();
        
    } catch(const exception& e) {
        cerr << "Error: " << e.what() << endl;
    }
}

int main() {
    cout << "UART Test Suite for Jetson Orin Nano" << endl;
    cout << "=====================================" << endl;
    cout << "Note: Connect TX to RX for loopback tests" << endl;
    
    testBasicCommunication();
    testBaudRates();
    testAsyncRead();
    testHardwareFlowControl();
    
    cout << "\nTest completed!" << endl;
    return 0;
}
