// The 3.49's PCF85063 clock on its own bus (GPIO 47/48). Both of the S3's I2C
// controllers are taken (touch, power board), and the clock is only read at
// boot and written when set, so it's driven by software. The clock chip says
// when it has lost power; then the time isn't known until it's set again.
#pragma once

#include <stdbool.h>
#include <stdint.h>

bool rtc_start(void);             // reads the chip and sets the system time; true if it's good
bool rtc_time_known(void);
uint32_t rtc_now(void);           // Unix time, 0 if not known
bool rtc_set(uint32_t unix_time); // sets the chip and the system time
bool rtc_chip_found(void);
