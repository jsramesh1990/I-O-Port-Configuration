/**
 * GPIO Input Example
 * 
 * Demonstrates reading a button or switch input
 * Connect button between GPIO23 (pin 16) and GND
 * Enable internal pull-up resistor
 */

#include <iostream>
#include <chrono>
#include <thread>
#include <signal.h>
#include "gpio.hpp"

static volatile bool running = true;

void signalHandler(int signum) {
    std::cout << "\nExiting..." << std::endl;
    running = false;
}

int main() {
    signal(SIGINT, signalHandler);
    
    std::cout << "=== GPIO Input Example ===" << std::endl;
    std::cout << "Button connected to GPIO23 (pin 16)" << std::endl;
    std::cout << "Press button to see state change" << std::endl;
    std::cout << "Press Ctrl+C to exit" << std::endl;
    std::cout << std::endl;
    
    try {
        // Initialize GPIO23 as input with pull-up
        GPIO button(23, GPIO::Direction::INPUT);
        button.setPull(GPIO::Pull::PULL_UP);
        
        bool last_state = false;
        int press_count = 0;
        
        while(running) {
            // Read button state (LOW when pressed due to pull-up)
            bool pressed = (button.read() == GPIO::Value::LOW);
            
            if(pressed && !last_state) {
                // Button just pressed
                press_count++;
                std::cout << "Button PRESSED! Count: " << press_count << std::endl;
            } else if(!pressed && last_state) {
                // Button just released
                std::cout << "Button released" << std::endl;
            }
            
            last_state = pressed;
            
            // Small delay to debounce
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
    } catch(const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
