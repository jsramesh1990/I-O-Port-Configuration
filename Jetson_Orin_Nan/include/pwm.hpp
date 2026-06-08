#ifndef JETSON_PWM_HPP
#define JETSON_PWM_HPP

#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <functional>

class PWM {
public:
    enum class Polarity {
        NORMAL,
        INVERTED
    };
    
    enum class Mode {
        HARDWARE,    // Dedicated PWM hardware
        SOFTWARE     // GPIO bit-banging
    };
    
    struct Config {
        double frequency = 1000.0;      // Hz
        double duty_cycle = 50.0;       // Percent (0-100)
        Polarity polarity = Polarity::NORMAL;
        Mode mode = Mode::HARDWARE;
        bool enabled = false;
    };
    
    // Constructor/Destructor
    PWM(const std::string& chip, unsigned int channel);
    explicit PWM(unsigned int pin);  // Auto-detect from pin number
    ~PWM();
    
    // Basic control
    bool enable();
    void disable();
    bool isEnabled() const;
    void toggle();
    
    // Configuration
    void setPeriod(uint64_t period_ns);
    void setFrequency(double frequency_hz);
    void setDutyCycle(double duty_cycle_percent);
    void setDutyCycleNano(uint64_t duty_ns);
    void setPolarity(Polarity polarity);
    void setMode(Mode mode);
    void setConfig(const Config& config);
    
    // Ramping/fading
    void fadeTo(double target_duty, int duration_ms);
    void frequencySweep(double start_freq, double end_freq, int duration_ms);
    void startBreathing(int period_ms = 3000);
    void stopBreathing();
    
    // Preset patterns
    void setBurstMode(int pulses, double on_time_ms, double off_time_ms);
    void setPulseTrain(const std::vector<double>& duty_cycle_sequence,
                      const std::vector<int>& duration_ms);
    
    // Reading current state
    uint64_t getPeriod() const;
    uint64_t getDutyCycle() const;
    double getFrequency() const;
    double getDutyCyclePercent() const;
    Polarity getPolarity() const;
    Mode getMode() const;
    Config getConfig() const;
    
    // Callbacks for synchronization
    using CycleCallback = std::function<void(uint64_t cycle_count)>;
    void onCycleComplete(CycleCallback callback);
    
    // Utility
    unsigned int getChannel() const { return channel_; }
    std::string getChip() const { return chip_path_; }
    
    // Static methods
    static bool isCapablePin(unsigned int pin);
    static std::vector<unsigned int> getHardwarePwmPins();
    static std::vector<unsigned int> getSoftwarePwmPins();
    static std::vector<std::string> getAvailableChips();
    
private:
    std::string chip_path_;
    unsigned int channel_;
    Config config_;
    bool enabled_;
    Mode current_mode_;
    std::unique_ptr<class SoftwarePWMImpl> sw_impl_;
    std::unique_ptr<class HardwarePWMImpl> hw_impl_;
    
    std::string getChannelPath() const;
    bool writeToFile(const std::string& filename, const std::string& value);
    std::string readFromFile(const std::string& filename);
    void initHardware();
    void initSoftware(unsigned int pin);
};

#endif // JETSON_PWM_HPP
