#ifndef CAPTURE_H
#define CAPTURE_H

#include <string>
#include <vector>

class Capturer {

public:
    Capturer() {}
    ~Capturer() {}

    bool capture(float quality);

    std::vector<unsigned char> jpg;
    size_t width;
    size_t height;
};

#endif //CAPTURE_H