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

bool nl_pix_parse_byte(nl_pix_parser_t *p, uint8_t byte, nl_pix_chunk_t *out)
{
    if (byte != 0) {
        if (p->n < sizeof(p->buf)) p->buf[p->n++] = byte;
        else p->overflow = true;
        return false;
    }
    const size_t n = p->n;
    const bool overflow = p->overflow;
    p->n = 0;
    p->overflow = false;
    if (n == 0) return false;  // back-to-back delimiters
    if (overflow) {
        p->bad_chunks++;
        return false;
    }
    const size_t len = nl_cobs_decode(p->buf, n, p->dec, sizeof(p->dec));
    if (len < NL_PIX_HEADER_LEN + 3 + NL_CRC_LEN) {
        p->bad_chunks++;
        return false;
    }
    const uint16_t crc = (uint16_t)(p->dec[len - 2] | (p->dec[len - 1] << 8));
    if (nl_crc16(p->dec, len - NL_CRC_LEN) != crc) {
        p->crc_errors++;
        return false;
    }
    const size_t data = len - NL_PIX_HEADER_LEN - NL_CRC_LEN;
    const uint16_t start = (uint16_t)(p->dec[4] | (p->dec[5] << 8));
    const uint16_t total = (uint16_t)(p->dec[6] | (p->dec[7] << 8));
    if (p->dec[0] != NL_MSG_PIXELS || p->dec[1] != NL_VERSION || data % 3 || (size_t)start + data / 3 > total) {
        p->bad_chunks++;
        return false;
    }
    out->frame = p->dec[2];
    out->flags = p->dec[3];
    out->start = start;
    out->total = total;
    out->count = (uint16_t)(data / 3);
    out->rgb = p->dec + NL_PIX_HEADER_LEN;
    p->chunks++;
    if (out->flags & NL_PIX_LAST) p->frames++;
    return true;
}
