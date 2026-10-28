#include "handler_v2_scoreboard.h"

#include "esp_http_server.h"
#include "esp_log.h"

#include "ArduinoJson.h"
#include "psram_allocator.h"
#include "global_state.h"
#include "http_cors.h"
#include "http_utils.h"
#include "scoreboard.h"

static const char *TAG = "http_v2_scoreboard";

esp_err_t GET_V2_scoreboard(httpd_req_t *req)
{
    ConGuard g(http_server, req);

    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    httpd_resp_set_type(req, "application/json");
    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    scoreboard_entry_t entries[SCOREBOARD_SIZE];
    size_t count = scoreboard_get(entries, SCOREBOARD_SIZE);

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    JsonArray arr = doc["entries"].to<JsonArray>();
    for (size_t i = 0; i < count; i++) {
        JsonObject e = arr.add<JsonObject>();
        e["rank"]       = (int) (i + 1);
        e["difficulty"] = entries[i].difficulty;
        e["timestamp"]  = entries[i].timestamp; // unix seconds, 0 = clock was not synced
    }

    ESP_LOGD(TAG, "sending %u entries", (unsigned) count);
    return sendJsonResponse(req, doc);
}
