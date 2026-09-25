#include "nibbles_link.h"

#include <string.h>

_Static_assert(sizeof(nl_heartbeat_t) == 14, "nl_heartbeat_t layout");
_Static_assert(sizeof(nl_eye_telemetry_t) == 24, "nl_eye_telemetry_t layout");
_Static_assert(sizeof(nl_cmd_t) == 6, "nl_cmd_t layout");
_Static_assert(sizeof(nl_ack_t) == 3, "nl_ack_t layout");
_Static_assert(sizeof(nl_wled_telemetry_t) == 10, "nl_wled_telemetry_t layout");
_Static_assert(sizeof(nl_bump_t) == 8, "nl_bump_t layout");
_Static_assert(sizeof(nl_radio_hdr_t) == 10, "nl_radio_hdr_t layout");
_Static_assert(NL_RADIO_MAX <= 250, "ESP-NOW packets are at most 250 bytes");
_Static_assert(sizeof(nl_audio_t) == 36, "nl_audio_t layout");
_Static_assert(sizeof(nl_eye_state_t) == 80, "nl_eye_state_t layout");
_Static_assert(sizeof(nl_eye_state_t) <= NL_MAX_PAYLOAD, "payload too big");

uint16_t nl_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++) crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
    return crc;
}

// COBS: replace every 0x00 so the frame can be delimited by 0x00.
static size_t cobs_encode(const uint8_t *in, size_t len, uint8_t *out)
{
    size_t code_at = 0, o = 1;
    uint8_t code = 1;
    for (size_t i = 0; i < len; i++) {
        if (in[i] == 0) {
            out[code_at] = code;
            code_at = o++;
            code = 1;
        } else {
            out[o++] = in[i];
            if (++code == 0xFF) {
                out[code_at] = code;
                code_at = o++;
                code = 1;
            }
        }
    }
    out[code_at] = code;
    return o;
}

// Returns the decoded length, or 0 if the input is malformed.
static size_t cobs_decode(const uint8_t *in, size_t len, uint8_t *out, size_t cap)
{
    size_t i = 0, o = 0;
    while (i < len) {
        const uint8_t code = in[i++];
        if (code == 0) return 0;
        for (uint8_t k = 1; k < code; k++) {
            if (i >= len || o >= cap) return 0;
            out[o++] = in[i++];
        }
        if (code != 0xFF && i < len) {
            if (o >= cap) return 0;
            out[o++] = 0;
        }
    }
    return o;
}

size_t nl_encode(uint8_t type, uint16_t seq, const void *payload, size_t len, uint8_t *out, size_t cap)
{
    if (len > NL_MAX_PAYLOAD || cap < NL_MAX_FRAME) return 0;
    uint8_t raw[NL_MAX_RAW];
    raw[0] = type;
    raw[1] = NL_VERSION;
    raw[2] = (uint8_t)(seq & 0xFF);
    raw[3] = (uint8_t)(seq >> 8);
    if (len) memcpy(raw + NL_HEADER_LEN, payload, len);
    const uint16_t crc = nl_crc16(raw, NL_HEADER_LEN + len);
    raw[NL_HEADER_LEN + len] = (uint8_t)(crc & 0xFF);
    raw[NL_HEADER_LEN + len + 1] = (uint8_t)(crc >> 8);
    size_t n = cobs_encode(raw, NL_HEADER_LEN + len + NL_CRC_LEN, out);
    out[n++] = 0;
    return n;
}

void nl_parser_init(nl_parser_t *p)
{
    memset(p, 0, sizeof(*p));
}

bool nl_parse_byte(nl_parser_t *p, uint8_t byte, nl_frame_t *out)
{
    if (byte != 0) {
        if (p->n < sizeof(p->buf)) p->buf[p->n++] = byte;
        else p->overflow = true;
        return false;
    }
    // End of frame.
    const size_t n = p->n;
    const bool overflow = p->overflow;
    p->n = 0;
    p->overflow = false;
    if (n == 0) return false;  // back-to-back delimiters
    if (overflow) {
        p->bad_frames++;
        return false;
    }
    const size_t len = cobs_decode(p->buf, n, p->dec, sizeof(p->dec));
    if (len < NL_HEADER_LEN + NL_CRC_LEN) {
        p->bad_frames++;
        return false;
    }
    const uint16_t crc = (uint16_t)(p->dec[len - 2] | (p->dec[len - 1] << 8));
    if (nl_crc16(p->dec, len - NL_CRC_LEN) != crc) {
        p->crc_errors++;
        return false;
    }
    out->type = p->dec[0];
    out->version = p->dec[1];
    out->seq = (uint16_t)(p->dec[2] | (p->dec[3] << 8));
    out->payload = p->dec + NL_HEADER_LEN;
    out->len = len - NL_HEADER_LEN - NL_CRC_LEN;
    p->frames++;
    return true;
}

size_t nl_radio_build(uint8_t *out, size_t cap, uint8_t type, uint16_t seq, uint8_t role, uint8_t side,
                      const void *payload, size_t len)
{
    if (len > NL_MAX_PAYLOAD || cap < sizeof(nl_radio_hdr_t) + len) return 0;
    const nl_radio_hdr_t h = {
        .magic = { NL_RADIO_MAGIC0, NL_RADIO_MAGIC1 },
        .net_id = NL_NET_ID,
        .type = type,
        .version = NL_VERSION,
        .seq = seq,
        .role = role,
        .side = side,
    };
    memcpy(out, &h, sizeof(h));
    if (len) memcpy(out + sizeof(h), payload, len);
    return sizeof(h) + len;
}

bool nl_radio_parse(const uint8_t *in, size_t n, nl_radio_hdr_t *hdr, const uint8_t **payload, size_t *len)
{
    if (n < sizeof(nl_radio_hdr_t)) return false;
    memcpy(hdr, in, sizeof(*hdr));
    if (hdr->magic[0] != NL_RADIO_MAGIC0 || hdr->magic[1] != NL_RADIO_MAGIC1 || hdr->net_id != NL_NET_ID ||
        hdr->version != NL_VERSION)
        return false;
    *payload = in + sizeof(*hdr);
    *len = n - sizeof(*hdr);
    return true;
}
