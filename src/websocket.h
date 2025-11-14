#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <string>
#include <functional>
#include <uv.h>
/**
 * WebSocket 错误码枚举（基于 RFC 6455 及常见实践）
 * 标准码范围：1000-1015（协议定义）
 * 自定义应用码范围：3000-3999（需双方约定）
 */

enum class WsErrorCode : uint16_t {
    // 标准错误码（1000-1015）
    NORMAL_CLOSURE = 1000,         ///< 正常关闭（成功完成通信）
    GOING_AWAY = 1001,             ///< 端点离开（如页面关闭、服务器下线）
    PROTOCOL_ERROR = 1002,         ///< 协议错误（收到无效格式的帧）
    UNSUPPORTED_DATA = 1003,       ///< 不支持的数据类型（如服务器不处理二进制帧）
    NO_STATUS_RCVD = 1005,         ///< 未收到状态码（保留值，由协议自动使用）
    ABNORMAL_CLOSURE = 1006,       ///< 异常关闭（如网络中断，无CLOSE帧）
    INVALID_PAYLOAD_DATA = 1007,   ///< 无效的payload（如文本帧含非UTF-8数据）
    POLICY_VIOLATION = 1008,       ///< 违反策略（如内容不符合服务器规则）
    MESSAGE_TOO_BIG = 1009,        ///< 消息过大（超过服务器限制）
    MANDATORY_EXT = 1010,          ///< 缺少必需的扩展（客户端要求的扩展不支持）
    INTERNAL_ERROR = 1011,         ///< 服务器内部错误（处理消息时异常）
    SERVICE_RESTART = 1012,        ///< 服务重启（服务器临时关闭，可重连）
    TRY_AGAIN_LATER = 1013,        ///< 稍后重试（服务器过载）
    BAD_GATEWAY = 1014,            ///< 网关错误（作为网关时上游响应无效）
    TLS_HANDSHAKE = 1015,          ///< TLS握手失败（wss连接加密失败，保留值）

    // 自定义应用错误码（3000-3999，需双方约定）
    AUTH_FAILED = 3000,            ///< 用户认证失败
    SESSION_EXPIRED = 3001,        ///< 会话超时
    PERMISSION_DENIED = 3002,      ///< 权限不足
    RESOURCE_NOT_FOUND = 3003,     ///< 请求的资源不存在
    RATE_LIMIT_EXCEEDED = 3004     ///< 超过请求频率限制
};

inline const char* wsErrorToString(WsErrorCode code) {
    switch (code) {
        case WsErrorCode::NORMAL_CLOSURE: return "Normal closure";
        case WsErrorCode::GOING_AWAY: return "Endpoint is going away";
        case WsErrorCode::PROTOCOL_ERROR: return "Protocol error";
        case WsErrorCode::UNSUPPORTED_DATA: return "Unsupported data type";
        case WsErrorCode::NO_STATUS_RCVD: return "No status code received";
        case WsErrorCode::ABNORMAL_CLOSURE: return "Abnormal closure";
        case WsErrorCode::INVALID_PAYLOAD_DATA: return "Invalid payload data";
        case WsErrorCode::POLICY_VIOLATION: return "Policy violation";
        case WsErrorCode::MESSAGE_TOO_BIG: return "Message too big";
        case WsErrorCode::MANDATORY_EXT: return "Mandatory extension missing";
        case WsErrorCode::INTERNAL_ERROR: return "Server internal error";
        case WsErrorCode::SERVICE_RESTART: return "Service restart";
        case WsErrorCode::TRY_AGAIN_LATER: return "Try again later";
        case WsErrorCode::BAD_GATEWAY: return "Bad gateway";
        case WsErrorCode::TLS_HANDSHAKE: return "TLS handshake failed";
        case WsErrorCode::AUTH_FAILED: return "Authentication failed";
        case WsErrorCode::SESSION_EXPIRED: return "Session expired";
        case WsErrorCode::PERMISSION_DENIED: return "Permission denied";
        case WsErrorCode::RESOURCE_NOT_FOUND: return "Resource not found";
        case WsErrorCode::RATE_LIMIT_EXCEEDED: return "Rate limit exceeded";
        default: return "Unknown error code";
    }
}

// WebSocket 帧类型枚举（对应 opcode）
enum class WsFrameType {
    TEXT = 0x1,
    BINARY = 0x2,
    CLOSE = 0x8,
    PING = 0x9,
    PONG = 0xA,
    CONTINUATION = 0x0,
    UNKNOWN = 0xFF
};


static inline const char* wsFrameTypeToString(WsFrameType type) {
    switch (type) {
        case WsFrameType::TEXT: return "TEXT";
        case WsFrameType::BINARY: return "BINARY";
        case WsFrameType::CLOSE: return "CLOSE";
        case WsFrameType::PING: return "PING";
        case WsFrameType::PONG: return "PONG";
        case WsFrameType::CONTINUATION: return "CONTINUATION";
        case WsFrameType::UNKNOWN: return "UNKNOWN";
        default: return "INVALID";
    }
}

// WebSocket 客户端连接类
class WsClient {
public:

    WsClient() {
    }

    static std::pair<WsFrameType, std::string> parse_frame(const char* data, size_t size);
    static std::string pack_frame(WsFrameType type, const char *data, size_t len);
    static std::string pack_handshake(const std::string& key);
    static std::string pack_close_frame(WsErrorCode code, const std::string& reason);
    static std::string pack_pong_frame(const std::string& data);
    static std::string pack_text_frame(const std::string& data);
    static std::string pack_binary_frame(const char* data, size_t len);
};

#endif // WEBSOCKET_H