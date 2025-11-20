#include <iostream>
#include <sstream>
#include <thread>
#include <atomic> 
#include <chrono>
#include "server.h"
#include "logger.h"
#include "pages.h"
#include "utils.h"
#include "gui.h"
#include "capture/capturer.h"

std::atomic<bool> terminal(false);
std::atomic<bool> running(false);
std::mutex mtx;
uv_async_t async_stop;

GUI gui;

static void async_stop_cb(uv_async_t* handle) {
    LOG_INFO_STREAM << "Async stop signal received";
    HttpServer* server = static_cast<HttpServer*>(handle->data);
    pages::stop();
    server->stop([](uv_handle_t* handle){
        std::this_thread::sleep_for(std::chrono::seconds(3));
        uv_stop(handle->loop);
        LOG_INFO_STREAM << "HTTP Server stoped";
    });
}

static void awakeWorker() {
    mtx.unlock();
}

static void stopWorker() {
    uv_async_send(&async_stop);
}

void worker() {
    // uv_loop_t* loop = uv_default_loop();
    uv_loop_t* loop = uv_loop_new();
    if (!loop) {
        LOG_ERROR_STREAM << "Failed to create uv loop";
        return;
    }
    uv_async_init(loop, &async_stop, async_stop_cb);

    LOG_INFO_STREAM << "DeskShare " APP_VERSION " Start";

    auto [w, h] = Capturer::getResolution();
    static int width = w;
    static int height = h;

    LOG_INFO_STREAM << "Screen Resolution: " << width << " x " << height;
    uv_timer_t t;
    uv_timer_init(loop, &t);
    uv_timer_start(&t, [](uv_timer_t*){
        auto [w, h] = Capturer::getResolution();
        if (w != width || h != height) {
                LOG_INFO_STREAM << "Screen Resolution changed to " << w << " x " << h;
            width = w;
            height = h;
        }
    }, 0, 3000);

    mtx.lock();
    HttpServer server(loop);
    pages::init(loop, server);
    async_stop.data = &server;

    while (!terminal) {
        running = false;

        gui.activate();
        Fl::awake();

        mtx.lock(); // waiting for start

        if (terminal) break;

        LOG_INFO_STREAM << g_config.str();
        server.setTimeout(g_config.timeout, [](uv_timer_t* handle, HttpServer* self){
            uv_async_send(&async_stop);
        });
 
        gui.deactivate();
        Fl::awake();

        if (!server.start("0.0.0.0", g_config.port)) {
            LOG_ERROR_STREAM << "Failed to start server on port " << g_config.port;
            continue;
        }

        running = true;

        std::stringstream addrs;
        for (const auto& [eth, ip] : utils::getIpAddress()) {
            LOG_INFO_STREAM << ">> http://" << ip << ":" << g_config.port;
            addrs << " http://" << ip << ":" << g_config.port << " ";
        }

        gui.setInfo(addrs.str());
        Fl::awake();

        uv_run(loop, UV_RUN_DEFAULT);
        running = false;
    }

    uv_close((uv_handle_t*)&async_stop, nullptr);
    uv_loop_close(loop);
}

int main(int argc, char* argv[]) {
    if (utils::parse_args(g_config, argc, argv)) {
        return -1;
    }

    gui.init([]() {
        if (!running) {
            if (!gui.updateArgs()) {
                LOG_ERROR_STREAM << "Invalid options";
                return;
            }
            gui.output("\n", 1);
            mtx.unlock();
            awakeWorker();
        } else {
            stopWorker();
        }
    });

    Logger& log = getLogger();
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

    terminal = true;
    stopWorker();
    awakeWorker();
    LOG_INFO_STREAM << "GUI Stoped";

    t.join();
    
    return 0;
}