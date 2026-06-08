/**
 * PWM LED Dimming Example
 * 
 * Demonstrates LED brightness control with PWM fading effects
 * Connect LED with resistor (330Ω) between PWM0 (pin 32) and GND
 */

#include <iostream>
#include <cmath>
#include <thread>
#include <signal.h>
#include "pwm.hpp"

static volatile bool running = true;

void signalHandler(int signum) {
    running = false;
}

int main() {
    signal(SIGINT, signalHandler);
    
    std::cout << "=== PWM LED Dimming Example ===" << std::endl;
    std::cout << "PWM channel: 0 (pin 32)" << std::endl;
    std::cout << "LED connected with 330Ω resistor" << std::endl;
    std::cout << "Press Ctrl+C to exit" << std::endl;
    std::cout << std::endl;
    
    try {
        PWM pwm("/sys/class/pwm/pwmchip0", 0);
        
        // Configure for LED (1kHz to avoid flicker)
        pwm.setFrequency(1000);
        pwm.enable();
        
        std::cout << "LED PWM initialized at 1kHz" << std::endl;
        
        while(running) {
            // Fade in
            std::cout << "Fading in..." << std::endl;
            for(int brightness = 0; brightness <= 100 && running; brightness += 2) {
                pwm.setDutyCyclePercent(brightness);
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // Fade out
            std::cout << "Fading out..." << std::endl;
            for(int brightness = 100; brightness >= 0 && running; brightness -= 2) {
                pwm.setDutyCyclePercent(brightness);
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // Breathing effect (sinusoidal)
            std::cout << "Breathing effect..." << std::endl;
            auto start = std::chrono::steady_clock::now();
            
            while(running) {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
                
                if(elapsed > 8000) break;
                
                double brightness = 50 + 50 * sin(elapsed * M_PI * 2 / 4000);
                pwm.setDutyCyclePercent(brightness);
                
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            
            std::cout << std::endl;
        }
        
        // Turn off LED before exit
        pwm.setDutyCyclePercent(0);
        pwm.disable();
        std::cout << "\nLED turned off." << std::endl;
        
    } catch(const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
