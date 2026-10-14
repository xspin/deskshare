#include "capture/capturer.h"
#include "gui.h"
#include "logger.h"
#include "pages.h"
#include "server.h"
#include "utils.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <functional>
#include <iostream>
#include <sstream>
#include <thread>

std::atomic<bool> running(false);
uv_async_t async_stop;
uv_async_t async_start;
uv_loop_t worker_loop;
std::function<void()> on_stop;

GUI gui;

static void on_start() {
    static std::stringstream addrs;
    addrs.str("");
    addrs.clear();

    auto tmp = utils::getIpAddress();
    addrs << "Running on ";
    int cnt = 0;
    for (const auto &[eth, ip] : tmp) {
        LOG_INFO_STREAM << ">> http://" << ip << ":" << g_config.port;
        if (ip.compare(0, 8, "127.0.0.") == 0) {
            continue;
        }
        addrs << " http://" << ip << ":" << g_config.port << " ";
        ++cnt;
    }
    if (cnt == 0) {
        addrs << " http://"
              << "127.0.0.1"
              << ":" << g_config.port << " ";
    }

    Fl::awake(
        [](void *data) {
            std::stringstream *ss = static_cast<std::stringstream *>(data);
            gui.setInfo(ss->str());
            gui.deactivate();
        },
        &addrs);

    running = true;
}

static void async_stop_cb(uv_async_t *handle) {
    LOG_INFO_STREAM << "Async stop signal received";
    HttpServer *server = static_cast<HttpServer *>(handle->data);
    pages::stop();
    server->stop([](uv_handle_t *handle) {
        // std::this_thread::sleep_for(std::chrono::seconds(3));
        LOG_INFO_STREAM << "HTTP Server stopped";
        Fl::awake([](void *) { gui.activate(); }, nullptr);
        running = false;
        if (on_stop) {
            on_stop();
        }
    });
}

static void async_start_cb(uv_async_t *handle) {
    LOG_INFO_STREAM << "Async start signal received";
    HttpServer *server = static_cast<HttpServer *>(handle->data);

    Fl::awake([](void *) { gui.deactivate(); }, nullptr);

    LOG_INFO_STREAM << g_config.str();
    server->setTimeout(g_config.timeout,
                       [](uv_timer_t *handle, HttpServer *self) { uv_async_send(&async_stop); });

    if (!server->start("0.0.0.0", g_config.port)) {
        LOG_ERROR_STREAM << "Failed to start server on port " << g_config.port;
        Fl::awake(
            [](void *) {
                gui.activate();
                gui.showLog();
                gui.setError("Failed to start server");
            },
            nullptr);
        return;
    }
    pages::start();

    on_start();
}

static void startWorker() {
    uv_async_send(&async_start);
}

static void stopWorker(std::function<void()> cb) {
    on_stop = cb;
    uv_async_send(&async_stop);
}

void worker() {
    uv_loop_t *loop = &worker_loop;
    uv_loop_init(loop);
    uv_async_init(loop, &async_stop, async_stop_cb);
    uv_async_init(loop, &async_start, async_start_cb);

    LOG_INFO_STREAM << "DeskShare " APP_VERSION;

    auto [w, h] = Capturer::getResolution();

    LOG_INFO_STREAM << "Screen Resolution: " << w << " x " << h;

    HttpServer server(loop);
    pages::init(loop, server);
    async_stop.data = &server;
    async_start.data = &server;
    Fl::awake([](void *) { gui.activate(); }, nullptr);

    uv_run(loop, UV_RUN_DEFAULT);

    pages::exit();

    uv_close((uv_handle_t *)&async_stop, nullptr);
    uv_close((uv_handle_t *)&async_start, [](uv_handle_t *hd) { uv_loop_close(hd->loop); });
}

int main(int argc, char *argv[]) {
    if (utils::parse_args(g_config, argc, argv)) {
        return -1;
    }

    Fl::lock(); //初始化线程支持

    gui.init([]() {
        if (!running) {
            if (!gui.updateArgs()) {
                LOG_ERROR_STREAM << "Invalid options";
                return;
            }
            gui.output("\n", 1);
            startWorker();
        } else {
            stopWorker(nullptr);
        }
    });

    Logger &log = getLogger();
    log.setColor(false);
    log.setMask(LOG_STIME);
    if (g_config.debug) {
        log.setLogLevel(LogLevel::DEBUG);
        log.setMask(LOG_ALL);
    } else {
        log.setLogLevel(LogLevel::INFO);
    }

    GuiBuf buf(&gui);
    std::ostream os(&buf);
    log.setOutStream(&os);

    std::thread t(worker);

    gui.run();

    stopWorker([]() { uv_stop(&worker_loop); });

    LOG_INFO_STREAM << "GUI Exit";

    t.join();

    return 0;
}
