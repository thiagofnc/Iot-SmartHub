#include "Motor.h"

Motor::Motor() {
    pos = 0;
    // VERY IMPORTANT: GPIO 19 is the USB_D- pin on the ESP32-S3! 
    // Because ARDUINO_USB_CDC_ON_BOOT=1 is set, GPIO 19 is locked for USB Serial.
    // Changed to a safe GPIO like GPIO 42. Please move the wire to GPIO 42 (Pin 7 on Header J3).
    motorPin = 42; 
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
    // attach(pin, minMicroseconds, maxMicroseconds)
    servo.attach(motorPin, 500, 2500);
}

void Motor::rotate(int deg) {
    // Keep the requested angle safely within the servo's physical 0-270 degree limits
    deg = constrain(deg, 0, 270);
    pos = deg;
    
    Serial.printf("Motor rotating to %d degrees\n", deg);
    
    // Map the 0-270 degree angle to the corresponding 500-2500 microsecond pulse width
    int pulseWidth = map(deg, 0, 270, 500, 2500);
    
    // Use writeMicroseconds for precise control over the 270-degree range
    servo.writeMicroseconds(pulseWidth);
}
