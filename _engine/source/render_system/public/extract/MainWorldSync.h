#pragma once
#include "ecs/type/entity_registry.h"
#include "core/container/hash_map.h"

namespace Entelechy
{

// MainWorldSync — bidirectional entity mapping between main world and render world.
// AI-observable: given a main-world entity, you can ask which render-world entity it became.
struct MainWorldSync
{
    HashMap<Entity, Entity> main_to_render;
    HashMap<Entity, Entity> render_to_main;

    void clear()
    {
        main_to_render.clear();
        render_to_main.clear();
    }

    void map(Entity main, Entity render)
    {
        main_to_render.insert(main, render);
        render_to_main.insert(render, main);
    }
};

} // namespace Entelechy
