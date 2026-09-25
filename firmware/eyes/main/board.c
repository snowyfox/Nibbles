#include "board.h"

#include <string.h>
#include "config.h"
#include "esp_mac.h"

nl_side_t board_side(void)
{
    static nl_side_t side = NL_SIDE_NONE;
    if (side == NL_SIDE_NONE) {
        static const uint8_t port[][6] = PORT_EYE_MACS;
        uint8_t mac[6];
        side = NL_SIDE_STARBOARD;
        if (esp_efuse_mac_get_default(mac) == ESP_OK) {
            for (size_t i = 0; i < sizeof(port) / sizeof(port[0]); i++)
                if (memcmp(mac, port[i], 6) == 0) side = NL_SIDE_PORT;
        }
    }
    return side;
}

nl_role_t board_role(void)
{
    return board_side() == NL_SIDE_PORT ? NL_ROLE_EYE_EARS : NL_ROLE_EYE_LEADER;
}
