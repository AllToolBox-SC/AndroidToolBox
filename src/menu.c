/* Minimal C port of some menu.py utilities.
 * - Wraps `cptk_menu_choice_interactive` and `cptk_menu_multi_choice_interactive`
 * - Provides tiny, forgiving JSON extractors to read option lists/actions
 *   from the project's menus.json-style files.
 *
 * Note: This is intentionally small and pragmatic (not a full JSON parser).
 */

#include "menu.h"
#include "c_prompt_toolkit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#ifdef _WIN32
#  include <windows.h>
#  include <wininet.h>
#  include <conio.h>
#  include <io.h>
#  pragma comment(lib, "wininet.lib")
#endif

#define MENU_DOUBLE_CLICK_DEFAULT_MS 300u
#define MENU_DOUBLE_CLICK_MIN_MS 100u
#define MENU_DOUBLE_CLICK_MAX_MS 1000u

static menu_mouse_click_state g_menu_mouse_click_state = {-1, 0};

menu_mouse_click_state *menu_get_global_mouse_click_state(void) {
    return &g_menu_mouse_click_state;
}

unsigned int menu_get_double_click_threshold_ms(void) {
    const char *raw = getenv("ATB_DOUBLE_CLICK_MS");
    unsigned long value = MENU_DOUBLE_CLICK_DEFAULT_MS;
    char *end = NULL;

    if (raw && raw[0]) {
        unsigned long parsed = strtoul(raw, &end, 10);
        if (end && *end == '\0' && parsed > 0) {
            value = parsed;
        }
    }

    if (value < MENU_DOUBLE_CLICK_MIN_MS) value = MENU_DOUBLE_CLICK_MIN_MS;
    if (value > MENU_DOUBLE_CLICK_MAX_MS) value = MENU_DOUBLE_CLICK_MAX_MS;
    return (unsigned int)value;
}

unsigned long long menu_now_ms(void) {
#ifdef _WIN32
    return (unsigned long long)GetTickCount64();
#else
    return (unsigned long long)((double)clock() * 1000.0 / (double)CLOCKS_PER_SEC);
#endif
}

void menu_mouse_click_state_reset(menu_mouse_click_state *state) {
    if (!state) return;
    state->last_click_index = -1;
    state->last_click_tick_ms = 0;
}

void menu_mouse_click_state_update(menu_mouse_click_state *state, int index, unsigned long long now_ms) {
    if (!state) return;
    state->last_click_index = index;
    state->last_click_tick_ms = now_ms;
}

int menu_mouse_double_click_hit(const menu_mouse_click_state *state, int index, unsigned long long now_ms, unsigned int threshold_ms) {
    unsigned long long delta;

    if (!state || index < 0) return 0;
    if (state->last_click_index != index) return 0;
    if (now_ms < state->last_click_tick_ms) return 0;
    delta = now_ms - state->last_click_tick_ms;
    return delta <= (unsigned long long)threshold_ms;
}

/* --- Helpers: file read --- */
static char *read_entire_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long len = ftell(f);
    if (len < 0) { fclose(f); return NULL; }
    rewind(f);
    char *buf = (char *)malloc((size_t)len + 1);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)len, f) != (size_t)len) {
        /* best-effort: still null-terminate */
    }
    buf[len] = '\0';
    fclose(f);
    return buf;
}

/* Extract a quoted JSON string for `key` between [start,end).
 * Returns a newly allocated string or NULL. This is forgiving and doesn't
 * handle all JSON escape cases; it's suitable for the project's menus.json.
 */
static char *extract_json_field(const char *start, const char *end, const char *key) {
    const char *p = start;
    size_t keylen = strlen(key);
    while (p < end) {
        const char *k = strstr(p, key);
        if (!k || k >= end) break;
        const char *q = k + keylen;
        /* find ':' after key */
        const char *colon = strchr(q, ':');
        if (!colon || colon >= end) { p = q; continue; }
        /* find first '"' after colon */
        const char *open = strchr(colon, '"');
        if (!open || open >= end) { p = q; continue; }
        open++;
        const char *close = open;
        while (close < end && *close != '"') {
            if (*close == '\\' && (close + 1) < end) close += 2;
            else close++;
        }
        if (close >= end) { p = q; continue; }
        size_t len = (size_t)(close - open);
        char *out = (char *)malloc(len + 1);
        if (!out) return NULL;
        /* naive copy (does not unescape) */
        memcpy(out, open, len);
        out[len] = '\0';
        return out;
    }
    return NULL;
}

/* Find JSON array start for either top-level array or object with "options": [] */
static const char *find_options_array(const char *buf) {
    const char *p = strstr(buf, "\"options\"");
    if (p) {
        p = strchr(p, '[');
        if (p) return p;
    }
    /* if no "options", maybe file itself is an array */
    p = strchr(buf, '[');
    return p;
}

/* Find matching ']' for a '[' while skipping quoted strings. */
static const char *find_matching_array_end(const char *arr_start) {
    const char *p;
    int depth = 0;
    int in_string = 0;
    int escaped = 0;

    if (!arr_start || *arr_start != '[') return NULL;

    for (p = arr_start; *p; ++p) {
        char c = *p;
        if (in_string) {
            if (escaped) {
                escaped = 0;
            } else if (c == '\\') {
                escaped = 1;
            } else if (c == '"') {
                in_string = 0;
            }
            continue;
        }
        if (c == '"') {
            in_string = 1;
            continue;
        }
        if (c == '[') {
            depth++;
            continue;
        }
        if (c == ']') {
            depth--;
            if (depth == 0) return p;
        }
    }
    return NULL;
}

int menu_load_options_from_json(const char *path, char ***out_values, char ***out_labels, size_t *out_count) {
    if (!path || !out_values || !out_labels || !out_count) return 0;
    *out_values = NULL; *out_labels = NULL; *out_count = 0;
    char *buf = read_entire_file(path);
    if (!buf) return 0;
    const char *arr = find_options_array(buf);
    if (!arr) { free(buf); return 0; }
    const char *arr_end = find_matching_array_end(arr);
    if (!arr_end) { free(buf); return 0; }
    const char *p = arr + 1; /* after '[' */
    size_t cap = 0, cnt = 0;
    char **vals = NULL, **labs = NULL;
    while (p < arr_end) {
        /* find next '{' */
        const char *obj = strchr(p, '{');
        if (!obj || obj >= arr_end) break;
        const char *obj_end = obj + 1;
        int depth = 1;
        while (obj_end < arr_end && depth > 0) {
            if (*obj_end == '{') depth++;
            else if (*obj_end == '}') depth--;
            obj_end++;
        }
        if (depth != 0 || obj_end > arr_end) break;
        /* extract label/value (prefer key as return token) */
        char *label = extract_json_field(obj, obj_end, "\"label\"");
        char *value = extract_json_field(obj, obj_end, "\"key\"");
        if (!value) value = extract_json_field(obj, obj_end, "\"value\"");
        if (!label) label = extract_json_field(obj, obj_end, "\"text\"");
        if (!label) label = extract_json_field(obj, obj_end, "\"name\"");
        if (label && value) {
            if (cnt + 1 > cap) {
                size_t ncap = cap ? cap * 2 : 8;
                char **nv = (char **)realloc(vals, ncap * sizeof(char*));
                char **nl = (char **)realloc(labs, ncap * sizeof(char*));
                if (!nv || !nl) { free(label); free(value); break; }
                vals = nv; labs = nl; cap = ncap;
            }
            vals[cnt] = value; labs[cnt] = label; cnt++;
        } else {
            if (label) free(label);
            if (value) free(value);
        }
        p = obj_end;
    }
    free(buf);
    if (cnt == 0) {
        if (vals) free(vals);
        if (labs) free(labs);
        return 0;
    }
    *out_values = vals; *out_labels = labs; *out_count = cnt;
    return 1;
}

/* --- CSV / TXT helpers --- */
static char *trim_copy_range(const char *start, const char *end) {
    while (start < end && (unsigned char)*start && isspace((unsigned char)*start)) start++;
    while (end > start && (unsigned char)*(end - 1) && isspace((unsigned char)*(end - 1))) end--;
    size_t len = (size_t)(end - start);
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, start, len);
    out[len] = '\0';
    return out;
}

/* Read one line from FILE* into a reusable dynamic buffer.
 * Returns 1 when a line is read, 0 on EOF/error with no data.
 */
static int read_line_dynamic(FILE *fp, char **line, size_t *cap) {
    if (!fp || !line || !cap) return 0;
    if (!*line || *cap == 0) {
        *cap = 1024;
        *line = (char *)malloc(*cap);
        if (!*line) { *cap = 0; return 0; }
    }

    size_t len = 0;
    int ch = 0;
    while ((ch = fgetc(fp)) != EOF) {
        if (len + 1 >= *cap) {
            size_t ncap = (*cap < (SIZE_MAX / 2)) ? (*cap * 2) : (*cap + 1024);
            char *nline = (char *)realloc(*line, ncap);
            if (!nline) return 0;
            *line = nline;
            *cap = ncap;
        }
        (*line)[len++] = (char)ch;
        if (ch == '\n') break;
    }

    if (len == 0 && ch == EOF) return 0;
    (*line)[len] = '\0';
    return 1;
}

static int option_arrays_push(char ***vals, char ***labs, size_t *cnt, size_t *cap, char *value, char *label) {
    if (!vals || !labs || !cnt || !cap || !value || !label) return 0;
    if (*cnt + 1 > *cap) {
        size_t ncap = *cap ? (*cap * 2) : 16;
        char **nv = (char **)realloc(*vals, ncap * sizeof(char *));
        if (!nv) return 0;
        *vals = nv;
        char **nl = (char **)realloc(*labs, ncap * sizeof(char *));
        if (!nl) return 0;
        *labs = nl;
        *cap = ncap;
    }
    (*vals)[*cnt] = value;
    (*labs)[*cnt] = label;
    (*cnt)++;
    return 1;
}

/* Parse next CSV field from *pp, advance *pp past comma/newline. Returns allocated string. */
static char *csv_next_field(const char **pp) {
    if (!pp || !*pp) return NULL;
    const char *p = *pp;
    /* skip leading spaces */
    while (*p && (*p == ' ' || *p == '\t')) p++;
    size_t cap = 128; size_t pos = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    if (*p == '"') {
        p++; /* inside quoted field */
        while (*p) {
            if (*p == '"') {
                if (p[1] == '"') { /* escaped quote */
                    if (pos + 1 >= cap) { cap *= 2; buf = (char *)realloc(buf, cap); if (!buf) return NULL; }
                    buf[pos++] = '"'; p += 2; continue;
                } else { p++; break; }
            }
            if (pos + 1 >= cap) { cap *= 2; buf = (char *)realloc(buf, cap); if (!buf) return NULL; }
            buf[pos++] = *p++;
        }
        /* skip until comma or end */
        while (*p && *p != ',' && *p != '\n' && *p != '\r') p++;
        if (*p == ',') p++;
    } else {
        const char *start = p;
        while (*p && *p != ',' && *p != '\n' && *p != '\r') p++;
        size_t flen = (size_t)(p - start);
        if (flen + 1 > cap) { buf = (char *)realloc(buf, flen + 1); if (!buf) return NULL; cap = flen + 1; }
        memcpy(buf, start, flen); pos = flen;
        if (*p == ',') p++;
    }
    buf[pos] = '\0';
    /* skip trailing spaces */
    while (*p && (*p == ' ' || *p == '\t')) p++;
    *pp = p;
    return buf;
}

int menu_load_options_from_csv(const char *path, char ***out_values, char ***out_labels, size_t *out_count) {
    if (!path || !out_values || !out_labels || !out_count) return 0;
    *out_values = NULL; *out_labels = NULL; *out_count = 0;
    FILE *fp = fopen(path, "rb");
    if (!fp) return 0;

    size_t cap = 0, cnt = 0;
    char **vals = NULL, **labs = NULL;
    char *line = NULL;
    size_t line_cap = 0;
    int first_line = 1;

    while (read_line_dynamic(fp, &line, &line_cap)) {
        char *line_start = line;
        char *line_end = line + strlen(line);

        /* Skip UTF-8 BOM only once */
        if (first_line && line_end - line_start >= 3 &&
            (unsigned char)line_start[0] == 0xEF &&
            (unsigned char)line_start[1] == 0xBB &&
            (unsigned char)line_start[2] == 0xBF) {
            line_start += 3;
        }
        first_line = 0;

        while (line_end > line_start && (line_end[-1] == '\n' || line_end[-1] == '\r')) line_end--;

        const char *tmp = line_start;
        int only_ws = 1;
        while (tmp < line_end) { if (!isspace((unsigned char)*tmp)) { only_ws = 0; break; } tmp++; }

        if (!only_ws) {
            const char *pp = line_start;
            char *f1 = csv_next_field(&pp);
            char *f2 = NULL;
            if (f1) {
                if (pp && *pp) f2 = csv_next_field(&pp);
                if (!f2) {
                    /* treat single column as value==label */
                    f2 = _strdup(f1);
                }

                if (!f2 || !option_arrays_push(&vals, &labs, &cnt, &cap, f1, f2)) {
                    if (f1) free(f1);
                    if (f2) free(f2);
                    break;
                }
            }
        }
    }

    if (line) free(line);
    fclose(fp);
    if (cnt == 0) { free(vals); free(labs); return 0; }
    *out_values = vals; *out_labels = labs; *out_count = cnt; return 1;
}

int menu_load_options_from_txt(const char *path, char ***out_values, char ***out_labels, size_t *out_count) {
    if (!path || !out_values || !out_labels || !out_count) return 0;
    *out_values = NULL; *out_labels = NULL; *out_count = 0;
    FILE *fp = fopen(path, "rb");
    if (!fp) return 0;

    size_t cap = 0, cnt = 0;
    char **vals = NULL, **labs = NULL;
    char *line = NULL;
    size_t line_cap = 0;
    int first_line = 1;

    while (read_line_dynamic(fp, &line, &line_cap)) {
        char *line_start = line;
        char *line_end = line + strlen(line);

        if (first_line && line_end - line_start >= 3 &&
            (unsigned char)line_start[0] == 0xEF &&
            (unsigned char)line_start[1] == 0xBB &&
            (unsigned char)line_start[2] == 0xBF) {
            line_start += 3;
        }
        first_line = 0;

        while (line_end > line_start && (line_end[-1] == '\n' || line_end[-1] == '\r')) line_end--;

        /* trim and skip empty */
        const char *s = line_start; while (s < line_end && isspace((unsigned char)*s)) s++;
        const char *e = line_end; while (e > s && isspace((unsigned char)*(e - 1))) e--;
        if (s < e) {
            /* find separator '|' first */
            const char *sep = NULL;
            for (const char *q = s; q < e; ++q) if (*q == '|') { sep = q; break; }
            char *value = NULL; char *label = NULL;
            if (sep) {
                value = trim_copy_range(s, sep);
                label = trim_copy_range(sep + 1, e);
            } else {
                label = trim_copy_range(s, e);
                value = _strdup(label);
            }
            if (value && label) {
                if (!option_arrays_push(&vals, &labs, &cnt, &cap, value, label)) {
                    free(value);
                    free(label);
                    break;
                }
            } else {
                if (value) free(value);
                if (label) free(label);
            }
        }
    }

    if (line) free(line);
    fclose(fp);
    if (cnt == 0) { free(vals); free(labs); return 0; }
    *out_values = vals; *out_labels = labs; *out_count = cnt; return 1;
}

/* --- XML / YAML / TOML minimal loaders --- */
static char *extract_xml_attr(const char *tag_start, const char *tag_end, const char *attr) {
    if (!tag_start || !tag_end || !attr) return NULL;
    const char *p = tag_start;
    size_t alen = strlen(attr);
    while (p < tag_end) {
        const char *k = strstr(p, attr);
        if (!k || k >= tag_end) break;
        const char *eq = k + alen;
        while (eq < tag_end && isspace((unsigned char)*eq)) eq++;
        if (eq >= tag_end || *eq != '=') { p = k + 1; continue; }
        eq++;
        while (eq < tag_end && isspace((unsigned char)*eq)) eq++;
        if (eq >= tag_end) break;
        char quote = 0;
        if (*eq == '"' || *eq == '\'') { quote = *eq; eq++; }
        const char *start = eq;
        const char *q = start;
        while (q < tag_end) {
            if (quote) {
                if (*q == quote) break;
            } else {
                if (isspace((unsigned char)*q) || *q == '>') break;
            }
            q++;
        }
        return trim_copy_range(start, q);
    }
    return NULL;
}

int menu_load_options_from_xml(const char *path, char ***out_values, char ***out_labels, size_t *out_count) {
    if (!path || !out_values || !out_labels || !out_count) return 0;
    *out_values = NULL; *out_labels = NULL; *out_count = 0;
    char *buf = read_entire_file(path);
    if (!buf) return 0;
    char *p = buf;
    size_t cap = 0, cnt = 0;
    char **vals = NULL, **labs = NULL;
    /* search for <option or <item tags */
    /* helper: find needle but ensure following char is not an ASCII letter (avoid matching "<options>") */
    char *find_tag = NULL;
    while (1) {
        char *tag = strstr(p, "<option");
        while (tag && isalpha((unsigned char)tag[7])) tag = strstr(tag + 1, "<option");
        char *tag2 = strstr(p, "<item");
        while (tag2 && isalpha((unsigned char)tag2[5])) tag2 = strstr(tag2 + 1, "<item");
        char *use = NULL;
        if (tag && tag2) use = (tag < tag2) ? tag : tag2;
        else if (tag) use = tag;
        else if (tag2) use = tag2;
        if (!use) break;
        /* find end of opening tag */
        char *tag_end = strchr(use, '>');
        if (!tag_end) break;
        /* determine closing tag */
        const char *close_name = NULL;
        if (strncmp(use, "<option", 7) == 0) close_name = "</option>";
        else if (strncmp(use, "<item", 5) == 0) close_name = "</item>";
        const char *close_tag = NULL;
        if (close_name) close_tag = strstr(tag_end + 1, close_name);
        const char *inner_start = tag_end + 1;
        const char *inner_end = close_tag ? close_tag : inner_start;
        /* extract attributes */
        char *key = extract_xml_attr(use, tag_end, "key");
        if (!key) key = extract_xml_attr(use, tag_end, "value");
        char *label = NULL;
        if (close_tag) label = trim_copy_range(inner_start, inner_end);
        /* no-op */
        if (!label && key) {
            label = strdup(key);
        }
        if (label) {
            if (!key) key = strdup(label);
            if (cnt + 1 > cap) {
                size_t ncap = cap ? cap * 2 : 8;
                char **nv = (char **)realloc(vals, ncap * sizeof(char*));
                char **nl = (char **)realloc(labs, ncap * sizeof(char*));
                if (!nv || !nl) { if (key) free(key); if (label) free(label); break; }
                vals = nv; labs = nl; cap = ncap;
            }
            vals[cnt] = key; labs[cnt] = label; cnt++;
        } else {
            if (key) free(key);
        }
        if (close_tag) p = (char *)close_tag + strlen(close_name); else p = tag_end + 1;
    }
    free(buf);
    if (cnt == 0) { if (vals) free(vals); if (labs) free(labs); return 0; }
    *out_values = vals; *out_labels = labs; *out_count = cnt; return 1;
}

int menu_load_options_from_yaml(const char *path, char ***out_values, char ***out_labels, size_t *out_count) {
    if (!path || !out_values || !out_labels || !out_count) return 0;
    *out_values = NULL; *out_labels = NULL; *out_count = 0;
    char *buf = read_entire_file(path);
    if (!buf) return 0;
    char *p = buf;
    size_t cap = 0, cnt = 0;
    char **vals = NULL, **labs = NULL;
    /* find 'options:' */
    char *opts = strstr(p, "options:");
    if (!opts) { free(buf); return 0; }
    /* move to next line */
    char *line = opts;
    while (*line && *line != '\n') line++;
    if (*line == '\n') line++;
    while (*line) {
        /* skip blank/whitespace */
        while (*line && isspace((unsigned char)*line)) line++;
        if (!*line) break;
        if (*line == '-') {
            line++; /* start item */
            /* read rest of this line */
            char *lstart = line;
            while (*lstart && *lstart == ' ') lstart++;
            char *lend = lstart;
            while (*lend && *lend != '\n') lend++;
            /* parse inline '- key: value' or '- value|label' */
            char *value = NULL; char *label = NULL;
            /* look for ':' */
            const char *colon = NULL;
            for (const char *q = lstart; q < lend; ++q) if (*q == ':') { colon = q; break; }
            if (colon) {
                char *left = trim_copy_range(lstart, colon);
                char *right = trim_copy_range(colon + 1, lend);
                if (left && right) {
                    if (strcmp(left, "key") == 0 || strcmp(left, "value") == 0) {
                        value = right; free(left);
                    } else if (strcmp(left, "label") == 0 || strcmp(left, "text") == 0) {
                        label = right; free(left);
                    } else {
                        value = left; label = right;
                    }
                } else { if (left) free(left); if (right) free(right); }
            } else {
                /* check '|' */
                const char *bar = NULL;
                for (const char *q = lstart; q < lend; ++q) if (*q == '|') { bar = q; break; }
                if (bar) {
                    value = trim_copy_range(lstart, bar);
                    label = trim_copy_range(bar + 1, lend);
                } else {
                    /* single token becomes label/value */
                    label = trim_copy_range(lstart, lend);
                    if (label) value = strdup(label);
                }
            }
            /* if no inline fields, check following indented lines for key/label */
            char *next = lend;
            if (*next == '\n') next++;
            while (*next && (isspace((unsigned char)*next))) {
                /* parse lines like 'key: value' or 'label: Label' */
                const char *ln = next;
                /* skip indentation */
                while (*ln && isspace((unsigned char)*ln)) ln++;
                if (*ln == '-' || *ln == '\n' || *ln == '\0') break;
                const char *lnend = ln;
                while (*lnend && *lnend != '\n') lnend++;
                /* find ':' */
                const char *c = NULL;
                for (const char *q = ln; q < lnend; ++q) if (*q == ':') { c = q; break; }
                if (c) {
                    char *k = trim_copy_range(ln, c);
                    char *v = trim_copy_range(c + 1, lnend);
                    if (k && v) {
                        if (strcmp(k, "key") == 0 || strcmp(k, "value") == 0) {
                            if (value) free(value); value = v; v = NULL;
                        } else if (strcmp(k, "label") == 0 || strcmp(k, "text") == 0) {
                            if (label) free(label); label = v; v = NULL;
                        } else {
                            if (v) free(v);
                        }
                    }
                    if (k) free(k);
                }
                if (*lnend == '\n') next = (char *)lnend + 1; else next = (char *)lnend;
            }
            if (label && !value) value = strdup(label);
            if (value && label) {
                if (cnt + 1 > cap) {
                    size_t ncap = cap ? cap * 2 : 8;
                    char **nv = (char **)realloc(vals, ncap * sizeof(char*));
                    char **nl = (char **)realloc(labs, ncap * sizeof(char*));
                    if (!nv || !nl) { if (value) free(value); if (label) free(label); break; }
                    vals = nv; labs = nl; cap = ncap;
                }
                vals[cnt] = value; labs[cnt] = label; cnt++;
            } else { if (value) free(value); if (label) free(label); }
            line = lend;
            if (*line == '\n') line++;
            continue;
        }
        /* else skip line */
        while (*line && *line != '\n') line++;
        if (*line == '\n') line++;
    }
    free(buf);
    if (cnt == 0) { if (vals) free(vals); if (labs) free(labs); return 0; }
    *out_values = vals; *out_labels = labs; *out_count = cnt; return 1;
}

int menu_load_options_from_toml(const char *path, char ***out_values, char ***out_labels, size_t *out_count) {
    if (!path || !out_values || !out_labels || !out_count) return 0;
    *out_values = NULL; *out_labels = NULL; *out_count = 0;
    char *buf = read_entire_file(path);
    if (!buf) return 0;
    char *p = buf;
    size_t cap = 0, cnt = 0;
    char **vals = NULL, **labs = NULL;
    char *line = p;
    char *cur_value = NULL, *cur_label = NULL; int in_table = 0;
    while (*line) {
        char *lend = line; while (*lend && *lend != '\n') lend++;
        const char *s = line; while (s < lend && isspace((unsigned char)*s)) s++;
        if (s < lend && *s == '[' && s + 1 < lend && *(s + 1) == '[') {
            /* '[[options]]' */
            if (in_table && cur_label) {
                if (!cur_value) cur_value = strdup(cur_label);
                if (cnt + 1 > cap) {
                    size_t ncap = cap ? cap * 2 : 8;
                    char **nv = (char **)realloc(vals, ncap * sizeof(char*));
                    char **nl = (char **)realloc(labs, ncap * sizeof(char*));
                    if (!nv || !nl) { break; }
                    vals = nv; labs = nl; cap = ncap;
                }
                vals[cnt] = cur_value ? cur_value : strdup(""); labs[cnt] = cur_label ? cur_label : strdup(""); cnt++;
                cur_value = NULL; cur_label = NULL;
            }
            in_table = (strstr(s, "[[options]]") != NULL);
        } else if (in_table) {
            /* parse key = "value" */
            const char *eq = NULL;
            for (const char *q = s; q < lend; ++q) if (*q == '=') { eq = q; break; }
            if (eq) {
                char *k = trim_copy_range(s, eq);
                char *v = NULL;
                const char *r = eq + 1;
                while (r < lend && isspace((unsigned char)*r)) r++;
                if (*r == '"' || *r == '\'') {
                    char quote = *r; r++; const char *rv = r; while (rv < lend && *rv != quote) rv++; v = trim_copy_range(r, rv);
                } else {
                    v = trim_copy_range(r, lend);
                }
                if (k && v) {
                    if (strcmp(k, "key") == 0 || strcmp(k, "value") == 0) { if (cur_value) free(cur_value); cur_value = v; }
                    else if (strcmp(k, "label") == 0 || strcmp(k, "text") == 0) { if (cur_label) free(cur_label); cur_label = v; }
                    else { free(v); }
                }
                if (k) free(k);
            }
        }
        line = (*lend == '\n') ? lend + 1 : lend;
    }
    if (in_table && cur_label) {
        if (!cur_value) cur_value = strdup(cur_label);
        if (cnt + 1 > cap) {
            size_t ncap = cap ? cap * 2 : 8;
            char **nv = (char **)realloc(vals, ncap * sizeof(char*));
            char **nl = (char **)realloc(labs, ncap * sizeof(char*));
            if (!nv || !nl) { /* fallthrough */ }
            else { vals = nv; labs = nl; cap = ncap; }
        }
        vals[cnt] = cur_value ? cur_value : strdup(""); labs[cnt] = cur_label ? cur_label : strdup(""); cnt++;
    }
    free(buf);
    if (cnt == 0) { if (vals) free(vals); if (labs) free(labs); return 0; }
    *out_values = vals; *out_labels = labs; *out_count = cnt; return 1;
}

/* Heuristic file loader: pick based on extension or content. Tries JSON first,
 * then CSV/TXT. Additional formats (XML/YAML/TOML) can be added later.
 */
int menu_load_options_from_file(const char *path, char ***out_values, char ***out_labels, size_t *out_count) {
    if (!path || !out_values || !out_labels || !out_count) return 0;
    /* extension-based route */
    const char *dot = strrchr(path, '.');
    if (dot) {
        if (_stricmp(dot, ".json") == 0) return menu_load_options_from_json(path, out_values, out_labels, out_count);
        if (_stricmp(dot, ".csv") == 0) return menu_load_options_from_csv(path, out_values, out_labels, out_count);
        if (_stricmp(dot, ".txt") == 0) return menu_load_options_from_txt(path, out_values, out_labels, out_count);
        if (_stricmp(dot, ".xml") == 0) {
            return menu_load_options_from_xml(path, out_values, out_labels, out_count);
        }
        if (_stricmp(dot, ".yml") == 0 || _stricmp(dot, ".yaml") == 0) {
            return menu_load_options_from_yaml(path, out_values, out_labels, out_count);
        }
        if (_stricmp(dot, ".toml") == 0) {
            return menu_load_options_from_toml(path, out_values, out_labels, out_count);
        }
    }
    /* content-based heuristics */
    char *buf = read_entire_file(path);
    if (!buf) return 0;
    const char *p = buf;
    /* skip BOM and leading whitespace */
    if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF) p += 3;
    while (*p && isspace((unsigned char)*p)) p++;
    int rv = 0;
    if (*p == '{' || *p == '[') {
        rv = menu_load_options_from_json(path, out_values, out_labels, out_count);
    } else if (*p == '<') {
        rv = menu_load_options_from_json(path, out_values, out_labels, out_count);
    } else {
        /* try CSV then TXT */
        rv = menu_load_options_from_csv(path, out_values, out_labels, out_count);
        if (!rv) rv = menu_load_options_from_txt(path, out_values, out_labels, out_count);
    }
    free(buf);
    return rv;
}

void menu_free_options(char **values, char **labels, size_t count) {
    if (values) {
        for (size_t i = 0; i < count; ++i) free(values[i]);
        free(values);
    }
    if (labels) {
        for (size_t i = 0; i < count; ++i) free(labels[i]);
        free(labels);
    }
}

int menu_choice_interactive(const char *title, const char *const *items, size_t item_count, char *out_value, size_t out_capacity, size_t default_index) {
    if (!items || item_count == 0 || !out_value || out_capacity == 0) return -1;
    menu_mouse_click_state_reset(menu_get_global_mouse_click_state());
    /* Try cptk interactive menu */
    cptk_context *ctx = cptk_context_create();
    size_t sel = 0;
    if (ctx) {
        cptk_status st = cptk_menu_choice_interactive(ctx, title ? title : "", items, item_count, default_index, &sel);
        cptk_context_destroy(ctx);
        if (st == CPTK_STATUS_OK) {
            strncpy(out_value, items[sel] ? items[sel] : "", out_capacity - 1);
            out_value[out_capacity - 1] = '\0';
            return 0;
        }
    }
    /* Fallback to console prompt */
    for (size_t i = 0; i < item_count; ++i) {
        printf("%3zu. %s\n", i + 1, items[i] ? items[i] : "");
    }
    printf("%s", title ? title : "选择: "); fflush(stdout);
    int idx = 0;
    if (scanf("%d", &idx) != 1) {
        return -1;
    }
    if (idx < 1) idx = 1;
    if ((size_t)idx > item_count) idx = (int)item_count;
    strncpy(out_value, items[idx - 1] ? items[idx - 1] : "", out_capacity - 1);
    out_value[out_capacity - 1] = '\0';
    return 0;
}

int menu_multi_choice_interactive(const char *title, const char *const *items, size_t item_count, char *out_csv, size_t out_capacity, int animate_in, int max_height) {
    if (!items || item_count == 0 || !out_csv || out_capacity == 0) return -1;
    menu_mouse_click_state_reset(menu_get_global_mouse_click_state());
    unsigned char *mask = (unsigned char *)malloc(item_count);
    if (!mask) return -1;
    memset(mask, 0, item_count);
    cptk_context *ctx = cptk_context_create();
    if (ctx) {
        cptk_status st = cptk_menu_multi_choice_interactive(ctx, title ? title : "", items, item_count, mask, animate_in, max_height);
        cptk_context_destroy(ctx);
        if (st != CPTK_STATUS_OK) {
            free(mask);
            return -1;
        }
    } else {
        /* Non-interactive fallback: prompt for comma-separated indices */
        for (size_t i = 0; i < item_count; ++i) printf("%3zu. %s\n", i + 1, items[i] ? items[i] : "");
        printf("选择（以逗号分隔索引）: "); fflush(stdout);
        char line[4096];
        if (!fgets(line, sizeof(line), stdin)) { free(mask); return -1; }
        char *tok = strtok(line, ", \t\n");
        while (tok) {
            int idx = atoi(tok);
            if (idx >= 1 && (size_t)idx <= item_count) mask[idx - 1] = 1;
            tok = strtok(NULL, ", \t\n");
        }
    }

    /* Compose CSV using item strings */
    size_t pos = 0; int first = 1;
    for (size_t i = 0; i < item_count; ++i) {
        if (!mask[i]) continue;
        const char *s = items[i] ? items[i] : "";
        size_t need = strlen(s);
        if (!first) need += 1; /* comma */
        if (pos + need + 1 >= out_capacity) break;
        if (!first) out_csv[pos++] = ',';
        memcpy(out_csv + pos, s, strlen(s)); pos += strlen(s);
        out_csv[pos] = '\0';
        first = 0;
    }
    if (pos == 0) {
        out_csv[0] = '\0';
    }
    free(mask);
    return 0;
}

char *menu_load_action_from_json(const char *path) {
    if (!path) return NULL;
    char *buf = read_entire_file(path);
    if (!buf) return NULL;
    char *act = extract_json_field(buf, buf + strlen(buf), "\"action\"");
    free(buf);
    return act; /* caller frees */
}

int menu_init_cloud_control(const char *url, int timeout_ms) {
    if (!url) return 0;
#ifdef _WIN32
    HINTERNET h = InternetOpenA("AllToolBox/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!h) return 0;
    DWORD to = (DWORD)(timeout_ms > 0 ? timeout_ms : 3000);
    InternetSetOptionA(h, INTERNET_OPTION_CONNECT_TIMEOUT, &to, sizeof(to));
    InternetSetOptionA(h, INTERNET_OPTION_RECEIVE_TIMEOUT, &to, sizeof(to));
    HINTERNET hf = InternetOpenUrlA(h, url, NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!hf) { InternetCloseHandle(h); return 0; }
    char tmp[256]; DWORD read = 0; int got = 0;
    if (InternetReadFile(hf, tmp, (DWORD)sizeof(tmp) - 1, &read) && read > 0) got = 1;
    InternetCloseHandle(hf);
    InternetCloseHandle(h);
    return got ? 1 : 0;
#else
    (void)url; (void)timeout_ms; return 0;
#endif
}

int menu_pause(const char *message, int timeout_ms) {
    if (message && message[0]) printf("%s", message);
    fflush(stdout);
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    DWORD start = GetTickCount();
    while (1) {
        DWORD avail = 0;
        if (PeekConsoleInputA(h, NULL, 0, &avail) && avail > 0) break;
        if (timeout_ms > 0 && (int)(GetTickCount() - start) >= timeout_ms) return 1;
        Sleep(50);
        /* Fall back to kbhit style check */
        if (_kbhit()) { (void)_getch(); break; }
    }
    return 0;
#else
    if (timeout_ms <= 0) { getchar(); return 0; }
    /* Simple blocking wait with timeout not implemented for POSIX in this minimal helper */
    getchar(); return 0;
#endif
}
