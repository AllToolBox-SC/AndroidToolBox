#include "c_prompt_toolkit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct demo_menu_labels {
    char **items;
    size_t count;
    size_t capacity;
} demo_menu_labels;

static const char *k_demo_cmds[] = {
    "onekeyroot",
    "openshell",
    "about",
    "mods",
    "commonly",
    "help-links",
    "man-apps",
    "magisk-mod",
    "exit"
};

static char *demo_strdup(const char *s) {
    size_t len;
    char *out;
    if (!s) {
        return NULL;
    }
    len = strlen(s);
    out = (char *)malloc(len + 1);
    if (!out) {
        return NULL;
    }
    memcpy(out, s, len + 1);
    return out;
}

static const char *demo_skip_ws(const char *p) {
    while (p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
        p++;
    }
    return p;
}

static int demo_parse_json_string(const char **pp, char *out, size_t out_cap) {
    const char *p;
    size_t n = 0;

    if (!pp || !*pp || !out || out_cap == 0) {
        return -1;
    }

    p = demo_skip_ws(*pp);
    if (!p || *p != '"') {
        return -1;
    }
    p++;

    while (*p && *p != '"') {
        char ch = *p++;
        if (ch == '\\') {
            char esc = *p++;
            if (esc == '\0') {
                return -1;
            }
            if (esc == '"' || esc == '\\' || esc == '/') {
                ch = esc;
            } else if (esc == 'b') {
                ch = '\b';
            } else if (esc == 'f') {
                ch = '\f';
            } else if (esc == 'n') {
                ch = '\n';
            } else if (esc == 'r') {
                ch = '\r';
            } else if (esc == 't') {
                ch = '\t';
            } else if (esc == 'u') {
                size_t i;
                for (i = 0; i < 4 && p[i]; ++i) {
                    char c = p[i];
                    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                        return -1;
                    }
                }
                p += 4;
                ch = '?';
            } else {
                ch = esc;
            }
        }

        if (n + 1 < out_cap) {
            out[n++] = ch;
        }
    }

    if (*p != '"') {
        return -1;
    }
    p++;

    out[n] = '\0';
    *pp = p;
    return 0;
}

static const char *demo_find_matching_bracket(const char *start, char open_ch, char close_ch) {
    const char *p;
    int depth = 0;
    int in_string = 0;
    int escaped = 0;

    if (!start || *start != open_ch) {
        return NULL;
    }

    for (p = start; *p; ++p) {
        char ch = *p;
        if (in_string) {
            if (escaped) {
                escaped = 0;
            } else if (ch == '\\') {
                escaped = 1;
            } else if (ch == '"') {
                in_string = 0;
            }
            continue;
        }

        if (ch == '"') {
            in_string = 1;
            continue;
        }
        if (ch == open_ch) {
            depth++;
            continue;
        }
        if (ch == close_ch) {
            depth--;
            if (depth == 0) {
                return p;
            }
        }
    }

    return NULL;
}

static int demo_labels_add(demo_menu_labels *labels, const char *text) {
    char *copy;
    char **next;
    size_t next_cap;

    if (!labels || !text) {
        return -1;
    }
    if (labels->count == labels->capacity) {
        next_cap = (labels->capacity == 0) ? 8u : labels->capacity * 2u;
        next = (char **)realloc(labels->items, next_cap * sizeof(char *));
        if (!next) {
            return -1;
        }
        labels->items = next;
        labels->capacity = next_cap;
    }

    copy = demo_strdup(text);
    if (!copy) {
        return -1;
    }

    labels->items[labels->count++] = copy;
    return 0;
}

static void demo_labels_free(demo_menu_labels *labels) {
    size_t i;
    if (!labels) {
        return;
    }
    for (i = 0; i < labels->count; ++i) {
        free(labels->items[i]);
    }
    free(labels->items);
    labels->items = NULL;
    labels->count = 0;
    labels->capacity = 0;
}

static char *demo_read_text_file(const char *path) {
    FILE *fp = NULL;
    long size;
    size_t nread;
    char *data;

    if (!path) {
        return NULL;
    }

    #ifdef _WIN32
    if (fopen_s(&fp, path, "rb") != 0) {
        fp = NULL;
    }
    #else
    fp = fopen(path, "rb");
    #endif
    if (!fp) {
        return NULL;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }
    size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        return NULL;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return NULL;
    }

    data = (char *)malloc((size_t)size + 1u);
    if (!data) {
        fclose(fp);
        return NULL;
    }

    nread = fread(data, 1, (size_t)size, fp);
    fclose(fp);
    if (nread != (size_t)size) {
        free(data);
        return NULL;
    }

    data[nread] = '\0';
    return data;
}

static int demo_load_main_menu_labels(const char *json_path, demo_menu_labels *labels) {
    char *json;
    const char *p;
    const char *options_key;
    const char *array_begin;
    const char *array_end;

    if (!json_path || !labels) {
        return -1;
    }
    memset(labels, 0, sizeof(*labels));

    json = demo_read_text_file(json_path);
    if (!json) {
        return -1;
    }

    p = json;
    while ((p = strstr(p, "\"id\"")) != NULL) {
        const char *colon = strchr(p, ':');
        char id[64];
        if (!colon) {
            break;
        }
        colon++;
        if (demo_parse_json_string(&colon, id, sizeof(id)) == 0 && strcmp(id, "main") == 0) {
            break;
        }
        p = colon;
    }

    if (!p) {
        free(json);
        return -1;
    }

    options_key = strstr(p, "\"options\"");
    if (!options_key) {
        free(json);
        return -1;
    }
    array_begin = strchr(options_key, '[');
    if (!array_begin) {
        free(json);
        return -1;
    }
    array_end = demo_find_matching_bracket(array_begin, '[', ']');
    if (!array_end) {
        free(json);
        return -1;
    }

    p = array_begin;
    while ((p = strstr(p, "\"label\"")) != NULL && p < array_end) {
        const char *colon = strchr(p, ':');
        char label[256];
        if (!colon || colon >= array_end) {
            break;
        }
        colon++;
        if (demo_parse_json_string(&colon, label, sizeof(label)) == 0) {
            if (demo_labels_add(labels, label) != 0) {
                demo_labels_free(labels);
                free(json);
                return -1;
            }
        }
        p = colon;
    }

    free(json);
    return (labels->count > 0) ? 0 : -1;
}

static int demo_completion(const cptk_buffer_state *state, cptk_completion_list *out_list, void *user_data) {
    size_t i;
    size_t prefix_len;
    (void)user_data;

    if (!state || !out_list || !out_list->items || out_list->capacity == 0) {
        return 0;
    }

    prefix_len = state->text_len;
    out_list->count = 0;

    for (i = 0; i < sizeof(k_demo_cmds) / sizeof(k_demo_cmds[0]); ++i) {
        if (prefix_len == 0 || strncmp(k_demo_cmds[i], state->text_utf8, prefix_len) == 0) {
            out_list->items[out_list->count].text = k_demo_cmds[i];
            out_list->items[out_list->count].display = k_demo_cmds[i];
            out_list->items[out_list->count].meta = "command";
            out_list->count++;
            if (out_list->count >= out_list->capacity) {
                break;
            }
        }
    }

    return 0;
}

static void demo_log(cptk_log_level level, const char *message, void *user_data) {
    (void)user_data;
    if (level <= CPTK_LOG_INFO) {
        fprintf(stderr, "[cptk:%d] %s\n", (int)level, message ? message : "");
    }
}

int main(void) {
    cptk_context *ctx = cptk_context_create();
    cptk_callbacks callbacks;
    cptk_prompt_options options;
    const cptk_loop *loop = NULL;
    cptk_status st;
    char line[1024];
    const char *fallback_items[] = {
        "One-key Root",
        "Open shell with adb",
        "About",
        "Extension manager",
        "Exit"
    };
    demo_menu_labels loaded_items;
    const char *const *menu_items = fallback_items;
    size_t menu_count = sizeof(fallback_items) / sizeof(fallback_items[0]);
    size_t selected = 0;

    if (!ctx) {
        fprintf(stderr, "failed to create context\n");
        return 1;
    }

    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.on_completion = demo_completion;
    callbacks.on_log = demo_log;
    cptk_context_set_callbacks(ctx, &callbacks);

    st = cptk_loop_get(ctx, &loop);
    if (st == CPTK_STATUS_OK && loop) {
        printf("=== c_prompt_toolkit PoC ===\n");
        printf("backend: %s\n", cptk_loop_backend_name(loop));
    }

    cptk_default_prompt_options(&options);
    options.prompt_text = "ATB> ";

    st = cptk_prompt_run(ctx, &options, line, sizeof(line));
    if (st != CPTK_STATUS_OK) {
        fprintf(stderr, "prompt failed: %s\n", cptk_status_message(st));
        cptk_context_destroy(ctx);
        return 2;
    }

    printf("You typed: %s\n", line);

    memset(&loaded_items, 0, sizeof(loaded_items));
    if (demo_load_main_menu_labels("src/menu/start/menus.json", &loaded_items) == 0 && loaded_items.count > 0) {
        menu_items = (const char *const *)loaded_items.items;
        menu_count = loaded_items.count;
        printf("menu source: src/menu/start/menus.json (%u items)\n", (unsigned)menu_count);
    } else {
        printf("menu source: builtin fallback (%u items)\n", (unsigned)menu_count);
    }

    st = cptk_menu_choice(ctx, "Main Menu", menu_items, menu_count, 0, &selected);
    if (st != CPTK_STATUS_OK) {
        fprintf(stderr, "menu failed: %s\n", cptk_status_message(st));
        demo_labels_free(&loaded_items);
        cptk_context_destroy(ctx);
        return 3;
    }

    printf("Selected: %u. %s\n", (unsigned)(selected + 1), menu_items[selected]);

    demo_labels_free(&loaded_items);

    cptk_context_destroy(ctx);
    return 0;
}
