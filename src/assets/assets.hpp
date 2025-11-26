#ifndef ASSETS_H
#define ASSETS_H

#include <string>
#include "../utils.h"
#include "include/index.html.h"
#include "include/player.js.h"
#include "include/player.css.h"
#include "include/favicon.ico.h"
#include "include/mjpeg.index.html.h"
#include "include/mjpeg.js.h"

#define TO_STRING(v) \
    std::string(reinterpret_cast<char*>(v), v##_len)


std::string getPlayerJs() {
    return TO_STRING(output_player_js);
}

std::string getPlayerCss() {
    return TO_STRING(output_player_css);
}

std::string getIndexHtml() {
    std::string s = TO_STRING(index_html);
    return utils::renderTemplate(s, {{"version", APP_VERSION}});
}

std::string getMjpegIndexHtml() {
    std::string s = TO_STRING(mjpeg_index_html);
    return utils::renderTemplate(s, {{"version", APP_VERSION}});
}

std::string getMjpegJs() {
    return TO_STRING(output_mjpeg_js);
}

std::string getFavicon() {
    return TO_STRING(favicon_ico);
}

std::string getRawIndex() {
    return R"(
<html>
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width, minimum-scale=0.1">
<title>DeskShare Raw</title>
</head>
<body style="margin: 0px; width: 100%; background-color: rgb(14, 14, 14);">
<img style="display: block;-webkit-user-select: none;margin: auto;cursor: background-color: hsl(0, 0%, 90%);" src="/mjpeg" height="100%">
</body>
</html>
    )";
}

#endif