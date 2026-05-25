#pragma once
#include "world.h"
#include "type_registry.h"
#include <tuple>
#include <iterator>
#include <type_traits>

namespace Entelechy {

// QueryImpl — unified implementation for both mutable and const world queries.
// WorldT = World        → Query<Cs...>     (mutable, returns Cs*)
// WorldT = const World  → ConstQuery<Cs...> (read-only, returns const Cs*)
template<typename WorldT, typename... Cs>
    requires (sizeof...(Cs) > 0)
class QueryImpl {
    using FirstC = std::tuple_element_t<0, std::tuple<Cs...>>;
    using PrimaryArrayPtr = std::conditional_t<std::is_const_v<WorldT>,
        const ComponentArray<FirstC>*,
        ComponentArray<FirstC>*>;

    template<typename T>
    using MaybeConstPtr = std::conditional_t<std::is_const_v<WorldT>, const T*, T*>;

public:
    explicit QueryImpl(WorldT& world) : m_world(&world) {
        m_primaryArray = world.template getComponentArray<FirstC>();
    }

    struct Iterator {
        using iterator_category = std::input_iterator_tag;
        using value_type = std::tuple<Entity, MaybeConstPtr<Cs>...>;
        using difference_type = isize;
        using pointer = value_type*;
        using reference = value_type;

        WorldT* world;
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
            return std::make_tuple(e, world->template getComponent<Cs>(e)...);
        }
    };

    Iterator begin() const {
        u32 requiredMask = (TypeRegistry::instance().getMask<Cs>() | ...);
        if (!m_primaryArray || m_primaryArray->count() == 0) {
            return end();
        }
        Iterator it{m_world, m_primaryArray->entityIds(), 0, m_primaryArray->count(), requiredMask};
        it.advanceToNextValid();
        return it;
    }

    Iterator end() const {
        u32 requiredMask = (TypeRegistry::instance().getMask<Cs>() | ...);
        usize count = m_primaryArray ? m_primaryArray->count() : 0;
        return Iterator{m_world, nullptr, count, count, requiredMask};
    }

private:
    WorldT* m_world;
    PrimaryArrayPtr m_primaryArray = nullptr;
};

template<typename... Cs>
using Query = QueryImpl<World, Cs...>;

template<typename... Cs>
using ConstQuery = QueryImpl<const World, Cs...>;

} // namespace Entelechy
