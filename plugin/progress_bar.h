// Copyright 2019 Luma Pictures
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
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
