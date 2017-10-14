#pragma once

#include <maya/MComputation.h>
#include <maya/MString.h>

#include <tbb/atomic.h>
#include <tbb/mutex.h>

#include <cstdint>


// Based on the gpuCache plugin from the devkit.

class ProgressBar {
public:
    ProgressBar(
            const MString& msg,
            const uint32_t max_progress=100,
            const bool is_interruptable = true);
    ~ProgressBar();

    void reset(const MString& msg, const uint32_t max_progress);
    void setProgress(const int percent);

    void setMaxProgress(const uint32_t max_progress)
    {
        m_max_progress = max_progress;
    }

    // The public methods below are thread-safe.
    void addProgress(uint32_t progress_to_add);
    bool isCancelled();

private:
    ProgressBar(const ProgressBar&) = delete;
    ProgressBar& operator=(const ProgressBar&) = delete;
    ProgressBar(ProgressBar&&) = delete;
    ProgressBar&& operator=(ProgressBar&&) = delete;

    void beginProgress(const MString& msg);
    void endProgress();

    MComputation m_computation;
    bool m_show_progress_bar;
    bool m_is_interruptible;
    uint32_t m_max_progress;
    tbb::atomic<uint32_t> m_progress;

    mutable tbb::mutex m_mutex;
};
