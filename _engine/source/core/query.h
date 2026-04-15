#pragma once
#include "world.h"
#include "type_registry.h"
#include <tuple>
#include <cstdint>

namespace Entelechy {

template<typename... Cs>
class Query {
public:
    explicit Query(World& world) : m_world(&world) {
        m_primaryArray = world.getComponentArray<std::tuple_element_t<0, std::tuple<Cs...>>>();
    }

    struct Iterator {
        World* world;
        const uint32_t* ids;
        size_t index;
        size_t count;
        uint32_t requiredMask;

        void advanceToNextValid() {
            while (index < count) {
                uint32_t id = ids[index];
                if ((world->getEntityMask(id) & requiredMask) == requiredMask) {
                    break;
                }
                ++index;
            }
        }

        bool operator!=(const Iterator& other) const {
            return index != other.index;
        }

        void operator++() {
            ++index;
            advanceToNextValid();
        }

        std::tuple<Entity, Cs*...> operator*() const {
            Entity e{ids[index], world->getEntityGeneration(ids[index])};
            return std::make_tuple(e, world->getComponent<Cs>(e)...);
        }
    };

    Iterator begin() {
        uint32_t requiredMask = (TypeRegistry::instance().getMask<Cs>() | ...);
        if (!m_primaryArray || m_primaryArray->count() == 0) {
            return end();
        }
        Iterator it{m_world, m_primaryArray->entityIds(), 0, m_primaryArray->count(), requiredMask};
        it.advanceToNextValid();
        return it;
    }

    Iterator end() {
        uint32_t requiredMask = (TypeRegistry::instance().getMask<Cs>() | ...);
        size_t count = m_primaryArray ? m_primaryArray->count() : 0;
        return Iterator{m_world, nullptr, count, count, requiredMask};
    }

private:
    World* m_world;
    ComponentArray<std::tuple_element_t<0, std::tuple<Cs...>>>* m_primaryArray = nullptr;
};

} // namespace Entelechy
