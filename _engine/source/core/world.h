#pragma once
#include "types.h"
#include <vector>
#include <cstdio>

class World {
public:
    Entity spawn() {
        Entity e;
        if (!m_free_list.empty()) {
            e = m_free_list.back();
            m_free_list.pop_back();
            m_alive[e] = true;
        } else {
            e = m_next_id++;
            if (e >= m_positions.size()) {
                m_positions.resize(e + 1);
                m_velocities.resize(e + 1);
                m_alive.resize(e + 1);
            }
            m_alive[e] = true;
        }
        // reset components
        m_positions[e] = Position{};
        m_velocities[e] = Velocity{};
        return e;
    }

    void destroy(Entity e) {
        if (valid(e)) {
            m_alive[e] = false;
            m_free_list.push_back(e);
        }
    }

    bool valid(Entity e) const {
        return e < m_alive.size() && m_alive[e];
    }

    size_t entityCount() const {
        return m_next_id - m_free_list.size();
    }

    // direct component access (dense arrays)
    std::vector<Position> m_positions;
    std::vector<Velocity> m_velocities;
    // FIXME/TODO: std::vector<bool> 是位压缩特化容器，读写较慢且不能返回真实引用。
    // 后续若追求性能，应替换为 std::vector<uint8_t> 或 Sparse Set。
    std::vector<bool> m_alive;

private:
    Entity m_next_id = 0;
    std::vector<Entity> m_free_list;
};
