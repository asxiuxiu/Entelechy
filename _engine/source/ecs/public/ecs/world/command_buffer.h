#pragma once
#include "ecs/world/world.h"
#include "core/container/dynamic_array.h"
#include <cstring>

namespace Entelechy {

class CommandBuffer {
    struct ICommand {
        virtual ~ICommand() = default;
        virtual void apply(World& world) = 0;
    };

    template<typename T>
    struct SetCommand : ICommand {
        Entity entity;
        T value;
        SetCommand(Entity e, const T& v) : entity(e), value(v) {}
        void apply(World& world) override {
            world.addComponentRaw(entity, TypeRegistry::instance().getTypeID<T>(), &value);
        }
    };

    template<typename T>
    struct RemoveCommand : ICommand {
        Entity entity;
        RemoveCommand(Entity e) : entity(e) {}
        void apply(World& world) override {
            world.removeComponentRaw(entity, TypeRegistry::instance().getTypeID<T>());
        }
    };

    struct DestroyCommand : ICommand {
        Entity entity;
        DestroyCommand(Entity e) : entity(e) {}
        void apply(World& world) override {
            world.destroyImmediate(entity);
        }
    };

    struct DestroyWithChildrenCommand : ICommand {
        Entity entity;
        DestroyWithChildrenCommand(Entity e) : entity(e) {}
        void apply(World& world) override {
            DynamicArray<Entity> toDestroy;
            struct Collector {
                World& w;
                DynamicArray<Entity>& out;
                void collect(Entity e) {
                    const Children* children = w.getComponent<Children>(e);
                    if (children) {
                        for (const auto& child : children->entities) {
                            collect(child);
                            out.pushBack(child);
                        }
                    }
                }
            };
            Collector{world, toDestroy}.collect(entity);
            for (const auto& e : toDestroy) {
                world.destroyImmediate(e);
            }
            world.destroyImmediate(entity);
        }
    };

    struct SetParentCommand : ICommand {
        Entity child;
        Entity parent;
        SetParentCommand(Entity c, Entity p) : child(c), parent(p) {}
        void apply(World& world) override {
            world.setParentImmediate(child, parent);
        }
    };

    struct BatchRange {
        usize start;
        usize end;
    };

public:
    using BatchID = u32;

    CommandBuffer() = default;
    ~CommandBuffer() { clear(); }

    template<typename T>
    void set(Entity e, const T& value) {
        void* mem = DefaultAllocator::alloc(sizeof(SetCommand<T>), alignof(SetCommand<T>));
        m_commands.pushBack(new (mem) SetCommand<T>(e, value));
    }

    template<typename T>
    void remove(Entity e) {
        void* mem = DefaultAllocator::alloc(sizeof(RemoveCommand<T>), alignof(RemoveCommand<T>));
        m_commands.pushBack(new (mem) RemoveCommand<T>(e));
    }

    void destroy(Entity e) {
        void* mem = DefaultAllocator::alloc(sizeof(DestroyCommand), alignof(DestroyCommand));
        m_commands.pushBack(new (mem) DestroyCommand(e));
    }

    void destroyWithChildren(Entity e) {
        void* mem = DefaultAllocator::alloc(sizeof(DestroyWithChildrenCommand), alignof(DestroyWithChildrenCommand));
        m_commands.pushBack(new (mem) DestroyWithChildrenCommand(e));
    }

    void setParent(Entity child, Entity parent) {
        void* mem = DefaultAllocator::alloc(sizeof(SetParentCommand), alignof(SetParentCommand));
        m_commands.pushBack(new (mem) SetParentCommand(child, parent));
    }

    BatchID beginBatch() {
        BatchID id = static_cast<u32>(m_batches.size());
        m_batches.pushBack({m_commands.size(), 0});
        return id;
    }

    void endBatch(BatchID id) {
        if (id < m_batches.size()) {
            m_batches[id].end = m_commands.size();
        }
    }

    void apply(World& world) {
        for (auto* cmd : m_commands) {
            cmd->apply(world);
            cmd->~ICommand();
            DefaultAllocator::free(cmd);
        }
        m_commands.clear();
        m_batches.clear();
    }

    void clear() {
        for (auto* cmd : m_commands) {
            cmd->~ICommand();
            DefaultAllocator::free(cmd);
        }
        m_commands.clear();
        m_batches.clear();
    }

    [[nodiscard]] bool empty() const {
        return m_commands.empty();
    }

private:
    DynamicArray<ICommand*> m_commands;
    DynamicArray<BatchRange> m_batches;
};

} // namespace Entelechy
