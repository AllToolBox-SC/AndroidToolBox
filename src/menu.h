/* Lightweight C port of menu utilities (partial replacement for menu.py)
 * Provides simple wrappers around c_prompt_toolkit interactive menus
 * and small JSON helpers for loading option lists.
 */
#ifndef ATB_MENU_H
#define ATB_MENU_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Shared mouse click tracking state for menu interactions.
 * Single-click updates focus only; double-click on the same index confirms.
 */
typedef struct menu_mouse_click_state {
    int last_click_index;
    unsigned long long last_click_tick_ms;
} menu_mouse_click_state;

unsigned int menu_get_double_click_threshold_ms(void);
unsigned long long menu_now_ms(void);
void menu_mouse_click_state_reset(menu_mouse_click_state *state);
void menu_mouse_click_state_update(menu_mouse_click_state *state, int index, unsigned long long now_ms);
int menu_mouse_double_click_hit(const menu_mouse_click_state *state, int index, unsigned long long now_ms, unsigned int threshold_ms);
menu_mouse_click_state *menu_get_global_mouse_click_state(void);

/* Initialize cloud-control probe. Returns 1 on reachable, 0 on unreachable. */
int menu_init_cloud_control(const char *url, int timeout_ms);

/* Load options from a JSON file. On success returns 1 and sets
 * `*out_values` and `*out_labels` to newly allocated arrays of `count`
 * null-terminated strings. Caller must free via `menu_free_options`.
 */
int menu_load_options_from_json(const char *path, char ***out_values, char ***out_labels, size_t *out_count);
/* Load options from a file of various supported formats (JSON, CSV, TXT, XML, YAML, TOML).
 * On success returns 1 and sets out arrays; caller must free with menu_free_options.
 */
int menu_load_options_from_file(const char *path, char ***out_values, char ***out_labels, size_t *out_count);
void menu_free_options(char **values, char **labels, size_t count);

/* Interactive single-choice menu. `items` is array of display strings.
 * On success returns 0 and copies the selected item into `out_value`.
 * `out_capacity` is buffer size. Returns non-zero on error or cancel.
 */
int menu_choice_interactive(const char *title,
                            const char *const *items,
                            size_t item_count,
                            char *out_value,
                            size_t out_capacity,
                            size_t default_index);

/* Interactive multi-choice menu. On success returns 0 and writes a
 * comma-separated list of selected item values into `out_csv`.
 */
int menu_multi_choice_interactive(const char *title,
                                  const char *const *items,
                                  size_t item_count,
                                  char *out_csv,
                                  size_t out_capacity,
                                  int animate_in,
                                  int max_height);

/* Load action string from a JSON file. Returns newly allocated string
 * or NULL. Caller must free the returned pointer. */
char *menu_load_action_from_json(const char *path);

/* Simple pause helper: print message and wait for any key or mouse click.
 * `timeout_ms` <=0 means wait indefinitely. Returns 0 on key/mouse, 1 on timeout.
 */
int menu_pause(const char *message, int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif
