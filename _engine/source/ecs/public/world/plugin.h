#pragma once
#include "core/foundation_types.h"
#include "core/container/dynamic_array.h"
#include "core/string/string_id.h"
#include "core/string/string_intern_pool.h"

namespace Entelechy {

class App;

// ------------------------------------------------------------------
// LoadingPhase -- plugin initialization grouping
// ------------------------------------------------------------------
// Plugins in earlier phases are built before plugins in later phases.
// Within the same phase, plugins are ordered by dependency topology.
// ------------------------------------------------------------------
enum class LoadingPhase : u8 {
    Core       = 0,  // Register component types, reflection types
    Systems    = 1,  // Register systems into scheduler
    Services   = 2,  // Initialize external services (renderer, physics)
    Gameplay   = 3,  // Load scenes, spawn initial entities
};

// ------------------------------------------------------------------
// PluginManifest -- AI-observable metadata of what a plugin registered
// ------------------------------------------------------------------
struct PluginManifest {
    StringId name;
    LoadingPhase phase = LoadingPhase::Systems;
    DynamicArray<StringId> dependencies;
    // Future: registered_components, registered_systems, registered_resources
};

// ------------------------------------------------------------------
// IPlugin -- Bevy-style plugin interface with dependency management
// ------------------------------------------------------------------
// Plugins are the primary mechanism for extending engine functionality.
// Each plugin receives callbacks during lifecycle phases:
//
//   1. build()   -- register systems, components, resources
//   2. ready()   -- polled until true (for async init; default always true)
//   3. finish()  -- cross-plugin configuration after all build()s complete
//   4. setup()   -- one-time initialization (after all finish()s)
//   5. teardown()-- cleanup before shutdown (reverse order)
//
// Plugins are owned by App. App takes ownership when addPlugin() is called.
// ------------------------------------------------------------------
class IPlugin {
public:
    virtual ~IPlugin() = default;

    // Called during App::build(). Register systems, component types,
    // and initial resources here.
    virtual void build(App& app) = 0;

    // Called after all plugins' build() complete.
    // Return true when this plugin's asynchronous initialization is done.
    // App::build() will poll this until all plugins report ready.
    virtual bool ready(const App& app) const { (void)app; return true; }

    // Called after all plugins' ready() return true.
    // Use for cross-plugin configuration that requires all types registered.
    virtual void finish(App& app) { (void)app; }

    // Called during App::setup(), after all plugins have finished build/ready/finish.
    // Perform one-time initialization that may depend on other plugins.
    virtual void setup(App& app) { (void)app; }

    // Called during App::teardown(). Release resources in reverse order.
    virtual void teardown(App& app) { (void)app; }

    // Return true if this plugin should only be added once.
    // App will assert/ignore duplicate adds when isUnique() == true.
    virtual bool isUnique() const { return true; }

    // Human-readable name for debugging and logging.
    virtual const char* name() const = 0;

    // Declare plugin dependencies by name.
    // Plugins named here will be guaranteed to build() before this one
    // (if they are in the same LoadingPhase; cross-phase ordering is implicit).
    virtual DynamicArray<StringId> dependencies() const { return {}; }

    // Declare which loading phase this plugin belongs to.
    virtual LoadingPhase phase() const { return LoadingPhase::Systems; }

    // Get manifest for AI observability.
    virtual PluginManifest manifest() const {
        PluginManifest m;
        m.name = StringInternPool::instance().intern(name());
        m.phase = phase();
        m.dependencies = dependencies();
        return m;
    }
};

} // namespace Entelechy
