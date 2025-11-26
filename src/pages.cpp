#include <sstream>
#include <iomanip>

#include "utils.h"
#include "pages.h"
#include "websocket.h"
#include "logger.h"
#include "capture/capturer.h"
#include "assets/assets.hpp"

class CapturerManager {
public:
    CapturerManager()
    : loop(nullptr), timestamp(0), min_interval(100), quality(0.1), avg_interval(0) {
    }

    ~CapturerManager() {}

    void setup(uv_loop_t* loop, int max_fps, float quality) {
        if (loop) this->loop = loop;
        min_interval = 1000 / max_fps;
        this->quality = quality;
        avg_interval = min_interval;
    }

    int timeToNextFrame(size_t id) {
        auto it = captime.find(id);
        if (it != captime.end()) {
            return it->second + min_interval - uv_now(loop);
        }
        captime[id] = uv_now(loop);
        return 0;
    }

    std::pair<const char*, size_t> getJpeg(size_t id) {
        updateInterval(id);
        uint64_t now = uv_now(loop);
        if (now - timestamp >= avg_interval) {
            if (!cap.capture(quality)) {
                LOG_WARNING_STREAM << "capture failed";
            }
            g_config.frames++;
            timestamp = now;
        }
        captime[id] = now;
        g_config.bytes += cap.jpg.size();
        if (g_config.bytes > (1<<30)) g_config.bytes = 0;
        return {reinterpret_cast<char*>(cap.jpg.data()), cap.jpg.size()};
    }

    void updateInterval(size_t id) {
        if (min_interval >= 100) { // <= 10 fps
            avg_interval = min_interval;
            return;
        }

        const float alpha = 0.6;
        auto it = captime.find(id);
        if (it != captime.end()) {
            uint64_t dt = uv_now(loop) - captime[id];
            avg_interval = alpha * dt + (1.0 - alpha) * avg_interval;
        }

        if (avg_interval < min_interval) {
            avg_interval = min_interval;
        }
    }

    void requestStart(const std::string& ip, uint16_t port, size_t id) {
        std::string key = ip + ":" + std::to_string(port);
        reqs[key] = std::time(nullptr);
        LOG_INFO_STREAM << key << " id-" << id << " Enter. Remote Clients: " << reqs.size();
        g_config.clients = reqs.size();
    }

    void requestEnd(const std::string& ip, uint16_t port, size_t id) {
        std::string key = ip + ":" + std::to_string(port);
        reqs.erase(key);
        LOG_INFO_STREAM << key << " id-" << id << " Leave. Remote Clients: " << reqs.size();
        g_config.clients = reqs.size();
        captime.erase(id);
    }

    std::string listRequests() {
        std::stringstream ss;
        ss << "<ul>";
        for (const auto& [key, t] : reqs) {
            ss << "<li>" <<  key << "&emsp;" << utils::timeFmt(t) << "</li>";
        }
        ss << "</ul>";
        ss << "<p>FPS: " << 1000.0/avg_interval << "</p>";
        return ss.str();
    }

    std::string getClientsList() {
        std::stringstream ss;
        ss << "[";
        bool first = true;
        for (const auto& [key, t] : reqs) {
            if (!first) ss << ",";
            ss << '"' << key << '"';
            first = false;
        }
        ss << "]";
        return ss.str();
    }

    void stop() {
        reqs.clear();
        captime.clear();
        g_config.clients = 0;
    }
    std::unordered_map<std::string, time_t> getReqs() {
        return reqs;
    }

private:
    uv_loop_t* loop;
    Capturer cap;
    uint64_t timestamp;
    uint64_t min_interval; // ms
    float quality;
    uint64_t avg_interval;

    std::unordered_map<std::string, time_t> reqs;
    std::unordered_map<size_t, uint64_t> captime;
};

#define IDL_TIMEOUT 30000 // ms

class TimeoutTimerQueue {

public:
    TimeoutTimerQueue() : loop(nullptr), server(nullptr) {}

    void start(uv_loop_t* loop, HttpServer* server, uint64_t interval) {
        this->loop = loop;
        this->server = server;
        uv_timer_init(loop, &checkTimer);
        checkTimer.data = this;
        uv_timer_start(&checkTimer, onCheckTimeout, 0, interval);
    }

    static void onCheckTimeout(uv_timer_t* handle) {
        TimeoutTimerQueue* self = static_cast<TimeoutTimerQueue*>(handle->data);
        LOG_DEBUG_STREAM<< "check timeout " << self->timers.size() << " clients";
        for (auto it = self->timers.begin(); it != self->timers.end(); ) {
            uint64_t now = uv_now(self->loop);
            size_t client_id = it->first;
            uint64_t last_active = it->second.first;
            uint64_t timeout = it->second.second;
            if (now - last_active >= timeout) {
                LOG_INFO_STREAM << "Client id-" << client_id << " timed out, closing connection";
                it = self->timers.erase(it);
                self->server->closeClient(client_id);
            } else {
                ++it;
            }
        }
    }

    void push(size_t client_id, uint64_t timeout) {
        timers[client_id] = std::make_pair(uv_now(loop), timeout);
    }

    void pop(size_t client_id) {
        timers.erase(client_id);
    }

    void refresh(size_t client_id) {
        auto it = timers.find(client_id);
        if (it != timers.end()) {
            it->second.first = uv_now(loop);
        }
    }

    void stop() {
        timers.clear();
    }

private:
    uv_loop_t* loop;
    HttpServer* server;
    uv_timer_t checkTimer;
    std::unordered_map<size_t, std::pair<uint64_t, uint64_t>> timers;
};

static const char* info = R"(
<html>
<head>
    <title>DeskShare HTTP Server</title>
    <style>
        body {
        font-family: Arial, sans-serif; margin: 40px;
        }
        .header { background: #f0f0f0; padding: 20px; border-radius: 5px; }
        .endpoint { background: #f9f9f9; padding: 15px; margin: 10px 0; border-left: 4px solid #007cba; }
    </style>
</head>
<body>
    <div class="header">
        <h1>Welcome to DeskShare HTTP Server (ver {{version}})</h1>
        <p>A simple HTTP server for DeskShare to share remote screen</p>
        <p>{{options}}</p>
        <p>{{time}}</p>
    </div>
    <div style="margin-top: 20px;">
        <a>Remote Clients:</a>
        {{clients}}
    </div>
</body>
</html>
    )";

namespace pages {

static CapturerManager s_cap;

static TimeoutTimerQueue s_timeoutQueue;

std::unordered_map<std::string, time_t> getReqs() {
    return s_cap.getReqs();
}

static void initStreamRoutes(HttpServer& server) {
    Response mjpegResp([](std::ostream& body, Response* self) {
        int d = s_cap.timeToNextFrame(self->getClientId());
        if (d > 0) {
            self->setDelayOnce(d);
            return;
        }
        auto [frame, size] = s_cap.getJpeg(self->getClientId());

        body << "--jpegframe\r\n"
            << "Content-Type: image/jpeg\r\n"
            << "Content-Length: " << size << "\r\n";
        body << "\r\n";
        body.write(frame, size);
        body << "\r\n";
        self->markRepeat();
    }, "multipart/x-mixed-replace; boundary=jpegframe", HttpStatus::OK, {
        {"Cache-Control", "no-cache"},
        {"Pragma", "no-cache"},
        {"Access-Control-Allow-Origin", "*"}
    });

    mjpegResp.onRequestStart = [](const std::string& ip, uint16_t port, Response* resp) {
        s_cap.requestStart("*" + ip, port, resp->getClientId());
    };

    mjpegResp.onRequestEnd = [](const std::string& ip, uint16_t port, Response* resp) {
        s_cap.requestEnd("*" + ip, port, resp->getClientId());
    };

    server.addRoute("GET", "/mjpeg", mjpegResp);

    server.addRoute("OPTIONS", "/mjpeg", Response([](std::ostream& os, Response* self) {
        // No body for OPTIONS response
    }, "text/plain", HttpStatus::NO_CONTENT, {
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, OPTIONS"},
        {"Access-Control-Allow-Headers", "*"},
        {"Access-Control-Max-Age", "86400"}
    }));

    Response wsResp([](std::ostream& os, Response* self) {
        if (self->getWsFrameType() == WsFrameType::TEXT) {
            std::string data = self->getWsFrame();
            if (data == "STREAM:DIFF") {
                os << WsClient::pack_text_frame("DIFF message");
            } else if (data == "STREAM:FULL") {
                s_timeoutQueue.refresh(self->getClientId());
                int d = s_cap.timeToNextFrame(self->getClientId());
                if (d > 0) {
                    self->setDelayOnce(d);
                } else {
                    auto [w, h] = Capturer::getResolution();
                    auto [x, y] = Capturer::getCursorPos();
                    std::stringstream ss;
                    ss << "{"
                        << "\"type\":" << "\"cursor\","
                        << "\"x\":" << x << ","
                        << "\"y\":" << y << ","
                        << "\"w\":" << w << ","
                        << "\"h\":" << h << ","
                        << "\"connections\":" << g_config.clients << ","
                        << "\"clients\":" << s_cap.getClientsList()
                        << "}";
                    os << WsClient::pack_text_frame(ss.str());

                    auto [frame, size] = s_cap.getJpeg(self->getClientId());
                    os << WsClient::pack_binary_frame(frame, size);
                }
            }
        }
    });

    wsResp.onRequestStart = [](const std::string& ip, uint16_t port, Response* resp) {
        s_cap.requestStart(ip, port, resp->getClientId());
        s_timeoutQueue.push(resp->getClientId(), IDL_TIMEOUT);
    };

    wsResp.onRequestEnd = [](const std::string& ip, uint16_t port, Response* resp) {
        s_cap.requestEnd(ip, port, resp->getClientId());
        s_timeoutQueue.pop(resp->getClientId());
    };

    server.addWsRoute("/stream", wsResp);
}

void stop() {
    s_timeoutQueue.stop();
    s_cap.stop();
}

void setup() {
    s_cap.setup(nullptr, g_config.fps, g_config.quality);
}

void init(uv_loop_t* loop, HttpServer& server) {

    s_timeoutQueue.start(loop, &server, IDL_TIMEOUT);
    s_cap.setup(loop, g_config.fps, g_config.quality);

    initStreamRoutes(server);

    server.addRoute("GET", "/info", Response([](std::ostream& os, Response* self) {
        os << utils::renderTemplate(info, {
            {"version", APP_VERSION},
            {"options", g_config.str()},
            {"time", utils::getTime()},
            {"clients", s_cap.listRequests()}
        });
    }, "text/html"));


    server.addRoute("GET", "/", Response(getIndexHtml()));
    server.addRoute("GET", "/favicon.ico", Response(getFavicon()));

    server.addRoute("GET", "/player.css", Response(getPlayerCss()));
    server.addRoute("GET", "/player.js", Response(getPlayerJs()));
    
    server.addRoute("GET", "/video", Response(getMjpegIndexHtml()));
    server.addRoute("GET", "/mjpeg.js", Response(getMjpegJs()));

    server.addRoute("GET", "/raw", Response(getRawIndex()));

    server.addWsRoute("/ws/test", Response([](std::ostream& os, Response* self) {
        LOG_INFO_STREAM << "Received " << wsFrameTypeToString(self->getWsFrameType()) << " "
            << self->getWsFrame();

        switch (self->getWsFrameType()) {
            case WsFrameType::PING :
                os << WsClient::pack_pong_frame("PONG: " + utils::getTime());
                break;
            case WsFrameType::TEXT :
                os << WsClient::pack_text_frame("Received: " + self->getWsFrame());
                break;
            default:
                os << WsClient::pack_text_frame("invalid message");
        }
    }));
}

} // namespace pages