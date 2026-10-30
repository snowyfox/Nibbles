#include "nibbles_link.h"

#include <string.h>

_Static_assert(sizeof(nl_heartbeat_t) == 14, "nl_heartbeat_t layout");
_Static_assert(sizeof(nl_eye_telemetry_t) == 46, "nl_eye_telemetry_t layout");
_Static_assert(sizeof(nl_cmd_t) == 6, "nl_cmd_t layout");
_Static_assert(sizeof(nl_ack_t) == 3, "nl_ack_t layout");
_Static_assert(sizeof(nl_wled_telemetry_t) == 30, "nl_wled_telemetry_t layout");
_Static_assert(sizeof(nl_bump_t) == 8, "nl_bump_t layout");
_Static_assert(sizeof(nl_radio_hdr_t) == 10, "nl_radio_hdr_t layout");
_Static_assert(NL_RADIO_MAX <= 250, "ESP-NOW packets are at most 250 bytes");
_Static_assert(sizeof(nl_audio_t) == 40, "nl_audio_t layout");
_Static_assert(sizeof(nl_eye_state_t) == 82, "nl_eye_state_t layout");
_Static_assert(sizeof(nl_eye_state_t) <= NL_MAX_PAYLOAD, "payload too big");
_Static_assert(sizeof(nl_eye_look_t) == 108, "nl_eye_look_t layout");
_Static_assert(sizeof(nl_eye_look_t) <= NL_MAX_PAYLOAD, "eye look too big");

// CRC-16/CCITT-FALSE, a byte at a time (the bitwise loop cost ~1.3 ms per
// frame of wired pixels on an ESP32-S3).
static const uint16_t crc_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0,
};

uint16_t nl_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) crc = (uint16_t)((crc << 8) ^ crc_table[(crc >> 8) ^ data[i]]);
    return crc;
}

// COBS: replace every 0x00 so the frame can be delimited by 0x00. out needs
// len + len / 254 + 1 bytes.
size_t nl_cobs_encode(const uint8_t *in, size_t len, uint8_t *out)
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
size_t nl_cobs_decode(const uint8_t *in, size_t len, uint8_t *out, size_t cap)
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
    size_t n = nl_cobs_encode(raw, NL_HEADER_LEN + len + NL_CRC_LEN, out);
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
    const size_t len = nl_cobs_decode(p->buf, n, p->dec, sizeof(p->dec));
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
