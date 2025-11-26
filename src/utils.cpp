#include <sstream>
#include <unistd.h>
#include <algorithm>
#include "utils.h"

Args g_config;

namespace utils {

int parse_args(Args& args, int argc, char* argv[]) {
    std::stringstream help;
    help << "DeskShare " APP_VERSION 
        << " ( Tool to share screen on web browser )\n"
        << "Usage: " << argv[0] << " [-h] [-d] [-p PORT] [-q QUALITY] [-f FPS] [-t SECONDS]\n"
        << "    -h           show this helpful usage message\n"
        << "    -p PORT      specify the http port (default " << args.port << ")\n"
        << "    -q QUALITY   specify jpeg compress quality (0~1) (default " << args.quality << ")\n"
        << "    -f FPS       specify max fps (default " << args.fps << ")\n"
        << "    -t SECONDS   idl timeout to exit (default " << args.timeout << ", 0 for infinity)\n"
        << "    -c           colored log output (default disabled on windows)\n"
        << "    -v           show version info\n"
        << "    -d           log in debug level";

    int opt;
    while ((opt = getopt(argc, argv, "t:f:q:p:dhcv")) != -1) {
        switch (opt) {
            case 'v':
                std::cout << "DeskShare version " << APP_VERSION << std::endl;
                return 1;
            case 't':
                args.timeout = std::stoi(optarg);
                if (args.timeout < 1) {
                    std::cerr << "invalid timeout: " << args.timeout << std::endl;
                    return -1;
                }
                break;
            case 'p':
                args.port = std::stoi(optarg);
                break;
            case 'q':
                args.quality = std::stof(optarg);
                if (args.quality<=0 || args.quality>1) {
                    std::cerr << "invalid quality: " << args.quality << std::endl;
                    return 1;
                }
                break;
            case 'f':
                args.fps = std::stoi(optarg);
                if (args.fps < 1) {
                    std::cerr << "invalid fps: " << args.fps << std::endl;
                    return 1;
                }
            case 'c':
                args.color = true; break;
            case 'd':
                args.debug = true; break;
            case 'h':
                std::cout << help.str() << std::endl;
                return 1;
            case '?': // 未知选项
                std::cerr << "invalid option" << std::endl;
            default: 
                std::cerr << help.str() << std::endl;
                return 1;
        }
    }
    return 0;
}

std::string timeFmt(time_t t, const std::string& fmt) {
    auto tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, fmt.c_str());
    return oss.str();
}

std::string getTime() {
    auto t = std::time(nullptr);
    return timeFmt(t);
}

std::string gmTimeFmt(time_t t, const std::string& fmt) {
    auto tm = *std::gmtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, fmt.c_str());
    return oss.str();
}

std::vector<std::pair<std::string,std::string>> getIpAddress() {
    std::vector<std::pair<std::string,std::string>> res;

    char buf[INET_ADDRSTRLEN];
    uv_interface_address_t *info;
    int count;

    uv_interface_addresses(&info, &count);
    uv_interface_address_t* itf = info;

    for (int i=0; i<count; i++, itf++) {
        if (itf->address.address4.sin_family == AF_INET) {
            uv_ip4_name(&itf->address.address4, buf, sizeof(buf));
            res.emplace_back(itf->name, buf);
        }
    }

    uv_free_interface_addresses(info, count);

    return res;
}

std::string renderTemplate(const std::string& tpl, std::unordered_map<std::string,std::string> mp) {
    // {{key}} -> value
    size_t n = tpl.size();
    std::string res;
    res.reserve(n);
    auto findPlaceholder = [&tpl, n](size_t i) -> std::pair<int,int> {
        size_t j = i;
        while (i < n) {
            while (i < n-1 && !(tpl[i] == '{' && tpl[i+1] == '{')) i++;
            j = i + 2;
            while (j < n && tpl[j-1] != '}') j++;
            if (j < n && tpl[j] == '}') break;
            i = j + 1;
        }
        return std::make_pair(i, j);
    };

    size_t k = 0;
    while (k < n) {
        auto [i, j] = findPlaceholder(k);
        if (i < j) {
            std::string key = tpl.substr(i+2, j-i-3);
            auto it = mp.find(key);
            if (it != mp.end()) {
                res += tpl.substr(k, i-k);
                res += it->second;
            } else {
                res += tpl.substr(k, j-k+1);
            }
        } else {
            res += tpl.substr(k);
            break;
        }
        k = j + 1;
    }

    return res;
}

// 获取 MIME 类型
std::string getMimeType(const std::string& filename) {
    size_t dot_pos = filename.rfind('.');
    if (dot_pos == std::string::npos) {
        return "text/html";
    }
    
    std::string ext = filename.substr(dot_pos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    static const std::unordered_map<std::string, std::string> mime_types = {
        {"html", "text/html"},
        {"htm", "text/html"},
        {"css", "text/css"},
        {"txt", "text/plain"},
        {"js", "application/javascript"},
        {"json", "application/json"},
        {"icon", "image/x-icon"},
        {"ico", "image/x-icon"},
        {"png", "image/png"},
        {"jpg", "image/jpeg"},
        {"jpeg", "image/jpeg"},
        {"gif", "image/gif"},
        {"svg", "image/svg+xml"},
        {"pdf", "application/pdf"}
    };
    
    auto it = mime_types.find(ext);
    if (it != mime_types.end()) {
        return it->second;
    }
    
    return "application/octet-stream";
}

// URL 解码
std::string urlDecode(const std::string& str) {
    std::string result;
    result.reserve(str.length());
    
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%' && i + 2 < str.length()) {
            int value;
            std::istringstream hex_stream(str.substr(i + 1, 2));
            if (hex_stream >> std::hex >> value) {
                result += static_cast<char>(value);
                i += 2;
            } else {
                result += str[i];
            }
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    
    return result;
}

std::string speedString(size_t bps) {
    size_t KB = 1024;
    size_t MB = KB * 1024;
    std::stringstream ss;
    if (bps >= MB) {
        ss << bps/MB << " MB/s";
    } else if (bps >= KB) {
        ss << bps/KB << " KB/s";
    } else {
        ss << bps << " B/s";
    }
    return ss.str();
}

std::pair<std::string,std::string> bisect(const std::string& s, char c) {
    size_t i = s.find(c);
    if (i == std::string::npos) {
        return {s, ""};
    }
    return {s.substr(0, i), s.substr(i+1)};
}

// {<idx,len>, ...}
std::vector<std::pair<size_t,size_t>> split(const std::string& s, char c, size_t n) {
    std::vector<std::pair<size_t,size_t>> res;
    size_t k = 0;
    size_t i = 0;
    while (i < s.size()) {
        size_t j = i;
        while (j < s.size() && s[j] != c) j++;
        res.emplace_back(i, j-i);
        i = j + 1;
        if ((n > 0 && ++k >= n) || (i == s.size() && s[j] == c)) {
            res.emplace_back(i, s.size()-i);
            break;
        }
    }

    return res;
}

std::string trim(const std::string& str) {
    auto front = std::find_if_not(str.begin(), str.end(), 
        [](unsigned char ch) { return std::isspace(ch); });
    
    auto back = std::find_if_not(str.rbegin(), str.rend(), 
        [](unsigned char ch) { return std::isspace(ch); }).base();
    
    if (front >= back) {
        return "";
    }
    
    return std::string(front, back);
}

} // namespace

