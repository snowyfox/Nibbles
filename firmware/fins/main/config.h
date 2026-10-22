// Fin sensor node settings.
#pragma once

#define FINS_FW_BUILD        1

// MT6701 magnetic angle sensor on its analog output (the breakout's I2C
// doesn't answer: it is set up for analog). GPIO 4 = ADC1 channel 3.
#define SENSOR_ADC_CHANNEL   ADC_CHANNEL_3
#define SAMPLE_MS            10      // a reading every 10 ms...
#define SAMPLES_PER_READ     16      // ...averaging this many conversions
#define SMOOTH_ALPHA         0.35f   // then smoothed (1 = none)

#define SEND_MS              20      // fin pose broadcast, 50 Hz
#define START_CHANNEL        11      // where to start scanning for WLED's channel
