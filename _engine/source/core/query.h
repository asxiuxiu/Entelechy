#pragma once
#include "world.h"
#include "type_registry.h"
#include <tuple>
#include <iterator>

namespace Entelechy {

template<typename... Cs>
    requires (sizeof...(Cs) > 0)
class Query {
public:
    explicit Query(World& world) : m_world(&world) {
        m_primaryArray = world.getComponentArray<std::tuple_element_t<0, std::tuple<Cs...>>>();
    }

    struct Iterator {
        using iterator_category = std::input_iterator_tag;
        using value_type = std::tuple<Entity, Cs*...>;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type;

        World* world;
        const u32* ids;
        usize index;
        usize count;
        u32 requiredMask;

        void advanceToNextValid() {
            while (index < count) {
                u32 id = ids[index];
                if ((world->getEntityMask(id) & requiredMask) == requiredMask) {
                    break;
                }
                ++index;
            }
        }

        bool operator==(const Iterator& other) const {
            return index == other.index;
        }

        bool operator!=(const Iterator& other) const {
            return index != other.index;
        }

        void operator++() {
            ++index;
            advanceToNextValid();
        }

        value_type operator*() const {
            Entity e{ids[index], world->getEntityGeneration(ids[index])};
            return std::make_tuple(e, world->getComponent<Cs>(e)...);
        }
    };

    Iterator begin() {
        u32 requiredMask = (TypeRegistry::instance().getMask<Cs>() | ...);
        if (!m_primaryArray || m_primaryArray->count() == 0) {
            return end();
        }
        Iterator it{m_world, m_primaryArray->entityIds(), 0, m_primaryArray->count(), requiredMask};
        it.advanceToNextValid();
        return it;
    }

    Iterator end() {
        u32 requiredMask = (TypeRegistry::instance().getMask<Cs>() | ...);
        usize count = m_primaryArray ? m_primaryArray->count() : 0;
        return Iterator{m_world, nullptr, count, count, requiredMask};
    }

private:
    World* m_world;
    ComponentArray<std::tuple_element_t<0, std::tuple<Cs...>>>* m_primaryArray = nullptr;
};

} // namespace Entelechy
