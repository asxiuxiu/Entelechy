#include "asset/loader/asset_server.h"

namespace Entelechy
{

AssetServer::AssetServer(VFS *vfs) : m_vfs(vfs)
{
    m_loader_thread = std::thread(&AssetServer::loadingThreadLoop, this);
}

AssetServer::~AssetServer()
{
    shutdown();
}

void AssetServer::shutdown()
{
    bool expected = true;
    if (m_running.compare_exchange_strong(expected, false))
    {
        if (m_loader_thread.joinable())
        {
            m_loader_thread.join();
        }
    }
}

void AssetServer::loadingThreadLoop()
{
    while (m_running.load())
    {
        std::function<void()> task;
        {
            std::lock_guard<std::mutex> lock(m_pending_mutex);
            if (!m_pending_tasks.empty())
            {
                task = std::move(m_pending_tasks.front());
                m_pending_tasks.pop_front();
            }
        }
        if (task)
        {
            task();
        }
        else
        {
            std::this_thread::yield();
        }
    }
}

void AssetServer::processEvents()
{
    std::deque<std::function<void()>> callbacks;
    {
        std::lock_guard<std::mutex> lock(m_completed_mutex);
        callbacks.swap(m_completed_callbacks);
    }
    for (auto &cb : callbacks)
    {
        cb();
    }
}

} // namespace Entelechy
