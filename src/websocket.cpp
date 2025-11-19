#include <uv.h>
#include <iostream>
#include <memory>
#include <string>
#include <cstring>

#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>

#include "websocket.h"

#ifdef PLATFORM_MACOS
#include <machine/endian.h>
#else
#include <winsock2.h> 
#endif

// Base64 编码（依赖 OpenSSL）
std::string base64_encode(const unsigned char* data, size_t len) {
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL); // 不添加换行符

    BIO_write(bio, data, len);
    BIO_flush(bio);

    char* buf;
    long buf_len = BIO_get_mem_data(bio, &buf);
    std::string result(buf, buf_len);

    BIO_free_all(bio);
    return result;
}

// 计算 WebSocket 握手响应密钥
static std::string compute_accept_key(const std::string& key) {
    const std::string magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string combined = key + magic;

    unsigned char sha1_hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(combined.c_str()), combined.size(), sha1_hash);
    return base64_encode(sha1_hash, SHA_DIGEST_LENGTH);
}

// 解析握手请求中的 Sec-WebSocket-Key
std::string parse_handshake_key(const std::string& buffer) {
    const std::string prefix = "Sec-WebSocket-Key: ";
    size_t start = buffer.find(prefix);
    if (start == std::string::npos) return "";

    start += prefix.size();
    size_t end = buffer.find("\r\n", start);
    if (end == std::string::npos) return "";

    return buffer.substr(start, end - start);
}

std::string WsClient::pack_handshake(const std::string& key) {
    std::string accept_key = compute_accept_key(key);
    std::string response = 
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + accept_key + "\r\n"
        "\r\n";
    return response;
}

std::string WsClient::pack_frame(WsFrameType type, const char* data, size_t len) {
    uint8_t opcode = static_cast<uint8_t>(type);
    size_t header_len = 0;

    // 计算头部长度
    if (len <= 125) {
        header_len = 2; // 基础头部（2字节）
    } else if (len <= 65535) {
        header_len = 4; // 扩展长度（2字节）+ 基础头部
    } else {
        header_len = 10; // 长扩展长度（8字节）+ 基础头部
    }

    // 分配帧内存（头部 + 数据）
    // char* frame = new char[header_len + len];
    auto frame = std::make_unique<char[]>(header_len + len);
    memset(frame.get(), 0, header_len + len);

    // 第一个字节：FIN=1（帧结束）+ 保留位=0 + opcode
    frame[0] = 0x80 | opcode;

    // 第二个字节及后续：长度编码
    if (len <= 125) {
        frame[1] = static_cast<uint8_t>(len); // 无掩码（服务器→客户端）
    } else if (len <= 65535) {
        frame[1] = 126; // 标识使用 2 字节扩展长度
        // 存储 16 位大端序长度
        uint16_t len16 = htons(static_cast<uint16_t>(len));
        memcpy(frame.get() + 2, &len16, 2);
    } else {
        frame[1] = 127; // 标识使用 8 字节扩展长度
        // 存储 64 位大端序长度（注意：libuv 可能需要兼容 64 位系统）
        uint64_t len64 = htonll(static_cast<uint64_t>(len)); // htobe64 需包含 <endian.h> 或自定义实现
        memcpy(frame.get() + 2, &len64, 8);
    }

    // 复制数据到帧中
    memcpy(frame.get() + header_len, data, len);
    return std::string(frame.get(), header_len + len);
}

std::string ack_frame(WsFrameType type, const char* data, size_t len) {
    uint8_t opcode = static_cast<uint8_t>(type);
    size_t header_len = 2; // 默认头部长度（2字节）
    if (len > 125) {
        header_len = 4; // 长度 >125 时，头部扩展为 4 字节
    }

    // 分配帧内存（头部 + 数据）
    std::string frame(header_len, 0);

    frame[0] = 0x80 | opcode; // FIN=1 + 帧类型

    // 设置长度字段
    if (len <= 125) {
        frame[1] = static_cast<uint8_t>(len); // 无掩码（服务器→客户端）
    } else {
        frame[1] = 126; // 扩展长度标识
        frame[2] = static_cast<uint8_t>((len >> 8) & 0xFF); // 高8位
        frame[3] = static_cast<uint8_t>(len & 0xFF);         // 低8位
    }

    frame.append(data, len);

    return frame;
}

// 封装 WebSocket 帧（返回堆上的 uv_buf_t，需手动释放）
uv_buf_t pack_ws_frame(WsFrameType type, const void* data, size_t len) {
    uint8_t opcode = static_cast<uint8_t>(type);
    size_t header_len = 2; // 默认头部长度（2字节）
    if (len > 125) {
        header_len = 4; // 长度 >125 时，头部扩展为 4 字节
    }

    // 分配帧内存（头部 + 数据）
    char* frame = new char[header_len + len];
    frame[0] = 0x80 | opcode; // FIN=1 + 帧类型

    // 设置长度字段
    if (len <= 125) {
        frame[1] = static_cast<uint8_t>(len); // 无掩码（服务器→客户端）
    } else {
        frame[1] = 126; // 扩展长度标识
        frame[2] = static_cast<uint8_t>((len >> 8) & 0xFF); // 高8位
        frame[3] = static_cast<uint8_t>(len & 0xFF);         // 低8位
    }

    // 复制数据
    std::memcpy(frame + header_len, data, len);
    return uv_buf_init(frame, header_len + len);
}

// 解析 WebSocket 帧头部
bool unpack_ws_header(const char* buffer, ssize_t nread, 
                     WsFrameType& type, size_t& payload_len, size_t& header_len) {
    if (nread < 2) return false;

    // 解析 opcode（帧类型）
    uint8_t opcode = buffer[0] & 0x0F;
    switch (opcode) {
        case 0x1: type = WsFrameType::TEXT; break;
        case 0x2: type = WsFrameType::BINARY; break;
        case 0x8: type = WsFrameType::CLOSE; break;
        case 0x9: type = WsFrameType::PING; break;
        case 0xA: type = WsFrameType::PONG; break;
        case 0x0: type = WsFrameType::CONTINUATION; break;
        default: type = WsFrameType::UNKNOWN; return false;
    }

    // 解析长度和头部长度
    uint8_t len_field = buffer[1] & 0x7F;
    header_len = 2;
    if (len_field == 126) {
        if (nread < 4) return false;
        payload_len = (static_cast<uint16_t>(buffer[2]) << 8) | buffer[3];
        header_len += 2;
    } else if (len_field == 127) {
        return false; // 简化：不处理超过 65535 的长度
    } else {
        payload_len = len_field;
    }

    // 检查掩码（客户端帧必须带掩码）
    bool has_mask = (buffer[1] & 0x80) != 0;
    if (has_mask) {
        header_len += 4; // 掩码占 4 字节
        if (nread < static_cast<ssize_t>(header_len + payload_len)) {
            return false; // 数据不完整
        }
    }

    return true;
}

// 解掩码（客户端帧解密）
void unmask_payload(const char* mask, std::string& payload) {
    for (size_t i = 0; i < payload.size(); ++i) {
        payload[i] ^= mask[i % 4];
    }
}

std::string WsClient::pack_text_frame(const std::string& data) {
    return pack_frame(WsFrameType::TEXT, data.c_str(), data.size());
}

std::string WsClient::pack_pong_frame(const std::string& data) {
    return pack_frame(WsFrameType::PONG, data.c_str(), data.size());
}

std::string WsClient::pack_binary_frame(const char* data, size_t len) {
    return pack_frame(WsFrameType::BINARY, data, len);
}

std::string WsClient::pack_close_frame(WsErrorCode code, const std::string& reason) {
    std::string payload;

    // 状态码转为网络字节序（大端）
    uint16_t code_be = htons(static_cast<uint16_t>(code));
    payload.append(reinterpret_cast<const char*>(&code_be), 2);
    payload.append(reason.substr(0, 123));

    return pack_frame(WsFrameType::CLOSE, payload.c_str(), payload.size());
}


// 读取数据回调
std::pair<WsFrameType, std::string> WsClient::parse_frame(const char* data, size_t size) {
    WsFrameType frame_type;
    size_t payload_len, header_len;
    if (!unpack_ws_header(data, size, frame_type, payload_len, header_len)) {
        return {WsFrameType::UNKNOWN, ""};
    }

    std::string payload(data + header_len, payload_len);

    if (data[1] & 0x80) { // 客户端帧带掩码，需解密
        const char* mask = data + 2; // 掩码位于头部后 4 字节
        unmask_payload(mask, payload);
    }

    return {frame_type, payload};
}
