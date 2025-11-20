#ifndef CAPTURE_H
#define CAPTURE_H

#include <string>
#include <vector>

class Capturer {

public:
    Capturer() {}
    ~Capturer() {}

    static std::pair<int,int> getCursorPos();
    static std::pair<int,int> getResolution();

    bool capture(float quality);

    std::vector<unsigned char> jpg;
    size_t width;
    size_t height;
};

#endif //CAPTURE_H