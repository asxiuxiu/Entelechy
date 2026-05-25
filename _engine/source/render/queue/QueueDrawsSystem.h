#pragma once
#include "world.h"

namespace Entelechy {

// QueueDrawsSystem — generates PhaseItems from ViewVisibleList.
// Opaque/AlphaMask go to BinnedRenderPhase; Transparent/UI go to SortedRenderPhase.
class QueueDrawsSystem {
public:
    void run(World& renderWorld);
};

} // namespace Entelechy
