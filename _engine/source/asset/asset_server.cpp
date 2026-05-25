#include "asset_server.h"

namespace Entelechy {

AssetServer::AssetServer(VFS* vfs) : m_vfs(vfs) {
    m_loaderThread = std::thread(&AssetServer::loadingThreadLoop, this);
}

AssetServer::~AssetServer() {
    shutdown();
}

void AssetServer::shutdown() {
    bool expected = true;
    if (m_running.compare_exchange_strong(expected, false)) {
        if (m_loaderThread.joinable()) {
            m_loaderThread.join();
        }
    }
}

void AssetServer::loadingThreadLoop() {
    while (m_running.load()) {
        std::function<void()> task;
        {
            std::lock_guard<std::mutex> lock(m_pendingMutex);
            if (!m_pendingTasks.empty()) {
                task = std::move(m_pendingTasks.front());
                m_pendingTasks.pop_front();
            }
        }
        if (task) {
            task();
        } else {
            std::this_thread::yield();
        }
    }
}

void AssetServer::processEvents() {
    std::deque<std::function<void()>> callbacks;
    {
        std::lock_guard<std::mutex> lock(m_completedMutex);
        callbacks.swap(m_completedCallbacks);
    }
    for (auto& cb : callbacks) {
        cb();
    }
}

} // namespace Entelechy
