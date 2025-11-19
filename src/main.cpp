#include <iostream>
#include <sstream>
#include <csignal>
#include <atomic>

#include "server.h"
#include "logger.h"
#include "pages.h"
#include "utils.h"


static void signal_handler(uv_signal_t* handle, int signum) {
    LOG_INFO_STREAM << "Received signal " << signum << ", shutting down ...";
    uv_signal_stop(handle);
    uv_stop(handle->loop);
}

int main(int argc, char* argv[]) {
    if (utils::parse_args(g_config, argc, argv)) {
        return -1;
    }

    Logger& log = getLogger();
    if (g_config.debug) {
        log.setLogLevel(LogLevel::DEBUG);
    } else {
        log.setLogLevel(LogLevel::INFO);
    }
    // log.setLogFile("./test.log");

#ifdef PLATFORM_WINDOWS
    log.setColor(g_config.color);
#endif

    LOG_INFO_STREAM << "DeskShare " APP_VERSION " Start";

    LOG_INFO_STREAM << g_config.str();

    uv_loop_t* loop = uv_default_loop();

    uv_signal_t sig;
    uv_signal_init(loop, &sig);
    uv_signal_start(&sig, signal_handler, SIGINT);

    uv_timer_t exit_timer;
    uv_timer_init(loop, &exit_timer);
    bool failed = false;

    HttpServer server(loop);
    server.setTimeout(g_config.timeout);
    
    pages::init(loop, server);
 
    int port = g_config.port;
    if (!server.start("0.0.0.0", port)) {
        failed = true;
        LOG_ERROR_STREAM << "Failed to start server on port " << port;
    }

    std::string dash(80, '=');
    std::cout << dash << std::endl;
    for (const auto& [eth, ip] : utils::getIpAddress()) {
        std::cout << ">> http://" << ip << ":" << port << std::endl;
    }
    std::cout << dash << std::endl;


    std::cout << "Press Ctrl+C to stop the server\n\n";
    
    if (failed) {
        LOG_WARNING_STREAM << "Start failed. Exiting in 5 seconds...";
        uv_timer_start(&exit_timer, [](uv_timer_t* handle) {
            uv_stop(handle->loop);
        }, 5000, 0);
    }

    // 运行事件循环
    uv_run(loop, UV_RUN_DEFAULT);
    
    uv_loop_close(loop);

    LOG_INFO_STREAM << "Server stopped";

    return 0;
}
