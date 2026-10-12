#pragma once

#include "server.h"

namespace pages {

void init(uv_loop_t *loop, HttpServer &server);
void setup();
void stop();
void start();
void exit();
} // namespace pages
