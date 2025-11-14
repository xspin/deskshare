#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include <ctime>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <uv.h>
#include <unordered_map>
#include <cassert>

struct Args {
    bool debug = false;
    int port = 2333;
    float quality = 0.6;
    int fps = 10;
    int timeout = 3600; //seconds
    bool color = 0;

    std::string str() {
        std::stringstream ss;
        ss << "Options: { "
            << "port: " << port << ", "
            << "quality: " << quality << ", "
            << "timeout: " << timeout << ", "
            << "fps: " << fps << ", "
            << "debug: " << debug << " }";
        return ss.str();
    }
};

extern Args g_args;

namespace utils {

int parse_args(Args& args, int argc, char* argv[]);

std::string getTime();
std::string getTime(time_t t);

std::vector<std::pair<std::string,std::string>> getIpAddress();

std::string renderTemplate(const std::string& tpl, std::unordered_map<std::string,std::string> mp);

std::string getMimeType(const std::string& filename); 


template <typename T>
struct is_string_like : std::false_type {};

template <> struct is_string_like<std::string> : std::true_type {};
template <> struct is_string_like<const char*> : std::true_type {};
template <> struct is_string_like<char*> : std::true_type {};
template <size_t N> struct is_string_like<const char[N]> : std::true_type {};
template <size_t N> struct is_string_like<char[N]> : std::true_type {};
template <typename T> constexpr bool is_string_like_v = is_string_like<T>::value;

} // namespace

#endif // UTILS_H