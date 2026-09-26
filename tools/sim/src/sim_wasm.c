// WebAssembly entry points for the simulator page.
#include <emscripten/emscripten.h>
#include <stdint.h>
#include "ar_sim.h"

static float block[AR_BLOCK];
static uint8_t packet[AR_PACKET_SIZE];
static ar_output_t out;

EMSCRIPTEN_KEEPALIVE float *sim_block_buffer(void) { return block; }
EMSCRIPTEN_KEEPALIVE uint8_t *sim_packet(void) { return packet; }
EMSCRIPTEN_KEEPALIVE ar_output_t *sim_ar_output(void) { return &out; }

EMSCRIPTEN_KEEPALIVE void sim_ar_init(int squelch, int gain, int agc, int input_level)
{
    const ar_config_t c = { .squelch = squelch, .gain = gain, .agc = agc, .input_level = input_level };
    ar_init(&c);
}

// Process the AR_BLOCK samples in sim_block_buffer(); the packet is in sim_packet().
EMSCRIPTEN_KEEPALIVE void sim_ar_block(void) { ar_process_block(block, packet, &out); }
