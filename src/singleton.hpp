#pragma once

template <typename T> class Singleton {
  public:
    // 禁止拷贝和移动
    Singleton(const Singleton &) = delete;
    Singleton &operator=(const Singleton &) = delete;
    Singleton(Singleton &&) = delete;
    Singleton &operator=(Singleton &&) = delete;

    // 获取全局唯一实例
    static T &instance() {
        static T inst; // C++11 保证局部静态变量的线程安全初始化
        return inst;
    }

  protected:
    Singleton() = default;
    virtual ~Singleton() = default;
};

// 使用示例
class MyService : public Singleton<MyService> {
    friend class Singleton<MyService>; // 允许基类访问私有构造函数
  public:
    void doSomething() { /* ... */
    }

  private:
    MyService() = default; // 构造函数私有化
    ~MyService() = default;
};
