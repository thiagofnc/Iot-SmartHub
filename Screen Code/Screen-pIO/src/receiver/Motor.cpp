#include "Motor.h"

Motor::Motor() {
    pos = 0;
    // VERY IMPORTANT: GPIO 19 is the USB_D- pin on the ESP32-S3! 
    // Because ARDUINO_USB_CDC_ON_BOOT=1 is set, GPIO 19 is locked for USB Serial.
    // Changed to a safe GPIO like GPIO 42. Please move the wire to GPIO 42 (Pin 7 on Header J3).
    motorPin = 41; 
}

void Motor::init() {
    // Allow allocation of all timers for ESP32Servo
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    ESP32PWM::allocateTimer(2);
    ESP32PWM::allocateTimer(3);

    init(motorPin);
}

void Motor::init(int pin) {
    motorPin = pin;
    Serial.printf("Motor initialized on pin %d\n", motorPin);
    
    // Standard analog servos run at 50Hz
    servo.setPeriodHertz(50);
    
    // Hiwonder 20KG 270-degree servos typically use a pulse width of 500us to 2500us
    // You can gently tweak these min/max values if the 0 and 90-degree positions are off a bit.
    // E.g., if 0 relates to 500us, you might change it to 550us if it goes too far.
    servo.attach(motorPin, 500, 2500);
    
    // Command the motor to strictly go to the 0 position immediately on startup
    rotate(0);
}

void Motor::rotate(int deg) {
    // Keep the requested angle safely within the servo's physical 0-90 degree limits
    deg = constrain(deg, 0, 90);
    
    Serial.printf("Motor rotating from %d to %d degrees\n", pos, deg);
    
    // Move to the target angle slowly
    if (pos != deg) {
        int step = (deg > pos) ? 1 : -1;
        for (int currentDeg = pos; currentDeg != deg; currentDeg += step) {
            // Tweak to values around 500 and 1050 to adjust for the 0 and 90 degrees positions
            int pulseWidth = map(currentDeg, 0, 90, 500, 1200);
            servo.writeMicroseconds(pulseWidth);
            delay(10); // 20ms delay per degree. Increase for slower, decrease for faster.
        }
    }
    
    // Ensure it reaches the exact final position
    int finalPulseWidth = map(deg, 0, 90, 500, 1150);
    servo.writeMicroseconds(finalPulseWidth);
    
    pos = deg;
}
