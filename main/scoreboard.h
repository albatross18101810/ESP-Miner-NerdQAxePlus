#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SCOREBOARD_SIZE 20

typedef struct {
    double difficulty;  // double on purpose: a uint32 caps out at 4.29G
    uint32_t timestamp; // unix time, or 0 if the clock was not synced yet
} scoreboard_entry_t;

// Call once after nvs_flash_init(). Loads the saved list and starts the
// periodic save timer. Safe to call when nothing has been saved yet.
void scoreboard_init(void);

// Call for every share (or at least every share above some floor).
// Returns true if the share made the top 20.
bool scoreboard_submit(double difficulty, uint32_t timestamp);

// Copies up to max entries (highest first) into out. Returns the count.
size_t scoreboard_get(scoreboard_entry_t *out, size_t max);

// Empties the list and saves immediately. Protect the caller with the
// firmware's existing OTP/auth check.
void scoreboard_clear(void);

// Saves now if anything changed. Call before a planned restart or OTA reboot.
void scoreboard_flush(void);

#ifdef __cplusplus
}
#endif
