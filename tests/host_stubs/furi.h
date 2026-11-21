/* Minimal host-side stub of Furi core, for compile-testing board drivers. */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define UNUSED(x)      (void)(x)
#define COUNT_OF(x)    (sizeof(x) / sizeof((x)[0]))
#define FURI_LOG_I(...) ((void)0)
#define FURI_LOG_E(...) ((void)0)
#define FURI_LOG_W(...) ((void)0)
#define FURI_LOG_D(...) ((void)0)

void furi_delay_ms(uint32_t ms);
void furi_delay_us(uint32_t us);
void furi_crash(const char* message);
