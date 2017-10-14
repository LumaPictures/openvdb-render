#include "progress_bar.h"

#include <maya/MGlobal.h>


ProgressBar::ProgressBar(
        const MString& msg,
        const uint32_t max_progress,
        const bool is_interruptible)
    : m_show_progress_bar(MGlobal::mayaState() == MGlobal::kInteractive)
    , m_is_interruptible(is_interruptible)
{
    if (!m_show_progress_bar)
        return;

    reset(msg, max_progress);
}

ProgressBar::~ProgressBar()
{
    if (!m_show_progress_bar)
        return;

    endProgress();
}

void ProgressBar::reset(const MString& msg, const uint32_t max_progress)
{
    if (!m_show_progress_bar)
        return;

    m_max_progress = max_progress;
    m_progress = 0;
    beginProgress(msg);
}

void ProgressBar::addProgress(uint32_t progress_to_add)
{
    if (!m_show_progress_bar)
        return;

    const auto old_progress = m_progress.fetch_and_add(progress_to_add);
    const auto old_percents = uint8_t(old_progress * 100 / m_max_progress);
    const auto new_percents = uint8_t(
            (old_progress + progress_to_add) * 100 / m_max_progress);
    if (new_percents > old_percents) {
        tbb::mutex::scoped_lock lock(m_mutex);
        m_computation.setProgress(new_percents);
    }
}

void ProgressBar::setProgress(const int percent)
{
    if (!m_show_progress_bar)
        return;

    m_computation.setProgress(percent);
}

bool ProgressBar::isCancelled()
{
    if (!m_show_progress_bar || !m_is_interruptible)
        return false;

    tbb::mutex::scoped_lock lock(m_mutex);

    return m_computation.isInterruptRequested();
}

void ProgressBar::beginProgress(const MString& msg)
{
    m_computation.beginComputation(true, m_is_interruptible);
    m_computation.setProgressStatus(msg);
}

void ProgressBar::endProgress()
{
    m_computation.endComputation();
}
