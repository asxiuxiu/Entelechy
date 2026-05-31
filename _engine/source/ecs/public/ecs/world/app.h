#pragma once
#include "ecs/world/plugin.h"
#include "ecs/world/world.h"
#include "ecs/world/scheduler.h"
#include "core/container/dynamic_array.h"
#include "core/allocator/allocator.h"
#include <type_traits>
#include <utility>
#include <memory>

namespace Entelechy {

// ------------------------------------------------------------------
// App -- top-level application container
// ------------------------------------------------------------------
// Owns World, Scheduler, and all registered plugins.
// Plugin lifecycle:
//   1. addPlugin()   -- register plugins (any order)
//   2. build()       -- sort by LoadingPhase + dependency topology,
//                       then call plugin->build(), poll ready(), call finish()
//   3. setup()       -- call plugin->setup() in sorted order
//   4. update(dt)    -- tick scheduler
//   5. teardown()    -- call plugin->teardown() in reverse order
//
// Usage:
//   App app;
//   app.emplacePlugin<MyPlugin>();
//   app.build();
//   app.setup();
//   while (app.isRunning()) {
//       app.update(dt);
//   }
//   app.teardown();
// ------------------------------------------------------------------
class App {
public:
    App();
    ~App();

    // Add a plugin. App takes ownership.
    void addPlugin(IPlugin* plugin);

    // Convenience: construct and add in one call.
    template<typename T, typename... Args>
    T& emplacePlugin(Args&&... args) {
        static_assert(std::is_base_of_v<IPlugin, T>, "T must derive from IPlugin");
        T* ptr = static_cast<T*>(DefaultAllocator::alloc(sizeof(T), alignof(T)));
        std::construct_at(ptr, std::forward<Args>(args)...);
        addPlugin(ptr);
        return *ptr;
    }

    // Build all plugins:
    //   - Sort by LoadingPhase, then topological sort within each phase
    //   - Call plugin->build() in sorted order
    //   - Poll plugin->ready() until all report true
    //   - Call plugin->finish() in sorted order
    //   - Build scheduler dependency graph
    void build();

    // Setup all plugins (calls plugin->setup() in sorted order).
    void setup();

    // Run one simulation tick. Internally calls scheduler.tick(world, dt).
    void update(f32 dt);

    // Teardown all plugins in reverse order.
    void teardown();

    // Request quit. isRunning() will return false next check.
    void quit() { m_running = false; }

    [[nodiscard]] bool isRunning() const { return m_running; }
    [[nodiscard]] bool isBuilt() const { return m_built; }

    [[nodiscard]] World& world() { return m_world; }
    [[nodiscard]] const World& world() const { return m_world; }

    [[nodiscard]] Scheduler& scheduler() { return m_scheduler; }
    [[nodiscard]] const Scheduler& scheduler() const { return m_scheduler; }

    [[nodiscard]] usize pluginCount() const { return m_plugins.size(); }

    // Get sorted plugin manifests for AI observability.
    [[nodiscard]] DynamicArray<PluginManifest> pluginManifests() const;

private:
    // Sort plugins by LoadingPhase, then topological sort within each phase.
    void sortPlugins();

    World m_world;
    Scheduler m_scheduler;
    DynamicArray<IPlugin*> m_plugins;
    bool m_running = false;
    bool m_built = false;
};

} // namespace Entelechy
