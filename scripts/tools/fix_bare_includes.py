#!/usr/bin/env python3
import re
from pathlib import Path

# Build include map from pre-migration manifest (recreate)
modules = [
    Path("_engine/source/asset"),
    Path("_engine/source/bridge"),
    Path("_engine/source/core"),
    Path("_engine/source/ecs"),
    Path("_engine/source/imgui"),
    Path("_engine/source/log"),
    Path("_engine/source/motor"),
    Path("_engine/source/render"),
    Path("_engine/source/test"),
    Path("_engine/source/thread_pool"),
    Path("_engine/source/vfs"),
    Path("_engine/source/window"),
    Path("_game/source/runtime"),
]

mapping = {}
for mod in modules:
    pub = mod / "public" / mod.name
    if pub.exists():
        for f in pub.rglob("*.h"):
            rel = f.relative_to(pub).as_posix()
            new = f"{mod.name}/{rel}"
            mapping[rel] = new
            # Also map bare filename for subdirectory files
            if "/" in rel:
                bare = f.name
                mapping[bare] = new

print(f"Map: {len(mapping)} entries")

# Find all source files
files = list(Path(".").rglob("*.cpp")) + list(Path(".").rglob("*.h")) + list(Path(".").rglob("*.hpp"))
files = [f for f in files if "build" not in str(f) and "scripts" not in str(f) and f.name != "CMakeLists.txt"]

fixed = 0
for filepath in files:
    content = filepath.read_text(encoding="utf-8")
    original = content
    
    def repl(m):
        inc = m.group(1)
        if inc in mapping:
            return f'#include "{mapping[inc]}"'
        return m.group(0)
    
    content = re.sub(r'#include\s+"([^"]+)"', repl, content)
    if content != original:
        filepath.write_text(content, encoding="utf-8")
        fixed += 1

print(f"Fixed {fixed} files")
