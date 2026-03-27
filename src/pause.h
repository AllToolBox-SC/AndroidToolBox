#ifndef ATB_PAUSE_H
#define ATB_PAUSE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Wait for any key or mouse click.
 * timeout_ms < 0 means wait forever.
 * timeout_ms == 0 means return immediately.
 * Returns 0 when resumed by input, 1 when timeout elapsed.
 */
int pause_wait(const char *message, int timeout_ms, const char *ansi_spec);

#ifdef __cplusplus
}
#endif

#endif
