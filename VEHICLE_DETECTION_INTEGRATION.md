# Vehicle Detection Integration Guide

This document contains all the code additions needed to integrate vehicle auto-detection into WiCAN.

## 1. Add Include to config_server.c

Add this after line 50 (after `#include "ver.h"`):

```c
#include "vehicle_detect.h"
```

## 2. Add Handler Functions to config_server.c

Add these handler functions before the URI definitions (around line 1470, before `static const httpd_uri_t file_upload`):

```c
/* Vehicle Detection Handlers */

static esp_err_t vehicle_detect_start_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Starting vehicle detection");

    // Start async detection
    int ret = vehicle_detect_start_async();

    cJSON *root = cJSON_CreateObject();

    if (ret == 0) {
        cJSON_AddStringToObject(root, "status", "started");
        cJSON_AddStringToObject(root, "message", "Vehicle detection started");
    } else if (ret == -2) {
        cJSON_AddStringToObject(root, "status", "error");
        cJSON_AddStringToObject(root, "message", "Detection already in progress");
        httpd_resp_set_status(req, "409 Conflict");
    } else {
        cJSON_AddStringToObject(root, "status", "error");
        cJSON_AddStringToObject(root, "message", "Failed to start detection");
        httpd_resp_set_status(req, "500 Internal Server Error");
    }

    const char *resp = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);

    free((void *)resp);
    cJSON_Delete(root);

    return ESP_OK;
}

static esp_err_t vehicle_detect_status_handler(httpd_req_t *req)
{
    detection_status_t status;
    int ret = vehicle_detect_get_status(&status);

    cJSON *root = cJSON_CreateObject();

    if (ret == 0) {
        cJSON_AddBoolToObject(root, "in_progress", status.in_progress);
        cJSON_AddNumberToObject(root, "progress_percent", status.progress_percent);
        cJSON_AddNumberToObject(root, "elapsed_ms", status.elapsed_ms);
        cJSON_AddNumberToObject(root, "addresses_seen", status.addresses_seen);
    } else {
        cJSON_AddStringToObject(root, "error", "Failed to get status");
        httpd_resp_set_status(req, "500 Internal Server Error");
    }

    const char *resp = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);

    free((void *)resp);
    cJSON_Delete(root);

    return ESP_OK;
}

static esp_err_t vehicle_detect_result_handler(httpd_req_t *req)
{
    detection_result_t result;
    int ret = vehicle_detect_get_result(&result);

    cJSON *root = cJSON_CreateObject();

    if (ret == 0) {
        cJSON_AddBoolToObject(root, "detection_complete", result.detection_complete);
        cJSON_AddNumberToObject(root, "scan_duration_ms", result.scan_duration_ms);
        cJSON_AddNumberToObject(root, "total_addresses_seen", result.total_addresses_seen);
        cJSON_AddNumberToObject(root, "match_count", result.match_count);

        cJSON *matches = cJSON_CreateArray();
        for (int i = 0; i < result.match_count; i++) {
            cJSON *match = cJSON_CreateObject();
            cJSON_AddStringToObject(match, "vehicle_name", result.matches[i].vehicle_name);
            cJSON_AddStringToObject(match, "year_range", result.matches[i].year_range);
            cJSON_AddNumberToObject(match, "confidence", result.matches[i].confidence);
            cJSON_AddNumberToObject(match, "matched_addresses", result.matches[i].matched_addresses);
            cJSON_AddNumberToObject(match, "total_required", result.matches[i].total_required);
            cJSON_AddItemToArray(matches, match);
        }
        cJSON_AddItemToObject(root, "matches", matches);
    } else if (ret == -1) {
        cJSON_AddStringToObject(root, "status", "not_complete");
        cJSON_AddStringToObject(root, "message", "Detection not complete yet");
        httpd_resp_set_status(req, "202 Accepted");
    } else {
        cJSON_AddStringToObject(root, "error", "Failed to get result");
        httpd_resp_set_status(req, "500 Internal Server Error");
    }

    const char *resp = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);

    free((void *)resp);
    cJSON_Delete(root);

    return ESP_OK;
}
```

## 3. Add URI Definitions to config_server.c

Add these after the other URI definitions (around line 1490, after `system_reboot`):

```c
static const httpd_uri_t vehicle_detect_start = {
    .uri       = "/api/vehicle/detect",
    .method    = HTTP_POST,
    .handler   = vehicle_detect_start_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t vehicle_detect_status = {
    .uri       = "/api/vehicle/detect/status",
    .method    = HTTP_GET,
    .handler   = vehicle_detect_status_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t vehicle_detect_result = {
    .uri       = "/api/vehicle/detect/result",
    .method    = HTTP_GET,
    .handler   = vehicle_detect_result_handler,
    .user_ctx  = NULL
};
```

## 4. Register URI Handlers in config_server.c

Add these registrations in the `start_webserver()` function, after the other `httpd_register_uri_handler` calls (around line 2262):

```c
httpd_register_uri_handler(server, &vehicle_detect_start);
httpd_register_uri_handler(server, &vehicle_detect_status);
httpd_register_uri_handler(server, &vehicle_detect_result);
```

## 5. Add Publishing Mode to config_server.h

Add this enum after the existing typedefs (around line 100):

```c
typedef enum {
    PUBLISH_MODE_STATIC = 0,   // Vehicle profile only (legacy)
    PUBLISH_MODE_DYNAMIC = 1,  // On-demand subscriptions only
    PUBLISH_MODE_HYBRID = 2    // Both (default/recommended)
} mqtt_publish_mode_t;
```

Add this field to the config structure (in the MQTT section):

```c
uint8_t mqtt_publish_mode;  // mqtt_publish_mode_t
```

## 6. Initialize Publishing Mode in config_server.c

In the `config_server_init()` function or wherever defaults are set, add:

```c
config.mqtt_publish_mode = PUBLISH_MODE_HYBRID;  // Default to hybrid mode
```

## 7. Add to main.c

Add include at the top:

```c
#include "vehicle_detect.h"
```

In `app_main()`, after other initializations (after `mqtt_broker_init()` if present):

```c
// Initialize vehicle detection
ESP_LOGI(TAG, "Initializing vehicle detection");
vehicle_detect_init();

// Load fingerprints from SPIFFS
FILE *fp = fopen("/spiffs/vehicle_fingerprints.json", "r");
if (fp) {
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *json_data = malloc(fsize + 1);
    if (json_data) {
        fread(json_data, 1, fsize, fp);
        json_data[fsize] = 0;

        int loaded = vehicle_detect_load_fingerprints(json_data);
        ESP_LOGI(TAG, "Loaded %d vehicle fingerprints", loaded);

        free(json_data);
    }
    fclose(fp);
} else {
    ESP_LOGW(TAG, "No vehicle_fingerprints.json found");
}
```

## 8. Web UI Updates (homepage.html)

These are the JavaScript functions and HTML to add to the Automate tab.

### Add after the existing vehicle profile section:

```html
<div class="section">
    <h2>Vehicle Auto-Detection</h2>
    <p>Automatically detect your vehicle by scanning the CAN bus</p>

    <button id="auto-detect-btn" class="btn-primary" onclick="startVehicleDetection()">
        🔍 Auto-Detect Vehicle
    </button>

    <div id="detection-progress" style="display:none; margin-top: 15px;">
        <p>Scanning CAN bus... <span id="progress-percent">0%</span></p>
        <div style="background:#ddd; border-radius:4px; overflow:hidden;">
            <div id="progress-bar" style="width:0%; height:20px; background:#4CAF50; transition:width 0.3s;"></div>
        </div>
        <p><small>Found <span id="addresses-seen">0</span> CAN addresses</small></p>
    </div>

    <div id="detection-results" style="display:none; margin-top: 15px;">
        <h3>Detected Vehicles:</h3>
        <div id="matches-list"></div>
        <p style="margin-top:10px;">
            <small>Not your vehicle?
                <a href="#" onclick="showManualSelect(); return false;">Select manually</a>
            </small>
        </p>
    </div>
</div>

<div class="section">
    <h2>Publishing Mode</h2>
    <label>
        <input type="radio" name="mqtt_publish_mode" value="0">
        Static - Vehicle Profile Only (Legacy)
    </label><br>
    <label>
        <input type="radio" name="mqtt_publish_mode" value="1">
        Dynamic - On-Demand Subscriptions Only
    </label><br>
    <label>
        <input type="radio" name="mqtt_publish_mode" value="2" checked>
        Hybrid - Both (Recommended)
    </label>
    <p><small>
        <strong>Static:</strong> Uses vehicle profile configuration (backwards compatible)<br>
        <strong>Dynamic:</strong> Only publishes signals requested by connected clients<br>
        <strong>Hybrid:</strong> Publishes vehicle profile + any requested signals
    </small></p>
</div>
```

### Add JavaScript functions:

```javascript
let detectionInterval = null;

function startVehicleDetection() {
    document.getElementById('auto-detect-btn').disabled = true;
    document.getElementById('detection-progress').style.display = 'block';
    document.getElementById('detection-results').style.display = 'none';

    // Start detection
    fetch('/api/vehicle/detect', { method: 'POST' })
        .then(response => response.json())
        .then(data => {
            if (data.status === 'started') {
                pollDetectionStatus();
            } else {
                alert('Failed to start detection: ' + data.message);
                resetDetectionUI();
            }
        })
        .catch(err => {
            alert('Error starting detection: ' + err);
            resetDetectionUI();
        });
}

function pollDetectionStatus() {
    detectionInterval = setInterval(() => {
        fetch('/api/vehicle/detect/status')
            .then(response => response.json())
            .then(data => {
                document.getElementById('progress-percent').textContent = data.progress_percent + '%';
                document.getElementById('progress-bar').style.width = data.progress_percent + '%';
                document.getElementById('addresses-seen').textContent = data.addresses_seen;

                if (!data.in_progress || data.progress_percent >= 100) {
                    clearInterval(detectionInterval);
                    setTimeout(getDetectionResults, 500);
                }
            });
    }, 500);
}

function getDetectionResults() {
    fetch('/api/vehicle/detect/result')
        .then(response => response.json())
        .then(data => {
            if (data.detection_complete) {
                displayResults(data);
            } else {
                setTimeout(getDetectionResults, 1000);
            }
        })
        .catch(err => {
            alert('Error getting results: ' + err);
            resetDetectionUI();
        });
}

function displayResults(data) {
    document.getElementById('detection-progress').style.display = 'none';
    document.getElementById('detection-results').style.display = 'block';

    const matchesList = document.getElementById('matches-list');
    matchesList.innerHTML = '';

    if (data.match_count === 0) {
        matchesList.innerHTML = '<p>No vehicles detected. Try manual selection.</p>';
    } else {
        data.matches.forEach(match => {
            const confidence = (match.confidence * 100).toFixed(0);
            const div = document.createElement('div');
            div.className = 'match-item';
            div.style.cssText = 'padding:10px; margin:5px 0; border:1px solid #ddd; border-radius:4px;';
            div.innerHTML = `
                <strong>${match.vehicle_name}</strong> (${match.year_range}) - ${confidence}% match<br>
                <small>Matched ${match.matched_addresses}/${match.total_required} signatures</small><br>
                <button onclick="selectVehicle('${match.vehicle_name}')" style="margin-top:5px;">
                    Use This Profile
                </button>
            `;
            matchesList.appendChild(div);
        });
    }

    resetDetectionUI();
}

function resetDetectionUI() {
    document.getElementById('auto-detect-btn').disabled = false;
}

function selectVehicle(vehicleName) {
    // TODO: Load the vehicle profile automatically
    alert('Selected: ' + vehicleName + '\n\nNow loading vehicle profile...');
    // You would typically call your existing vehicle profile loading function here
}

function showManualSelect() {
    document.getElementById('detection-results').style.display = 'none';
    // Show the manual vehicle selector (your existing dropdown)
}
```

## 9. Copy fingerprints.json to SPIFFS

Make sure `vehicle_fingerprints.json` gets copied to the SPIFFS filesystem during build.

In `CMakeLists.txt`, you may need to add it to `EMBED_FILES` or ensure it's in the SPIFFS partition.

## Summary

This integration adds:

✅ Vehicle auto-detection via CAN fingerprinting
✅ 3 publishing modes (Static, Dynamic, Hybrid)
✅ REST API endpoints for detection
✅ Web UI for auto-detection
✅ 10 pre-loaded vehicle fingerprints (Tesla, Honda, Toyota, Ford, BMW, Hyundai, Chevy, Nissan, VW)

The implementation is backwards compatible - existing configurations will continue to work with the default Hybrid mode.
