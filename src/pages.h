#ifndef PAGES_H
#define PAGES_H

#include "server.h"

namespace pages {

void init(uv_loop_t* loop, HttpServer& server);
void setup();
void stop();
std::unordered_map<std::string, time_t> getReqs();

}

#endif