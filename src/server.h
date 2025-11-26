#ifndef SERVER_H
#define SERVER_H

#include <uv.h>
#include <string>
#include <unordered_map>
#include <functional>
#include <optional>
#include <queue>
#include <ostream>
#include "websocket.h"

enum class HttpStatus {
    OK = 200,
    CREATED = 201,
    ACCEPTED = 202,
    NO_CONTENT = 204,
    MOVED_PERMANENTLY = 301,
    FOUND = 302,
    NOT_MODIFIED = 304,
    BAD_REQUEST = 400,
    UNAUTHORIZED = 401,
    FORBIDDEN = 403,
    NOT_FOUND = 404,
    METHOD_NOT_ALLOWED = 405,
    INTERNAL_SERVER_ERROR = 500,
    NOT_IMPLEMENTED = 501,
    BAD_GATEWAY = 502,
    SERVICE_UNAVAILABLE = 503,
};

struct client_context;

struct http_write_req;

// HTTP 请求结构
struct http_request {
    std::string method;
    std::string url;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::unordered_map<std::string, std::string> params;
    std::string body;
};


// HTTP 响应结构
class Response {
    using RespFunc = std::function<void(std::ostream&, Response*)>;
    using RequestCallbackFn = std::function<void(const std::string&, uint16_t, Response*)>;

private:
    http_request* request;
    bool repeat;
    RespFunc respFunc;
    std::string content_type;
    HttpStatus status_code;
    std::string status_text;
    std::unordered_map<std::string, std::string> headers;
    std::string body;

    WsFrameType ws_type;
    std::string ws_frame;

    uint64_t delay;
    uv_timer_t* delay_timer;

    size_t client_id;

    uv_loop_t* loop;

    void init();
    void initDelayTimer();

public:
    Response(): request(nullptr), delay_timer(nullptr), client_id(0), loop(nullptr) {}

    Response(const std::string& payload,
        const std::string& content_type="",
        HttpStatus status=HttpStatus::OK,
        const std::unordered_map<std::string, std::string>& headers={});

    Response(RespFunc resp,
        const std::string& content_type="",
        HttpStatus status=HttpStatus::OK,
        const std::unordered_map<std::string, std::string>& headers={});

    ~Response();

    void markRepeat();
    void unmarkRepeat();
    bool isRepeat();
    void setStatus(HttpStatus status);

    template<typename T> void setHeader(const std::string& key, const T& val);
    std::string getHeader(const std::string& key);

    void setBody(const std::string& body);
    void setDelayOnce(uint64_t delay);

    void setWsFrame(WsFrameType type, const std::string& frame);
    WsFrameType getWsFrameType();
    std::string getWsFrame();

    std::string str() const;
    HttpStatus getStatus() const;
    void setContentType(const std::string& type);
    std::string getContentType();

    void setClientId(size_t id);
    size_t getClientId();

    void setLoop(uv_loop_t* loop);
    void next(const std::function<void(std::stringstream&)>& callback);

    RequestCallbackFn onRequestStart;
    RequestCallbackFn onRequestEnd;

};

// HTTP 服务器类
class HttpServer {
    using ClientCallbackFn = std::function<void(const std::string&, uint16_t)>;

public:
    HttpServer(uv_loop_t* loop);
    ~HttpServer();

    // 启动服务器
    bool start(const std::string& host, int port);
    
    // 停止服务器
    void stop(uv_close_cb cb=nullptr);
    
    // 添加路由处理函数
    void addRoute(const std::string& method, const std::string& path, const Response& response);
    void addWsRoute(const std::string& path, const Response& response);

    void setTimeout(uint32_t timeout, std::function<void(uv_timer_t*, HttpServer*)> cb=nullptr);
    void setOnClientConnected(ClientCallbackFn);
    void setOnClientClosed(ClientCallbackFn);
    
    static std::string getStatusText(HttpStatus status_code);
    void closeStream(uv_stream_t* handle);
    void closeClient(size_t client_id);

    size_t addClient(client_context* client);
    void removeClient(size_t client_id);

private:
    
    // libuv 回调函数
    static void on_connection(uv_stream_t* server, int status);
    static void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
    static void on_read(uv_stream_t* client, ssize_t nread, const uv_buf_t* buf);
    static void on_write(uv_write_t* req, int status);
    static void on_close(uv_handle_t* handle);
    static void on_send(uv_timer_t* handle);
    
    // HTTP 解析和处理
    void handleHttpRequest(uv_stream_t* client, const std::string& request_data);
    bool parseHttpRequest(const std::string& data, http_request& req);
    void sendHttpResponse(uv_stream_t* client, const Response& resp);
    void handleWsRequest(uv_stream_t* client, const std::string& request_data);

    void enqueueWriteRequest(http_write_req* write_req);
    void dequeueWriteQueue();
    void idle();
    
    void sendErrorResponse(uv_stream_t* client, HttpStatus status_code, const std::string& message="");

    Response* findResponse(const std::string& method, const std::string& url);

private:
    static std::unordered_map<int, std::string> status_map;
    uv_loop_t* loop;
    uv_tcp_t server;
    std::queue<http_write_req*> write_queue;
    uv_timer_t write_timer;
    uv_timer_t timeout_timer;
    uint32_t timeout; // seconds
    uint64_t idl_time;
    std::function<void(uv_timer_t*, HttpServer*)> timeout_callback;
    
    // 路由表: method -> path -> handler
    std::unordered_map<std::string, 
        std::unordered_map<std::string, Response>> routes;
    
    ClientCallbackFn onClientConnected;
    ClientCallbackFn onClientClosed;

    size_t client_counter;
    std::unordered_map<size_t, client_context*> clients;
};


#endif // SERVER_H