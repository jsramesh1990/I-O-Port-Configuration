#include <iostream>
#include <chrono>
#include <thread>
#include "gpio.hpp"

using namespace std;

void testBasicIO() {
    cout << "=== GPIO Basic I/O Test ===" << endl;
    
    try {
        // Test output pin
        GPIO output(18, GPIO::Direction::OUTPUT);
        cout << "Testing GPIO18 as output..." << endl;
        
        for(int i = 0; i < 5; i++) {
            output.write(GPIO::Value::HIGH);
            cout << "  HIGH";
            this_thread::sleep_for(chrono::milliseconds(500));
            
            output.write(GPIO::Value::LOW);
            cout << "  LOW" << endl;
            this_thread::sleep_for(chrono::milliseconds(500));
        }
        
        cout << "✓ Output test passed" << endl;
        
        // Test input pin (requires external connection)
        GPIO input(23, GPIO::Direction::INPUT);
        input.setPull(GPIO::Pull::PULL_UP);
        
        cout << "Reading GPIO23 (connect to GND to test): ";
        for(int i = 0; i < 10; i++) {
            GPIO::Value val = input.read();
            cout << (val == GPIO::Value::HIGH ? "1" : "0") << " ";
            this_thread::sleep_for(chrono::milliseconds(200));
        }
        cout << endl;
        
    } catch(const exception& e) {
        cerr << "Error: " << e.what() << endl;
    }
}

void testInterrupt() {
    cout << "\n=== GPIO Interrupt Test ===" << endl;
    
    try {
        GPIO input(23, GPIO::Direction::INPUT);
        input.setEdge(GPIO::Edge::BOTH);
        
        int count = 0;
        input.registerCallback([&count]() {
            count++;
            cout << "Interrupt triggered! Count: " << count << endl;
        });
        
        input.enableInterrupts();
        
        cout << "Waiting for interrupts on GPIO23 (5 seconds)..." << endl;
        cout << "Connect a jumper wire to toggle the pin" << endl;
        
        this_thread::sleep_for(chrono::seconds(5));
        
        input.disableInterrupts();
        cout << "Total interrupts: " << count << endl;
        
    } catch(const exception& e) {
        cerr << "Error: " << e.what() << endl;
    }
}

void testPerformance() {
    cout << "\n=== GPIO Performance Test ===" << endl;
    
    try {
        GPIO output(18, GPIO::Direction::OUTPUT);
        
        auto start = chrono::high_resolution_clock::now();
        
        const int iterations = 1000000;
        for(int i = 0; i < iterations; i++) {
            output.fastWrite(GPIO::Value::HIGH);
            output.fastWrite(GPIO::Value::LOW);
        }
        
        auto end = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::microseconds>(end - start);
        
        double freq = (2.0 * iterations / duration.count()) * 1e6;
        cout << "Toggle frequency: " << freq / 1e6 << " MHz" << endl;
        cout << "Time per toggle: " << duration.count() / (2.0 * iterations) << " us" << endl;
        
    } catch(const exception& e) {
        cerr << "Error: " << e.what() << endl;
    }
}

int main() {
    cout << "GPIO Test Suite for Jetson Orin Nano" << endl;
    cout << "=====================================" << endl;
    
    testBasicIO();
    testInterrupt();
    testPerformance();
    
    cout << "\nTest completed!" << endl;
    return 0;
}
