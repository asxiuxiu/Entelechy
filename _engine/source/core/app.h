#pragma once
#include "plugin.h"
#include "world.h"
#include "scheduler.h"
#include "dynamic_array.h"
#include <type_traits>
#include <utility>

namespace Entelechy {

// ------------------------------------------------------------------
// App -- top-level application container
// ------------------------------------------------------------------
// Owns World, Scheduler, and all registered plugins.
// Usage:
//   App app;
//   app.addPlugin(new MyPlugin());
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
        T* ptr = new T(std::forward<Args>(args)...);
        addPlugin(ptr);
        return *ptr;
    }

    // Build all plugins (calls plugin->build() in registration order).
    void build();

    // Setup all plugins (calls plugin->setup() in registration order).
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

private:
    World m_world;
    Scheduler m_scheduler;
    DynamicArray<IPlugin*> m_plugins;
    bool m_running = false;
    bool m_built = false;
};

} // namespace Entelechy
