// Console text between a brain and the base (see nibbles_link.h).
#include <string.h>

#include "nibbles_link.h"

_Static_assert(sizeof(nl_console_t) == NL_CONSOLE_HEAD + NL_CONSOLE_TEXT, "nl_console_t layout");
_Static_assert(sizeof(nl_console_t) <= NL_MAX_PAYLOAD, "nl_console_t fits a radio packet");

size_t nl_console_question(nl_console_t *m, uint16_t id, const char *text)
{
    const size_t n = strlen(text);
    if (!n || n >= NL_CONSOLE_TEXT) return 0;
    m->id = id;
    m->part = 0;
    m->flags = 0;
    memcpy(m->text, text, n + 1);
    return NL_CONSOLE_HEAD + n + 1;
}

size_t nl_console_answer_part(nl_console_t *m, uint16_t id, uint8_t part, const char *all, size_t *offset)
{
    const size_t total = strlen(all);
    size_t at = *offset < total ? *offset : total;
    size_t n = total - at;
    if (n > NL_CONSOLE_TEXT - 1) n = NL_CONSOLE_TEXT - 1;
    m->id = id;
    m->part = part;
    m->flags = NL_CONSOLE_ANSWER | (at + n >= total ? NL_CONSOLE_LAST : 0);
    memcpy(m->text, all + at, n);
    m->text[n] = 0;
    *offset = at + n;
    return NL_CONSOLE_HEAD + n + 1;
}

bool nl_console_parse(const uint8_t *payload, size_t len, nl_console_t *m)
{
    if (len <= NL_CONSOLE_HEAD || len > sizeof(*m)) return false;
    memcpy(m, payload, len);
    if (m->text[len - NL_CONSOLE_HEAD - 1] != 0) return false;   // sent with its NUL, and nothing after
    return strlen(m->text) == len - NL_CONSOLE_HEAD - 1;
}

void nl_console_rx_start(nl_console_rx_t *rx, uint16_t id)
{
    rx->id = id;
    rx->next = 0;
    rx->done = rx->broken = false;
    rx->len = 0;
    rx->text[0] = 0;
}

bool nl_console_rx_add(nl_console_rx_t *rx, const nl_console_t *m)
{
    if (!(m->flags & NL_CONSOLE_ANSWER) || m->id != rx->id || rx->done) return rx->done;
    if (m->part != rx->next) {   // a part went missing: this answer can't be finished
        if (m->part > rx->next) rx->broken = true;
        return false;
    }
    if (rx->broken) return false;
    const size_t n = strlen(m->text);
    if (rx->len + n >= sizeof(rx->text)) {
        rx->broken = true;
        return false;
    }
    memcpy(rx->text + rx->len, m->text, n + 1);
    rx->len += n;
    rx->next++;
    if (m->flags & NL_CONSOLE_LAST) rx->done = true;
    return rx->done;
}
