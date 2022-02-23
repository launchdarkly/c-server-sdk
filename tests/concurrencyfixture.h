#ifndef LDSERVERAPI_CONCURRENCYFIXTURE_H
#define LDSERVERAPI_CONCURRENCYFIXTURE_H

#include "commonfixture.h"

#include <thread>
#include <random>
#include <chrono>
#include <mutex>
#include <functional>

// This fixture can be used to make concurrency testing more convenient.
// It allows for easy creation of multiple competing threads, with ability to set finalizers
// for whatever objects need to be cleaned up after the tests.
class ConcurrencyFixture : public CommonFixture {
private:
    std::mt19937_64 eng;
    std::uniform_int_distribution<> dist;
    std::mutex mux;
protected:
    std::vector<std::thread> pool;
    std::vector<std::function<void()>> finalizers;

    ConcurrencyFixture():
            pool{},
            eng{std::random_device{}()},
            dist{1, 100},
            mux{},
            finalizers{}
    {}

    void SetUp() override {
        CommonFixture::SetUp();
    }

    void TearDown() override  {
        for (std::thread& t : pool) {
            if (t.joinable()) {
                t.join();
            }
        }
        for (std::function<void()>& fn : finalizers) {
            fn();
        }
        CommonFixture::TearDown();
    }

    // Run a callable in the context of a new thread.
    template<typename Callable, typename ...Args>
    void Run(Callable&& fn, Args ...args) {
        pool.emplace_back(std::forward<Callable>(fn), std::forward<Args>(args)...);
    }

    // Run a callable in the context of N threads.
    template<typename Callable, typename ...Args>
    void RunMany(std::size_t N, Callable&& fn, Args ...args) {
        for (std::size_t i = 0; i < N; i++) {
            Run(fn, args...);
        }
    }

    // Defers a piece of code for execution after all threads created by Run have finished.
    void Defer(std::function<void()>&& fn){
        finalizers.emplace_back(std::move(fn));
    }

    // Sleeps the current thread for 1-100 ms.
    void Sleep() {
        mux.lock();
        auto duration = std::chrono::milliseconds(dist(eng));
        mux.unlock();
        std::this_thread::sleep_for(duration);
    }
};

#endif //LDSERVERAPI_CONCURRENCYFIXTURE_H
