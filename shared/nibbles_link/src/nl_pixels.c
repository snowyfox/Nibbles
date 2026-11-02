#include "nibbles_link.h"

#include <string.h>

size_t nl_pix_encode(uint8_t frame, uint16_t start, uint16_t total, bool last,
                     const uint8_t *rgb, size_t leds, uint8_t *out, size_t cap)
{
    if (!leds || leds > NL_PIX_CHUNK_LEDS || (size_t)start + leds > total || cap < NL_PIX_FRAME_MAX) return 0;
    uint8_t raw[NL_PIX_RAW_MAX];
    raw[0] = NL_MSG_PIXELS;
    raw[1] = NL_VERSION;
    raw[2] = frame;
    raw[3] = last ? NL_PIX_LAST : 0;
    raw[4] = (uint8_t)(start & 0xFF);
    raw[5] = (uint8_t)(start >> 8);
    raw[6] = (uint8_t)(total & 0xFF);
    raw[7] = (uint8_t)(total >> 8);
    memcpy(raw + NL_PIX_HEADER_LEN, rgb, leds * 3);
    const size_t len = NL_PIX_HEADER_LEN + leds * 3;
    const uint16_t crc = nl_crc16(raw, len);
    raw[len] = (uint8_t)(crc & 0xFF);
    raw[len + 1] = (uint8_t)(crc >> 8);
    size_t n = nl_cobs_encode(raw, len + NL_CRC_LEN, out);
    out[n++] = 0;
    return n;
}

void nl_pix_parser_init(nl_pix_parser_t *p)
{
    memset(p, 0, sizeof(*p));
}

size_t nl_relay_encode(const uint8_t mac[6], const uint8_t *pkt, size_t len, uint8_t *out, size_t cap)
{
    if (!len || len > NL_RELAY_PKT_MAX || cap < NL_RELAY_FRAME_MAX) return 0;
    uint8_t raw[NL_RELAY_RAW_MAX];
    raw[0] = NL_MSG_RELAY;
    raw[1] = NL_VERSION;
    memcpy(raw + 2, mac, 6);
    memcpy(raw + NL_RELAY_HEADER_LEN, pkt, len);
    const size_t n = NL_RELAY_HEADER_LEN + len;
    const uint16_t crc = nl_crc16(raw, n);
    raw[n] = (uint8_t)(crc & 0xFF);
    raw[n + 1] = (uint8_t)(crc >> 8);
    size_t k = nl_cobs_encode(raw, n + NL_CRC_LEN, out);
    out[k++] = 0;
    return k;
}

nl_wire_event_t nl_wire_parse_byte(nl_pix_parser_t *p, uint8_t byte, nl_pix_chunk_t *chunk, nl_relay_t *relay)
{
    if (byte != 0) {
        if (p->n < sizeof(p->buf)) p->buf[p->n++] = byte;
        else p->overflow = true;
        return NL_WIRE_NONE;
    }
    const size_t n = p->n;
    const bool overflow = p->overflow;
    p->n = 0;
    p->overflow = false;
    if (n == 0) return NL_WIRE_NONE;  // back-to-back delimiters
    if (overflow) {
        p->bad_chunks++;
        return NL_WIRE_NONE;
    }
    const size_t len = nl_cobs_decode(p->buf, n, p->dec, sizeof(p->dec));
    if (len < 2 + NL_CRC_LEN) {
        p->bad_chunks++;
        return NL_WIRE_NONE;
    }
    const uint16_t crc = (uint16_t)(p->dec[len - 2] | (p->dec[len - 1] << 8));
    if (nl_crc16(p->dec, len - NL_CRC_LEN) != crc) {
        p->crc_errors++;
        return NL_WIRE_NONE;
    }
    if (p->dec[1] != NL_VERSION) {
        p->bad_chunks++;
        return NL_WIRE_NONE;
    }
    if (p->dec[0] == NL_MSG_RELAY) {
        if (len <= NL_RELAY_HEADER_LEN + NL_CRC_LEN) {
            p->bad_chunks++;
            return NL_WIRE_NONE;
        }
        relay->mac = p->dec + 2;
        relay->pkt = p->dec + NL_RELAY_HEADER_LEN;
        relay->len = len - NL_RELAY_HEADER_LEN - NL_CRC_LEN;
        p->relays++;
        return NL_WIRE_RELAY;
    }
    if (p->dec[0] != NL_MSG_PIXELS || len < NL_PIX_HEADER_LEN + 3 + NL_CRC_LEN) {
        p->bad_chunks++;
        return NL_WIRE_NONE;
    }
    const size_t data = len - NL_PIX_HEADER_LEN - NL_CRC_LEN;
    const uint16_t start = (uint16_t)(p->dec[4] | (p->dec[5] << 8));
    const uint16_t total = (uint16_t)(p->dec[6] | (p->dec[7] << 8));
    if (data % 3 || (size_t)start + data / 3 > total) {
        p->bad_chunks++;
        return NL_WIRE_NONE;
    }
    chunk->frame = p->dec[2];
    chunk->flags = p->dec[3];
    chunk->start = start;
    chunk->total = total;
    chunk->count = (uint16_t)(data / 3);
    chunk->rgb = p->dec + NL_PIX_HEADER_LEN;
    p->chunks++;
    if (chunk->flags & NL_PIX_LAST) p->frames++;
    return NL_WIRE_CHUNK;
}

bool nl_pix_parse_byte(nl_pix_parser_t *p, uint8_t byte, nl_pix_chunk_t *out)
{
    nl_relay_t relay;
    return nl_wire_parse_byte(p, byte, out, &relay) == NL_WIRE_CHUNK;
}
