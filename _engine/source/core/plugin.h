#pragma once
#include "foundation_types.h"
#include "small_string.h"

namespace Entelechy {

class App;

// ------------------------------------------------------------------
// IPlugin -- Bevy-style plugin interface
// ------------------------------------------------------------------
// Plugins are the primary mechanism for extending engine functionality.
// Each plugin receives callbacks during three lifecycle phases:
//
//   1. build()   -- register systems, components, resources
//   2. setup()   -- one-time initialization (after all build()s)
//   3. teardown()-- cleanup before shutdown
//
// Plugins are owned by App. App takes ownership when addPlugin() is called.
// ------------------------------------------------------------------
class IPlugin {
public:
    virtual ~IPlugin() = default;

    // Called during App::build(). Register systems, component types,
    // and initial resources here.
    virtual void build(App& app) = 0;

    // Called during App::setup(), after all plugins have finished build().
    // Perform one-time initialization that may depend on other plugins.
    virtual void setup(App& app) {}

    // Called during App::teardown(). Release resources in reverse order.
    virtual void teardown(App& app) {}

    // Return true if this plugin should only be added once.
    // App will assert/ignore duplicate adds when isUnique() == true.
    virtual bool isUnique() const { return true; }

    // Human-readable name for debugging and logging.
    virtual const char* name() const = 0;
};

} // namespace Entelechy
