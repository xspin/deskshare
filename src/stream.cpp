#include "stream.hpp"
#include "logger.h"
#include "server.h"
#include <cassert>
#include <cstddef>
#include <iterator>
#include <memory>
#include <sstream>
#include <utility>

Stream::Stream() : _loop(nullptr) {
    uv_loop_init(&_worker_loop);
    uv_async_init(&_worker_loop, &_worker_async, worker_cb);
    _worker_async.data = this;
    _terminal = false;
}

Stream::~Stream() {
}

void Stream::close() {
    stop();
    LOG_INFO_STREAM << "close stream";
    _terminal = true;

    uv_async_send(&_worker_async);
    uv_close((uv_handle_t *)&_worker_async, [](uv_handle_t *hd) { uv_loop_close(hd->loop); });

    if (_loop) {
        uv_close((uv_handle_t *)&_async, nullptr);
    }

    if (_thread.joinable()) {
        _thread.join();
    }
}

void Stream::setup(int max_fps, float quality, EncodeType type) {
    LOG_INFO("stream setup: fps=%d, quality=%f, type=%s", max_fps, quality,
             type == EncodeType::H264 ? "H264" : "MJPEG");

    _type = type;
    if (type == EncodeType::H264) {
        _frame = static_cast<Frame *>(&_frame_h264);
    } else {
        _frame = static_cast<Frame *>(&_frame_jpeg);
    }
    _frame->setup(quality, max_fps);
}

void Stream::init(uv_loop_t *loop) {
    _loop = loop;
    LOG_INFO_STREAM << "init stream";
    uv_async_init(_loop, &_async, async_cb);
    _async.data = this;
    _thread = std::thread(worker, this);
}

void Stream::start() {
    assert(_loop);
    LOG_INFO_STREAM << "start stream";
    _running = true;
    _frame_ts_ms = uv_now(_loop); // milliseconds
    _frame_idx = 0;
}

void Stream::stop() {
    LOG_INFO_STREAM << "stop stream";
    _running = false;
    _reqs.clear();
}

// run on main thread
void Stream::async_cb(uv_async_t *handle) {
    Stream *self = static_cast<Stream *>(handle->data);
    self->sendFrame();
}

// run on worker thread
void Stream::worker_cb(uv_async_t *handle) {
    Stream *self = static_cast<Stream *>(handle->data);
    if (self->_terminal) {
        uv_stop(&self->_worker_loop);
        return;
    }
    if (!self->_running) {
        return;
    }
    self->_frame->next();
    uv_async_send(&self->_async);
}

void Stream::worker(Stream *self) {
    LOG_INFO_STREAM << "frame worker started";

    uv_run(&self->_worker_loop, UV_RUN_DEFAULT);

    LOG_INFO_STREAM << "frame worker stopped";
}

std::pair<const void *, size_t> Stream::getFrame() {
    auto [data, size] = _frame->getFrame();
    return {data, size};
}

void Stream::sendFrame() {
    if (_reqs.empty() || !_running)
        return;

    auto [w, h] = Capturer::getResolution();
    auto [x, y] = Capturer::getCursorPos();
    bool key_frame = _frame_h264.isKeyFrame();
    std::stringstream ss;
    ss << "{"
       << "\"type\":"
       << "\"cursor\","
       << "\"x\":" << x << ","
       << "\"y\":" << y << ","
       << "\"w\":" << w << ","
       << "\"h\":" << h << ","
       << "\"keyFrame\":" << key_frame << ","
       << "\"h264\":" << (isH264() ? "true" : "false") << ","
       << "\"connections\":" << _reqs.size() << ","
       << "\"clients\":" << getClientsList() << "}";

    auto os = std::make_shared<std::stringstream>();
    *os << WsClient::pack_text_frame(ss.str());
    auto [data, size] = _frame->getFrame();
    *os << WsClient::pack_binary_frame(data, size);

    uv_async_send(&_worker_async);

    const auto &buf = os->str();
    size_t sz = buf.size();
    int sended = 0;
    for (const auto &[key, info] : _reqs) {
        WsResponse *resp = std::get<2>(info);
        if (resp) {
            resp->onSend([os](int ret) {}); // ref os
            resp->send(buf.data(), sz);
            ++sended;
        }
    }
    _frames++;
    _bytes += sz * sended;
    LOG_DEBUG_STREAM << "send frame-" << _frame_idx << " send reqs " << sended;
    _frame_idx++;
    // TODO fps
}

std::pair<float, float> Stream::stat() {
    auto prev = _frame_ts_ms;
    _frame_ts_ms = uv_now(_loop); // milliseconds

    auto elasped = _frame_ts_ms - prev;
    float fps = 0, speed = 0;
    if (elasped > 0) {
        fps = _frames * 1000.0 / elasped;
        speed = _bytes * 1000.0 / elasped;
    }
    // reset counter
    _frames = 0;
    _bytes = 0;
    return {fps, speed};
}
bool Stream::isH264() {
    return _type == EncodeType::H264;
}

void Stream::requestStart(const std::string &ip, uint16_t port, size_t id, WsResponse *resp) {
    std::string key = ip + ":" + std::to_string(port);
    _reqs[key] = {id, std::time(nullptr), resp};
    LOG_INFO_STREAM << key << " id-" << id << " Enter. Remote Clients: " << _reqs.size();
    _frame_h264.setKeyFrame();
    if (_reqs.size() == 1) {
        start();
        uv_async_send(&_worker_async);
    }
}

void Stream::requestEnd(const std::string &ip, uint16_t port, size_t id) {
    std::string key = ip + ":" + std::to_string(port);
    _reqs.erase(key);
    LOG_INFO_STREAM << key << " id-" << id << " Leave. Remote Clients: " << _reqs.size();
    captime.erase(id);
    if (_reqs.empty()) {
        stop();
    }
}

std::unordered_map<std::string, Stream::DetailTuple> &Stream::getReqs() {
    return _reqs;
}

std::string Stream::listRequests() {
    auto [fps, Bps] = stat();
    std::stringstream ss;
    ss << "<ul>";
    for (const auto &[key, info] : _reqs) {
        auto t = std::get<1>(info);
        ss << "<li>" << key << "&emsp;" << utils::timeFmt(t) << "</li>";
    }
    ss << "</ul>";
    ss << "<p>FPS: " << fps << "</p>";
    return ss.str();
}

std::string Stream::getClientsList() {
    std::stringstream ss;
    ss << "[";
    bool first = true;
    for (const auto &[key, info] : _reqs) {
        if (!first)
            ss << ",";
        ss << '"' << key << '"';
        first = false;
    }
    ss << "]";
    return ss.str();
}
