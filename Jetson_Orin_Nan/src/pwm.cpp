#include "pwm.hpp"
#include <fstream>
#include <cmath>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

class SoftwarePWMImpl {
public:
    SoftwarePWMImpl(unsigned int pin) : gpio(pin, GPIO::Direction::OUTPUT), running(false) {
        gpio.write(GPIO::Value::LOW);
    }
    
    ~SoftwarePWMImpl() {
        stop();
    }
    
    void start() {
        if (running) return;
        running = true;
        pwm_thread = std::thread(&SoftwarePWMImpl::pwmLoop, this);
    }
    
    void stop() {
        if (!running) return;
        running = false;
        if (pwm_thread.joinable()) {
            pwm_thread.join();
        }
        gpio.write(GPIO::Value::LOW);
    }
    
    void setPeriod(uint64_t period_ns) {
        period_ns_ = period_ns;
        updateDuty();
    }
    
    void setDutyCycleNano(uint64_t duty_ns) {
        duty_ns_ = duty_ns;
        updateDuty();
    }
    
    bool isRunning() const { return running; }
    
private:
    GPIO gpio;
    std::thread pwm_thread;
    std::atomic<bool> running;
    std::atomic<uint64_t> period_ns_{1000000};
    std::atomic<uint64_t> duty_ns_{500000};
    
    void updateDuty() {
        // Will be used by pwmLoop
    }
    
    void pwmLoop() {
        auto last_time = std::chrono::high_resolution_clock::now();
        
        while (running) {
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - last_time).count();
            
            if (elapsed >= period_ns_) {
                last_time = now;
                gpio.write(GPIO::Value::HIGH);
                
                auto duty_end = now + std::chrono::nanoseconds(duty_ns_.load());
                std::this_thread::sleep_until(duty_end);
                
                if (running) {
                    gpio.write(GPIO::Value::LOW);
                }
            }
            
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
};

class HardwarePWMImpl {
public:
    HardwarePWMImpl(const std::string& chip, unsigned int channel) 
        : chip_path(chip), channel(channel), enabled(false) {
        
        // Export channel
        std::ofstream export_file(chip_path + "/export");
        if (export_file.is_open()) {
            export_file << channel;
            export_file.close();
            usleep(100000);
        }
        
        setPeriod(1000000);
        setDutyCycle(500000);
        setPolarity(false);
    }
    
    ~HardwarePWMImpl() {
        disable();
        
        // Unexport channel
        std::ofstream unexport_file(chip_path + "/unexport");
        if (unexport_file.is_open()) {
            unexport_file << channel;
        }
    }
    
    bool enable() {
        if (writeToFile("enable", "1")) {
            enabled = true;
            return true;
        }
        return false;
    }
    
    bool disable() {
        if (writeToFile("enable", "0")) {
            enabled = false;
            return true;
        }
        return false;
    }
    
    bool setPeriod(uint64_t period_ns) {
        if (writeToFile("period", std::to_string(period_ns))) {
            period_ns_ = period_ns;
            return true;
        }
        return false;
    }
    
    bool setDutyCycle(uint64_t duty_ns) {
        if (writeToFile("duty_cycle", std::to_string(duty_ns))) {
            duty_ns_ = duty_ns;
            return true;
        }
        return false;
    }
    
    bool setPolarity(bool inverted) {
        std::string polarity = inverted ? "inversed" : "normal";
        return writeToFile("polarity", polarity);
    }
    
    uint64_t getPeriod() const { return period_ns_; }
    uint64_t getDutyCycle() const { return duty_ns_; }
    bool isEnabled() const { return enabled; }
    
private:
    std::string chip_path;
    unsigned int channel;
    uint64_t period_ns_;
    uint64_t duty_ns_;
    bool enabled;
    
    std::string getChannelPath() const {
        return chip_path + "/pwm" + std::to_string(channel);
    }
    
    bool writeToFile(const std::string& filename, const std::string& value) {
        std::string path = getChannelPath() + "/" + filename;
        std::ofstream file(path);
        if (!file.is_open()) return false;
        file << value;
        return file.good();
    }
};

PWM::PWM(const std::string& chip, unsigned int channel)
    : chip_path(chip), channel(channel), enabled(false), current_mode_(Mode::HARDWARE) {
    hw_impl_ = std::make_unique<HardwarePWMImpl>(chip, channel);
}

PWM::PWM(unsigned int pin) : enabled(false) {
    if (isCapablePin(pin)) {
        current_mode_ = Mode::HARDWARE;
        // Determine chip and channel from pin
        chip_path = "/sys/class/pwm/pwmchip0";
        channel = 0;  // Simplified mapping
        hw_impl_ = std::make_unique<HardwarePWMImpl>(chip_path, channel);
    } else {
        current_mode_ = Mode::SOFTWARE;
        sw_impl_ = std::make_unique<SoftwarePWMImpl>(pin);
    }
}

PWM::~PWM() {
    disable();
}

bool PWM::enable() {
    if (current_mode_ == Mode::HARDWARE && hw_impl_) {
        return hw_impl_->enable();
    } else if (sw_impl_) {
        sw_impl_->start();
        enabled = true;
        return true;
    }
    return false;
}

void PWM::disable() {
    if (current_mode_ == Mode::HARDWARE && hw_impl_) {
        hw_impl_->disable();
    } else if (sw_impl_) {
        sw_impl_->stop();
    }
    enabled = false;
}

bool PWM::isEnabled() const {
    if (current_mode_ == Mode::HARDWARE && hw_impl_) {
        return hw_impl_->isEnabled();
    } else if (sw_impl_) {
        return sw_impl_->isRunning();
    }
    return false;
}

void PWM::toggle() {
    if (isEnabled()) {
        disable();
    } else {
        enable();
    }
}

void PWM::setPeriod(uint64_t period_ns) {
    if (current_mode_ == Mode::HARDWARE && hw_impl_) {
        hw_impl_->setPeriod(period_ns);
    } else if (sw_impl_) {
        sw_impl_->setPeriod(period_ns);
    }
    config_.frequency = 1000000000.0 / period_ns;
}

void PWM::setFrequency(double frequency_hz) {
    setPeriod(static_cast<uint64_t>(1000000000.0 / frequency_hz));
}

void PWM::setDutyCycle(double duty_cycle_percent) {
    uint64_t duty_ns = static_cast<uint64_t>((duty_cycle_percent / 100.0) * getPeriod());
    setDutyCycleNano(duty_ns);
    config_.duty_cycle = duty_cycle_percent;
}

void PWM::setDutyCycleNano(uint64_t duty_ns) {
    if (current_mode_ == Mode::HARDWARE && hw_impl_) {
        hw_impl_->setDutyCycle(duty_ns);
    } else if (sw_impl_) {
        sw_impl_->setDutyCycleNano(duty_ns);
    }
}

void PWM::setPolarity(Polarity polarity) {
    if (current_mode_ == Mode::HARDWARE && hw_impl_) {
        hw_impl_->setPolarity(polarity == Polarity::INVERTED);
    }
    config_.polarity = polarity;
}

void PWM::setMode(Mode mode) {
    if (mode == current_mode_) return;
    
    // Stop current mode
    disable();
    
    current_mode_ = mode;
    // Re-initialize with new mode
    // Implementation would need to recreate the appropriate impl
}

void PWM::setConfig(const Config& config) {
    config_ = config;
    setFrequency(config.frequency);
    setDutyCycle(config.duty_cycle);
    setPolarity(config.polarity);
    if (config.enabled && !isEnabled()) {
        enable();
    } else if (!config.enabled && isEnabled()) {
        disable();
    }
}

void PWM::fadeTo(double target_duty, int duration_ms) {
    double start_duty = getDutyCyclePercent();
    double delta = target_duty - start_duty;
    auto start_time = std::chrono::steady_clock::now();
    
    while (true) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
        
        if (elapsed >= duration_ms) break;
        
        double duty = start_duty + delta * (elapsed / (double)duration_ms);
        setDutyCycle(duty);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    setDutyCycle(target_duty);
}

void PWM::frequencySweep(double start_freq, double end_freq, int duration_ms) {
    auto start_time = std::chrono::steady_clock::now();
    
    while (true) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
        
        if (elapsed >= duration_ms) break;
        
        double freq = start_freq + (end_freq - start_freq) * (elapsed / (double)duration_ms);
        setFrequency(freq);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    setFrequency(end_freq);
}

void PWM::startBreathing(int period_ms) {
    // Implementation would start a thread for breathing effect
}

void PWM::stopBreathing() {
    // Implementation would stop breathing thread
}

void PWM::setBurstMode(int pulses, double on_time_ms, double off_time_ms) {
    // Implementation for burst mode
}

void PWM::setPulseTrain(const std::vector<double>& duty_cycle_sequence,
                        const std::vector<int>& duration_ms) {
    // Implementation for pulse train
}

uint64_t PWM::getPeriod() const {
    if (current_mode_ == Mode::HARDWARE && hw_impl_) {
        return hw_impl_->getPeriod();
    }
    return 0;
}

uint64_t PWM::getDutyCycle() const {
    if (current_mode_ == Mode::HARDWARE && hw_impl_) {
        return hw_impl_->getDutyCycle();
    }
    return 0;
}

double PWM::getFrequency() const {
    return 1000000000.0 / getPeriod();
}

double PWM::getDutyCyclePercent() const {
    uint64_t period = getPeriod();
    if (period == 0) return 0;
    return (getDutyCycle() * 100.0) / period;
}

PWM::Polarity PWM::getPolarity() const {
    return config_.polarity;
}

PWM::Mode PWM::getMode() const {
    return current_mode_;
}

PWM::Config PWM::getConfig() const {
    return config_;
}

void PWM::onCycleComplete(CycleCallback callback) {
    // Implementation for cycle completion callback
}

bool PWM::isCapablePin(unsigned int pin) {
    // List of hardware PWM pins on Jetson Orin Nano
    static const std::vector<unsigned int> pwm_pins = {32, 33};
    return std::find(pwm_pins.begin(), pwm_pins.end(), pin) != pwm_pins.end();
}

std::vector<unsigned int> PWM::getHardwarePwmPins() {
    return {32, 33};
}

std::vector<unsigned int> PWM::getSoftwarePwmPins() {
    std::vector<unsigned int> pins;
    for (unsigned int i = 0; i < 40; i++) {
        if (i != 32 && i != 33 && i != 0) {
            pins.push_back(i);
        }
    }
    return pins;
}

std::vector<std::string> PWM::getAvailableChips() {
    std::vector<std::string> chips;
    for (int i = 0; i < 4; i++) {
        std::string path = "/sys/class/pwm/pwmchip" + std::to_string(i);
        if (access(path.c_str(), R_OK) == 0) {
            chips.push_back(path);
        }
    }
    return chips;
}
