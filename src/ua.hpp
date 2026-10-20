#pragma once

#include <iostream>
#include <optional>
#include <regex>
#include <sstream>
#include <string>

struct BrowserInfo {
    std::string name;    // 浏览器名称
    std::string version; // 浏览器版本

    BrowserInfo(const std::string &ua) {
        // 按优先级匹配：先匹配更具体的标识，避免被 Chrome 截胡
        // 顺序：Edge -> Opera -> Firefox -> Chrome -> Safari
        struct Rule {
            const char *pattern; // 正则
            const char *name;    // 浏览器名
        };
        static const Rule rules[] = {
            {R"(Edg(?:e|A|iOS)?/([\d.]+))", "Edge"},
            {R"(OPR/([\d.]+))", "Opera"},
            {R"(Firefox/([\d.]+))", "Firefox"},
            {R"(Chrome/([\d.]+))", "Chrome"},
        };

        for (const auto &r : rules) {
            std::regex re(r.pattern);
            std::smatch m;
            if (std::regex_search(ua, m, re)) {
                name = r.name;
                version = m[1];
                return;
            }
        }

        // Safari 特殊处理：取 Version/ 后的版本，而不是 Safari/ 后的 WebKit 构建号
        {
            std::regex re_version(R"(Version/([\d.]+))");
            std::regex re_safari(R"(Safari/)");
            std::smatch m;
            if (std::regex_search(ua, re_safari) && std::regex_search(ua, m, re_version)) {
                name = "Safari";
                version = m[1];
                return;
            }
            if (std::regex_search(ua, re_safari)) {
                name = "Safari";
                return;
            }
        }
    }
};

struct OsInfo {
    std::string os;      // 系统名称，如 "Windows", "macOS", "iOS", "Android", "Linux"
    std::string version; // 版本号，如 "10.0", "14.5", "17.4.1"

    OsInfo(const std::string &os, const std::string &ver) : os(os), version(ver){};

    OsInfo(const std::string &ua) {
        // 1) Windows NT x.y  ->  映射到常见版本号
        {
            std::regex re(R"(Windows NT (\d+\.\d+))");
            std::smatch m;
            if (std::regex_search(ua, m, re)) {
                std::string nt = m[1];
                // NT 版本到市场版本的映射（可选，方便阅读）
                std::string ver = nt;
                if (nt == "10.0")
                    ver = "10/11"; // NT 10.0 同时用于 Win10 和 Win11
                os = "Windows";
                version = ver;
                return;
            }
            if (ua.find("Windows") != std::string::npos) {
                os = "Windows";
                return;
            }
        }

        // 2) iOS:  iPhone OS 17_4_1  或  CPU OS 17_4 like Mac OS X
        {
            std::regex re(R"((?:iPhone|CPU) OS (\d+[_.]\d+(?:[_.]\d+)*))");
            std::smatch m;
            if (std::regex_search(ua, m, re)) {
                std::string ver = m[1];
                std::replace(ver.begin(), ver.end(), '_', '.');
                os = "iOS";
                version = ver;
                return;
            }
        }

        // 3) macOS:  Mac OS X 10_15_7  或  Mac OS X 14.5
        {
            std::regex re(R"(Mac OS X (\d+[_.]\d+(?:[_.]\d+)*))");
            std::smatch m;
            if (std::regex_search(ua, m, re)) {
                std::string ver = m[1];
                std::replace(ver.begin(), ver.end(), '_', '.');
                os = "macOS";
                version = ver;
                return;
            }
        }

        // 4) Android:  Android 14;  或  Android 13
        {
            std::regex re(R"(Android (\d+(?:\.\d+)*))");
            std::smatch m;
            if (std::regex_search(ua, m, re)) {
                os = "Android";
                version = m[1];
                return;
            }
        }

        // 5) Linux（无版本号，UA 里通常只有 "Linux"）
        if (ua.find("Linux") != std::string::npos) {
            os = "Linux";
            return;
        }
    }
};

static inline std::string getUaInfo(const std::string &ua) {
    BrowserInfo binfo(ua);
    OsInfo oinfo(ua);
    std::stringstream ss;
    ss << "(" << oinfo.os << " " << oinfo.version << "; " << binfo.name << " " << binfo.version
       << ")";
    return ss.str();
}
