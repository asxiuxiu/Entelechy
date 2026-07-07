#include "ecs/world/scheduler.h"
#include "log/core/log_macros.h"
#include "core/string/string_intern_pool.h"

namespace Entelechy
{

void Scheduler::registerSystem(System *system)
{
    SystemDesc desc;
    desc.system = system;
    desc.phase = static_cast<u8>(DefaultPhase::Update);
    m_systems.pushBack(std::move(desc));
}

void Scheduler::registerSystem(const SystemDesc &desc)
{
    m_systems.pushBack(desc);
}

void Scheduler::build()
{
    m_phase_groups.clear();

    // Group systems by phase index.
    for (usize i = 0; i < m_systems.size(); ++i)
    {
        SystemDesc *desc = &m_systems[i];
        bool found = false;
        for (auto &group : m_phase_groups)
        {
            if (group.phaseIndex == desc->phase)
            {
                group.systems.pushBack(desc);
                found = true;
                break;
            }
        }
        if (!found)
        {
            PhaseGroup group;
            group.phaseIndex = desc->phase;
            group.systems.pushBack(desc);
            m_phase_groups.pushBack(std::move(group));
        }
    }

    // Sort phase groups by phase index (bubble sort, N is tiny).
    for (usize i = 0; i < m_phase_groups.size(); ++i)
    {
        for (usize j = i + 1; j < m_phase_groups.size(); ++j)
        {
            if (m_phase_groups[j].phaseIndex < m_phase_groups[i].phaseIndex)
            {
                PhaseGroup tmp = std::move(m_phase_groups[i]);
                m_phase_groups[i] = std::move(m_phase_groups[j]);
                m_phase_groups[j] = std::move(tmp);
            }
        }
    }

    // Topological sort within each phase group (Kahn's algorithm).
    for (auto &group : m_phase_groups)
    {
        auto &sys = group.systems;
        usize n = sys.size();
        if (n <= 1)
            continue;

        // Name -> index map for this group.
        HashMap<StringId, usize> nameToIndex;
        for (usize i = 0; i < n; ++i)
        {
            if (sys[i]->name.value() != 0)
            {
                nameToIndex.insert(sys[i]->name, i);
            }
        }

        // Build adjacency list and in-degree.
        DynamicArray<DynamicArray<usize>> adj;
        adj.resize(n);
        DynamicArray<usize> inDegree;
        inDegree.resize(n, 0);

        for (usize i = 0; i < n; ++i)
        {
            for (const auto &beforeName : sys[i]->before)
            {
                auto *idx = nameToIndex.find(beforeName);
                if (idx)
                {
                    // i must run before *idx  => edge i -> *idx
                    adj[i].pushBack(*idx);
                    inDegree[*idx]++;
                }
            }
            for (const auto &afterName : sys[i]->after)
            {
                auto *idx = nameToIndex.find(afterName);
                if (idx)
                {
                    // i must run after *idx  => edge *idx -> i
                    adj[*idx].pushBack(i);
                    inDegree[i]++;
                }
            }
        }

        // Kahn — use index front instead of removeAt(0) for O(1) dequeue.
        DynamicArray<usize> queue;
        for (usize i = 0; i < n; ++i)
        {
            if (inDegree[i] == 0)
                queue.pushBack(i);
        }

        DynamicArray<SystemDesc *> sorted;
        usize front = 0;
        while (front < queue.size())
        {
            usize u = queue[front++];
            sorted.pushBack(sys[u]);
            for (usize vIdx = 0; vIdx < adj[u].size(); ++vIdx)
            {
                usize v = adj[u][vIdx];
                if (--inDegree[v] == 0)
                {
                    queue.pushBack(v);
                }
            }
        }

        if (sorted.size() != n)
        {
            LOG_WARN(Entelechy::LogCategories::kEngine, "Scheduler: cycle detected in phase %d dependency graph",
                     group.phaseIndex);
        }

        sys = std::move(sorted);
    }

    detectAmbiguities();
    m_built = true;
}

void Scheduler::detectAmbiguities()
{
    for (const auto &group : m_phase_groups)
    {
        const auto &sys = group.systems;
        usize n = sys.size();
        for (usize i = 0; i < n; ++i)
        {
            for (usize j = i + 1; j < n; ++j)
            {
                // Check data conflict: any write intersects with other's read/write.
                bool conflict = false;
                for (usize wi = 0; wi < sys[i]->writes.size() && !conflict; ++wi)
                {
                    for (usize wj = 0; wj < sys[j]->writes.size(); ++wj)
                    {
                        if (sys[i]->writes[wi] == sys[j]->writes[wj])
                        {
                            conflict = true;
                            break;
                        }
                    }
                }
                if (!conflict)
                {
                    for (usize wi = 0; wi < sys[i]->writes.size() && !conflict; ++wi)
                    {
                        for (usize rj = 0; rj < sys[j]->reads.size(); ++rj)
                        {
                            if (sys[i]->writes[wi] == sys[j]->reads[rj])
                            {
                                conflict = true;
                                break;
                            }
                        }
                    }
                }
                if (!conflict)
                {
                    for (usize wj = 0; wj < sys[j]->writes.size() && !conflict; ++wj)
                    {
                        for (usize ri = 0; ri < sys[i]->reads.size(); ++ri)
                        {
                            if (sys[j]->writes[wj] == sys[i]->reads[ri])
                            {
                                conflict = true;
                                break;
                            }
                        }
                    }
                }
                if (!conflict)
                    continue;

                // Check if already ordered by before/after.
                bool ordered = false;
                for (const auto &b : sys[i]->before)
                {
                    if (b == sys[j]->name)
                    {
                        ordered = true;
                        break;
                    }
                }
                if (!ordered)
                {
                    for (const auto &a : sys[i]->after)
                    {
                        if (a == sys[j]->name)
                        {
                            ordered = true;
                            break;
                        }
                    }
                }
                if (!ordered)
                {
                    for (const auto &b : sys[j]->before)
                    {
                        if (b == sys[i]->name)
                        {
                            ordered = true;
                            break;
                        }
                    }
                }
                if (!ordered)
                {
                    for (const auto &a : sys[j]->after)
                    {
                        if (a == sys[i]->name)
                        {
                            ordered = true;
                            break;
                        }
                    }
                }

                if (!ordered)
                {
                    const char *nameA =
                        sys[i]->name.value() == 0 ? "<unnamed>" : StringInternPool::instance().resolve(sys[i]->name);
                    const char *nameB =
                        sys[j]->name.value() == 0 ? "<unnamed>" : StringInternPool::instance().resolve(sys[j]->name);
                    LOG_WARN(Entelechy::LogCategories::kEngine,
                             "Scheduler Ambiguity: '%s' and '%s' conflict on component data but have no ordering "
                             "dependency in phase %d",
                             nameA, nameB, group.phaseIndex);
                }
            }
        }
    }
}

void Scheduler::tick(World &world, f32 raw_dt)
{
    m_accumulator += raw_dt;
    while (m_accumulator >= FIXED_DT)
    {
        tickFixed(world, FIXED_DT);
        m_accumulator -= FIXED_DT;
        ++m_current_frame;
    }
}

void Scheduler::tickOnce(World &world, f32 dt)
{
    tickFixed(world, dt);
    ++m_current_frame;
}

void Scheduler::tickFixed(World &world, f32 dt)
{
    if (!m_built)
        build();

    world.setCurrentFrame(m_current_frame);
    m_frame_arena.reset();
    for (auto &group : m_phase_groups)
    {
        for (auto *desc : group.systems)
        {
            if (desc->system)
            {
                desc->system->tick(world, m_frame_arena, dt);
            }
        }
    }
    m_command_buffer.apply(world);
}

FrameArena &Scheduler::frameArena()
{
    return m_frame_arena;
}

CommandBuffer &Scheduler::commandBuffer()
{
    return m_command_buffer;
}

} // namespace Entelechy
