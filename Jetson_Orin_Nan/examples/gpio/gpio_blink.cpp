/**
 * GPIO Blink Example
 * 
 * Demonstrates basic GPIO output to blink an LED
 * Connect LED with resistor (330Ω) between GPIO18 (pin 12) and GND
 */

#include <iostream>
#include <chrono>
#include <thread>
#include <signal.h>
#include "gpio.hpp"

static volatile bool running = true;

void signalHandler(int signum) {
    std::cout << "\nInterrupt signal received. Stopping..." << std::endl;
    running = false;
}

int main() {
    // Register signal handler for Ctrl+C
    signal(SIGINT, signalHandler);
    
    std::cout << "=== GPIO Blink Example ===" << std::endl;
    std::cout << "LED connected to GPIO18 (pin 12)" << std::endl;
    std::cout << "Press Ctrl+C to exit" << std::endl;
    
    try {
        // Initialize GPIO18 as output
        GPIO led(18, GPIO::Direction::OUTPUT, GPIO::Value::LOW);
        
        int blink_count = 0;
        
        while(running) {
            // Turn LED on
            led.write(GPIO::Value::HIGH);
            std::cout << "LED ON  (count: " << ++blink_count << ")" << std::endl;
            
            // Wait 500ms
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // Turn LED off
            led.write(GPIO::Value::LOW);
            std::cout << "LED OFF" << std::endl;
            
            // Wait 500ms
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        
        // Cleanup
        led.write(GPIO::Value::LOW);
        std::cout << "\nLED turned off. Exiting." << std::endl;
        
    } catch(const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
