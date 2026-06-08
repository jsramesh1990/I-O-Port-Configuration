/**
 * PWM Servo Control Example
 * 
 * Demonstrates controlling a servo motor with PWM
 * Connect servo signal to PWM0 (pin 32)
 * Power servo with 5V external supply
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
    
    std::cout << "=== PWM Servo Control Example ===" << std::endl;
    std::cout << "PWM channel: 0 (pin 32)" << std::endl;
    std::cout << "Servo: Standard 50Hz (20ms period)" << std::endl;
    std::cout << "Press Ctrl+C to exit" << std::endl;
    std::cout << std::endl;
    
    try {
        PWM pwm("/sys/class/pwm/pwmchip0", 0);
        
        // Configure for servo (50Hz = 20ms period)
        pwm.setFrequency(50);
        pwm.enable();
        
        std::cout << "Servo initialized at 50Hz" << std::endl;
        
        // Sweep servo back and forth
        while(running) {
            // Sweep from 0 to 180 degrees
            for(int angle = 0; angle <= 180 && running; angle += 5) {
                // Convert angle to duty cycle
                // 0° = 1ms pulse (5% duty at 50Hz)
                // 180° = 2ms pulse (10% duty at 50Hz)
                double duty = 5.0 + (angle / 180.0) * 5.0;
                pwm.setDutyCyclePercent(duty);
                
                std::cout << "Angle: " << angle << "° (Duty: " << duty << "%)" << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            
            // Sweep back
            for(int angle = 180; angle >= 0 && running; angle -= 5) {
                double duty = 5.0 + (angle / 180.0) * 5.0;
                pwm.setDutyCyclePercent(duty);
                
                std::cout << "Angle: " << angle << "° (Duty: " << duty << "%)" << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        }
        
        // Center servo before exit
        pwm.setDutyCyclePercent(7.5);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        pwm.disable();
        std::cout << "\nServo centered and disabled." << std::endl;
        
    } catch(const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
