#include "log_format.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char LOG_HEADER[] =
    "t_s,time,sim,pack_mv,pack_pct,pack_ma,input,vbus_mv,ibus_ma,in_ma,ibat_ma,vsys_mv,light_mv,light_ma,light_mw,"
    "cell_c,die_c,chg_state,fault0,fault1,ts_flags,efuse_pg,lights_off,pole_off,wled_preset,wled_bri,wled_fps,fin_follow,dropped,chg_limit_ma\n";

// Tenths as "12.3", empty with no reading.
static int tenths(char *out, size_t cap, int v)
{
    if (v == INT16_MIN) { if (cap) out[0] = 0; return 0; }
    const int a = v < 0 ? -v : v;
    return snprintf(out, cap, "%s%d.%d", v < 0 ? "-" : "", a / 10, a % 10);
}

size_t log_row(char *out, size_t cap, const log_sample_t *s)
{
    char cell[12], die[12];
    tenths(cell, sizeof(cell), s->cell_c10);
    tenths(die, sizeof(die), s->die_c10);
    const int n = snprintf(out, cap, "%lu,%lu,%d,%d,%d,%d,%s,%d,%d,%d,%d,%d,%d,%d,%d,%s,%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%lu,%d\n",
                           (unsigned long)s->t_s, (unsigned long)s->unix_time, s->sim, s->pack_mv, s->pack_pct, s->pack_ma, s->input, s->vbus_mv, s->ibus_ma,
                           s->in_ma, s->ibat_ma, s->vsys_mv, s->light_mv, s->light_ma, s->light_mw, cell, die, s->chg_state,
                           s->fault0, s->fault1, s->ts_flags, s->efuse_pg, s->lights_off, s->pole_off, s->wled_preset,
                           s->wled_bri, s->wled_fps, s->fin_follow, (unsigned long)s->dropped, s->chg_limit_ma);
    return n > 0 && (size_t)n < cap ? (size_t)n : 0;
}

bool log_parse_charge(const char *row, uint32_t *unix_time, int *pct, bool as_real)
{
    // t_s,time,sim,pack_mv,pack_pct
    const char *p = strchr(row, ',');
    if (!p || p == row || row[0] < '0' || row[0] > '9') return false;
    char *end;
    const unsigned long t = strtoul(p + 1, &end, 10);
    if (end == p + 1 || *end != ',' || !t) return false;
    p = end + 1;
    if (!(*p == '0' || (*p == '2' && as_real)) || p[1] != ',') return false;   // made-up readings
    p = strchr(p, ',');
    p = p ? strchr(p + 1, ',') : NULL;
    if (!p) return false;
    const long v = strtol(p + 1, &end, 10);
    if (end == p + 1 || *end != ',' || v < 0 || v > 100) return false;
    *unix_time = (uint32_t)t;
    *pct = (int)v;
    return true;
}

void log_history_add(int8_t *cols, int n, float slice_s, uint32_t now, uint32_t t, int pct)
{
    if (t > now) return;
    const int back = (int)((now - t) / slice_s);
    if (back < n) cols[n - 1 - back] = (int8_t)pct;
}

static uint8_t bcd(uint8_t v) { return (uint8_t)((v >> 4) * 10 + (v & 15)); }
static uint8_t to_bcd(int v) { return (uint8_t)(v / 10 << 4 | v % 10); }

// Days since 1970 from a date and back (Howard Hinnant's civil calendar).
static int64_t days_from_civil(int y, int m, int d)
{
    y -= m <= 2;
    const int era = y / 400, yoe = y - era * 400, doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    return era * 146097LL + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468;
}

uint32_t rtc_regs_to_unix(const uint8_t r[7])
{
    if (r[0] & 0x80) return 0;   // OS: the oscillator stopped
    const int s = bcd(r[0] & 0x7F), mi = bcd(r[1] & 0x7F), h = bcd(r[2] & 0x3F), d = bcd(r[3] & 0x3F), mo = bcd(r[5] & 0x1F), y = bcd(r[6]);
    if (s > 59 || mi > 59 || h > 23 || d < 1 || d > 31 || mo < 1 || mo > 12 || y < 24) return 0;
    return (uint32_t)(days_from_civil(2000 + y, mo, d) * 86400 + h * 3600 + mi * 60 + s);
}

void rtc_unix_to_regs(uint32_t t, uint8_t r[7])
{
    const int64_t z = t / 86400 + 719468;
    const int secs = t % 86400;
    const int era = (int)(z / 146097), doe = (int)(z - era * 146097LL);
    const int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
    const int d = doy - (153 * mp + 2) / 5 + 1, m = mp < 10 ? mp + 3 : mp - 9, y = yoe + era * 400 + (m <= 2);
    r[0] = to_bcd(secs % 60);
    r[1] = to_bcd(secs / 60 % 60);
    r[2] = to_bcd(secs / 3600);
    r[3] = to_bcd(d);
    r[4] = (uint8_t)((t / 86400 + 4) % 7);
    r[5] = to_bcd(m);
    r[6] = to_bcd(y - 2000);
}

int log_file_number(const char *name)
{
    if (toupper((unsigned char)name[0]) != 'L') return -1;
    int v = 0;
    for (int i = 1; i <= 5; i++) {
        if (!isdigit((unsigned char)name[i])) return -1;
        v = v * 10 + name[i] - '0';
    }
    const bool csv = name[6] == '.' && toupper((unsigned char)name[7]) == 'C' && toupper((unsigned char)name[8]) == 'S' &&
                     toupper((unsigned char)name[9]) == 'V' && !name[10];
    return csv ? v : -1;
}
