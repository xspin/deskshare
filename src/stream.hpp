
#include "frame.hpp"
#include "server.h"
#include "singleton.hpp"
#include "utils.h"
#include <atomic>
#include <cstddef>
#include <iterator>
#include <mutex>
#include <thread>
#include <tuple>
#include <vector>

enum class EncodeType { MJPEG, H264 };

class Stream : public Singleton<Stream> {
    friend class Singleton<Stream>;

  private:
    Stream();

    ~Stream();

  public:
    using DetailTuple = std::tuple<size_t, time_t, WsResponse *, std::string>; // id, time, Response
                                                                               //
    void setup(int max_fps, float quality, EncodeType type);
    void init(uv_loop_t *loop);
    void start();
    void stop();
    void close();

    void requestStart(const std::string &ip, uint16_t port, size_t id, WsResponse *resp = nullptr);
    void requestEnd(const std::string &ip, uint16_t port, size_t id);
    std::string listRequests();
    std::string listClients();

    bool isH264();
    std::pair<const void *, size_t> getFrame();
    std::pair<const void *, size_t> capJpegFrame();

    std::pair<float, float> stat();
    std::unordered_map<std::string, DetailTuple> &getReqs();

  private:
    void sendFrame();
    static void worker(Stream *self);
    static void async_cb(uv_async_t *handle);
    static void worker_cb(uv_async_t *handle);
    static void worker_timer_cb(uv_timer_t *handle);

    uv_loop_t *_loop = nullptr;
    uv_async_t _async;
    uv_loop_t _worker_loop;
    uv_async_t _worker_async;
    uv_timer_t _worker_timer;

    std::atomic<size_t> _frames{0}, _bytes{0};
    uint64_t _stat_ts_ms{0};
    volatile bool _running = false;
    volatile bool _terminal = false;
    uint64_t _frame_ts_ms{0};
    int _max_fps = 10;

    size_t _frame_idx{0};
    EncodeType _type;
    FrameH264 _frame_h264;
    FrameJpeg _frame_jpeg;
    Frame *_frame = nullptr;
    std::thread _thread;

    std::unordered_map<std::string, DetailTuple> _reqs;
};
