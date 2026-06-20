#include "HttpServer.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <States/GlobalState.h>
#include <Enums/ModeEnum.h>
#include <Enums/TerminalTypeEnum.h>
#include <esp_timer.h>
#include <esp_system.h>


void HttpServer::setupRoutes() {
    // Page HTML
    static httpd_uri_t root_uri;
    root_uri.uri = "/";
    root_uri.method = HTTP_GET;
    root_uri.handler = [](httpd_req_t *req) -> esp_err_t {
        HttpServer* self = static_cast<HttpServer*>(req->user_ctx);
        return self->handleRootRequest(req);
    };
    root_uri.user_ctx = this;
    httpd_register_uri_handler(server, &root_uri);

    // CSS
    static httpd_uri_t css_uri;
    css_uri.uri = "/style.css";
    css_uri.method = HTTP_GET;
    css_uri.handler = [](httpd_req_t *req) -> esp_err_t {
        HttpServer* self = static_cast<HttpServer*>(req->user_ctx);
        return self->handleCssRequest(req);
    };
    css_uri.user_ctx = this;
    httpd_register_uri_handler(server, &css_uri);

    // JavaScript
    static httpd_uri_t js_uri;
    js_uri.uri = "/scripts.js";
    js_uri.method = HTTP_GET;
    js_uri.handler = [](httpd_req_t *req) -> esp_err_t {
        HttpServer* self = static_cast<HttpServer*>(req->user_ctx);
        return self->handleJsRequest(req);
    };
    js_uri.user_ctx = this;
    httpd_register_uri_handler(server, &js_uri);

    // Mount for routes LittleFS
    littleFsService.begin(/*formatIfFail=*/true, /*readOnly=*/false);
    
    // GET /littlefs/list?dir=/chemin
    static httpd_uri_t lfs_ls_uri;
    lfs_ls_uri.uri = "/littlefs/list";
    lfs_ls_uri.method = HTTP_GET;
    lfs_ls_uri.user_ctx = this;
    lfs_ls_uri.handler = [](httpd_req_t *req) -> esp_err_t {
        HttpServer* self = static_cast<HttpServer*>(req->user_ctx);
        return self->handleLittlefsList(req);
    };
    httpd_register_uri_handler(server, &lfs_ls_uri);

    // POST /littlefs/upload?file=<filename>
    static httpd_uri_t lfs_up_uri;
    lfs_up_uri.uri = "/littlefs/upload";
    lfs_up_uri.method = HTTP_POST;
    lfs_up_uri.user_ctx = this;
    lfs_up_uri.handler = [](httpd_req_t *req) -> esp_err_t {
        HttpServer* self = static_cast<HttpServer*>(req->user_ctx);
        return self->handleLittlefsUpload(req);
    };
    httpd_register_uri_handler(server, &lfs_up_uri);

    // DELETE /littlefs/delete?file=<filename>
    static httpd_uri_t lfs_del_uri;
    lfs_del_uri.uri = "/littlefs/delete";
    lfs_del_uri.method = HTTP_DELETE;
    lfs_del_uri.user_ctx = this;
    lfs_del_uri.handler = [](httpd_req_t *req) -> esp_err_t {
        HttpServer* self = static_cast<HttpServer*>(req->user_ctx);
        return self->handleLittlefsDelete(req);
    };
    httpd_register_uri_handler(server, &lfs_del_uri);

    // GET /littlefs/download?file=<filename>
    static httpd_uri_t lfs_dl_uri;
    lfs_dl_uri.uri = "/littlefs/download";
    lfs_dl_uri.method = HTTP_GET;
    lfs_dl_uri.user_ctx = this;
    lfs_dl_uri.handler = [](httpd_req_t *req) -> esp_err_t {
        HttpServer* self = static_cast<HttpServer*>(req->user_ctx);
        return self->handleLittlefsDownload(req);
    };
    httpd_register_uri_handler(server, &lfs_dl_uri);


    // GET /api/status -- AI/automation liveness + mode/status endpoint
    static httpd_uri_t api_status_uri;
    api_status_uri.uri = "/api/status";
    api_status_uri.method = HTTP_GET;
    api_status_uri.user_ctx = this;
    api_status_uri.handler = [](httpd_req_t *req) -> esp_err_t {
        HttpServer* self = static_cast<HttpServer*>(req->user_ctx);
        return self->handleApiStatus(req);
    };
    httpd_register_uri_handler(server, &api_status_uri);

    // POST /api/command -- inject one command into the Web terminal queue and
    // return bounded captured output. This is intended for AI/direct clients.
    static httpd_uri_t api_command_uri;
    api_command_uri.uri = "/api/command";
    api_command_uri.method = HTTP_POST;
    api_command_uri.user_ctx = this;
    api_command_uri.handler = [](httpd_req_t *req) -> esp_err_t {
        HttpServer* self = static_cast<HttpServer*>(req->user_ctx);
        return self->handleApiCommand(req);
    };
    httpd_register_uri_handler(server, &api_command_uri);

    // Keep config.max_uri_handlers high enough in main.cpp for ws + web + API + LittleFS routes.
}

esp_err_t HttpServer::handleRootRequest(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, (const char*)index_html, strlen((const char*)index_html));
}

esp_err_t HttpServer::handleCssRequest(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/css");
    return httpd_resp_send(req, (const char*)style_css, strlen((const char*)style_css));
}

esp_err_t HttpServer::handleJsRequest(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/javascript");
    return httpd_resp_send(req, (const char*)scripts_js, strlen((const char*)scripts_js));
}

esp_err_t HttpServer::handleLittlefsList(httpd_req_t *req) {
    // Parse ?dir=/...
    std::string dir = "/";
    int qlen = httpd_req_get_url_query_len(req);
    if (qlen > 0) {
        std::vector<char> query(qlen + 1, '\0');
        if (httpd_req_get_url_query_str(req, query.data(), query.size()) == ESP_OK) {
            char val[256];
            if (httpd_query_key_value(query.data(), "dir", val, sizeof(val)) == ESP_OK) {
                dir = val;
            }
        }
    }

    // Not found
    if (!littleFsService.isDir(dir)) {
        httpd_resp_set_type(req, "application/json; charset=utf-8");
        httpd_resp_set_hdr(req, "Cache-Control", "no-store");
        const std::string payload =
            std::string("{\"error\":\"dir not found\",\"dir\":\"") +
            JsonTransformer::escape(dir) + "\"}";
        httpd_resp_set_status(req, "404 Not Found");
        return httpd_resp_send(req, payload.c_str(), HTTPD_RESP_USE_STRLEN);
    }

    // Espace total/used
    size_t total = 0, used = 0;
    littleFsService.getSpace(total, used);

    // Entries
    auto lfsEntries = littleFsService.list(dir);
    std::vector<std::string> names; names.reserve(lfsEntries.size());
    std::vector<size_t>      sizes; sizes.reserve(lfsEntries.size());
    std::vector<uint8_t>     isDirs; isDirs.reserve(lfsEntries.size());

    for (const auto& e : lfsEntries) {
        names.push_back(e.name);
        sizes.push_back(e.size);
        isDirs.push_back(e.isDir ? 1u : 0u);
    }

    // JSON
    std::string payload = JsonTransformer::makeLsJson(dir, total, used, names, sizes, isDirs);
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");

    return httpd_resp_send(req, payload.c_str(), HTTPD_RESP_USE_STRLEN);
}

esp_err_t HttpServer::handleLittlefsDelete(httpd_req_t *req) {
    // Parse ?name=<file>
    std::string name;
    int qlen = httpd_req_get_url_query_len(req);
    if (qlen > 0) {
        std::vector<char> query(qlen + 1, '\0');
        if (httpd_req_get_url_query_str(req, query.data(), query.size()) == ESP_OK) {
            char val[256];
            if (httpd_query_key_value(query.data(), "file", val, sizeof(val)) == ESP_OK) {
                name = val;
            }
        }
    }

    // Json header
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");

    // Validate name
    if (!littleFsService.isSafeRootFileName(name)) {
        httpd_resp_set_status(req, "400 Bad Request");
        std::string payload = std::string("{\"error\":\"invalid name\",\"name\":\"") +
                              JsonTransformer::escape(name) + "\"}";
        return httpd_resp_send(req, payload.c_str(), HTTPD_RESP_USE_STRLEN);
    }

    // Construct path
    std::string path = "/" + name;

    // Attempt delete
    bool rc = littleFsService.removeFile(path);
    if (!rc) {
        httpd_resp_set_status(req, "404 Not Found");
        std::string payload = std::string("{\"error\":\"file not found or cannot delete\",\"name\":\"") +
                              JsonTransformer::escape(name) + "\"}";
        return httpd_resp_send(req, payload.c_str(), HTTPD_RESP_USE_STRLEN);
    }

    // OK
    const char* ok = "{\"ok\":true}";

    return httpd_resp_send(req, ok, HTTPD_RESP_USE_STRLEN);
}

esp_err_t HttpServer::handleLittlefsDownload(httpd_req_t* req) {
    std::string name;
    bool forceDownload = false;

    // Get ?file=<name>
    int qlen = httpd_req_get_url_query_len(req);
    if (qlen > 0) {
        std::vector<char> query(qlen + 1, '\0');
        if (httpd_req_get_url_query_str(req, query.data(), query.size()) == ESP_OK) {
            char val[256];
            if (httpd_query_key_value(query.data(), "file", val, sizeof(val)) == ESP_OK) {
                name = val;
            }
            if (httpd_query_key_value(query.data(), "dl", val, sizeof(val)) == ESP_OK) {
                forceDownload = (strcmp(val, "1") == 0 || strcasecmp(val, "true") == 0);
            }
        }
    }

    if (name.empty()) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing 'file' parameter");
        return ESP_FAIL;
    }
    if (!littleFsService.isSafeRootFileName(name)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad filename");
        return ESP_FAIL;
    }

    // Remove /
    std::string baseName = name;
    auto pos = baseName.find_last_of("/\\");
    if (pos != std::string::npos) {
        baseName = baseName.substr(pos + 1);
    }

    // Force download headers
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    std::string cd = std::string("attachment; filename=\"") + baseName + "\"";
    httpd_resp_set_hdr(req, "Content-Disposition", cd.c_str());
    httpd_resp_set_hdr(req, "Content-Transfer-Encoding", "binary");
    httpd_resp_set_type(req, "application/octet-stream");
    
    // Stream the file
    const std::string userPath = "/" + name;
    fs::File f = LittleFS.open(userPath.c_str(), "r");
    const size_t CHUNK = 1024;
    std::unique_ptr<uint8_t[]> buf(new (std::nothrow) uint8_t[CHUNK]);
    while (true) {
        int n = f.read(buf.get(), CHUNK);
        if (n < 0) { f.close(); return -1; }
        if (n == 0) break;
        if (httpd_resp_send_chunk(req, reinterpret_cast<const char*>(buf.get()), n) != ESP_OK) {
            f.close();
            return -1;
        }
    }
    f.close();
    httpd_resp_send_chunk(req, nullptr, 0);

    return 0; // ESP_OK
}

esp_err_t HttpServer::handleLittlefsUpload(httpd_req_t* req) {
    // Get ?file=<name>
    std::string name;
    int qlen = httpd_req_get_url_query_len(req);
    if (qlen > 0) {
        std::vector<char> query(qlen + 1, '\0');
        if (httpd_req_get_url_query_str(req, query.data(), query.size()) == ESP_OK) {
            char val[256];
            if (httpd_query_key_value(query.data(), "file", val, sizeof(val)) == ESP_OK) {
                name = sanitizeUploadFilename(val);
            }
        }
    }

    // Validate name
    if (name.empty() || !littleFsService.isSafeRootFileName(name)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing or bad ?file");
        return ESP_FAIL;
    }

    // Open file
    const std::string path = "/" + name;
    fs::File out = LittleFS.open(path.c_str(), "w");
    if (!out) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Cannot open file");
        return ESP_FAIL;
    }

    // Init buffer
    const size_t CHUNK = 1024;
    std::unique_ptr<uint8_t[]> buf(new (std::nothrow) uint8_t[CHUNK]);
    if (!buf) {
        out.close();
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
        return ESP_FAIL;
    }

    // Read all data in chunks
    int remaining = req->content_len;
    while (remaining > 0) {
        int to_read = remaining > (int)CHUNK ? (int)CHUNK : remaining;
        int n = httpd_req_recv(req, (char*)buf.get(), to_read);
        if (n <= 0) { out.close(); httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Recv error"); return ESP_FAIL; }
        if (out.write(buf.get(), n) != (size_t)n) { out.close(); httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Write error"); return ESP_FAIL; }
        remaining -= n;
    }
    out.close();

    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");

    return httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
}

std::string HttpServer::urlDecode(const char* s) {
    // Decode %XX and + to space
    std::string out;
    for (const char* p = s; *p; ++p) {
        if (*p == '+') out.push_back(' ');
        else if (*p == '%' && std::isxdigit((unsigned char)p[1]) && std::isxdigit((unsigned char)p[2])) {
            auto hex = [](char c){ c=std::toupper((unsigned char)c); return std::isdigit((unsigned char)c)? c-'0' : c-'A'+10; };
            out.push_back(char((hex(p[1])<<4) | hex(p[2])));
            p += 2;
        } else out.push_back(*p);
    }

    return out;
}

std::string HttpServer::sanitizeUploadFilename(const char* raw) {
    std::string name = urlDecode(raw);

    // space to underscore
    std::string tmp; tmp.reserve(name.size());
    bool prevUnderscore = false;
    for (unsigned char c : name) {
        if (std::isspace(c) || c == 0xA0 /*NBSP*/) {
            if (!prevUnderscore) { tmp.push_back('_'); prevUnderscore = true; }
        } else {
            tmp.push_back((char)c);
            prevUnderscore = false;
        }
    }

    // only a-z A-Z 0-9 . _ -
    std::string safe; safe.reserve(tmp.size());
    prevUnderscore = false;
    for (unsigned char c : tmp) {
        bool ok = (c >= 'A' && c <= 'Z') ||
                  (c >= 'a' && c <= 'z') ||
                  (c >= '0' && c <= '9') ||
                  c == '.' || c == '_' || c == '-';
        char outc = ok ? (char)c : '_';
        if (outc == '_' && prevUnderscore) continue; // avoid double __
        safe.push_back(outc);
        prevUnderscore = (outc == '_');
    }

    // limit length
    const size_t MAX_BASE = 100;
    if (safe.size() > MAX_BASE) safe.resize(MAX_BASE);
    
    // timestamp if empty
    if (safe.empty()) {
        safe = "file_" + std::to_string(millis());
    }

    return safe;
}



bool HttpServer::isApiAuthorized(httpd_req_t* req) {
#ifdef BITPIRATE_API_TOKEN
    char auth[160] = {0};
    if (httpd_req_get_hdr_value_str(req, "Authorization", auth, sizeof(auth)) != ESP_OK) {
        return false;
    }
    std::string expected = std::string("Bearer ") + BITPIRATE_API_TOKEN;
    return expected == auth;
#else
    (void)req;
    return true;
#endif
}

esp_err_t HttpServer::sendJson(httpd_req_t* req, const std::string& payload, const char* status) {
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    if (status) httpd_resp_set_status(req, status);
    return httpd_resp_send(req, payload.c_str(), HTTPD_RESP_USE_STRLEN);
}

std::string HttpServer::readRequestBody(httpd_req_t* req, size_t maxBytes) {
    if (req->content_len <= 0 || static_cast<size_t>(req->content_len) > maxBytes) return "";
    std::string body;
    body.resize(req->content_len);
    int remaining = req->content_len;
    int offset = 0;
    while (remaining > 0) {
        int n = httpd_req_recv(req, &body[offset], remaining);
        if (n <= 0) return "";
        offset += n;
        remaining -= n;
    }
    return body;
}

esp_err_t HttpServer::handleApiStatus(httpd_req_t* req) {
    if (!isApiAuthorized(req)) {
        return sendJson(req, "{\"ok\":false,\"error\":\"unauthorized\"}", "401 Unauthorized");
    }

    GlobalState& state = GlobalState::getInstance();
    JsonDocument doc;
    doc["ok"] = true;
    doc["api_version"] = 1;
    doc["device"] = "ESP32-Bit-Pirate";
    doc["firmware"] = state.getVersion();
    doc["uptime_ms"] = static_cast<uint32_t>(millis());
    doc["mode"] = ModeEnumMapper::toString(state.getCurrentMode());
    doc["terminal_mode"] = TerminalTypeEnumMapper::toString(state.getTerminalMode());
    doc["terminal_ip"] = state.getTerminalIp();
    doc["ip"] = WiFi.localIP().toString();
    doc["mac"] = WiFi.macAddress();
    doc["heap_free"] = ESP.getFreeHeap();
    doc["heap_min_free"] = ESP.getMinFreeHeap();
    doc["ws_client_connected"] = wsServer ? wsServer->hasClient() : false;
    doc["api_busy"] = false;
#ifdef BITPIRATE_API_TOKEN
    doc["auth"] = "bearer";
#else
    doc["auth"] = "none";
#endif

    std::string payload;
    serializeJson(doc, payload);
    return sendJson(req, payload);
}

esp_err_t HttpServer::handleApiCommand(httpd_req_t* req) {
    if (!isApiAuthorized(req)) {
        return sendJson(req, "{\"ok\":false,\"error\":\"unauthorized\"}", "401 Unauthorized");
    }
    if (!wsServer) {
        return sendJson(req, "{\"ok\":false,\"error\":\"web terminal unavailable\"}", "503 Service Unavailable");
    }
    if (!apiCommandMutex.try_lock()) {
        return sendJson(req, "{\"ok\":false,\"error\":\"busy\"}", "409 Conflict");
    }
    std::lock_guard<std::mutex> busyGuard(apiCommandMutex, std::adopt_lock);

    std::string body = readRequestBody(req, 2048);
    if (body.empty()) {
        return sendJson(req, "{\"ok\":false,\"error\":\"empty or oversized request\"}", "400 Bad Request");
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        return sendJson(req, "{\"ok\":false,\"error\":\"invalid json\"}", "400 Bad Request");
    }

    const char* cmdRaw = doc["cmd"] | "";
    std::string cmd = cmdRaw;
    if (cmd.empty() || cmd.size() > 512) {
        return sendJson(req, "{\"ok\":false,\"error\":\"cmd required, max 512 bytes\"}", "400 Bad Request");
    }
    uint32_t timeoutMs = doc["timeout_ms"] | 3000;
    uint32_t quietMs = doc["quiet_ms"] | 250;
    size_t maxBytes = doc["max_bytes"] | 4096;
    if (timeoutMs < 100) timeoutMs = 100;
    if (timeoutMs > 30000) timeoutMs = 30000;
    if (quietMs < 50) quietMs = 50;
    if (quietMs > 2000) quietMs = 2000;
    if (maxBytes < 256) maxBytes = 256;
    if (maxBytes > 8192) maxBytes = 8192;

    Serial.print("[API RX] ");
    Serial.println(cmd.c_str());

    if (cmd.back() != '\n') cmd.push_back('\n');
    wsServer->clearCapturedOutput();
    uint32_t startSeq = wsServer->getOutputSeq();
    uint32_t lastSeq = startSeq;
    uint32_t lastChange = millis();
    uint32_t started = millis();

    wsServer->injectInput(cmd);

    bool timedOut = false;
    while (true) {
        uint32_t seq = wsServer->getOutputSeq();
        if (seq != lastSeq) {
            lastSeq = seq;
            lastChange = millis();
        }
        uint32_t now = millis();
        if (seq != startSeq && (now - lastChange) >= quietMs) break;
        if ((now - started) >= timeoutMs) { timedOut = true; break; }
        delay(25);
    }

    std::string output = wsServer->getOutputSince(startSeq, maxBytes);
    Serial.print("[API TX] bytes=");
    Serial.print(output.size());
    Serial.print(" timeout=");
    Serial.println(timedOut ? "true" : "false");

    JsonDocument out;
    out["ok"] = !timedOut;
    out["timeout"] = timedOut;
    out["duration_ms"] = static_cast<uint32_t>(millis() - started);
    out["mode"] = ModeEnumMapper::toString(GlobalState::getInstance().getCurrentMode());
    out["output"] = output;
    out["truncated"] = output.size() >= maxBytes;
    std::string payload;
    serializeJson(out, payload);
    return sendJson(req, payload, timedOut ? "504 Gateway Timeout" : nullptr);
}
