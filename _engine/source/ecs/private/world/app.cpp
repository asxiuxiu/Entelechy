#include "ecs/world/app.h"
#include "log/core/log_macros.h"

namespace Entelechy {

App::App() {
}

App::~App() {
    teardown();
    for (IPlugin* plugin : m_plugins) {
        delete plugin;
    }
}

void App::addPlugin(IPlugin* plugin) {
    if (!plugin) return;

    // Check uniqueness
    if (plugin->isUnique()) {
        for (IPlugin* existing : m_plugins) {
            if (std::strcmp(existing->name(), plugin->name()) == 0) {
                LOG_WARN(LogCategories::kEngine,
                    "Plugin '%s' is unique and already registered; ignoring duplicate add.",
                    plugin->name());
                delete plugin;
                return;
            }
        }
    }

    m_plugins.pushBack(plugin);
    LOG_INFO(LogCategories::kEngine, "Plugin registered: %s", plugin->name());
}

void App::build() {
    if (m_built) {
        LOG_WARN(LogCategories::kEngine, "App::build() called twice; ignoring.");
        return;
    }

    LOG_INFO(LogCategories::kEngine, "Building %zu plugins...", m_plugins.size());
    for (IPlugin* plugin : m_plugins) {
        plugin->build(*this);
    }

    m_scheduler.build();
    m_built = true;
    LOG_INFO(LogCategories::kEngine, "App build complete.");
}

void App::setup() {
    if (!m_built) {
        LOG_WARN(LogCategories::kEngine, "App::setup() called before build(); calling build() first.");
        build();
    }

    LOG_INFO(LogCategories::kEngine, "Setting up %zu plugins...", m_plugins.size());
    for (IPlugin* plugin : m_plugins) {
        plugin->setup(*this);
    }
    m_running = true;
    LOG_INFO(LogCategories::kEngine, "App setup complete.");
}

void App::update(f32 dt) {
    if (!m_built) {
        LOG_ERROR(LogCategories::kEngine, "App::update() called before build(); skipping.");
        return;
    }
    m_scheduler.tick(m_world, dt);
}

void App::teardown() {
    if (!m_built) return;

    LOG_INFO(LogCategories::kEngine, "Tearing down %zu plugins...", m_plugins.size());
    // Reverse order teardown
    for (usize i = m_plugins.size(); i > 0; --i) {
        m_plugins[i - 1]->teardown(*this);
    }
    m_running = false;
    m_built = false;
    LOG_INFO(LogCategories::kEngine, "App teardown complete.");
}

} // namespace Entelechy
