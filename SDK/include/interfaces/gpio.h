#ifndef EXEC64_INTERFACES_GPIO_H
#define EXEC64_INTERFACES_GPIO_H

#include <exec/exec.h>

/* wiringPi-compatible constants */
#define INPUT             0
#define OUTPUT            1
#define PWM_OUTPUT        2
#define GPIO_CLOCK        3
#define SOFT_PWM_OUTPUT   4
#define SOFT_TONE_OUTPUT  5
#define PWM_MS_OUTPUT     6

#define LOW               0
#define HIGH              1

#define PUD_OFF           0
#define PUD_DOWN          1
#define PUD_UP            2

struct GPIOInterface {
    struct Interface i;

    /* --- wiringPi Core --- */
    
    /* Setup */
    int  (*wiringPiSetup)(void);
    int  (*wiringPiSetupGpio)(void);
    int  (*wiringPiSetupPhys)(void);
    int  (*wiringPiSetupSys)(void);

    /* GPIO */
    void (*pinMode)(int pin, int mode);
    void (*pullUpDnControl)(int pin, int pud);
    void (*digitalWrite)(int pin, int value);
    void (*pwmWrite)(int pin, int value);
    int  (*digitalRead)(int pin);
    
    /* Analog */
    void (*analogWrite)(int pin, int value);
    int  (*analogRead)(int pin);

    /* --- wiringPi Helpers --- */
    
    /* Timing */
    void     (*delay)(unsigned int howLong);
    void     (*delayMicroseconds)(unsigned int howLong);
    unsigned int (*millis)(void);
    unsigned int (*micros)(void);

    /* Priority/Interrupts */
    int  (*piHiPri)(int priority);
    int  (*wiringPiISR)(int pin, int mode, void (*function)(void));

    /* --- Software Modules --- */
    
    /* Soft PWM */
    int  (*softPwmCreate)(int pin, int value, int range);
    void (*softPwmWrite)(int pin, int value);
    void (*softPwmStop)(int pin);

    /* Soft Tone */
    int  (*softToneCreate)(int pin);
    void (*softToneWrite)(int pin, int freq);
    void (*softToneStop)(int pin);

    /* --- System Extensions --- */
    void (*setPadControl)(int pin, uint32_t flags);
    void (*pwmSetMode)(int mode);
    void (*pwmSetRange)(unsigned int range);
    void (*pwmSetClock)(int divisor);
};

#endif
