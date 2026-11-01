#include "log_format.h"

#include <ctype.h>
#include <stdio.h>

const char LOG_HEADER[] =
    "t_s,sim,pack_mv,pack_pct,pack_ma,input,vbus_mv,ibus_ma,in_ma,ibat_ma,vsys_mv,light_mv,light_ma,light_mw,"
    "cell_c,die_c,chg_state,fault0,fault1,ts_flags,efuse_pg,lights_off,pole_off,wled_preset,wled_bri,wled_fps,fin_follow,dropped\n";

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
    const int n = snprintf(out, cap, "%lu,%d,%d,%d,%d,%s,%d,%d,%d,%d,%d,%d,%d,%d,%s,%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%lu\n",
                           (unsigned long)s->t_s, s->sim, s->pack_mv, s->pack_pct, s->pack_ma, s->input, s->vbus_mv, s->ibus_ma,
                           s->in_ma, s->ibat_ma, s->vsys_mv, s->light_mv, s->light_ma, s->light_mw, cell, die, s->chg_state,
                           s->fault0, s->fault1, s->ts_flags, s->efuse_pg, s->lights_off, s->pole_off, s->wled_preset,
                           s->wled_bri, s->wled_fps, s->fin_follow, (unsigned long)s->dropped);
    return n > 0 && (size_t)n < cap ? (size_t)n : 0;
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
