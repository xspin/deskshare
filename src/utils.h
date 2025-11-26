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
    int fps = 7;
    int timeout = 3600; //seconds
    bool color = 0;
    size_t clients = 0;
    size_t frames = 0;
    size_t bytes = 0;

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

extern Args g_config;

namespace utils {

int parse_args(Args& args, int argc, char* argv[]);

std::string timeFmt(time_t t, const std::string& fmt="%Y-%m-%d %H:%M:%S");
std::string gmTimeFmt(time_t t, const std::string& fmt);
std::string getTime();

std::vector<std::pair<std::string,std::string>> getIpAddress();

std::string renderTemplate(const std::string& tpl, std::unordered_map<std::string,std::string> mp);

std::string getMimeType(const std::string& filename); 

std::string speedString(size_t bytes);

std::vector<std::pair<size_t,size_t>> split(const std::string& s, char c, size_t n=0);

std::pair<std::string,std::string> bisect(const std::string& s, char c);

std::string trim(const std::string& s);


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