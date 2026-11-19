/*
 * Project: Virtual Robot Console — Autonomous Obstacle Avoidance
 * MCU: PIC16F877A (4 MHz Crystal)
 * Compiler: MPLAB XC8
 * Author: Afnan Aref (Saba University - Mechatronics Engineering)
 */

#define _XTAL_FREQ 4000000UL
#include <xc.h>
#include <stdint.h>
#include <stdbool.h>

// Configuration Bits
#pragma config FOSC = HS
#pragma config WDTE = OFF
#pragma config PWRTE = ON
#pragma config BOREN = ON
#pragma config LVP = OFF
#pragma config CPD = OFF
#pragma config WRT = OFF
#pragma config CP = OFF

// Pin Definitions
#define LCD_RS_BIT    PORTBbits.RB2 // PIN 21
#define LCD_EN_BIT    PORTBbits.RB3 // PIN 22
#define TRIG_BIT      PORTAbits.RA0 // PIN 2
#define ECHO_BIT      PORTAbits.RA1 // PIN 3
#define LED_FRONT_BIT PORTCbits.RC6 // PIN 17
#define LED_BACK_BIT  PORTCbits.RC7 // PIN 18

// ---------------- LCD Driver (4-bit Mode on PORTB) ----------------

static void lcd_pulse_en(void) {
    LCD_EN_BIT = 1;
    __delay_us(2);
    LCD_EN_BIT = 0;
    __delay_us(40);
}

static void lcd_write_nibble(uint8_t nibble) {
    uint8_t low = PORTB & 0x0F;
    uint8_t towrite = (low & 0x0F) | ((uint8_t)((nibble & 0x0F) << 4));
    PORTB = towrite;
    lcd_pulse_en();
}

static void lcd_send_byte(uint8_t b, uint8_t rs) {
    LCD_RS_BIT = rs ? 1 : 0;
    lcd_write_nibble((uint8_t)((b >> 4) & 0x0F));
    lcd_write_nibble((uint8_t)(b & 0x0F));
    __delay_us(50);
}

static void lcd_cmd(uint8_t c) {
    lcd_send_byte(c, 0);
}

static void lcd_char(uint8_t ch) {
    lcd_send_byte(ch, 1);
}

static void lcd_init(void) {
    __delay_ms(20);
    lcd_write_nibble(0x03);
    __delay_ms(5);
    lcd_write_nibble(0x03);
    __delay_us(150);
    lcd_write_nibble(0x03);
    lcd_write_nibble(0x02);
    lcd_cmd(0x28); // 4-bit, 2 lines
    lcd_cmd(0x0C); // Display on, cursor off
    lcd_cmd(0x06); // Entry mode
    lcd_cmd(0x01); // Clear display
    __delay_ms(2);
}

static void lcd_puts(const char *s) {
    while (*s) {
        lcd_char((uint8_t)*s++);
    }
}

static void lcd_set_cursor(uint8_t row, uint8_t col) {
    uint8_t addr = (row == 0) ? 0x00 : 0x40;
    addr += col;
    lcd_cmd(0x80 | addr);
}

static void lcd_print_uint16(uint16_t val) {
    if (val == 0xFFFF) {
        lcd_puts("Err");
        return;
    }

    char buffer[6];
    uint8_t i = 0;

    if (val == 0) {
        lcd_char('0');
        return;
    }

    while (val > 0 && i < 5) {
        buffer[i++] = '0' + (val % 10);
        val /= 10;
    }

    for (int8_t j = i - 1; j >= 0; j--) {
        lcd_char(buffer[j]);
    }
}

// ---------------- LED Indicators ----------------

static void led_front_on(void) {
    LED_FRONT_BIT = 1;
    LED_BACK_BIT = 0;
}

static void led_back_on(void) {
    LED_BACK_BIT = 1;
    LED_FRONT_BIT = 0;
}

static void leds_off(void) {
    LED_FRONT_BIT = 0;
    LED_BACK_BIT = 0;
}

static void leds_on(void) {
    LED_FRONT_BIT = 1;
    LED_BACK_BIT = 1;
}

static void leds_blink(void) {
    LED_FRONT_BIT = 1;
    LED_BACK_BIT = 1;
    __delay_ms(100);
    LED_FRONT_BIT = 0;
    LED_BACK_BIT = 0;
    __delay_ms(100);
}

// ---------------- Ultrasonic Sensor (HC-SR04 + Timer1) ----------------

static uint16_t measure_distance_cm(void) {
    TRIG_BIT = 1;
    __delay_us(10);
    TRIG_BIT = 0;

    uint32_t timeout = 30000UL;
    while (!ECHO_BIT) {
        if (--timeout == 0) return 0xFFFF;
        __delay_us(1);
    }

    TMR1H = 0;
    TMR1L = 0;
    T1CON = 0x01; // Start Timer1

    timeout = 60000UL;
    while (ECHO_BIT) {
        if (--timeout == 0) {
            T1CONbits.TMR1ON = 0;
            return 0xFFFF;
        }
        __delay_us(1);
    }

    T1CONbits.TMR1ON = 0;
    uint16_t ticks = ((uint16_t)TMR1H << 8) | TMR1L;
    uint32_t distance = (uint32_t)ticks * 17UL / 1000UL;
    return (uint16_t)distance;
}

// ---------------- Motor Control (L293D on PORTC) ----------------

static void motors_stop(void) {
    PORTCbits.RC0 = 0;
    PORTCbits.RC1 = 0;
    PORTCbits.RC2 = 0;
    PORTCbits.RC3 = 0;
    PORTCbits.RC4 = 0;
    PORTCbits.RC5 = 0;
    leds_off();
}

static void motors_forward(void) {
    PORTCbits.RC1 = 1;
    PORTCbits.RC2 = 0;
    PORTCbits.RC0 = 1;

    PORTCbits.RC4 = 1;
    PORTCbits.RC5 = 0;
    PORTCbits.RC3 = 1;

    led_front_on();
}

static void motors_slow(void) {
    PORTCbits.RC1 = 1;
    PORTCbits.RC2 = 0;
    PORTCbits.RC0 = 1;

    PORTCbits.RC4 = 1;
    PORTCbits.RC5 = 0;
    PORTCbits.RC3 = 1;

    leds_blink();
}

static void motors_back(void) {
    PORTCbits.RC1 = 0;
    PORTCbits.RC2 = 1;
    PORTCbits.RC0 = 1;

    PORTCbits.RC4 = 0;
    PORTCbits.RC5 = 1;
    PORTCbits.RC3 = 1;

    led_back_on();
}

// ---------------- Diagnostics & Initialization ----------------

static void test_leds_detailed(void) {
    lcd_set_cursor(0, 0);
    lcd_puts("Testing LEDs... ");

    lcd_set_cursor(1, 0);
    lcd_puts("Front LED: ON   ");
    led_front_on();
    __delay_ms(800);
    leds_off();
    __delay_ms(300);

    lcd_set_cursor(1, 0);
    lcd_puts("Back LED: ON    ");
    led_back_on();
    __delay_ms(800);
    leds_off();
    __delay_ms(300);

    lcd_set_cursor(1, 0);
    lcd_puts("Both LEDs: ON   ");
    leds_on();
    __delay_ms(800);
    leds_off();
    __delay_ms(300);

    lcd_set_cursor(1, 0);
    lcd_puts("LEDs Blinking   ");
    for (uint8_t i = 0; i < 5; i++) {
        leds_blink();
    }

    lcd_set_cursor(1, 0);
    lcd_puts("LED Test: PASS  ");
    __delay_ms(1000);
}

static void system_init(void) {
    ADCON1 = 0x06; // Configure PORTA as digital I/O

    // PORTA: RA0 output (TRIG), RA1 input (ECHO)
    TRISAbits.TRISA0 = 0;
    TRISAbits.TRISA1 = 1;
    PORTA = 0x00;

    // PORTB: LCD
    TRISB = 0x00;
    PORTB = 0x00;

    // PORTC: Motors & LEDs
    TRISC = 0x00;
    PORTC = 0x00;

    INTCON = 0x00;
    T1CON = 0x00;

    lcd_init();
    leds_off();
}

// ---------------- Main Control Loop ----------------

int main(void) {
    system_init();
    test_leds_detailed();

    lcd_cmd(0x01);
    __delay_ms(2);
    lcd_set_cursor(0, 0);
    lcd_puts("Distance:");
    lcd_set_cursor(1, 0);
    lcd_puts("Status: Ready   ");

    while (1) {
        uint16_t distance = measure_distance_cm();

        lcd_set_cursor(0, 10);
        lcd_print_uint16(distance);
        lcd_puts("cm   ");

        if (distance == 0xFFFF) {
            motors_stop();
            lcd_set_cursor(1, 0);
            lcd_puts("Status: SensorErr");
        } else if (distance > 50) {
            motors_forward();
            lcd_set_cursor(1, 0);
            lcd_puts("Status: Forward ");
        } else if (distance > 30) {
            motors_slow();
            lcd_set_cursor(1, 0);
            lcd_puts("Status: Slow    ");
        } else {
            motors_back();
            lcd_set_cursor(1, 0);
            lcd_puts("Status: Back    ");
        }

        __delay_ms(100);
    }

    return 0;
}
