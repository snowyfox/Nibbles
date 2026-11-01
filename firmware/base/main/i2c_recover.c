#include "power.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "i2c_private.h"

// ESP-IDF 5.5.5's I2C master only clears contains_read when a read completes.
// After a read that doesn't (a device gone, a loose cable) it stays set, and
// the next transaction's interrupt stores through stale read state: a
// StoreProhibited panic, seen on a probe right after a keypad board was lost.
// A read to an absent device can even return ESP_OK with it still set, so this
// runs after every read, whatever it returned; a completed read has cleared it anyway.
void power_bus_after_read(void)
{
    i2c_master_bus_handle_t b = power_bus();
    if (!b || xSemaphoreTake(b->bus_lock_mux, pdMS_TO_TICKS(100)) != pdTRUE) return;
    b->contains_read = false;
    b->read_len_static = 0;
    xSemaphoreGive(b->bus_lock_mux);
}
