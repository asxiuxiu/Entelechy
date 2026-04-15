#pragma once
#include "world.h"
#include "dynamic_array.h"
#include <memory>

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
            world.addComponent<T>(entity, value);
        }
    };

    template<typename T>
    struct RemoveCommand : ICommand {
        Entity entity;
        RemoveCommand(Entity e) : entity(e) {}
        void apply(World& world) override {
            world.removeComponent<T>(entity);
        }
    };

    struct DestroyCommand : ICommand {
        Entity entity;
        DestroyCommand(Entity e) : entity(e) {}
        void apply(World& world) override {
            world.destroy(entity);
        }
    };

public:
    template<typename T>
    void set(Entity e, const T& value) {
        m_commands.pushBack(std::make_unique<SetCommand<T>>(e, value));
    }

    template<typename T>
    void remove(Entity e) {
        m_commands.pushBack(std::make_unique<RemoveCommand<T>>(e));
    }

    void destroy(Entity e) {
        m_commands.pushBack(std::make_unique<DestroyCommand>(e));
    }

    void apply(World& world) {
        for (auto& cmd : m_commands) {
            cmd->apply(world);
        }
        m_commands.clear();
    }

    void clear() {
        m_commands.clear();
    }

    bool empty() const {
        return m_commands.empty();
    }

private:
    DynamicArray<std::unique_ptr<ICommand>> m_commands;
};

} // namespace Entelechy
