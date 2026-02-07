// modules/hal/gpio.hpp
#ifndef MODULES_HAL_GPIO_HPP
#define MODULES_HAL_GPIO_HPP

#include <cstdint>

namespace system1::hal
{

/**
 * @brief Represents a generic GPIO pin.
 * Wouter van Ooijen style often templates this on Pin ID, but for 
 * runtime configuration via config files, a class is practical.
 */
class GpioPin
{
public:
    enum class Mode { Input, Output, Pwm };
    enum class State { Low, High };

    explicit GpioPin(int pin_id, Mode mode);

    void set(State state);
    State get() const;
    
    // Set PWM duty cycle (0.0 - 1.0)
    void set_pwm(float duty_cycle);

private:
    int id_;
};

} // namespace system1::hal

#endif // MODULES_HAL_GPIO_HPP
