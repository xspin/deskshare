#include <iostream>
#include <sstream>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <cctype>

#include "logger.h"
#include "server.h"
#include "utils.h"

#define METHOD_WS "WS"

// 客户端连接上下文
struct client_context {
    size_t id;
    uv_tcp_t handle;
    HttpServer* server;
    std::string request_data;
    std::pair<std::string, uint16_t> addr;
    std::string tag;
    bool closed;
    int reqs;
    http_write_req* ws_req;
    
    client_context(HttpServer *http_server)
    : closed(false), reqs(0), ws_req(nullptr) {
        server = http_server;
        handle.data = this;
        id = server->addClient(this);
    }
    ~client_context() {
        server->removeClient(id);
    }
};

struct http_write_req {
    Response resp;
    client_context* ctx;
    uv_stream_t* handle;
    uv_write_t req;
    uv_buf_t buf;
    bool inqueue;
    size_t size;
    char* data;
};


std::unordered_map<int, std::string> HttpServer::status_map = {
    // 1xx: 信息性状态码
    {100, "Continue"},
    {101, "Switching Protocols"},
    {102, "Processing"}, // WebDAV 扩展

    // 2xx: 成功状态码
    {200, "OK"},
    {201, "Created"},
    {202, "Accepted"},
    {203, "Non-Authoritative Information"},
    {204, "No Content"},
    {205, "Reset Content"},
    {206, "Partial Content"},

    // 3xx: 重定向状态码
    {300, "Multiple Choices"},
    {301, "Moved Permanently"},
    {302, "Found"},
    {303, "See Other"},
    {304, "Not Modified"},
    {307, "Temporary Redirect"},
    {308, "Permanent Redirect"},

    // 4xx: 客户端错误状态码
    {400, "Bad Request"},
    {401, "Unauthorized"},
    {402, "Payment Required"},
    {403, "Forbidden"},
    {404, "Not Found"},
    {405, "Method Not Allowed"},
    {406, "Not Acceptable"},
    {408, "Request Timeout"},
    {409, "Conflict"},
    {410, "Gone"},
    {415, "Unsupported Media Type"},
    {429, "Too Many Requests"},

    // 5xx: 服务器错误状态码
    {500, "Internal Server Error"},
    {501, "Not Implemented"},
    {502, "Bad Gateway"},
    {503, "Service Unavailable"},
    {504, "Gateway Timeout"},
    {505, "HTTP Version Not Supported"}
};

static std::string clearSlash(const std::string& url) {
    std::string res;
    for (char ch : url) {
        if (!res.empty() && res.back() == '/' && ch == '/') {
            continue;
        }
        res += ch;
    }
    if (res.size() > 1 && res.back() == '/') {
        return res.substr(0, res.size()-1);
    }
    return res;
}

static http_write_req* new_http_write_req(client_context* ctx, uv_stream_t* handle, const Response& resp) {
    assert(ctx && handle);

    http_write_req* req = new http_write_req;
    assert(req);

    req->size = 0;
    req->data = nullptr;
    req->inqueue = false;

    req->req.data = req;
    req->ctx = ctx;
    req->handle = handle;
    req->resp = resp;
    req->resp.setClientId(ctx->id);
    ctx->reqs++;
    LOG_DEBUG_STREAM << "new http_write_req " << req << " reqs " << ctx->reqs << " " << ctx;

    if (req->resp.onRequestStart) {
        req->resp.onRequestStart(req->ctx->addr.first, req->ctx->addr.second, &req->resp);
    }

    return req;
}

static void release_http_write_req(http_write_req* req) {
    LOG_DEBUG_STREAM << "delete http_write_req " << req;
    if (req->resp.onRequestEnd) {
        req->resp.onRequestEnd(req->ctx->addr.first, req->ctx->addr.second, &req->resp);
    }
    --req->ctx->reqs;
    if (req->ctx->reqs == 0 && req->ctx->closed) {
        LOG_DEBUG_STREAM << "free client " << req->ctx;
        delete req->ctx;
    } else {
        LOG_DEBUG_STREAM << "client " << req->ctx << " reqs left: " << req->ctx->reqs;
    }

    if (req->data) delete[] req->data;
    delete req;
}

static void set_http_write_buf(http_write_req* req, const std::string& data) {
    if (req->size < data.size()) {
        delete[] req->data;
        req->data = nullptr;
        req->size = 0;
    }
    if (!req->data) {
        req->data = new char[data.size()];
        assert(req->data);
        req->size = data.size();
    }
    std::memcpy(req->data, data.data(), data.size());
    if (req->size > data.size()) {
        req->data[data.size()] = '\0';
    }
    req->buf = uv_buf_init(req->data, data.size());
}

std::string HttpServer::getStatusText(HttpStatus status_code) {
    auto it = status_map.find(static_cast<int>(status_code));
    if (it != status_map.end()) {
        return it->second;
    }
    return "Unknown Status";
}

Response::Response(const std::string& payload, const std::string& content_type, HttpStatus status,
    const std::unordered_map<std::string, std::string>& headers)
: content_type(content_type), status_code(status), headers(headers), body(payload)
{
    init();
}

Response::Response(RespFunc resp, const std::string& content_type, HttpStatus status,
    const std::unordered_map<std::string, std::string>& headers)
: respFunc(resp), content_type(content_type), status_code(status), headers(headers)
{
    init();
}

void Response::init() {
    status_text = HttpServer::getStatusText(status_code);
    headers["Server"] = "deskshare-http-server";
    if (!content_type.empty()) {
        headers["Content-Type"] = content_type;
    }
    if (!body.empty()) {
        headers["Content-Length"] = std::to_string(body.size());
    }
    // headers["Connection"] = "Close";
    request = nullptr;
    repeat = false;
    delay = 0;
    delay_timer = nullptr;
    client_id = 0;
}

Response::~Response() {
    if (delay_timer && uv_is_active((uv_handle_t*) delay_timer)) {
        uv_close((uv_handle_t*)delay_timer, [](uv_handle_t* handle){
            delete handle;
        });
    }
}

void Response::initDelayTimer() {
    if (delay_timer == nullptr) {
        delay_timer = new uv_timer_t;
        if (uv_timer_init(uv_default_loop(), delay_timer)) {
            LOG_ERROR_STREAM << "delay_timer init failed" ;
        }
    }
}

void Response::setDelayOnce(uint64_t delay) {
    this->delay = delay;
}

void Response::next(const std::function<void(std::stringstream&)>& callback) {
    std::stringstream os;
    if (respFunc) {
        respFunc(os, this);
    }
    if (delay == 0) {
        callback(os);
        return;
    }

    initDelayTimer();

    using DataType = std::pair<Response*, std::function<void(std::stringstream&)>>;

    delay_timer->data = new DataType(this, callback);

    uv_timer_start(delay_timer, [](uv_timer_t* handle) {
        auto data = static_cast<DataType*>(handle->data);
        auto self = data->first;
        auto cb = data->second;
        delete data;
        self->next(cb);
    }, delay, 0);

    delay = 0; // once
}

void Response::markRepeat() {
    repeat = true;
}

void Response::unmarkRepeat() {
    repeat = false;
}

bool Response::isRepeat() {
    return repeat;
}

HttpStatus Response::getStatus() const {
    return status_code;
}

void Response::setStatus(HttpStatus status) {
    status_code = status;
    status_text = HttpServer::getStatusText(status_code);
}

std::string Response::getContentType() {
    return content_type;
}

void Response::setContentType(const std::string& type) {
    content_type = type;
    headers["Content-Type"] = content_type;
}

template<typename T>
void Response::setHeader(const std::string& key, const T& val) {
    if constexpr (utils::is_string_like_v<T>) {
        headers[key] = val;
    } else {
        headers[key] = std::to_string(val);
    }
}

std::string Response::getHeader(const std::string& key) {
    auto it = headers.find(key);
    if (it != headers.end()) return it->second;
    return "";
}

void Response::setBody(const std::string& body) {
    this->body = body;
}

void Response::setWsFrame(WsFrameType type, const std::string& frame) {
    ws_type = type;
    ws_frame = frame;
}

WsFrameType Response::getWsFrameType() {
    return ws_type;
}

std::string Response::getWsFrame() {
    return ws_frame;
}

void Response::setClientId(size_t id) {
    client_id = id;
}

size_t Response::getClientId() {
    return client_id;
}

std::string Response::str() const {
    // 构建响应头
    std::stringstream response_stream;
    response_stream << "HTTP/1.1 " << (int)status_code << " " << status_text << "\r\n";
    
    // 添加头部
    for (const auto& header : headers) {
        response_stream << header.first << ": " << header.second << "\r\n";
    }
    
    // 结束头部
    response_stream << "\r\n";
    
    // 添加响应体
    if (!body.empty()) {
        response_stream << body;
    }
    return response_stream.str();
}

HttpServer::HttpServer(uv_loop_t* loop, uint32_t timeout) : loop(loop), timeout(timeout) {
    memset(&server, 0, sizeof(server));
    write_timer.data = this;
    if (uv_timer_init(loop, &write_timer)) {
        LOG_ERROR_STREAM << "write_timer init failed";
    }
    timeout_timer.data = this;
    if (uv_timer_init(loop, &timeout_timer)) {
        LOG_ERROR_STREAM << "timeout_timer init failed";
    }
    client_counter = 0;
}

HttpServer::~HttpServer() {
    stop();
    if (uv_is_active((uv_handle_t*)&write_timer)) {
        uv_close((uv_handle_t*)&write_timer, nullptr);
    }
    if (uv_is_active((uv_handle_t*)&timeout_timer)) {
        uv_close((uv_handle_t*)&timeout_timer, nullptr);
    }
}

void HttpServer::idle() {
    if (timeout) {
        uv_timer_start(&timeout_timer, [](uv_timer_t* handle){
            LOG_WARNING_STREAM << "Timed out, stopping loop";
            uv_stop(handle->loop);
        }, timeout * 1000, 0);
    }
}

void HttpServer::on_send(uv_timer_t* handle) {
    HttpServer* self = static_cast<HttpServer*>(handle->data);

    if (self->write_queue.empty()) {
        uv_timer_stop(handle);
        self->idle();
    } else {
        http_write_req* write_req = self->write_queue.front();
        self->write_queue.pop();
        write_req->inqueue = false;

        client_context* ctx = write_req->ctx;

        if (ctx->closed) {
            release_http_write_req(write_req);
        } else {
            // if (write_req->buf.len < 100) {
            //     LOG_DEBUG_STREAM << ctx << " Response:\n" << write_req->buf.base;
            // }

            uv_write(&write_req->req, write_req->handle, &write_req->buf, 1, on_write);
        }
    }
}

void HttpServer::dequeueWriteQueue() {
    if (!uv_is_active((uv_handle_t*)&write_timer)) {
        uv_timer_start(&write_timer, on_send, 0, 1); // repeat
    }
    uv_timer_stop(&timeout_timer);
}

void HttpServer::enqueueWriteRequest(http_write_req* write_req) {
    client_context* ctx = write_req->ctx;
    if (!ctx->closed) {
        LOG_DEBUG_STREAM << "Enqueue: " << write_req->buf.len << " bytes, "
            << ctx->tag << " " << ctx << " reqs: " << ctx->reqs;
        write_req->inqueue = true;
        write_queue.push(write_req);
        dequeueWriteQueue();
    } else {
        release_http_write_req(write_req);
    }
}

bool HttpServer::start(const std::string& host, int port) {
    int r;

    // 初始化 TCP 服务器
    if (uv_tcp_init(loop, &server) != 0) {
        LOG_ERROR_STREAM << "Failed to initialize TCP server";
        return false;
    }
    
    // 绑定地址
    struct sockaddr_in addr;
    if (uv_ip4_addr(host.c_str(), port, &addr) != 0) {
        LOG_ERROR_STREAM << "Invalid address: " << host << ":" << port;
        return false;
    }
    
    if ((r = uv_tcp_bind(&server, (const struct sockaddr*)&addr, 0)) != 0) {
        LOG_ERROR_STREAM << "Failed to bind address: " << uv_strerror(r);
        return false;
    }
    
    // 开始监听
    if ((r = uv_listen((uv_stream_t*)&server, 128, on_connection)) != 0) {
        LOG_ERROR_STREAM << "Failed to listen: " << uv_strerror(r);
        return false;
    }

    server.data = this;
    
    LOG_INFO("Server is running on %s:%d", host.c_str(), port);
    if (timeout) {
        LOG_INFO_STREAM << "Timeout: " << timeout << " seconds";
    }

    idle();

    return true;
}

void HttpServer::stop() {
    if (uv_is_active((uv_handle_t*)&server)) {
        uv_close((uv_handle_t*)&server, nullptr);
    }

    LOG_DEBUG_STREAM << "Release write queue: " << write_queue.size();

    while (!write_queue.empty()) {
        http_write_req* write_req = write_queue.front();
        write_queue.pop();
        write_req->inqueue = false;
        auto ctx = write_req->ctx;
        ctx->closed = true;
        if (onClientClosed) {
            onClientClosed(ctx->addr.first, ctx->addr.second);
        }
        release_http_write_req(write_req);
    }
    LOG_INFO_STREAM << "HTTP server stoped";
}

void HttpServer::addRoute(const std::string& method, const std::string& path, 
    const Response& response) {
    std::string p = clearSlash(path);
    routes[method][p] = response;
    if (routes[method][p].getContentType().empty()) {
        routes[method][p].setContentType(utils::getMimeType(p));
    }
    LOG_DEBUG_STREAM << "Added route: " << method << " " << p;
}

void HttpServer::addWsRoute(const std::string& path, const Response& response) {
    std::string p = clearSlash(path);
    routes[METHOD_WS][p] = response;
    LOG_DEBUG_STREAM << "Added ws route: " << " " << p;
}


void HttpServer::setOnClientConnected(ClientCallbackFn fn) {
    onClientConnected = fn;
}

void HttpServer::setOnClientClosed(ClientCallbackFn fn) {
    onClientClosed = fn;
}

static std::pair<std::string, uint16_t> getPeerInfo(uv_tcp_t* client) {
    struct sockaddr_storage peername;
    int namelen = sizeof(peername);
    char ip[INET6_ADDRSTRLEN];
    uint16_t port;

    // 获取对端地址
    if (uv_tcp_getpeername(client, (struct sockaddr*)&peername, &namelen) == 0) {

        if (peername.ss_family == AF_INET) {
            struct sockaddr_in *sin = (struct sockaddr_in*)&peername;
            uv_inet_ntop(AF_INET, &sin->sin_addr, ip, sizeof(ip));
            port = ntohs(sin->sin_port);
        } else if (peername.ss_family == AF_INET6) {
            struct sockaddr_in6 *sin6 = (struct sockaddr_in6*)&peername;
            uv_inet_ntop(AF_INET6, &sin6->sin6_addr, ip, sizeof(ip));
            port = ntohs(sin6->sin6_port);
        } else {
            // 未知地址族
            strcpy(ip, "unknown");
            port = 0;
        }
    } 
    return make_pair(std::string(ip), port);
}
// libuv 连接回调
void HttpServer::on_connection(uv_stream_t* server, int status) {
    if (status != 0) {
        LOG_ERROR_STREAM << "Connection error: " << uv_strerror(status);
        return;
    }
    
    HttpServer* http_server = static_cast<HttpServer*>(server->data);
    
    // 创建客户端上下文
    client_context* client = new client_context(http_server);
    
    // 初始化客户端 TCP 连接
    if (uv_tcp_init(server->loop, &client->handle) == 0) {
        if (uv_accept(server, (uv_stream_t*)&client->handle) == 0) {
            client->addr = getPeerInfo(&client->handle);
            client->tag = client->addr.first + ":" + std::to_string(client->addr.second);
            LOG_INFO_STREAM << "Accepted new client from " 
                << client->tag << " " << client << " id " << client->id;

            if (http_server->onClientConnected) {
                http_server->onClientConnected(client->addr.first, client->addr.second);
            }

            // 开始读取数据
            uv_read_start((uv_stream_t*)&client->handle, alloc_buffer, on_read);
        } else {
            uv_close((uv_handle_t*)&client->handle, on_close);
        }
    } else {
        delete client;
    }
}

// 分配缓冲区
void HttpServer::alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    buf->base = new char[suggested_size];
    buf->len = buf->base? suggested_size : 0;
}


static bool isWebsocket(const std::string& data) {
    return data.find("Upgrade: websocket") != std::string::npos
        &&  data.find("Connection: Upgrade") != std::string::npos;
}

// 读取数据回调
void HttpServer::on_read(uv_stream_t* client, ssize_t nread, const uv_buf_t* buf) {
    client_context* ctx = static_cast<client_context*>(client->data);
    
    LOG_DEBUG_STREAM << "Read " << nread << " bytes from client " << ctx->tag << " " << ctx;

    if (nread > 0) {
        if (ctx->ws_req) {
            ctx->request_data.append(buf->base, nread);
            ctx->server->handleWsRequest(client, ctx->request_data);
            ctx->request_data.clear();
        } else {
            ctx->request_data.append(buf->base, nread);
            
            // 检查是否收到完整的 HTTP 请求（以空行结束）
            if (ctx->request_data.find("\r\n\r\n") != std::string::npos) {
                if (isWebsocket(ctx->request_data)) {
                    ctx->server->handleWsRequest(client, ctx->request_data);
                } else {
                    ctx->server->handleHttpRequest(client, ctx->request_data);
                }
                ctx->request_data.clear();
            }
        }
    } else if (nread < 0) {
        if (nread != UV_EOF) {
            LOG_WARNING_STREAM << "Read error: " << uv_strerror(nread);
        }
        uv_close((uv_handle_t*)client, on_close);
    }
    
    // 释放缓冲区
    delete[] buf->base;
}

// 写入完成回调
void HttpServer::on_write(uv_write_t* req, int status) {
    http_write_req* write_req = static_cast<http_write_req*>(req->data);
    LOG_DEBUG_STREAM << "Async response sent " << write_req->buf.len << " bytes, " << write_req->ctx;
    if (status) {
        LOG_WARNING_STREAM << "uv_write error: " << uv_strerror(status) << " " << write_req->ctx;
    }
    if (write_req->resp.isRepeat()) {
        write_req->resp.next([write_req](std::stringstream &ss){
            set_http_write_buf(write_req, ss.str());
            HttpServer* self = write_req->ctx->server;
            self->enqueueWriteRequest(write_req);
        });
    } else {
        if (write_req->ctx->ws_req == nullptr) {
            LOG_DEBUG_STREAM << "No next " << write_req->ctx;
            release_http_write_req(write_req);
        }
    }
}


size_t HttpServer::addClient(client_context* client) {
    clients[++client_counter] = client;
    return client_counter;
}

void HttpServer::removeClient(size_t client_id) {
    clients.erase(client_id);
}

void HttpServer::closeClient(uv_stream_t* client) {
    if (client) {
        uv_read_stop(client);
        uv_close((uv_handle_t*)client, on_close);
    }
}

void HttpServer::closeClient(size_t client_id) {
    auto it = clients.find(client_id);
    if (it != clients.end()) {
        closeClient((uv_stream_t*)&it->second->handle);
        clients.erase(it);
    }
}

// 关闭连接回调
void HttpServer::on_close(uv_handle_t* handle) {
    client_context* ctx = static_cast<client_context*>(handle->data);
    LOG_INFO_STREAM << "client " << ctx->tag << " " << ctx << " closed" << ", reqs " << ctx->reqs;
    if (ctx->server->onClientClosed) {
        ctx->server->onClientClosed(ctx->addr.first, ctx->addr.second);
    }
    if (ctx->ws_req) {
        if (!ctx->ws_req->inqueue) {
            release_http_write_req(ctx->ws_req);
        }
        ctx->ws_req = nullptr;
    }
    ctx->closed = true;
    if (ctx->reqs == 0) {
        LOG_DEBUG_STREAM << "free client " << ctx;
        delete ctx;
    }
}

Response* HttpServer::findResponse(const std::string& method, const std::string& url) {
    auto method_iter = routes.find(method);
    if (method_iter != routes.end()) {
        auto handler_iter = method_iter->second.find(clearSlash(url));
        if (handler_iter != method_iter->second.end()) {
            return &handler_iter->second;
        }
    }
    return nullptr;
}

static void doWsHandshake(uv_stream_t* client, const std::string& key) {
    auto response = WsClient::pack_handshake(key);
    uv_write_t* req = new uv_write_t();
    uv_buf_t buf = uv_buf_init(const_cast<char*>(response.c_str()), response.size());
    uv_write(req, client, &buf, 1, [](uv_write_t* req, int status) {
        delete req;
    });
}

static void sendWsClose(uv_stream_t* client, WsErrorCode code, const std::string& reason="") {
    std::string msg;
    if (reason.empty()) {
        msg = wsErrorToString(code);
    } else {
        msg = reason;
    }

    std::string frame = WsClient::pack_close_frame(code, msg);

    uv_write_t* req = new uv_write_t();
    req->data = client;
    uv_buf_t buf = uv_buf_init(const_cast<char*>(frame.c_str()), frame.size());
    uv_write(req, client, &buf, 1, [](uv_write_t* req, int status) {
        uv_stream_t* client = static_cast<uv_stream_t*>(req->data);
        client_context* ctx = static_cast<client_context*>(client->data);
        ctx->server->closeClient(client);
        delete req;
    });

    LOG_DEBUG_STREAM << "Sent ws: " << static_cast<uint16_t>(code) << " " << msg;
}

void HttpServer::handleWsRequest(uv_stream_t* client, const std::string& request_data) {
    client_context* ctx = static_cast<client_context*>(client->data);

    http_request req;
    if (!ctx->ws_req) {
        if (!parseHttpRequest(request_data, req)) {
            LOG_WARNING_STREAM << "Failed to parse HTTP request " << ctx;
            // sendErrorResponse(client, HttpStatus::BAD_REQUEST);
            sendWsClose(client, WsErrorCode::PROTOCOL_ERROR);
            return;
        }

        LOG_INFO_STREAM << "WS Request: " << req.method << " " << req.url << " " << client->data;
        LOG_DEBUG_STREAM << request_data;
        
        if (req.method != "GET") {
            sendWsClose(client, WsErrorCode::PROTOCOL_ERROR);
            return;
        }

        Response* resp = findResponse(METHOD_WS, req.url);
        if (!resp) {
            LOG_WARNING_STREAM << "route not found for " << req.url << " " << ctx;
            sendWsClose(client, WsErrorCode::RESOURCE_NOT_FOUND);
            return;
        }

        auto it = req.headers.find("Sec-WebSocket-Key");
        if (it != req.headers.end()) {
            doWsHandshake(client, it->second);
            assert(ctx->ws_req == nullptr);
            ctx->ws_req = new_http_write_req(ctx, client, *resp);
            return;
        } else {
            LOG_ERROR_STREAM << "WebSocket Key Not found " << ctx << " " << ctx->tag;
            sendWsClose(client, WsErrorCode::AUTH_FAILED);
            return;
        }
    }

    auto [type, frame] = WsClient::parse_frame(request_data.c_str(), request_data.size());

    if (type == WsFrameType::CLOSE) {
        LOG_DEBUG_STREAM << "Received ws CLOSE: " << frame;
        sendWsClose(client, WsErrorCode::NORMAL_CLOSURE);
        return;
    }

    LOG_DEBUG_STREAM << "Received ws frame " << wsFrameTypeToString(type) << " " << frame;

    http_write_req* write_req = ctx->ws_req;

    write_req->resp.setWsFrame(type, frame);

    write_req->resp.next([write_req](std::stringstream &ss){
        set_http_write_buf(write_req, ss.str());
        HttpServer* self = write_req->ctx->server;
        self->enqueueWriteRequest(write_req);
    });
}

// 处理 HTTP 请求
void HttpServer::handleHttpRequest(uv_stream_t* client, const std::string& request_data) {
    http_request req;
    
    // 解析 HTTP 请求
    if (!parseHttpRequest(request_data, req)) {
        LOG_WARNING_STREAM << "Failed to parse HTTP request";
        sendErrorResponse(client, HttpStatus::BAD_REQUEST);
        return;
    }
    
    LOG_DEBUG_STREAM << client->data << " Request: " << req.method << " " << req.url;

    auto response = findResponse(req.method, req.url);
    if (response) {
        sendHttpResponse(client, *response);
    } else {
        LOG_WARNING_STREAM << client->data << " " << req.method << " " << req.url << " Not Found";
        sendErrorResponse(client, HttpStatus::NOT_FOUND);
    }
}

// 解析 HTTP 请求
bool HttpServer::parseHttpRequest(const std::string& data, http_request& req) {
    std::istringstream stream(data);
    std::string line;
    
    // 解析请求行
    if (!std::getline(stream, line)) {
        return false;
    }
    
    std::istringstream request_line(line);
    if (!(request_line >> req.method >> req.url >> req.version)) {
        LOG_WARNING_STREAM << "invalid http request: " << line;
        return false;
    }
    
    // 解析头部
    while (std::getline(stream, line) && line != "\r" && !line.empty()) {
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);
            
            // 去除首尾空白字符
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t\r") + 1);
            
            req.headers[key] = value;
        }
    }
    
    return true;
}

// 发送 HTTP 响应
void HttpServer::sendHttpResponse(uv_stream_t* client, const Response& resp) {
    http_write_req* write_req = new_http_write_req(static_cast<client_context*>(client->data), client, resp);

    write_req->resp.next([write_req](std::stringstream &ss){
        if (!write_req->resp.isRepeat() && write_req->resp.getHeader("Content-Length").empty()) {
            write_req->resp.setHeader("Content-Length", ss.str().size());
        }

        std::string response_str = write_req->resp.str();
        response_str += ss.str();

        if (write_req->resp.getStatus() != HttpStatus::OK) {
            LOG_WARNING_STREAM << write_req->ctx << " Response:\n" << (int)write_req->resp.getStatus() << " " << response_str;
        }

        if (write_req->resp.getContentType().find("text") != std::string::npos) {
            LOG_DEBUG_STREAM << response_str;
        }

        set_http_write_buf(write_req, response_str);

        HttpServer* self = write_req->ctx->server;
        self->enqueueWriteRequest(write_req);
    });
}


// 发送错误响应
void HttpServer::sendErrorResponse(uv_stream_t* client, HttpStatus status_code, const std::string& message) {
    Response resp;

    resp.setStatus(status_code);
    resp.setHeader("Content-Type", "text/html");
    resp.setBody("<html><body><h1>" 
        + std::to_string((int)status_code) + " " 
        + getStatusText(status_code)  + "</h1>"
        + message + "</body></html>");
    
    LOG_WARNING_STREAM << (int)status_code << " " << getStatusText(status_code) << " " << client->data;
    
    sendHttpResponse(client, resp);
}