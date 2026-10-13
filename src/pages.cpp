#include <iomanip>
#include <sstream>

#include "assets/assets.hpp"
#include "capture/capturer.h"
#include "logger.h"
#include "pages.h"
#include "server.h"
#include "stream.hpp"
#include "utils.h"
#include "websocket.h"

#define IDL_TIMEOUT 30000 // ms

class TimeoutTimerQueue {

  public:
    TimeoutTimerQueue() : loop(nullptr), server(nullptr) {
    }

    void init(uv_loop_t *loop, HttpServer *server, uint64_t interval) {
        this->loop = loop;
        this->server = server;
        uv_timer_init(loop, &checkTimer);
        checkTimer.data = this;
        uv_timer_start(&checkTimer, onCheckTimeout, 0, interval);
    }

    static void onCheckTimeout(uv_timer_t *handle) {
        TimeoutTimerQueue *self = static_cast<TimeoutTimerQueue *>(handle->data);
        LOG_DEBUG_STREAM << "check timeout " << self->timers.size() << " clients";
        for (auto it = self->timers.begin(); it != self->timers.end();) {
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
    uv_loop_t *loop;
    HttpServer *server;
    uv_timer_t checkTimer;
    std::unordered_map<size_t, std::pair<uint64_t, uint64_t>> timers;
};

static const char *info = R"(
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

static Stream &s_stream = Stream::instance();

static TimeoutTimerQueue s_timeoutQueue;

static void initStreamRoutes(HttpServer &server) {
    Response mjpegResp(
        [](std::ostream &body, Response *self) {
            // int d = s_stream.timeToNextFrame(self->getClientId());
            // if (d > 0) {
            //     self->setDelayOnce(d);
            //     return;
            // }
            // auto [frame, size] = s_stream.getFrame();
            auto [frame, size] = s_stream.capJpegFrame();

            body << "--jpegframe\r\n"
                 << "Content-Type: image/jpeg\r\n"
                 << "Content-Length: " << size << "\r\n";
            body << "\r\n";
            body.write((char *)frame, size);
            body << "\r\n";
            // self->markRepeat();
            s_timeoutQueue.refresh(self->getClientId());
        },
        "multipart/x-mixed-replace; boundary=jpegframe", HttpStatus::OK,
        {{"Cache-Control", "no-cache"},
         {"Pragma", "no-cache"},
         {"Access-Control-Allow-Origin", "*"}});

    mjpegResp.markRepeat();
    mjpegResp.onRequestStart = [](const std::string &ip, uint16_t port, Response *resp) {
        s_stream.requestStart("*" + ip, port, resp->getClientId());
    };

    mjpegResp.onRequestEnd = [](const std::string &ip, uint16_t port, Response *resp) {
        s_stream.requestEnd("*" + ip, port, resp->getClientId());
    };

    server.addRoute("GET", "/mjpeg", mjpegResp);

    server.addRoute("OPTIONS", "/mjpeg",
                    Response(
                        [](std::ostream &os, Response *self) {
                            // No body for OPTIONS response
                        },
                        "text/plain", HttpStatus::NO_CONTENT,
                        {{"Access-Control-Allow-Origin", "*"},
                         {"Access-Control-Allow-Methods", "GET, OPTIONS"},
                         {"Access-Control-Allow-Headers", "*"},
                         {"Access-Control-Max-Age", "86400"}}));

    WsResponse wsResp([](std::ostream &os, Response *rsp) {
        WsResponse *self = static_cast<WsResponse *>(rsp);
        if (self->getWsFrameType() == WsFrameType::TEXT) {
            size_t cid = self->getClientId();
            std::string data = self->getWsFrame();
            // keepalive
            LOG_DEBUG_STREAM << "id-" << cid << " keepalive: " << data;
            s_timeoutQueue.refresh(cid);
            // self->unsetWsFrame();
            os << WsClient::pack_text_frame("PONG");
            LOG_DEBUG_STREAM << "reply: PONG";
        }
    });

    wsResp.onRequestStart = [](const std::string &ip, uint16_t port, Response *resp) {
        s_stream.requestStart(ip, port, resp->getClientId(), dynamic_cast<WsResponse *>(resp));
        s_timeoutQueue.push(resp->getClientId(), IDL_TIMEOUT);
    };

    wsResp.onRequestEnd = [](const std::string &ip, uint16_t port, Response *resp) {
        s_stream.requestEnd(ip, port, resp->getClientId());
        s_timeoutQueue.pop(resp->getClientId());
    };

    server.addWsRoute("/stream", wsResp);
}

void exit() {
    s_stream.close();
}
void stop() {
    s_timeoutQueue.stop();
    s_stream.stop();
}

void start() {
    s_stream.start();
}

void setup() {
    auto enc = g_config.h264 ? EncodeType::H264 : EncodeType::MJPEG;
    s_stream.setup(g_config.fps, g_config.quality, enc);
}

void init(uv_loop_t *loop, HttpServer &server) {
    setup();
    s_timeoutQueue.init(loop, &server, IDL_TIMEOUT);
    s_stream.init(loop);

    initStreamRoutes(server);

    server.addRoute("GET", "/info",
                    Response(
                        [](std::ostream &os, Response *self) {
                            os << utils::renderTemplate(info,
                                                        {{"version", APP_VERSION},
                                                         {"options", g_config.str()},
                                                         {"time", utils::getTime()},
                                                         {"clients", s_stream.listRequests()}});
                        },
                        "text/html"));

    server.addRoute("GET", "/", Response(getIndexHtml()));
    server.addRoute("GET", "/favicon.ico", Response(getFavicon()));

    server.addRoute("GET", "/player.css", Response(getPlayerCss()));
    server.addRoute("GET", "/player.js", Response(getPlayerJs()));
    server.addRoute("GET", "/jmuxer.js", Response(getJmuxerJs()));

    server.addRoute("GET", "/video", Response(getMjpegIndexHtml()));
    server.addRoute("GET", "/mjpeg.js", Response(getMjpegJs()));

    server.addRoute("GET", "/raw", Response(getRawIndex()));

    server.addWsRoute("/ws/test", WsResponse([](std::ostream &os, Response *rsp) {
                          WsResponse *self = static_cast<WsResponse *>(rsp);
                          LOG_INFO_STREAM << "Received "
                                          << wsFrameTypeToString(self->getWsFrameType()) << " "
                                          << self->getWsFrame();

                          switch (self->getWsFrameType()) {
                          case WsFrameType::PING:
                              os << WsClient::pack_pong_frame("PONG: " + utils::getTime());
                              break;
                          case WsFrameType::TEXT:
                              os << WsClient::pack_text_frame("Received: " + self->getWsFrame());
                              break;
                          default:
                              os << WsClient::pack_text_frame("invalid message");
                          }
                      }));
}

} // namespace pages
