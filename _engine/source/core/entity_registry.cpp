#include "entity_registry.h"

namespace Entelechy {

Entity EntityRegistry::create() {
    Entity e;
    if (!m_freeList.empty()) {
        e.id = m_freeList.back();
        m_freeList.popBack();
        e.generation = m_slots[e.id].generation;
        m_slots[e.id].alive = true;
    } else {
        e.id = static_cast<u32>(m_slots.size());
        m_slots.pushBack(Slot{0, true});
        e.generation = 0;
    }
    return e;
}

DynamicArray<Entity> EntityRegistry::allocateIDs(usize count) {
    DynamicArray<Entity> result;
    result.reserve(count);
    for (usize i = 0; i < count; ++i) {
        result.pushBack(create());
    }
    return result;
}

void EntityRegistry::destroy(Entity e) {
    if (!isAlive(e)) return;
    m_slots[e.id].alive = false;
    m_slots[e.id].generation++;
    m_freeList.pushBack(e.id);
}

bool EntityRegistry::isAlive(Entity e) const {
    return e.id < m_slots.size() && m_slots[e.id].generation == e.generation && m_slots[e.id].alive;
}

u32 EntityRegistry::getGeneration(u32 id) const {
    if (id < m_slots.size()) return m_slots[id].generation;
    return 0xFFFFFFFFu;
}

usize EntityRegistry::aliveCount() const {
    return m_slots.size() - m_freeList.size();
}

usize EntityRegistry::capacity() const {
    return m_slots.size();
}

void EntityRegistry::clear() {
    m_slots.clear();
    m_freeList.clear();
}

} // namespace Entelechy
