#include "render/rhi/rhi_resources.h"
#include "render/rhi/rhi_device.h"

namespace Entelechy {

void GPUResource::release() {
    if (m_ref_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        if (m_device) {
            m_device->queueResourceForDelete(this);
        } else {
            // No owning device: destroy immediately as a safe fallback.
            internalDestroy();
        }
    }
}

} // namespace Entelechy
