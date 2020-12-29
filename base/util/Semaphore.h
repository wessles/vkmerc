#pragma once

#include <mutex>
#include <condition_variable>

// https://stackoverflow.com/a/4793662
class Semaphore
{
private:
    std::mutex mutex_;
    std::condition_variable condition_;
    unsigned long count_ = 0; // Initialized as locked.

public:
    void release();

    void acquire();

    bool try_acquire();
};