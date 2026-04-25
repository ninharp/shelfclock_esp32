#pragma once
#include <stdbool.h>
#include <stddef.h>

bool wifi_manager_init(void);
bool wifi_manager_is_connected(void);
void wifi_manager_get_ip(char *buf, size_t len);
void captive_portal_start(void);
