/* SPDX-License-Identifier: MIT */
#ifndef LC_TVOS_PLATFORM_H
#define LC_TVOS_PLATFORM_H
#include <stddef.h>
struct SDL_Window;
void lc_tvos_attach_window(struct SDL_Window *window);
void lc_tvos_data_path(char *out, size_t capacity, const char *name);
int lc_tvos_http(const char *url, const char *body, char *out, size_t capacity, int timeout_ms);
void lc_tvos_edit(char *text, size_t capacity, int *length, int password, const char *title);
int lc_tvos_editing(void);
#endif
