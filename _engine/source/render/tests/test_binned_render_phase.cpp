#include "test/test_framework.h"
#include "render/queue/BinnedRenderPhase.h"

using namespace Entelechy;

namespace {

PhaseItem makeItem(u32 materialId, Entity entity) {
    PhaseItem item{};
    item.sort_key.packed.material_id = static_cast<u16>(materialId);
    item.render_entity = entity;
    item.instance_count = 1;
    return item;
}

} // namespace

TEST(BinnedRenderPhase, GroupsItemsByMaterial) {
    BinnedRenderPhase phase;
    phase.addItem(makeItem(1, Entity{1, 0}));
    phase.addItem(makeItem(2, Entity{2, 0}));
    phase.addItem(makeItem(1, Entity{3, 0}));
    phase.addItem(makeItem(3, Entity{4, 0}));
    phase.addItem(makeItem(2, Entity{5, 0}));

    const auto& bins = phase.getBins();
    ASSERT_EQ(bins.size(), 3u);

    ASSERT_EQ(bins[0].material_id, 1u);
    ASSERT_EQ(bins[0].items.size(), 2u);
    ASSERT_EQ(bins[0].items[0].render_entity.id, 1u);
    ASSERT_EQ(bins[0].items[1].render_entity.id, 3u);

    ASSERT_EQ(bins[1].material_id, 2u);
    ASSERT_EQ(bins[1].items.size(), 2u);

    ASSERT_EQ(bins[2].material_id, 3u);
    ASSERT_EQ(bins[2].items.size(), 1u);
}

TEST(BinnedRenderPhase, PreservesFirstSeenOrder) {
    BinnedRenderPhase phase;
    phase.addItem(makeItem(5, Entity{1, 0}));
    phase.addItem(makeItem(2, Entity{2, 0}));
    phase.addItem(makeItem(5, Entity{3, 0}));
    phase.addItem(makeItem(7, Entity{4, 0}));
    phase.addItem(makeItem(2, Entity{5, 0}));
    phase.addItem(makeItem(7, Entity{6, 0}));

    const auto& bins = phase.getBins();
    ASSERT_EQ(bins.size(), 3u);
    ASSERT_EQ(bins[0].material_id, 5u);
    ASSERT_EQ(bins[1].material_id, 2u);
    ASSERT_EQ(bins[2].material_id, 7u);
}

TEST(BinnedRenderPhase, ClearEmptiesAllBins) {
    BinnedRenderPhase phase;
    phase.addItem(makeItem(1, Entity{1, 0}));
    phase.addItem(makeItem(2, Entity{2, 0}));
    phase.clear();

    ASSERT_EQ(phase.getBins().size(), 0u);
}
