#include "atom_registry.h"
#include "string_intern_pool.h"

namespace Entelechy {

AtomRegistry& AtomRegistry::instance() {
    static AtomRegistry reg;
    return reg;
}

void AtomRegistry::registerAtom(const AtomType& type) {
    m_atoms.insert(type.name, type);
}

const AtomType* AtomRegistry::find(const SmallString& name) const {
    return m_atoms.find(name);
}

const AtomType* AtomRegistry::find(StringId name) const {
    const char* resolved = StringInternPool::instance().resolve(name);
    if (!resolved) return nullptr;
    return m_atoms.find(SmallString(resolved));
}

bool AtomRegistry::tryDraw(const SmallString& typeName, const char* label, void* ptr) const {
    auto* atom = find(typeName);
    if (atom && atom->inspectorDraw) {
        atom->inspectorDraw(label, ptr);
        return true;
    }
    return false;
}

} // namespace Entelechy
