// Gaps between a streamed picture's packets (see nibbles_link.h).
#include <string.h>

#include "nibbles_link.h"

_Static_assert(sizeof(nl_wled_stream_t) == 15, "nl_wled_stream_t layout");

void nl_stream_gaps_init(nl_stream_gaps_t *g)
{
    memset(g, 0, sizeof(*g));
}

void nl_stream_gaps_packet(nl_stream_gaps_t *g, uint32_t now_ms)
{
    const uint32_t gap = g->last_ms ? now_ms - g->last_ms : NL_STREAM_RESTART_MS;
    g->last_ms = now_ms ? now_ms : 1;
    if (gap < NL_STREAM_SLOW_MS || gap >= NL_STREAM_RESTART_MS) return;
    // Counted in the second the gap ended; a slot from an older minute starts over.
    const uint32_t second = now_ms / 1000;
    nl_stream_second_t *s = &g->sec[second % NL_STREAM_WINDOW_S];
    if (s->second != second) {
        memset(s, 0, sizeof(*s));
        s->second = second;
    }
    s->slow++;
    if (gap >= NL_STREAM_STALL_MS) {
        s->stalls++;
        g->stalls_total++;
    }
    if (gap > s->max_ms) s->max_ms = gap > 0xFFFF ? 0xFFFF : (uint16_t)gap;
}

void nl_stream_gaps_summary(const nl_stream_gaps_t *g, uint32_t now_ms, nl_wled_stream_t *out)
{
    const uint32_t second = now_ms / 1000;
    out->slow = out->stalls = out->max_gap_ms = 0;
    for (int i = 0; i < NL_STREAM_WINDOW_S; i++) {
        const nl_stream_second_t *s = &g->sec[i];
        if (!s->slow || second - s->second >= NL_STREAM_WINDOW_S) continue;
        out->slow += s->slow;
        out->stalls += s->stalls;
        if (s->max_ms > out->max_gap_ms) out->max_gap_ms = s->max_ms;
    }
    out->stalls_total = g->stalls_total;
}
