#include "ecs/world/app.h"
#include "log/core/log_macros.h"
#include "core/container/hash_map.h"
#include "core/allocator/allocator.h"
#include <memory>

namespace Entelechy {

App::App() {
}

App::~App() {
    teardown();
    for (IPlugin* plugin : m_plugins) {
        plugin->~IPlugin();
        DefaultAllocator::free(plugin);
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
                plugin->~IPlugin();
                DefaultAllocator::free(plugin);
                return;
            }
        }
    }

    m_plugins.pushBack(plugin);
    LOG_INFO(LogCategories::kEngine, "Plugin registered: %s", plugin->name());
}

void App::sortPlugins() {
    usize n = m_plugins.size();
    if (n <= 1) return;

    // Build name -> original index map for dependency resolution.
    HashMap<String, usize> nameToIndex;
    for (usize i = 0; i < n; ++i) {
        nameToIndex.insert(String(m_plugins[i]->name()), i);
    }

    // Group plugins by LoadingPhase.
    struct PhaseGroup {
        u8 phaseIndex = 0;
        DynamicArray<usize> indices;
    };
    DynamicArray<PhaseGroup> phaseGroups;

    for (usize i = 0; i < n; ++i) {
        u8 phase = static_cast<u8>(m_plugins[i]->phase());
        bool found = false;
        for (auto& g : phaseGroups) {
            if (g.phaseIndex == phase) {
                g.indices.pushBack(i);
                found = true;
                break;
            }
        }
        if (!found) {
            PhaseGroup g;
            g.phaseIndex = phase;
            g.indices.pushBack(i);
            phaseGroups.pushBack(std::move(g));
        }
    }

    // Sort phase groups by phase index (bubble sort, N is tiny).
    for (usize i = 0; i < phaseGroups.size(); ++i) {
        for (usize j = i + 1; j < phaseGroups.size(); ++j) {
            if (phaseGroups[j].phaseIndex < phaseGroups[i].phaseIndex) {
                PhaseGroup tmp = std::move(phaseGroups[i]);
                phaseGroups[i] = std::move(phaseGroups[j]);
                phaseGroups[j] = std::move(tmp);
            }
        }
    }

    // Topological sort within each phase group.
    DynamicArray<IPlugin*> sorted;
    for (auto& group : phaseGroups) {
        auto& idx = group.indices;
        usize gn = idx.size();
        if (gn <= 1) {
            for (usize i : idx) sorted.pushBack(m_plugins[i]);
            continue;
        }

        // Map original index -> local 0..gn-1 for this group.
        HashMap<usize, usize> localIndex;
        for (usize li = 0; li < gn; ++li) {
            localIndex.insert(idx[li], li);
        }

        DynamicArray<DynamicArray<usize>> adj;
        adj.resize(gn);
        DynamicArray<usize> inDegree;
        inDegree.resize(gn, 0);

        for (usize li = 0; li < gn; ++li) {
            IPlugin* p = m_plugins[idx[li]];
            for (const auto& depName : p->dependencies()) {
                auto* depGlobalIdx = nameToIndex.find(depName);
                if (!depGlobalIdx) {
                    LOG_WARN(LogCategories::kEngine,
                        "Plugin '%s' declares dependency '%s' which is not registered",
                        p->name(), depName.c_str());
                    continue;
                }
                auto* depLocalIdx = localIndex.find(*depGlobalIdx);
                if (!depLocalIdx) {
                    // Dependency is in a different phase; ordering is already
                    // guaranteed by phase grouping. Skip edge.
                    continue;
                }
                // Edge: dep -> p (dependency must build before this plugin)
                adj[*depLocalIdx].pushBack(li);
                inDegree[li]++;
            }
        }

        // Kahn's algorithm.
        DynamicArray<usize> queue;
        for (usize i = 0; i < gn; ++i) {
            if (inDegree[i] == 0) queue.pushBack(i);
        }

        DynamicArray<usize> localSorted;
        usize front = 0;
        while (front < queue.size()) {
            usize u = queue[front++];
            localSorted.pushBack(u);
            for (usize v : adj[u]) {
                if (--inDegree[v] == 0) {
                    queue.pushBack(v);
                }
            }
        }

        if (localSorted.size() != gn) {
            LOG_WARN(LogCategories::kEngine,
                "App: cycle detected in plugin dependency graph for phase %d", group.phaseIndex);
        }

        for (usize li : localSorted) {
            sorted.pushBack(m_plugins[idx[li]]);
        }
    }

    m_plugins = std::move(sorted);
}

void App::build() {
    if (m_built) {
        LOG_WARN(LogCategories::kEngine, "App::build() called twice; ignoring.");
        return;
    }

    LOG_INFO(LogCategories::kEngine, "Building %zu plugins...", m_plugins.size());

    // 1. Sort by LoadingPhase + topological dependencies.
    sortPlugins();

    // 2. build() -- register components, systems, resources.
    for (IPlugin* plugin : m_plugins) {
        LOG_DEBUG(LogCategories::kEngine, "  Plugin build: %s (phase=%d)",
            plugin->name(), static_cast<int>(plugin->phase()));
        plugin->build(*this);
    }

    // 3. ready() -- poll until all plugins are ready (supports async init).
    bool all_ready = false;
    u32 ready_iterations = 0;
    while (!all_ready) {
        all_ready = true;
        for (IPlugin* plugin : m_plugins) {
            if (!plugin->ready(*this)) {
                all_ready = false;
                break;
            }
        }
        ++ready_iterations;
        if (ready_iterations > 10000) {
            LOG_ERROR(LogCategories::kEngine,
                "App::build() -- plugins stuck in ready() poll, aborting.");
            break;
        }
    }

    // 4. finish() -- cross-plugin configuration after all types are registered.
    for (IPlugin* plugin : m_plugins) {
        plugin->finish(*this);
    }

    m_scheduler.build();
    m_built = true;
    LOG_INFO(LogCategories::kEngine, "App build complete (%u ready iterations).", ready_iterations);
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

DynamicArray<PluginManifest> App::pluginManifests() const {
    DynamicArray<PluginManifest> manifests;
    for (IPlugin* plugin : m_plugins) {
        manifests.pushBack(plugin->manifest());
    }
    return manifests;
}

} // namespace Entelechy
