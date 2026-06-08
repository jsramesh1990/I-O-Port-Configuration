/**
 * UART Baud Rate Test Example
 * 
 * Tests multiple baud rates and measures error rates
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>
#include "uart.hpp"

struct BaudRateTest {
    int baudrate;
    bool passed;
    double error_percent;
    int bytes_sent;
    int bytes_received;
};

int main() {
    std::cout << "=== UART Baud Rate Test ===" << std::endl;
    std::cout << "Connect TX to RX for loopback test" << std::endl;
    std::cout << std::endl;
    
    std::vector<int> baudrates = {
        9600, 19200, 38400, 57600, 115200, 
        230400, 460800, 921600, 1000000, 1500000, 2000000
    };
    
    std::vector<BaudRateTest> results;
    
    // Test pattern
    std::vector<uint8_t> test_data;
    for(int i = 0; i < 256; i++) {
        test_data.push_back(i);
    }
    
    for(int baudrate : baudrates) {
        std::cout << "Testing " << baudrate << " baud..." << std::flush;
        
        try {
            UART uart("/dev/ttyTHS1", baudrate);
            
            if(!uart.open()) {
                std::cout << " FAIL (cannot open)" << std::endl;
                continue;
            }
            
            // Send test data
            auto start = std::chrono::high_resolution_clock::now();
            ssize_t sent = uart.write(test_data.data(), test_data.size());
            
            if(sent != (ssize_t)test_data.size()) {
                std::cout << " FAIL (write error)" << std::endl;
                uart.close();
                continue;
            }
            
            // Read back data
            std::vector<uint8_t> received;
            received.resize(test_data.size());
            
            ssize_t received_bytes = 0;
            auto timeout_start = std::chrono::high_resolution_clock::now();
            
            while(received_bytes < (ssize_t)test_data.size()) {
                auto now = std::chrono::high_resolution_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - timeout_start);
                
                if(elapsed.count() > 5000) {
                    break;
                }
                
                ssize_t bytes = uart.readTimeout(received.data() + received_bytes, 
                                                test_data.size() - received_bytes, 100);
                if(bytes > 0) {
                    received_bytes += bytes;
                }
            }
            
            uart.close();
            
            // Verify data
            bool match = (received_bytes == (ssize_t)test_data.size());
            if(match) {
                for(size_t i = 0; i < test_data.size() && match; i++) {
                    if(received[i] != test_data[i]) {
                        match = false;
                    }
                }
            }
            
            // Calculate effective bit rate
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            double effective_rate = (test_data.size() * 8.0) / (duration.count() / 1000000.0);
            double error = std::abs(effective_rate - baudrate) / baudrate * 100;
            
            BaudRateTest result;
            result.baudrate = baudrate;
            result.passed = match;
            result.error_percent = error;
            result.bytes_sent = sent;
            result.bytes_received = received_bytes;
            results.push_back(result);
            
            if(match) {
                std::cout << " PASS (error: " << std::fixed << std::setprecision(2) 
                         << error << "%, rate: " << (int)effective_rate << " bps)" << std::endl;
            } else {
                std::cout << " FAIL (sent: " << sent << ", recv: " << received_bytes << ")" << std::endl;
            }
            
        } catch(const std::exception& e) {
            std::cout << " ERROR: " << e.what() << std::endl;
        }
    }
    
    // Print summary
    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << std::left << std::setw(12) << "Baud Rate" 
              << std::setw(10) << "Status" 
              << std::setw(12) << "Error %" << std::endl;
    std::cout << std::string(34, '-') << std::endl;
    
    for(const auto& result : results) {
        std::cout << std::left << std::setw(12) << result.baudrate
                  << std::setw(10) << (result.passed ? "PASS" : "FAIL")
                  << std::setw(12) << std::fixed << std::setprecision(2) 
                  << result.error_percent << std::endl;
    }
    
    return 0;
}
