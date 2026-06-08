#include <iostream>
#include <chrono>
#include <thread>
#include <cmath>
#include "pwm.hpp"

using namespace std;

void testBasicPWM() {
    cout << "=== PWM Basic Test ===" << endl;
    
    try {
        PWM pwm("/sys/class/pwm/pwmchip0", 0);
        
        cout << "Configuring PWM..." << endl;
        pwm.setFrequency(1000);  // 1 kHz
        pwm.setDutyCyclePercent(50);  // 50% duty cycle
        
        cout << "Frequency: " << pwm.getFrequency() << " Hz" << endl;
        cout << "Duty cycle: " << pwm.getDutyCyclePercent() << "%" << endl;
        
        cout << "Enabling PWM..." << endl;
        pwm.enable();
        
        cout << "Running for 3 seconds..." << endl;
        this_thread::sleep_for(chrono::seconds(3));
        
        cout << "Disabling PWM..." << endl;
        pwm.disable();
        
        cout << "✓ PWM test passed" << endl;
        
    } catch(const exception& e) {
        cerr << "Error: " << e.what() << endl;
    }
}

void testPWMFrequency() {
    cout << "\n=== PWM Frequency Test ===" << endl;
    
    try {
        PWM pwm("/sys/class/pwm/pwmchip0", 0);
        
        vector<double> frequencies = {10, 50, 100, 500, 1000, 5000, 10000, 50000};
        
        for(double freq : frequencies) {
            cout << "Testing " << freq << " Hz... ";
            
            pwm.setFrequency(freq);
            pwm.setDutyCyclePercent(50);
            pwm.enable();
            
            this_thread::sleep_for(chrono::milliseconds(500));
            
            double actual_freq = pwm.getFrequency();
            double error = abs(actual_freq - freq) / freq * 100;
            
            if(error < 5) {
                cout << "OK (Actual: " << actual_freq << " Hz, Error: " << error << "%)" << endl;
            } else {
                cout << "WARN (Actual: " << actual_freq << " Hz, Error: " << error << "%)" << endl;
            }
            
            pwm.disable();
        }
        
    } catch(const exception& e) {
        cerr << "Error: " << e.what() << endl;
    }
}

void testPWMDutyCycle() {
    cout << "\n=== PWM Duty Cycle Test ===" << endl;
    
    try {
        PWM pwm("/sys/class/pwm/pwmchip0", 0);
        
        pwm.setFrequency(1000);
        pwm.enable();
        
        for(int duty = 0; duty <= 100; duty += 10) {
            cout << "Duty cycle: " << duty << "% ";
            pwm.setDutyCyclePercent(duty);
            this_thread::sleep_for(chrono::milliseconds(500));
            
            double actual_duty = pwm.getDutyCyclePercent();
            cout << "(Actual: " << actual_duty << "%)" << endl;
        }
        
        pwm.disable();
        
    } catch(const exception& e) {
        cerr << "Error: " << e.what() << endl;
    }
}

void testPWMFade() {
    cout << "\n=== PWM Fade Test ===" << endl;
    
    try {
        PWM pwm("/sys/class/pwm/pwmchip0", 0);
        
        pwm.setFrequency(1000);
        pwm.enable();
        
        cout << "Fading in (0% to 100% over 2 seconds)..." << endl;
        pwm.fadeTo(100, 2000);
        
        cout << "Fading out (100% to 0% over 2 seconds)..." << endl;
        pwm.fadeTo(0, 2000);
        
        pwm.disable();
        
        cout << "✓ Fade test passed" << endl;
        
    } catch(const exception& e) {
        cerr << "Error: " << e.what() << endl;
    }
}

void testPWMServo() {
    cout << "\n=== PWM Servo Motor Test ===" << endl;
    
    try {
        PWM pwm("/sys/class/pwm/pwmchip0", 0);
        
        // Standard servo: 50Hz, 1ms=0°, 2ms=180°
        pwm.setFrequency(50);
        pwm.enable();
        
        cout << "Moving servo to center..." << endl;
        pwm.setDutyCyclePercent(7.5);  // 1.5ms pulse
        
        this_thread::sleep_for(chrono::seconds(2));
        
        cout << "Moving servo to 0 degrees..." << endl;
        pwm.setDutyCyclePercent(5);  // 1.0ms pulse
        
        this_thread::sleep_for(chrono::seconds(2));
        
        cout << "Moving servo to 180 degrees..." << endl;
        pwm.setDutyCyclePercent(10);  // 2.0ms pulse
        
        this_thread::sleep_for(chrono::seconds(2));
        
        cout << "Returning to center..." << endl;
        pwm.setDutyCyclePercent(7.5);
        
        this_thread::sleep_for(chrono::seconds(1));
        
        pwm.disable();
        
        cout << "✓ Servo test passed" << endl;
        
    } catch(const exception& e) {
        cerr << "Error: " << e.what() << endl;
    }
}

void testSoftwarePWM() {
    cout << "\n=== Software PWM Test ===" << endl;
    
    try {
        PWM pwm(18);  // Use GPIO18 for software PWM
        
        cout << "Using software PWM on GPIO18" << endl;
        
        pwm.setFrequency(100);
        pwm.setDutyCyclePercent(50);
        pwm.enable();
        
        cout << "Software PWM running for 3 seconds..." << endl;
        this_thread::sleep_for(chrono::seconds(3));
        
        pwm.disable();
        
        cout << "✓ Software PWM test passed" << endl;
        
    } catch(const exception& e) {
        cerr << "Error: " << e.what() << endl;
    }
}

int main() {
    cout << "PWM Test Suite for Jetson Orin Nano" << endl;
    cout << "===================================" << endl;
    cout << "Connect an oscilloscope or LED to PWM0 (pin 32)" << endl;
    
    testBasicPWM();
    testPWMFrequency();
    testPWMDutyCycle();
    testPWMFade();
    testPWMServo();
    testSoftwarePWM();
    
    cout << "\nTest completed!" << endl;
    return 0;
}
