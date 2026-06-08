#ifndef JETSON_PWM_HPP
#define JETSON_PWM_HPP

#include <string>
#include <chrono>

class PWM {
public:
    enum class Polarity {
        NORMAL,
        INVERTED
    };
    
    PWM(const std::string& chip, unsigned int channel);
    explicit PWM(unsigned int pin);  // Auto-detect from pin number
    ~PWM();
    
    bool enable();
    void disable();
    bool isEnabled() const;
    
    // Configuration
    void setPeriod(uint64_t period_ns);
    void setFrequency(double frequency_hz);
    void setDutyCycle(double duty_cycle);  // 0.0 to 100.0
    void setDutyCycleNano(uint64_t duty_ns);
    void setPolarity(Polarity polarity);
    
    // Software PWM (fallback for non-PWM pins)
    void setSoftwareMode(bool enable);
    
    // Utility
    uint64_t getPeriod() const;
    uint64_t getDutyCycle() const;
    double getFrequency() const;
    double getDutyCyclePercent() const;
    
    static bool isCapablePin(unsigned int pin);
    static std::vector<unsigned int> getPwmPins();
    
private:
    std::string chip_path_;
    unsigned int channel_;
    bool enabled_;
    bool software_mode_;
    
    std::string getChannelPath() const;
    bool writeToFile(const std::string& filename, const std::string& value);
    std::string readFromFile(const std::string& filename);
};

#endif // JETSON_PWM_HPP
