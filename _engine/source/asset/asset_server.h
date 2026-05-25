#pragma once
#include "assets.h"
#include "asset_loader.h"
#include "path.h"
#include "vfs.h"
#include <thread>
#include <mutex>
#include <deque>
#include <functional>
#include <atomic>
#include <utility>

namespace Entelechy {

// ------------------------------------------------------------------
// AssetServer — loading scheduler with a single background thread
// ------------------------------------------------------------------
// Simplified path (Phase 5):
//   - One dedicated loader thread (not the full ThreadPool).
//   - Mutex-protected task queue for pending loads.
//   - Mutex-protected callback queue for completed loads.
//   - Main thread calls processEvents() to consume completions.
//
// Future upgrades (Phase 8+):
//   - Replace single thread with ThreadPool + work-stealing.
//   - Replace mutex queues with lock-free MPSC channels.
//   - Add LoadingGraph for dependency DAG + topological post-process.
//   - Add FileWatcher for automatic hot-reload.
// ------------------------------------------------------------------
class AssetServer {
public:
    explicit AssetServer(VFS* vfs = nullptr);
    ~AssetServer();

    // Synchronous load: blocks current thread until asset is ready.
    // Returns a valid Handle immediately.
    template<typename T>
    Handle<T> loadSync(const Path& path, IAssetLoader<T>& loader, Assets<T>& storage);

    // Asynchronous load: returns Handle immediately, data arrives later.
    // The handle is valid but storage.get(handle) returns nullptr until
    // the load completes and processEvents() is called.
    template<typename T>
    Handle<T> loadAsync(const Path& path, IAssetLoader<T>& loader, Assets<T>& storage);

    // Process all completed load callbacks.
    // MUST be called from the main thread (e.g. inside a System tick).
    void processEvents();

    // Explicit unload. The caller is responsible for ensuring no
    // active references remain (reference counting is advisory in
    // this simplified path).
    template<typename T>
    void unload(Handle<T> handle, Assets<T>& storage);

    // Reload an existing handle synchronously.
    // The handle itself is preserved; only its data is replaced.
    template<typename T>
    void reload(Handle<T> handle, const Path& path, IAssetLoader<T>& loader, Assets<T>& storage);

    // Graceful shutdown. Called automatically by destructor.
    void shutdown();

private:
    void loadingThreadLoop();

    VFS* m_vfs = nullptr;

    std::thread m_loaderThread;
    std::mutex m_pendingMutex;
    std::deque<std::function<void()>> m_pendingTasks;

    std::mutex m_completedMutex;
    std::deque<std::function<void()>> m_completedCallbacks;

    std::atomic<bool> m_running{true};
};

// ---------- Template implementation ----------

template<typename T>
Handle<T> AssetServer::loadSync(const Path& path, IAssetLoader<T>& loader, Assets<T>& storage) {
    FileData data;
    if (m_vfs) {
        data = m_vfs->readFile(path);
    }
    T asset = loader.load(data, path);
    return storage.insert(std::move(asset));
}

template<typename T>
Handle<T> AssetServer::loadAsync(const Path& path, IAssetLoader<T>& loader, Assets<T>& storage) {
    Handle<T> handle = storage.allocateEmpty();

    auto task = [this, path, &loader, handle, &storage]() {
        FileData data;
        if (m_vfs) {
            data = m_vfs->readFile(path);
        }
        T asset = loader.load(data, path);

        std::lock_guard<std::mutex> lock(m_completedMutex);
        m_completedCallbacks.push_back([handle, asset = std::move(asset), &storage]() mutable {
            storage.fill(handle, std::move(asset));
        });
    };

    {
        std::lock_guard<std::mutex> lock(m_pendingMutex);
        m_pendingTasks.push_back(std::move(task));
    }

    return handle;
}

template<typename T>
void AssetServer::unload(Handle<T> handle, Assets<T>& storage) {
    storage.remove(handle);
}

template<typename T>
void AssetServer::reload(Handle<T> handle, const Path& path, IAssetLoader<T>& loader, Assets<T>& storage) {
    FileData data;
    if (m_vfs) {
        data = m_vfs->readFile(path);
    }
    T asset = loader.load(data, path);
    storage.fill(handle, std::move(asset));
}

} // namespace Entelechy
