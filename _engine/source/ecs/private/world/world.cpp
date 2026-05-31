#include "ecs/world/world.h"
#include "ecs/world/command_buffer.h"
#include "ecs/type/type_registry.h"
#include "core/math/mat4.h"
#include "ecs/component/transform_component.h"
#include "ecs/hierarchy/hierarchy.h"
#include "core/allocator/allocator.h"
#include <cstdio>
#include <memory>

namespace Entelechy {

World::World() : m_owns_registry(true) {
    m_registry = static_cast<EntityRegistry*>(DefaultAllocator::alloc(sizeof(EntityRegistry), alignof(EntityRegistry)));
    std::construct_at(m_registry);
}

World::World(EntityRegistry& registry) : m_registry(&registry), m_owns_registry(false) {}

World::~World() {
    for (auto pair : m_component_arrays) {
        std::destroy_at(pair.second);
        DefaultAllocator::free(pair.second);
    }
    if (m_owns_registry) {
        std::destroy_at(m_registry);
        DefaultAllocator::free(m_registry);
    }
}

DynamicArray<Entity> World::spawnBatch(usize count) {
    CHECK(m_registry != nullptr);
    DynamicArray<Entity> entities = m_registry->allocateIDs(count);
    u32 maxID = 0;
    for (const auto& e : entities) {
        if (e.id > maxID) maxID = e.id;
    }
    if (maxID >= m_entity_masks.size()) {
        m_entity_masks.resize(maxID + 1, 0);
    }
    if (maxID >= m_entity_ranks.size()) {
        m_entity_ranks.resize(maxID + 1, 0);
    }
    for (const auto& e : entities) {
        m_entity_masks[e.id] = 0;
        m_entity_ranks[e.id] = 0;
    }
    return entities;
}

void World::destroy(Entity e) {
    if (!valid(e)) return;
    if (m_cmd_buffer) {
        m_cmd_buffer->destroy(e);
    } else {
        destroyImmediate(e);
    }
}

void World::destroyImmediate(Entity e) {
    if (!valid(e)) return;
    for (auto pair : m_component_arrays) {
        pair.second->remove(e);
    }
    m_entity_masks[e.id] = 0;
    if (e.id < m_entity_ranks.size()) {
        m_entity_ranks[e.id] = 0;
    }
    m_registry->destroy(e);
}

void World::setParent(Entity child, Entity parent) {
    if (!valid(child)) return;
    if (m_cmd_buffer) {
        m_cmd_buffer->setParent(child, parent);
    } else {
        setParentImmediate(child, parent);
    }
}

u32 World::getRank(Entity e) const {
    if (e.id < m_entity_ranks.size()) return m_entity_ranks[e.id];
    return 0;
}

void World::setRank(Entity e, u32 rank) {
    if (e.id >= m_entity_ranks.size()) {
        m_entity_ranks.resize(e.id + 1, 0);
    }
    m_entity_ranks[e.id] = rank;
}

void World::setParentImmediate(Entity child, Entity parent) {
    if (!valid(child)) return;
    if (parent.valid() && !valid(parent)) return;
    if (parent.valid() && isDescendant(*this, child, parent)) return; // cycle guard

    // Detach from old parent
    ChildOf* oldChildOf = getComponent<ChildOf>(child);
    if (oldChildOf && oldChildOf->parent.valid()) {
        Children* oldSiblings = getComponent<Children>(oldChildOf->parent);
        if (oldSiblings) {
            for (usize i = 0; i < oldSiblings->entities.size(); ++i) {
                if (oldSiblings->entities[i] == child) {
                    oldSiblings->entities.removeAt(i);
                    break;
                }
            }
            if (oldSiblings->entities.empty()) {
                removeComponent<Children>(oldChildOf->parent);
            }
        }
        removeComponent<ChildOf>(child);
    }

    // Attach to new parent
    if (parent.valid()) {
        addComponent(child, ChildOf{parent});
        Children* siblings = getComponent<Children>(parent);
        if (!siblings) {
            siblings = addComponent(parent, Children{});
        }
        siblings->entities.pushBack(child);

        // Update ranks for the moved subtree
        u32 newRank = getRank(parent) + 1;
        setRank(child, newRank);

        DynamicArray<Entity> toProcess;
        toProcess.pushBack(child);
        for (usize i = 0; i < toProcess.size(); ++i) {
            Entity current = toProcess[i];
            u32 currentRank = getRank(current);
            const Children* childrenComp = getComponent<Children>(current);
            if (childrenComp) {
                for (Entity c : childrenComp->entities) {
                    setRank(c, currentRank + 1);
                    toProcess.pushBack(c);
                }
            }
        }
    } else {
        setRank(child, 0);
    }
}

static bool decomposeTRS(const Mat4& m, Vec3& outT, Quat& outR, Vec3& outS) {
    outT = Vec3{m.m[12], m.m[13], m.m[14]};

    Vec3 col0(m.m[0], m.m[1], m.m[2]);
    Vec3 col1(m.m[4], m.m[5], m.m[6]);
    Vec3 col2(m.m[8], m.m[9], m.m[10]);

    outS = Vec3{col0.length(), col1.length(), col2.length()};

    if (outS.x < 1e-6f) outS.x = 1e-6f;
    if (outS.y < 1e-6f) outS.y = 1e-6f;
    if (outS.z < 1e-6f) outS.z = 1e-6f;

    col0 = col0 / outS.x;
    col1 = col1 / outS.y;
    col2 = col2 / outS.z;

    // Matrix to quaternion (standard algorithm)
    f32 trace = col0.x + col1.y + col2.z;
    if (trace > 0.0f) {
        f32 s = 0.5f / std::sqrt(trace + 1.0f);
        outR.w = 0.25f / s;
        outR.x = (col1.z - col2.y) * s;
        outR.y = (col2.x - col0.z) * s;
        outR.z = (col0.y - col1.x) * s;
    } else if (col0.x > col1.y && col0.x > col2.z) {
        f32 s = 2.0f * std::sqrt(1.0f + col0.x - col1.y - col2.z);
        outR.w = (col1.z - col2.y) / s;
        outR.x = 0.25f * s;
        outR.y = (col1.x + col0.y) / s;
        outR.z = (col2.x + col0.z) / s;
    } else if (col1.y > col2.z) {
        f32 s = 2.0f * std::sqrt(1.0f + col1.y - col0.x - col2.z);
        outR.w = (col2.x - col0.z) / s;
        outR.x = (col1.x + col0.y) / s;
        outR.y = 0.25f * s;
        outR.z = (col2.y + col1.z) / s;
    } else {
        f32 s = 2.0f * std::sqrt(1.0f + col2.z - col0.x - col1.y);
        outR.w = (col0.y - col1.x) / s;
        outR.x = (col2.x + col0.z) / s;
        outR.y = (col2.y + col1.z) / s;
        outR.z = 0.25f * s;
    }

    outR = outR.normalized();
    return true;
}

void World::setParentInPlace(Entity child, Entity parent) {
    if (!valid(child)) return;
    if (parent.valid() && !valid(parent)) return;

    // Capture current world transform
    GlobalTransform* childGlobal = getComponent<GlobalTransform>(child);
    if (!childGlobal) {
        // No world transform to preserve; fall back to normal attach
        setParentImmediate(child, parent);
        return;
    }
    Mat4 oldWorld = childGlobal->matrix;

    // Detach / re-attach
    setParentImmediate(child, parent);

    // Recompute local transform so world transform stays the same
    Mat4 parentWorld = Mat4::identity();
    if (parent.valid()) {
        if (GlobalTransform* pg = getComponent<GlobalTransform>(parent)) {
            parentWorld = pg->matrix;
        }
    }

    Mat4 localMatrix = parentWorld.inverse() * oldWorld;

    Vec3 newT, newS;
    Quat newR;
    decomposeTRS(localMatrix, newT, newR, newS);

    Transform* trans = getComponent<Transform>(child);
    if (trans) {
        trans->translation = newT;
        trans->rotation = newR;
        trans->scale = newS;
        trans->dirty = 1;
    }
}

void World::addComponentRaw(Entity e, ComponentTypeID type, const void* data) {
    auto* v = m_component_arrays.find(type);
    if (v) {
        (*v)->setRaw(e, data);
    } else {
        CHECK(false && "Component array not found for type-erased addComponentRaw");
    }
    const ComponentDesc* desc = TypeRegistry::instance().findComponent(type);
    if (desc) {
        u64 mask = TypeRegistry::instance().getComponentMask(desc->name);
        m_entity_masks[e.id] |= mask;
    }
}

void World::removeComponentRaw(Entity e, ComponentTypeID type) {
    auto* v = m_component_arrays.find(type);
    if (v) {
        (*v)->remove(e);
    }
    const ComponentDesc* desc = TypeRegistry::instance().findComponent(type);
    if (desc) {
        u64 mask = TypeRegistry::instance().getComponentMask(desc->name);
        m_entity_masks[e.id] &= ~mask;
    }
}

const void* World::getComponentRaw(Entity e, ComponentTypeID type) const {
    auto* v = m_component_arrays.find(type);
    if (!v) return nullptr;
    return (*v)->getRaw(e);
}

void* World::getComponentRaw(Entity e, ComponentTypeID type) {
    auto* v = m_component_arrays.find(type);
    if (!v) return nullptr;
    return (*v)->getRaw(e);
}

bool World::hasComponentRaw(Entity e, ComponentTypeID type) const {
    auto* v = m_component_arrays.find(type);
    if (!v) return false;
    return (*v)->has(e);
}

void World::collectDescendants(Entity e, DynamicArray<Entity>& out) const {
    const Children* children = getComponent<Children>(e);
    if (!children) return;
    for (const auto& child : children->entities) {
        out.pushBack(child);
        collectDescendants(child, out);
    }
}

void initCore() {
    printf("[Entelechy::core] initialized\n");
    TypeRegistry::instance().registerBuiltinTypes();
}

void printWorld(const World& world) {
    printf("=== World State (entities: %zu) ===\n", world.entityCount());
    for (u32 id = 0; id < world.maxEntityID(); ++id) {
        Entity e{id, world.getEntityGeneration(id)};
        if (world.valid(e)) {
            const auto* pos = world.getComponent<Position>(e);
            const auto* vel = world.getComponent<Velocity>(e);
            const auto* health = world.getComponent<Health>(e);
            printf("Entity %u: ", id);
            if (pos) printf("Position(%.1f, %.1f) ", pos->x, pos->y);
            if (vel) printf("Velocity(%.1f, %.1f) ", vel->vx, vel->vy);
            if (health) printf("Health(%.1f) ", health->hp);
            printf("\n");
        }
    }
    printf("\n");
}

} // namespace Entelechy
