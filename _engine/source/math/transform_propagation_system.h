#pragma once
#include "scheduler.h"

namespace Entelechy {

class ThreadPool;

class TransformPropagationSystem : public System {
public:
    void tick(World& world, FrameArena& arena, f32 dt) override;

    void setThreadPool(ThreadPool* tp) { m_threadPool = tp; }

private:
    ThreadPool* m_threadPool = nullptr;
};

} // namespace Entelechy
