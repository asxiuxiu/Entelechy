#include "core/allocator/iallocator.h"
#include "core/allocator/allocator.h"

namespace Entelechy
{

usize IAllocator::quantizeSize(usize size) const
{
    return DefaultAllocator::quantizeSize(size);
}

} // namespace Entelechy
