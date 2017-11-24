#pragma once

#include <maya/MComputation.h>
#include <maya/MString.h>

#include <tbb/atomic.h>
#include <tbb/mutex.h>

#include <cstdint>


// Based on the gpuCache plugin from the devkit.

class ProgressBar {
public:
    explicit ProgressBar(
        const MString& msg,
        uint32_t max_progress = 100,
        bool is_interruptable = true);
    ProgressBar(const ProgressBar&) = delete;
    ProgressBar(ProgressBar&&) = delete;
    ProgressBar& operator=(const ProgressBar&) = delete;
    ProgressBar&& operator=(ProgressBar&&) = delete;

    ~ProgressBar();

    void reset(const MString& msg, uint32_t max_progress);
    // The public methods below are thread-safe.
    void addProgress(uint32_t progress_to_add);
    bool isCancelled();
private:
    void beginProgress(const MString& msg);
    void endProgress();

    tbb::mutex m_mutex;
    tbb::atomic<uint32_t> m_progress;
    uint32_t m_max_progress;
    MComputation m_computation;
    bool m_show_progress_bar;
    bool m_is_interruptible;
};
