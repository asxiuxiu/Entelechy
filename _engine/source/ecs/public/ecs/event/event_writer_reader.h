#pragma once
#include "core/foundation_types.h"
#include "ecs/world/world.h"
#include "ecs/query/query.h"

namespace Entelechy {

// ------------------------------------------------------------------
// EventWriter<T> -- type-safe event emission API
// ------------------------------------------------------------------
// Wraps the ECS event-entity pattern: instead of manually spawning
// an entity and attaching an event component, EventWriter provides
// a single `emit(event)` call.
//
// Usage:
//   void tick(World& w, FrameArena&, f32) {
//       EventWriter<KeyboardEvent> writer(w);
//       writer.emit(KeyboardEvent{32, true});
//   }
// ------------------------------------------------------------------
template<typename T>
class EventWriter {
public:
    explicit EventWriter(World& world) : m_world(world) {}

    // Emit a new event by spawning an entity with the event component.
    [[nodiscard]] Entity emit(const T& event) {
        Entity e = m_world.spawn();
        m_world.addComponent<T>(e, event);
        if constexpr (requires { EventLifetime{}; }) {
            m_world.addComponent<EventLifetime>(e, EventLifetime{});
        }
        return e;
    }

    // Convenience: emit with EventLifetime attached (if event type supports it).
    [[nodiscard]] Entity emit(const T& event, const EventLifetime& lifetime) {
        Entity e = m_world.spawn();
        m_world.addComponent<T>(e, event);
        m_world.addComponent<EventLifetime>(e, lifetime);
        return e;
    }

private:
    World& m_world;
};

// ------------------------------------------------------------------
// EventReader<T> -- type-safe event consumption API
// ------------------------------------------------------------------
// Wraps Query<T> to provide iterator-style or batch-drain access to
// events of a specific type. Automatically handles the common case of
// reading all events spawned since the last read.
//
// Usage:
//   void tick(World& w, FrameArena&, f32) {
//       EventReader<KeyboardEvent> reader(w);
//       for (const auto& evt : reader.read()) {
//           if (evt.pressed) { ... }
//       }
//   }
// ------------------------------------------------------------------
template<typename T>
class EventReader {
public:
    explicit EventReader(World& world) : m_world(world) {}

    // Read all current events of type T.
    // Returns a lightweight iterable view (not owning storage).
    // Note: events are entities; this iterates the component array.
    auto read() {
        return Query<T>(m_world);
    }

    // Drain all current events of type T, invoking callback for each.
    // Events are NOT destroyed; use EventCleanupSystem or manual destroy.
    template<typename F>
    void drain(F&& callback) {
        for (auto [e, evt] : Query<T>(m_world)) {
            if (evt) {
                callback(*evt);
            }
        }
    }

    // Drain and return a copy of all event values into a FrameArena-backed array.
    template<typename F>
    void drainWithEntity(F&& callback) {
        for (auto [e, evt] : Query<T>(m_world)) {
            if (evt) {
                callback(e, *evt);
            }
        }
    }

    // Check if any events of this type are pending.
    [[nodiscard]] bool isEmpty() const {
        auto* array = m_world.getComponentArray<T>();
        return !array || array->size() == 0;
    }

private:
    World& m_world;
};

} // namespace Entelechy
