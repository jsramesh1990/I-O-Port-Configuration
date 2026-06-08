
## docs/pwm.md

```markdown
# PWM (Pulse Width Modulation)

## Overview

The Jetson Orin Nano provides dedicated PWM channels for precise control of servos, LEDs, motors, and other analog-like devices. Software PWM fallback is available for non-PWM pins.

## Hardware Specifications

| Parameter | Value |
|-----------|-------|
| Dedicated PWM Channels | 2 (PWM0, PWM1) |
| Software PWM Channels | Up to 28 |
| Frequency Range | 1 Hz - 100 kHz |
| Duty Cycle Resolution | 0.1% (hardware) |
| Voltage Level | 3.3V |
| Max Output Current | 2mA typical |
| Hardware Timer Resolution | 1 ns |
| Software Timer Resolution | ~10 μs |

## Pin Mapping

### Dedicated PWM Pins (40-pin header)
```
Pin 32 (PWM0)  ─── PWM channel 0
Pin 33 (PWM1)  ─── PWM channel 1
```

### Alternative PWM Capable Pins (Software PWM)
```
Pin 11 (GPIO17)  ─── Software PWM capable
Pin 12 (GPIO18)  ─── Software PWM capable
Pin 13 (GPIO27)  ─── Software PWM capable
Pin 15 (GPIO22)  ─── Software PWM capable
Pin 16 (GPIO23)  ─── Software PWM capable
Pin 18 (GPIO24)  ─── Software PWM capable
Pin 22 (GPIO25)  ─── Software PWM capable
Pin 29 (GPIO5)   ─── Software PWM capable
Pin 31 (GPIO6)   ─── Software PWM capable
Pin 35 (GPIO19)  ─── Software PWM capable
Pin 36 (GPIO16)  ─── Software PWM capable
Pin 37 (GPIO26)  ─── Software PWM capable
Pin 38 (GPIO20)  ─── Software PWM capable
Pin 40 (GPIO21)  ─── Software PWM capable
```

## PWM Controllers

### Hardware PWM (PWM0/PWM1)
```
Base Address: 0x3280000 (PWM0)
              0x3290000 (PWM1)

Registers:
Offset 0x00: PWM_CONTROL
  Bit 0:   PWM_ENABLE
  Bit 1:   PWM_POLARITY
  Bit 2-3: PWM_MODE (0=oneshot, 1=continuous)
  
Offset 0x04: PWM_DUTY_CYCLE
  Value range: 0 - PERIOD
  
Offset 0x08: PWM_PERIOD
  Value range: 1 - 0xFFFFFFFF (in nanoseconds)
  
Offset 0x0C: PWM_COUNTER
  Current counter value (read-only)
```

## Implementation

### Hardware PWM Class
```cpp
class PWMManager {
private:
    std::string chip_path;
    unsigned int channel;
    int period_ns;
    int duty_ns;
    bool enabled;
    bool inverted;
    
    std::string getChannelPath() const {
        return chip_path + "/pwm" + std::to_string(channel);
    }
    
    bool writeToFile(const std::string& filename, const std::string& value) {
        std::string path = getChannelPath() + "/" + filename;
        std::ofstream file(path);
        if(!file.is_open()) return false;
        file << value;
        return file.good();
    }
    
    std::string readFromFile(const std::string& filename) {
        std::string path = getChannelPath() + "/" + filename;
        std::ifstream file(path);
        std::string value;
        if(file.is_open()) {
            std::getline(file, value);
        }
        return value;
    }
    
public:
    PWMManager(const std::string& chip, unsigned int ch) 
        : chip_path(chip), channel(ch), period_ns(1000000), 
          duty_ns(500000), enabled(false), inverted(false) {
        
        // Export channel if not already exported
        std::ofstream export_file(chip_path + "/export");
        if(export_file.is_open()) {
            export_file << channel;
            export_file.close();
            usleep(100000);  // Wait for sysfs to create files
        }
        
        // Set period
        writeToFile("period", std::to_string(period_ns));
        
        // Set initial duty cycle
        writeToFile("duty_cycle", std::to_string(duty_ns));
        
        // Set polarity (active high by default)
        if(inverted) {
            writeToFile("polarity", "inversed");
        } else {
            writeToFile("polarity", "normal");
        }
    }
    
    ~PWMManager() {
        disable();
        
        // Unexport channel
        std::ofstream unexport_file(chip_path + "/unexport");
        if(unexport_file.is_open()) {
            unexport_file << channel;
        }
    }
    
    bool enable() {
        if(writeToFile("enable", "1")) {
            enabled = true;
            return true;
        }
        return false;
    }
    
    bool disable() {
        if(writeToFile("enable", "0")) {
            enabled = false;
            return true;
        }
        return false;
    }
    
    bool isEnabled() const {
        return enabled;
    }
    
    bool setPeriod(uint64_t period_nanoseconds) {
        if(period_nanoseconds < 100) period_nanoseconds = 100;  // 10MHz max
        if(writeToFile("period", std::to_string(period_nanoseconds))) {
            period_ns = period_nanoseconds;
            return true;
        }
        return false;
    }
    
    bool setFrequency(double frequency_hz) {
        if(frequency_hz <= 0) return false;
        uint64_t period_nanoseconds = (uint64_t)(1000000000.0 / frequency_hz);
        return setPeriod(period_nanoseconds);
    }
    
    bool setDutyCycle(uint64_t duty_nanoseconds) {
        if(duty_nanoseconds > period_ns) duty_nanoseconds = period_ns;
        if(writeToFile("duty_cycle", std::to_string(duty_nanoseconds))) {
            duty_ns = duty_nanoseconds;
            return true;
        }
        return false;
    }
    
    bool setDutyCyclePercent(double percent) {
        if(percent < 0) percent = 0;
        if(percent > 100) percent = 100;
        uint64_t duty_nanoseconds = (uint64_t)((percent / 100.0) * period_ns);
        return setDutyCycle(duty_nanoseconds);
    }
    
    bool setPolarity(bool invert) {
        std::string polarity = invert ? "inversed" : "normal";
        if(writeToFile("polarity", polarity)) {
            inverted = invert;
            return true;
        }
        return false;
    }
    
    uint64_t getPeriod() const {
        return period_ns;
    }
    
    uint64_t getDutyCycle() const {
        return duty_ns;
    }
    
    double getFrequency() const {
        return 1000000000.0 / period_ns;
    }
    
    double getDutyCyclePercent() const {
        return (duty_ns * 100.0) / period_ns;
    }
    
    static std::vector<unsigned int> getAvailableChannels(const std::string& chip) {
        std::vector<unsigned int> channels;
        std::string npwm_path = chip + "/npwm";
        std::ifstream npwm_file(npwm_path);
        int num_pwms;
        if(npwm_file >> num_pwms) {
            for(int i = 0; i < num_pwms; i++) {
                // Check if channel exists
                std::string channel_path = chip + "/pwm" + std::to_string(i);
                if(access(channel_path.c_str(), F_OK) == 0) {
                    channels.push_back(i);
                }
            }
        }
        return channels;
    }
};
```

### Software PWM Implementation
```cpp
class SoftwarePWM {
private:
    GPIO gpio;
    std::thread pwm_thread;
    std::atomic<bool> running;
    std::atomic<uint64_t> period_ns;
    std::atomic<uint64_t> duty_ns;
    std::condition_variable cv;
    std::mutex mtx;
    
    void pwmLoop() {
        auto last_time = std::chrono::high_resolution_clock::now();
        
        while(running) {
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - last_time).count();
            
            if(elapsed >= period_ns) {
                // Start new period
                last_time = now;
                gpio.write(GPIO::Value::HIGH);
                
                // Schedule duty cycle end
                auto duty_end = now + std::chrono::nanoseconds(duty_ns);
                std::this_thread::sleep_until(duty_end);
                
                if(running) {
                    gpio.write(GPIO::Value::LOW);
                }
            }
            
            // Small sleep to prevent CPU saturation
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        
        gpio.write(GPIO::Value::LOW);
    }
    
public:
    SoftwarePWM(unsigned int pin) 
        : gpio(pin, GPIO::Direction::OUTPUT), 
          running(false), 
          period_ns(1000000), 
          duty_ns(500000) {
        gpio.write(GPIO::Value::LOW);
    }
    
    ~SoftwarePWM() {
        stop();
    }
    
    void start() {
        if(running) return;
        running = true;
        pwm_thread = std::thread(&SoftwarePWM::pwmLoop, this);
    }
    
    void stop() {
        if(!running) return;
        running = false;
        cv.notify_all();
        if(pwm_thread.joinable()) {
            pwm_thread.join();
        }
    }
    
    void setFrequency(double frequency_hz) {
        if(frequency_hz <= 0) return;
        period_ns = (uint64_t)(1000000000.0 / frequency_hz);
    }
    
    void setDutyCyclePercent(double percent) {
        if(percent < 0) percent = 0;
        if(percent > 100) percent = 100;
        duty_ns = (uint64_t)((percent / 100.0) * period_ns);
    }
    
    bool isRunning() const {
        return running;
    }
};
```

## Device Drivers

### Servo Motor Control
```cpp
class ServoMotor {
private:
    PWMManager pwm;
    double min_pulse_us;
    double max_pulse_us;
    double max_angle;
    
    double usToDutyCycle(double pulse_us) {
        double period_us = 20000.0;  // 50Hz = 20ms period
        return (pulse_us / period_us) * 100.0;
    }
    
public:
    ServoMotor(const std::string& chip, unsigned int channel,
               double min_pulse = 500.0,   // 0.5ms for 0 degrees
               double max_pulse = 2500.0,  // 2.5ms for 180 degrees
               double max_angle = 180.0)
        : pwm(chip, channel), min_pulse_us(min_pulse), 
          max_pulse_us(max_pulse), max_angle(max_angle) {
        
        pwm.setFrequency(50);  // 50Hz standard servo frequency
        pwm.enable();
    }
    
    void setAngle(double angle) {
        if(angle < 0) angle = 0;
        if(angle > max_angle) angle = max_angle;
        
        double pulse_us = min_pulse_us + (angle / max_angle) * (max_pulse_us - min_pulse_us);
        double duty_percent = usToDutyCycle(pulse_us);
        pwm.setDutyCyclePercent(duty_percent);
    }
    
    void center() {
        setAngle(max_angle / 2.0);
    }
    
    void sweep(int duration_ms = 2000) {
        auto start = std::chrono::high_resolution_clock::now();
        
        while(true) {
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
            
            if(elapsed >= duration_ms) break;
            
            double angle = (std::sin(2 * M_PI * elapsed / duration_ms) + 1) / 2.0 * max_angle;
            setAngle(angle);
            
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        
        center();
    }
};
```

### LED Dimming with Fade Effect
```cpp
class LEDDimming {
private:
    PWMManager pwm;
    std::thread fade_thread;
    std::atomic<bool> fading;
    
public:
    LEDDimming(const std::string& chip, unsigned int channel)
        : pwm(chip, channel) {
        pwm.setFrequency(1000);  // 1kHz for LED (no visible flicker)
        pwm.enable();
    }
    
    void setBrightness(double percent) {
        pwm.setDutyCyclePercent(percent);
    }
    
    void fadeTo(double target_percent, int duration_ms) {
        double start_percent = pwm.getDutyCyclePercent();
        double delta = target_percent - start_percent;
        auto start_time = std::chrono::high_resolution_clock::now();
        
        while(true) {
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
            
            if(elapsed >= duration_ms) break;
            
            double percent = start_percent + delta * (elapsed / (double)duration_ms);
            pwm.setDutyCyclePercent(percent);
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        pwm.setDutyCyclePercent(target_percent);
    }
    
    void startBreathing(int period_ms = 3000) {
        fading = true;
        fade_thread = std::thread([this, period_ms]() {
            while(fading) {
                fadeTo(100, period_ms / 2);
                fadeTo(0, period_ms / 2);
            }
        });
    }
    
    void stopBreathing() {
        fading = false;
        if(fade_thread.joinable()) {
            fade_thread.join();
        }
        setBrightness(0);
    }
    
    ~LEDDimming() {
        stopBreathing();
    }
};
```

### DC Motor Speed Control
```cpp
class DCMotor {
private:
    PWMManager pwm;
    GPIO direction_pin;
    GPIO enable_pin;
    double max_speed_rpm;
    double current_speed;
    
public:
    DCMotor(const std::string& chip, unsigned int pwm_channel,
            unsigned int dir_pin, unsigned int en_pin,
            double max_rpm = 3000)
        : pwm(chip, pwm_channel),
          direction_pin(dir_pin, GPIO::Direction::OUTPUT),
          enable_pin(en_pin, GPIO::Direction::OUTPUT),
          max_speed_rpm(max_rpm), current_speed(0) {
        
        pwm.setFrequency(20000);  // 20kHz (above audible range)
        pwm.setDutyCyclePercent(0);
        pwm.enable();
        
        enable_pin.write(GPIO::Value::HIGH);  // Enable motor driver
    }
    
    void setSpeed(double rpm) {
        if(rpm < 0) {
            direction_pin.write(GPIO::Value::LOW);  // Reverse
            rpm = -rpm;
        } else {
            direction_pin.write(GPIO::Value::HIGH); // Forward
        }
        
        if(rpm > max_speed_rpm) rpm = max_speed_rpm;
        
        double duty_percent = (rpm / max_speed_rpm) * 100.0;
        pwm.setDutyCyclePercent(duty_percent);
        current_speed = rpm;
    }
    
    void stop() {
        pwm.setDutyCyclePercent(0);
        current_speed = 0;
    }
    
    void brake() {
        // Implement braking (motor driver specific)
        pwm.setDutyCyclePercent(0);
        enable_pin.write(GPIO::Value::LOW);
        usleep(100000);
        enable_pin.write(GPIO::Value::HIGH);
    }
    
    void accelerateTo(double target_rpm, int duration_ms) {
        double start_speed = current_speed;
        double delta = target_rpm - start_speed;
        auto start_time = std::chrono::high_resolution_clock::now();
        
        while(true) {
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
            
            if(elapsed >= duration_ms) break;
            
            double rpm = start_speed + delta * (elapsed / (double)duration_ms);
            setSpeed(rpm);
            
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        setSpeed(target_rpm);
    }
};
```

### RGB LED Control
```cpp
class RGBLED {
private:
    PWMManager red;
    PWMManager green;
    PWMManager blue;
    
    uint8_t gammaCorrection(uint8_t value) {
        // Apply gamma correction for linear brightness perception
        float v = value / 255.0f;
        v = powf(v, 2.2f);
        return (uint8_t)(v * 255);
    }
    
public:
    RGBLED(const std::string& chip, unsigned int r_ch, 
           unsigned int g_ch, unsigned int b_ch)
        : red(chip, r_ch), green(chip, g_ch), blue(chip, b_ch) {
        
        red.setFrequency(1000);
        green.setFrequency(1000);
        blue.setFrequency(1000);
        
        red.enable();
        green.enable();
        blue.enable();
    }
    
    void setColor(uint8_t r, uint8_t g, uint8_t b) {
        r = gammaCorrection(r);
        g = gammaCorrection(g);
        b = gammaCorrection(b);
        
        red.setDutyCyclePercent((r / 255.0) * 100);
        green.setDutyCyclePercent((g / 255.0) * 100);
        blue.setDutyCyclePercent((b / 255.0) * 100);
    }
    
    void setColor(uint32_t hex_color) {
        uint8_t r = (hex_color >> 16) & 0xFF;
        uint8_t g = (hex_color >> 8) & 0xFF;
        uint8_t b = hex_color & 0xFF;
        setColor(r, g, b);
    }
    
    void setHSV(double hue, double saturation, double value) {
        // Convert HSV to RGB
        double c = value * saturation;
        double x = c * (1 - fabs(fmod(hue / 60.0, 2) - 1));
        double m = value - c;
        
        double r, g, b;
        if(hue < 60) { r = c; g = x; b = 0; }
        else if(hue < 120) { r = x; g = c; b = 0; }
        else if(hue < 180) { r = 0; g = c; b = x; }
        else if(hue < 240) { r = 0; g = x; b = c; }
        else if(hue < 300) { r = x; g = 0; b = c; }
        else { r = c; g = 0; b = x; }
        
        setColor((r + m) * 255, (g + m) * 255, (b + m) * 255);
    }
    
    void rainbow(int duration_ms = 2000) {
        auto start = std::chrono::high_resolution_clock::now();
        
        while(true) {
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
            
            if(elapsed >= duration_ms) break;
            
            double hue = fmod(elapsed * 360.0 / duration_ms, 360.0);
            setHSV(hue, 1.0, 0.5);
            
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
};
```

## Performance Optimization

### High-Frequency PWM (100kHz+)
```cpp
class HighFrequencyPWM {
private:
    int fd;
    void* reg_base;
    
    // Memory-mapped register access for high speed
    void writeRegister(uint32_t offset, uint32_t value) {
        *(volatile uint32_t*)((uint8_t*)reg_base + offset) = value;
    }
    
    uint32_t readRegister(uint32_t offset) {
        return *(volatile uint32_t*)((uint8_t*)reg_base + offset);
    }
    
public:
    HighFrequencyPWM(unsigned int channel) {
        // Map PWM registers directly
        int mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
        uint32_t base_addr = (channel == 0) ? 0x3280000 : 0x3290000;
        reg_base = mmap(NULL, 0x1000, PROT_READ | PROT_WRITE,
                        MAP_SHARED, mem_fd, base_addr);
        close(mem_fd);
    }
    
    void configure(uint32_t period_ns, uint32_t duty_ns) {
        // Direct register access (no sysfs overhead)
        writeRegister(0x08, period_ns);    // Set period
        writeRegister(0x04, duty_ns);      // Set duty cycle
        writeRegister(0x00, 0x01);         // Enable PWM
    }
    
    void setDuty(uint32_t duty_ns) {
        writeRegister(0x04, duty_ns);
    }
    
    ~HighFrequencyPWM() {
        munmap(reg_base, 0x1000);
    }
};
```

### Precision Timing with Hardware Timers
```cpp
class PrecisePWM {
private:
    int timer_fd;
    struct itimerspec timer_spec;
    
    void setupHardwareTimer(uint64_t period_ns) {
        timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
        
        timer_spec.it_interval.tv_sec = period_ns / 1000000000;
        timer_spec.it_interval.tv_nsec = period_ns % 1000000000;
        timer_spec.it_value = timer_spec.it_interval;
        
        timerfd_settime(timer_fd, 0, &timer_spec, NULL);
    }
    
public:
    PrecisePWM(unsigned int pin, uint64_t period_ns) 
        : gpio(pin, GPIO::Direction::OUTPUT) {
        setupHardwareTimer(period_ns);
        
        std::thread([this]() {
            uint64_t expirations;
            bool state = false;
            
            while(true) {
                read(timer_fd, &expirations, sizeof(expirations));
                state = !state;
                gpio.write(state ? GPIO::Value::HIGH : GPIO::Value::LOW);
            }
        }).detach();
    }
};
```

## Troubleshooting Guide

### Common Issues

1. **No PWM output**
   - Check pin mux configuration
   - Verify PWM channel is exported
   - Check with oscilloscope

2. **Incorrect frequency**
   - Verify period calculation
   - Check clock source frequency
   - Hardware limitations (min/max period)

3. **Jittery output**
   - Use hardware PWM instead of software
   - Increase CPU priority for PWM thread
   - Reduce system load

### Debug Commands
```bash
# List PWM chips
ls /sys/class/pwm/

# Check PWM channel status
cat /sys/class/pwm/pwmchip0/pwm0/enable
cat /sys/class/pwm/pwmchip0/pwm0/period
cat /sys/class/pwm/pwmchip0/pwm0/duty_cycle

# Test PWM with direct sysfs access
echo 0 > /sys/class/pwm/pwmchip0/export
echo 1000000 > /sys/class/pwm/pwmchip0/pwm0/period
echo 500000 > /sys/class/pwm/pwmchip0/pwm0/duty_cycle
echo 1 > /sys/class/pwm/pwmchip0/pwm0/enable

# Monitor CPU usage for software PWM
top -p $(pidof your_app)
```

## Best Practices

1. **Use hardware PWM** for critical timing applications
2. **Keep PWM frequency above 1kHz** to avoid visible flicker
3. **Add buffer circuits** for driving high-current loads
4. **Use optocouplers** for motor control to protect Jetson
5. **Implement soft-start** for motors to reduce current surge
6. **Monitor temperature** when driving heavy loads
7. **Use differential signaling** for long PWM cables
8. **Add flyback diodes** for inductive loads

## Industrial Applications

### PID Motor Speed Controller
```cpp
class PIDController {
private:
    double kp, ki, kd;
    double integral;
    double previous_error;
    double setpoint;
    std::chrono::steady_clock::time_point last_time;
    
public:
    PIDController(double p, double i, double d) 
        : kp(p), ki(i), kd(d), integral(0), previous_error(0), setpoint(0) {
        last_time = std::chrono::steady_clock::now();
    }
    
    double calculate(double measurement) {
        auto now = std::chrono::steady_clock::now();
        double dt = std::chrono::duration<double>(now - last_time).count();
        last_time = now;
        
        double error = setpoint - measurement;
        
        // Proportional term
        double p_term = kp * error;
        
        // Integral term with anti-windup
        integral += error * dt;
        double i_term = ki * integral;
        
        // Derivative term
        double derivative = (error - previous_error) / dt;
        double d_term = kd * derivative;
        
        previous_error = error;
        
        // Limit output to 0-100%
        double output = p_term + i_term + d_term;
        if(output > 100) output = 100;
        if(output < 0) output = 0;
        
        return output;
    }
    
    void setSetpoint(double sp) {
        setpoint = sp;
        integral = 0;  // Reset integral on setpoint change
    }
};

class MotorSpeedControl {
private:
    DCMotor motor;
    PIDController pid;
    std::thread control_thread;
    std::atomic<bool> running;
    double target_speed;
    
public:
    MotorSpeedControl(DCMotor& m, double p, double i, double d)
        : motor(m), pid(p, i, d), running(false), target_speed(0) {}
    
    void start() {
        running = true;
        control_thread = std::thread([this]() {
            while(running) {
                double current_speed = readEncoderSpeed();  // Implement encoder reading
                double output = pid.calculate(current_speed);
                motor.setSpeed(output);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }
    
    void setSpeed(double rpm) {
        target_speed = rpm;
        pid.setSetpoint(rpm);
    }
    
    void stop() {
        running = false;
        if(control_thread.joinable()) control_thread.join();
        motor.stop();
    }
    
private:
    double readEncoderSpeed() {
        // Implement encoder reading logic
        return 0.0;
    }
};
```

