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
    : loop(nullptr), timestamp(0), min_interval(10), quality(0.1) {
    }

    ~CapturerManager() {}

    void setup(uv_loop_t* loop, int max_fps, float quality) {
        this->loop = loop;
        min_interval = 1000 / max_fps;
        this->quality = quality;
    }

    int timeToNextFrame() {
        uint64_t ts = uv_now(loop);
        return timestamp + min_interval - ts;
    }

    std::pair<const char*, size_t> getJpeg() {
        uint64_t ts = uv_now(loop);
        if (ts - timestamp > min_interval) {
            if (!cap.capture(quality)) {
                LOG_WARNING_STREAM << "capture failed";
            }
            timestamp = ts;
        }
        return {reinterpret_cast<char*>(cap.jpg.data()), cap.jpg.size()};
    }

    void requestStart(const std::string& ip, uint16_t port) {
        std::string key = ip + ":" + std::to_string(port);
        reqs[key] = std::time(nullptr);
        LOG_INFO_STREAM << key << " Enter. Remote Clients: " << reqs.size();
    }

    void requestEnd(const std::string& ip, uint16_t port) {
        std::string key = ip + ":" + std::to_string(port);
        reqs.erase(key);
        LOG_INFO_STREAM << key << " Leave. Remote Clients: " << reqs.size();
    }

    std::string listRequests() {
        std::stringstream ss;
        ss << "<ul>";
        for (const auto& [key, t] : reqs) {
            ss << "<li>" <<  key << "&emsp;" << utils::getTime(t) << "</li>";
        }
        ss << "</ul>";
        return ss.str();
    }

private:
    uv_loop_t* loop;
    Capturer cap;
    uint64_t timestamp;
    uint64_t min_interval; // ms
    float quality;

    std::unordered_map<std::string, time_t> reqs;
};

#define IDL_TIMEOUT 60000 // 60 seconds

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

    void refresh(size_t client_id) {
        auto it = timers.find(client_id);
        if (it != timers.end()) {
            it->second.first = uv_now(loop);
        }
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

static void initStreamRoutes(HttpServer& server) {
    Response mjpegResp([](std::ostream& body, Response* self) {
        int d = s_cap.timeToNextFrame();
        if (d > 0) {
            self->setDelayOnce(d);
            return;
        }
        auto [frame, size] = s_cap.getJpeg();

        body << "--frame\r\n" "Content-Type: image/jpeg\r\n";
        body << "Content-Length: " + std::to_string(size);

        body << "\r\n\r\n";
        body.write(frame, size);
        body << "\r\n\r\n";
        self->markRepeat();
    }, "multipart/x-mixed-replace; boundary=frame");

    mjpegResp.onRequestStart = [](const std::string& ip, uint16_t port, Response* resp) {
        s_cap.requestStart("*" + ip, port);
    };

    mjpegResp.onRequestEnd = [](const std::string& ip, uint16_t port, Response* resp) {
        s_cap.requestEnd("*" + ip, port);
    };

    server.addRoute("GET", "/mjpeg", mjpegResp);

    Response wsResp([](std::ostream& os, Response* self) {
        if (self->getWsFrameType() == WsFrameType::TEXT) {
            std::string data = self->getWsFrame();
            if (data == "STREAM:DIFF") {
                os << WsClient::pack_text_frame("DIFF message");
            } else if (data == "STREAM:FULL") {
                s_timeoutQueue.refresh(self->getClientId());
                int d = s_cap.timeToNextFrame();
                if (d > 0) {
                    self->setDelayOnce(d);
                    return;
                }
                auto [frame, size] = s_cap.getJpeg();
                os << WsClient::pack_binary_frame(frame, size);
            }
        }
    });

    wsResp.onRequestStart = [](const std::string& ip, uint16_t port, Response* resp) {
        s_cap.requestStart(ip, port);
        s_timeoutQueue.push(resp->getClientId(), IDL_TIMEOUT);
    };

    wsResp.onRequestEnd = [](const std::string& ip, uint16_t port, Response* resp) {
        s_cap.requestEnd(ip, port);
    };

    server.addWsRoute("/stream", wsResp);
}

void init(uv_loop_t* loop, HttpServer& server) {

    s_cap.setup(loop, g_args.fps, g_args.quality);

    s_timeoutQueue.start(loop, &server, IDL_TIMEOUT);

    initStreamRoutes(server);

    server.addRoute("GET", "/info", Response([](std::ostream& os, Response* self) {
        os << utils::renderTemplate(info, {
            {"version", APP_VERSION},
            {"options", g_args.str()},
            {"time", utils::getTime()},
            {"clients", s_cap.listRequests()}
        });
    }, "text/html"));


    server.addRoute("GET", "/", Response(getIndexHtml()));

    server.addRoute("GET", "/wsjpeg.min.js", Response(getWsJpegJs()));

    server.addRoute("GET", "/favicon.ico", Response(getFavicon()));
    
    server.addRoute("GET", "/video", Response(getMjpegIndexHtml()));
    server.addRoute("GET", "/mjpeg.min.js", Response(getMjpegJs()));

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