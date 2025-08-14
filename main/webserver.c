#include "webserver.h"

esp_err_t index_get_handler(httpd_req_t *req)
{
    const char *html =
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
        "<title>ATHULYA NURSE ID</title>"
        "<style>"
        "body { font-family: Arial, sans-serif; text-align: center; padding: 20px; }"
        "input, select { margin: 10px; padding: 10px; font-size: 16px; width: 80%; max-width: 300px; }"
        "input[type='submit'], button { background-color: #4CAF50; color: white; border: none; cursor: pointer; }"
        "button { margin-top: 20px; }"
        "</style>"
        "<script>"
        "function updateForm(){"
        "const type=document.getElementById('placeType').value;"
        "const fields=['roomFields','officeFields','doctorFields','ebFields','stockFields','menHostelFields','womenHostelFields'];"
        "fields.forEach(id=>document.getElementById(id).style.display='none');"
        "document.getElementById('commonFields').style.display='block';"
        "if(type==='Room')document.getElementById('roomFields').style.display='block';"
        "else if(type==='Office')document.getElementById('officeFields').style.display='block';"
        "else if(type==='Doctor')document.getElementById('doctorFields').style.display='block';"
        "else if(type==='EB')document.getElementById('ebFields').style.display='block';"
        "else if(type==='Store/Stock')document.getElementById('stockFields').style.display='block';"
        "else if(type==='Hostel'){"
        "document.getElementById('hostelTypeSelector').style.display='block';"
        "const gender=document.querySelector('input[name=\"hostelType\"]:checked');"
        "if(gender&&gender.value==='Women')document.getElementById('womenHostelFields').style.display='block';"
        "else document.getElementById('menHostelFields').style.display='block';"
        "}else{document.getElementById('hostelTypeSelector').style.display='none';}"
        "}"
        "function ensureRoomValue(){"
        "const type=document.getElementById('placeType').value;"
        "let value=type;"
        "if(type==='Room')value=document.getElementById('roomNumberRoom').value;"
        "else if(type==='Doctor')value=document.getElementById('roomNumberDoctor').value;"
        "else if(type==='EB')value=document.getElementById('roomNumberEB').value;"
        "else if(type==='Office')value=document.getElementById('roomNumberOffice').value;"
        "else if(type==='Store/Stock')value=document.getElementById('roomNumberStock').value;"
        "else if(type==='Hostel'){"
        "const gender=document.querySelector('input[name=\"hostelType\"]:checked');"
        "if(gender&&gender.value==='Women'){"
        "const wValue=document.getElementById('roomNumberWomen').value;"
        "value=wValue?\"Women_\"+wValue:\"Women\";"
        "}else{value='Men';}"
        "}"
        "document.getElementById('roomNumberHidden').value=value;"
        "}"
        "</script>"
        "</head>"
        "<body>"
        "<h1>NODE CONFIGURATION</h1>"
        "<form action='/config' method='POST' onsubmit='ensureRoomValue()'>"
        "<input type='text' name='ssid' placeholder='Wi-Fi SSID' required><br>"
        "<input type='password' name='password' placeholder='Wi-Fi Password' required><br>"
        "<label for='placeType'>Place Type:</label>"
        "<select id='placeType' name='placeType' onchange='updateForm()' required>"
        "<option value='' selected disabled>Select</option>"
        "<option value='Room'>Room</option>"
        "<option value='Office'>Office</option>"
        "<option value='Reception'>Reception</option>"
        "<option value='Kitchen'>Kitchen</option>"
        "<option value='Security'>Security</option>"
        "<option value='IT'>IT</option>"
        "<option value='Physio'>Physio</option>"
        "<option value='Training'>Training</option>"
        "<option value='Conference'>Conference</option>"
        "<option value='Doctor'>Doctor</option>"
        "<option value='Activity'>Activity</option>"
        "<option value='Dinning'>Dinning</option>"
        "<option value='EB'>EB</option>"
        "<option value='Store/Stock'>Store/Stock</option>"
        "<option value='Academy Lab'>Academy Lab</option>"
        "<option value='Hostel'>Hostel</option>"
        "</select><br>"
        "<div id='roomFields' style='display:none;'><input id='roomNumberRoom' type='text' placeholder='Room Number'><br></div>"
        "<div id='officeFields' style='display:none;'><select id='roomNumberOffice'><option value='Athulya Office'>Athulya Office</option><option value='Academy Office'>Academy Office</option></select><br></div>"
        "<div id='doctorFields' style='display:none;'><input id='roomNumberDoctor' type='text' placeholder='Doctor Room Number'><br></div>"
        "<div id='ebFields' style='display:none;'><input id='roomNumberEB' type='text' placeholder='EB Identifier'><br></div>"
        "<div id='stockFields' style='display:none;'><select id='roomNumberStock'><option value='Kitchen Stock'>Kitchen Stock</option><option value='Stock Room'>Stock Room</option></select><br></div>"
        "<div id='hostelTypeSelector' style='display:none;'><label>Hostel Gender:</label><br><input type='radio' name='hostelType' value='Men' onchange='updateForm()'> Men <input type='radio' name='hostelType' value='Women' onchange='updateForm()'> Women<br></div>"
        "<div id='menHostelFields' style='display:none;'><label>Hostel Type: Men</label><br></div>"
        "<div id='womenHostelFields' style='display:none;'><input id='roomNumberWomen' type='text' placeholder='Women Hostel Room Number'><br></div>"
        "<div id='commonFields' style='display:none;'><input type='text' name='location' placeholder='Location'><br><input type='text' name='tower' placeholder='Tower'><br><input type='text' name='floorNumber' placeholder='Floor'><br></div>"
        "<input type='hidden' name='roomNumber' id='roomNumberHidden'>"
        "<input type='submit' value='Save Configuration'>"
        "</form>" 
        "</body></html>";

    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
}

static char* get_param(const char *buf, const char *key, char *value, size_t max_len)
{
    memset(value, 0, max_len);
    char *start = strstr(buf, key);
    if (!start)
        return value;
    start += strlen(key) + 1; // skip key=
    char *end = strchr(start, '&');
    if (!end)
        end = (char *)buf + strlen(buf);
    int len = end - start;
    if (len >= max_len)
        len = max_len - 1;
    strncpy(value, start, len);
    // Replace '+' with space (URL encoding)
    for (int i = 0; i < len; i++)
        if (value[i] == '+')
            value[i] = ' ';
    return value;
}

esp_err_t config_device_handler(httpd_req_t *req)
{
    ESP_LOGI("SERVER", "Request received for URI: %s", req->uri);
    char buf[256];
    int total_len = req->content_len;
    int received = 0;

    if (total_len >= sizeof(buf))
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Content too long");
        return ESP_FAIL;
    }

    // Read POST data
    while (received < total_len)
    {
        int ret = httpd_req_recv(req, buf + received, total_len - received);
        if (ret <= 0)
        {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT)
                continue;
            return ESP_FAIL;
        }
        received += ret;
    }
    buf[received] = '\0';

    // Parameter buffers
    char ssid[MAX_PARAM_LEN] = {0};
    char password[MAX_PARAM_LEN] = {0};
    char placeType[MAX_PARAM_LEN] = {0};
    char roomNumber[MAX_PARAM_LEN] = {0};
    char location[MAX_PARAM_LEN] = {0};
    char tower[MAX_PARAM_LEN] = {0};
    char floorNumber[MAX_PARAM_LEN] = {0};

    // Extract parameters safely
    get_param(buf, "ssid", ssid, sizeof(ssid));
    get_param(buf, "password", password, sizeof(password));
    get_param(buf, "placeType", placeType, sizeof(placeType));
    get_param(buf, "roomNumber", roomNumber, sizeof(roomNumber));
    get_param(buf, "location", location, sizeof(location));
    get_param(buf, "tower", tower, sizeof(tower));
    get_param(buf, "floorNumber", floorNumber, sizeof(floorNumber));

    // Log received parameters
    ESP_LOGI("SERVER", "Received parameters:");
    ESP_LOGI("SERVER", "ssid: %s", ssid);
    ESP_LOGI("SERVER", "password: %s", password);
    ESP_LOGI("SERVER", "placeType: %s", placeType);
    ESP_LOGI("SERVER", "roomNumber: %s", roomNumber);
    ESP_LOGI("SERVER", "location: %s", location);
    ESP_LOGI("SERVER", "tower: %s", tower);
    ESP_LOGI("SERVER", "floorNumber: %s", floorNumber);

    // Copy to global variables (assumes globals are large enough)
    strncpy(g_ssid, ssid, sizeof(g_ssid));
    strncpy(g_password, password, sizeof(g_password));
    strncpy(g_placeType, placeType, sizeof(g_placeType));
    strncpy(g_roomNumber, roomNumber, sizeof(g_roomNumber));
    strncpy(g_location, location, sizeof(g_location));
    strncpy(g_tower, tower, sizeof(g_tower));
    strncpy(g_floorNumber, floorNumber, sizeof(g_floorNumber));

    // Save all parameters to NVS
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("wifi_config", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK)
    {
        ESP_LOGI("SERVER", "Storing parameters to NVS...");
        nvs_set_str(nvs_handle, "ssid", ssid);
        ESP_LOGI("SERVER", "Stored ssid");
        nvs_set_str(nvs_handle, "password", password);
        ESP_LOGI("SERVER", "Stored password");
        nvs_set_str(nvs_handle, "placeType", placeType);
        ESP_LOGI("SERVER", "Stored placeType");
        nvs_set_str(nvs_handle, "roomNumber", roomNumber);
        ESP_LOGI("SERVER", "Stored roomNumber");
        nvs_set_str(nvs_handle, "location", location);
        ESP_LOGI("SERVER", "Stored location");
        nvs_set_str(nvs_handle, "tower", tower);
        ESP_LOGI("SERVER", "Stored tower");
        nvs_set_str(nvs_handle, "floorNumber", floorNumber);
        ESP_LOGI("SERVER", "Stored floorNumber");

        err = nvs_commit(nvs_handle);
        if (err != ESP_OK) {
            ESP_LOGE("SERVER", "Failed to commit NVS");
        } else {
            ESP_LOGI("SERVER", "NVS commit successful");
        }
        nvs_close(nvs_handle);
    }
    else
    {
        ESP_LOGE("SERVER", "Failed to open NVS");
    }

    // Configure Wi-Fi station mode with new credentials
    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));

    // esp_wifi_set_mode(WIFI_MODE_STA);
    // esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    // esp_wifi_connect();
    switch_wifi_mode(false);
    // Send confirmation response
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req,
                    "<html><body><h2>Configuration Saved!</h2><p>Device is connecting to the new Wi-Fi...</p></body></html>",
                    HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_uri_t index_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = index_get_handler,
            .user_ctx = NULL};

        httpd_uri_t config_device_uri = {
            .uri = "/config",
            .method = HTTP_POST,
            .handler = config_device_handler,
            .user_ctx = NULL};
        httpd_register_uri_handler(server, &index_uri);
        httpd_register_uri_handler(server, &config_device_uri);
    }
}
