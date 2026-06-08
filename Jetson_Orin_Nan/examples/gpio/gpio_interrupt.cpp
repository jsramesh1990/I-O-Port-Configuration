/**
 * GPIO Interrupt Example
 * 
 * Demonstrates GPIO interrupt handling
 * Connect motion sensor or button to GPIO23 (pin 16)
 */

#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <signal.h>
#include "gpio.hpp"

static volatile bool running = true;
static std::atomic<int> interrupt_count(0);

void signalHandler(int signum) {
    running = false;
}

void interruptCallback() {
    interrupt_count++;
    std::cout << "Interrupt triggered! Total: " << interrupt_count << std::endl;
}

int main() {
    signal(SIGINT, signalHandler);
    
    std::cout << "=== GPIO Interrupt Example ===" << std::endl;
    std::cout << "Interrupt on GPIO23 (pin 16)" << std::endl;
    std::cout << "Configured for BOTH edges" << std::endl;
    std::cout << "Press Ctrl+C to exit" << std::endl;
    std::cout << std::endl;
    
    try {
        // Initialize GPIO23 as input with interrupt
        GPIO sensor(23, GPIO::Direction::INPUT);
        sensor.setPull(GPIO::Pull::PULL_UP);
        sensor.setEdge(GPIO::Edge::BOTH);
        
        // Register callback
        sensor.registerCallback(interruptCallback);
        
        // Enable interrupts
        sensor.enableInterrupts();
        
        std::cout << "Waiting for interrupts..." << std::endl;
        
        // Main loop - just wait
        while(running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Print status every second
            static auto last_print = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            
            if(now - last_print >= std::chrono::seconds(1)) {
                std::cout << "Active - Interrupts: " << interrupt_count << std::endl;
                last_print = now;
            }
        }
        
        // Cleanup
        sensor.disableInterrupts();
        sensor.unregisterCallback();
        
        std::cout << "\nTotal interrupts received: " << interrupt_count << std::endl;
        
    } catch(const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
