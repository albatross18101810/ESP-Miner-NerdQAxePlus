#include "scoreboard.h"

#include <math.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"

static const char *TAG = "scoreboard";

#define NVS_NAMESPACE "scoreboard" // its own namespace, so no other reset touches it
#define NVS_KEY "top20"
#define BLOB_MAGIC 0x53434231u
#define BLOB_VERSION 1
#define SAVE_INTERVAL_US (5ULL * 60ULL * 1000000ULL) // flash-wear limit: at most one write per 5 min

typedef struct {
    uint32_t magic;
    uint8_t version;
    uint8_t count;
    uint16_t reserved;
    scoreboard_entry_t entries[SCOREBOARD_SIZE];
} scoreboard_blob_t;

static scoreboard_entry_t s_entries[SCOREBOARD_SIZE];
static size_t s_count = 0;
static bool s_dirty = false;
static SemaphoreHandle_t s_lock = NULL;
static esp_timer_handle_t s_timer = NULL;

// Caller holds the lock. Returns true if the entry was inserted.
static bool insert_locked(double difficulty, uint32_t timestamp)
{
    if (!isfinite(difficulty) || difficulty <= 0.0) {
        return false;
    }
    // Cheap rejection: list full and this share does not beat the lowest entry.
    if (s_count == SCOREBOARD_SIZE && difficulty <= s_entries[s_count - 1].difficulty) {
        return false;
    }
    for (size_t i = 0; i < s_count; i++) {
        if (s_entries[i].difficulty == difficulty) {
            return false; // duplicate
        }
    }

    size_t pos = 0;
    while (pos < s_count && s_entries[pos].difficulty > difficulty) {
        pos++;
    }
    size_t last = (s_count < SCOREBOARD_SIZE) ? s_count : SCOREBOARD_SIZE - 1;
    memmove(&s_entries[pos + 1], &s_entries[pos], (last - pos) * sizeof(scoreboard_entry_t));
    s_entries[pos].difficulty = difficulty;
    s_entries[pos].timestamp = timestamp;
    if (s_count < SCOREBOARD_SIZE) {
        s_count++;
    }
    s_dirty = true;
    return true;
}

static bool blob_is_valid(const scoreboard_blob_t *b)
{
    if (b->magic != BLOB_MAGIC || b->version != BLOB_VERSION || b->count > SCOREBOARD_SIZE) {
        return false;
    }
    for (size_t i = 0; i < b->count; i++) {
        double d = b->entries[i].difficulty;
        if (!isfinite(d) || d <= 0.0) {
            return false;
        }
        if (i > 0 && d >= b->entries[i - 1].difficulty) {
            return false; // must be strictly descending
        }
    }
    return true;
}

static void load_from_nvs(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) {
        ESP_LOGI(TAG, "no saved scoreboard, starting empty");
        return;
    }
    scoreboard_blob_t blob;
    size_t len = sizeof(blob);
    esp_err_t err = nvs_get_blob(h, NVS_KEY, &blob, &len);
    nvs_close(h);

    if (err != ESP_OK || len != sizeof(blob) || !blob_is_valid(&blob)) {
        ESP_LOGW(TAG, "saved scoreboard missing or invalid (%s), starting empty", esp_err_to_name(err));
        return;
    }
    memcpy(s_entries, blob.entries, sizeof(s_entries));
    s_count = blob.count;
    ESP_LOGI(TAG, "loaded %u entries", (unsigned) s_count);
}

static void save_to_nvs(const scoreboard_blob_t *blob)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        err = nvs_set_blob(h, NVS_KEY, blob, sizeof(*blob));
        if (err == ESP_OK) {
            err = nvs_commit(h);
        }
        nvs_close(h);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "save failed: %s", esp_err_to_name(err));
        xSemaphoreTake(s_lock, portMAX_DELAY);
        s_dirty = true; // try again on the next tick
        xSemaphoreGive(s_lock);
    }
}

void scoreboard_flush(void)
{
    if (!s_lock) {
        return;
    }
    scoreboard_blob_t blob;
    memset(&blob, 0, sizeof(blob));

    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool need = s_dirty;
    if (need) {
        blob.magic = BLOB_MAGIC;
        blob.version = BLOB_VERSION;
        blob.count = (uint8_t) s_count;
        memcpy(blob.entries, s_entries, sizeof(s_entries));
        s_dirty = false;
    }
    xSemaphoreGive(s_lock);

    if (need) {
        save_to_nvs(&blob); // flash write happens outside the lock
    }
}

static void save_timer_cb(void *arg)
{
    (void) arg;
    scoreboard_flush();
}

void scoreboard_init(void)
{
    if (s_lock) {
        return;
    }
    s_lock = xSemaphoreCreateMutex();
    load_from_nvs();

    const esp_timer_create_args_t args = {.callback = save_timer_cb, .name = "scoreboard_save"};
    if (esp_timer_create(&args, &s_timer) == ESP_OK) {
        esp_timer_start_periodic(s_timer, SAVE_INTERVAL_US);
    }
}

bool scoreboard_submit(double difficulty, uint32_t timestamp)
{
    if (!s_lock) {
        return false;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool inserted = insert_locked(difficulty, timestamp);
    xSemaphoreGive(s_lock);
    if (inserted) {
        ESP_LOGI(TAG, "new top-20 entry: %.0f", difficulty);
    }
    return inserted;
}

size_t scoreboard_get(scoreboard_entry_t *out, size_t max)
{
    if (!s_lock || !out) {
        return 0;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    size_t n = (s_count < max) ? s_count : max;
    memcpy(out, s_entries, n * sizeof(scoreboard_entry_t));
    xSemaphoreGive(s_lock);
    return n;
}

void scoreboard_clear(void)
{
    if (!s_lock) {
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_count = 0;
    memset(s_entries, 0, sizeof(s_entries));
    s_dirty = true;
    xSemaphoreGive(s_lock);
    scoreboard_flush();
}
