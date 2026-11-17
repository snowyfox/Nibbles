#include "keymap_text.h"

#include <stdio.h>
#include <string.h>

bool keymap_key(const char *name, ctl_key_t *key)
{
    if (!strcmp(name, "none")) {
        *key = KEY_NONE;
        return true;
    }
    const ctl_key_t k = ctl_key_by_name(name);
    if (k == KEY_NONE) return false;
    *key = k;
    return true;
}

int keymap_format(const ctl_key_t *keys, int n, char *out, size_t size)
{
    size_t len = 0;
    if (size) out[0] = 0;
    for (int i = 0; i < n; i++) {
        const int w = snprintf(out + len, size - len, "%s%s", i ? "," : "", ctl_key_name(keys[i]));
        if (w < 0 || (size_t)w >= size - len) return -1;
        len += (size_t)w;
    }
    return (int)len;
}

bool keymap_parse(const char *text, ctl_key_t *keys, int n)
{
    ctl_key_t got[16];
    if (n <= 0 || n > 16) return false;
    int count = 0;
    const char *p = text;
    for (;;) {
        const char *end = strchr(p, ',');
        const size_t len = end ? (size_t)(end - p) : strlen(p);
        char name[24];
        if (len == 0 || len >= sizeof(name) || count == n) return false;
        memcpy(name, p, len);
        name[len] = 0;
        if (!keymap_key(name, &got[count])) return false;
        count++;
        if (!end) break;
        p = end + 1;
    }
    if (count != n) return false;
    memcpy(keys, got, sizeof(got[0]) * (size_t)n);
    return true;
}
