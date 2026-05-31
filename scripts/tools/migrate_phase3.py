#!/usr/bin/env python3
"""
Phase 3 migration script: public/private directory boundary + module-prefix includes.
Moves .h files to public/module/ and .cpp files to private/, then updates
CMakeLists.txt and all #include statements.
"""

import os
import sys
import shutil
import re
from pathlib import Path

PROJECT_ROOT = Path("D:/workspace/Entelechy")
ENGINE_SOURCE = PROJECT_ROOT / "_engine/source"
GAME_SOURCE = PROJECT_ROOT / "_game/source"

# Module search paths
MODULE_ROOTS = [ENGINE_SOURCE, GAME_SOURCE]

# Extensions
HEADER_EXTS = {".h", ".hpp"}
SOURCE_EXTS = {".cpp", ".c"}


def find_modules():
    """Find all module directories (those with a CMakeLists.txt)."""
    modules = []
    for root in MODULE_ROOTS:
        if not root.exists():
            continue
        for entry in sorted(root.iterdir()):
            if entry.is_dir() and (entry / "CMakeLists.txt").exists():
                modules.append(entry)
    return modules


def discover_files(module_dir: Path):
    """Discover all .h/.hpp/.cpp files in a module (excluding tests/ and build artifacts)."""
    headers = []
    sources = []
    for f in sorted(module_dir.rglob("*")):
        if not f.is_file():
            continue
        # Skip tests, build dirs, CMake files
        rel = f.relative_to(module_dir)
        parts = rel.parts
        if parts[0] == "tests":
            continue
        if f.name == "CMakeLists.txt":
            continue
        if f.suffix.lower() in HEADER_EXTS:
            headers.append(rel)
        elif f.suffix.lower() in SOURCE_EXTS:
            sources.append(rel)
    return headers, sources


def migrate_module(module_dir: Path, headers, sources, dry_run: bool = False):
    """Move files into public/module/ and private/."""
    module_name = module_dir.name
    public_dir = module_dir / "public" / module_name
    private_dir = module_dir / "private"

    print(f"\n[{module_name}] Migrating {len(headers)} headers, {len(sources)} sources")

    # Create directories
    if not dry_run:
        public_dir.mkdir(parents=True, exist_ok=True)
        private_dir.mkdir(parents=True, exist_ok=True)

    # Move headers to public/module_name/
    for rel in headers:
        src = module_dir / rel
        dst = public_dir / rel
        if not dry_run:
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.move(str(src), str(dst))
        print(f"  H: {rel.as_posix()} -> public/{module_name}/{rel.as_posix()}")

    # Move sources to private/
    for rel in sources:
        src = module_dir / rel
        dst = private_dir / rel
        if not dry_run:
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.move(str(src), str(dst))
        print(f"  S: {rel.as_posix()} -> private/{rel.as_posix()}")


def update_module_cmake(module_dir: Path, dry_run: bool = False):
    """Rewrite module CMakeLists.txt to use auto-discovery (remove explicit SOURCES)."""
    cmake_file = module_dir / "CMakeLists.txt"
    content = cmake_file.read_text(encoding="utf-8")

    module_name = module_dir.name

    # Find the entelechy_module() call boundaries
    start = content.find("entelechy_module(")
    if start == -1:
        print(f"  [!] No entelechy_module() found in {cmake_file}")
        return

    # Find matching closing paren
    paren_depth = 0
    end = start
    for i in range(start, len(content)):
        if content[i] == '(':
            paren_depth += 1
        elif content[i] == ')':
            paren_depth -= 1
            if paren_depth == 0:
                end = i + 1
                break

    before = content[:start]
    call = content[start:end]
    after = content[end:]

    # Parse key arguments from the call
    name_match = re.search(r'NAME\s+([A-Za-z0-9_]+)', call)
    type_match = re.search(r'TYPE\s+([A-Za-z0-9_]+)', call)
    init_match = re.search(r'INIT_FUNCTION\s+([A-Za-z0-9_]+)', call)
    ns_match = re.search(r'NAMESPACE\s+([A-Za-z0-9_:]+)', call)
    public_deps_match = re.search(r'PUBLIC_DEPS\s+([^)]*?)(?=\s+(?:PRIVATE_DEPS|INIT_FUNCTION|NAMESPACE|NO_TESTS|\)))', call, re.DOTALL)
    private_deps_match = re.search(r'PRIVATE_DEPS\s+([^)]*?)(?=\s+(?:INIT_FUNCTION|NAMESPACE|NO_TESTS|\)))', call, re.DOTALL)
    no_tests = "NO_TESTS" in call

    name = name_match.group(1) if name_match else module_name

    # Special cases
    if module_name == "imgui":
        # ImGui has external backend sources from Conan that must stay in SOURCES
        lines = ["entelechy_module("]
        lines.append(f"    NAME {name}")
        if type_match:
            lines.append(f"    TYPE {type_match.group(1)}")
        lines.append("    SOURCES")
        lines.append("        ${IMGUI_BACKENDS_DIR}/imgui_impl_glfw.cpp")
        lines.append("        ${IMGUI_BACKENDS_DIR}/imgui_impl_opengl3.cpp")

        # PUBLIC_DEPS
        if public_deps_match:
            deps = public_deps_match.group(1).strip()
            if deps:
                lines.append("    PUBLIC_DEPS")
                for dep in deps.split():
                    lines.append(f"        {dep}")

        # PRIVATE_DEPS
        if private_deps_match:
            deps = private_deps_match.group(1).strip()
            if deps:
                lines.append("    PRIVATE_DEPS")
                for dep in deps.split():
                    lines.append(f"        {dep}")

        if init_match:
            lines.append(f"    INIT_FUNCTION {init_match.group(1)}")
        if ns_match:
            lines.append(f"    NAMESPACE {ns_match.group(1)}")
        if no_tests:
            lines.append("    NO_TESTS")

        lines.append(")")
        new_call = "\n".join(lines) + "\n"
        new_content = before + new_call + after

        if not dry_run:
            cmake_file.write_text(new_content, encoding="utf-8")
        print(f"  CMakeLists.txt rewritten (imgui special: auto-discovery + external SOURCES)")
        return

    elif module_name == "test_runner":
        # test_runner has ${TEST_OBJECTS} which cannot be removed
        lines = ["entelechy_module("]
        lines.append(f"    NAME {name}")
        if type_match:
            lines.append(f"    TYPE {type_match.group(1)}")
        lines.append("    SOURCES")
        lines.append("        ${CMAKE_CURRENT_LIST_DIR}/private/main.cpp")
        lines.append("        ${TEST_OBJECTS}")

        # PRIVATE_DEPS
        if private_deps_match:
            deps = private_deps_match.group(1).strip()
            if deps:
                lines.append("    PRIVATE_DEPS")
                for dep in deps.split():
                    lines.append(f"        {dep}")

        if no_tests:
            lines.append("    NO_TESTS")

        lines.append(")")
        new_call = "\n".join(lines) + "\n"
        new_content = before + new_call + after

        if not dry_run:
            cmake_file.write_text(new_content, encoding="utf-8")
        print(f"  CMakeLists.txt rewritten (test_runner special: auto-discovery + TEST_OBJECTS)")
        return

    # Standard module: auto-discovery (no SOURCES)
    lines = ["entelechy_module("]
    lines.append(f"    NAME {name}")
    if type_match:
        lines.append(f"    TYPE {type_match.group(1)}")

    # PUBLIC_DEPS
    if public_deps_match:
        deps = public_deps_match.group(1).strip()
        if deps:
            lines.append("    PUBLIC_DEPS")
            for dep in deps.split():
                lines.append(f"        {dep}")

    # PRIVATE_DEPS
    if private_deps_match:
        deps = private_deps_match.group(1).strip()
        if deps:
            lines.append("    PRIVATE_DEPS")
            for dep in deps.split():
                lines.append(f"        {dep}")

    if init_match:
        lines.append(f"    INIT_FUNCTION {init_match.group(1)}")
    if ns_match:
        lines.append(f"    NAMESPACE {ns_match.group(1)}")
    if no_tests:
        lines.append("    NO_TESTS")

    lines.append(")")
    new_call = "\n".join(lines) + "\n"

    # Remove old CoreLib-style extra target_include_directories that exposed subdirs
    after_cleaned = after
    if module_name == "core":
        after_cleaned = re.sub(
            r'target_include_directories\s*\(\s*CoreLib\s+PUBLIC\s+'
            r'\$\{CMAKE_CURRENT_LIST_DIR\}/memory\s+'
            r'\$\{CMAKE_CURRENT_LIST_DIR\}/math\s*\)\s*\n?',
            '',
            after_cleaned
        )

    new_content = before + new_call + after_cleaned

    if not dry_run:
        cmake_file.write_text(new_content, encoding="utf-8")
    print(f"  CMakeLists.txt rewritten (auto-discovery)")


def build_include_map(file_manifest):
    """Build a map: old_include_path -> new_include_path.
    
    file_manifest: dict {module_dir: (headers, sources)} as discovered BEFORE migration.
    """
    mapping = {}
    for module_dir, (headers, _) in file_manifest.items():
        module_name = module_dir.name
        for rel in headers:
            old_path = rel.as_posix()
            new_path = f"{module_name}/{old_path}"
            mapping[old_path] = new_path
    return mapping


def replace_includes_in_file(filepath: Path, mapping: dict, dry_run: bool = False):
    """Replace #include "..." statements in a single file."""
    try:
        content = filepath.read_text(encoding="utf-8")
    except Exception as e:
        print(f"  [!] Cannot read {filepath}: {e}")
        return 0

    original = content
    replaced = 0

    def replacer(m):
        nonlocal replaced
        old = m.group(1)
        if old in mapping:
            new = mapping[old]
            replaced += 1
            return f'#include "{new}"'
        return m.group(0)

    content = re.sub(r'#include\s+"([^"]+)"', replacer, content)

    if content != original and not dry_run:
        filepath.write_text(content, encoding="utf-8")

    return replaced


def replace_all_includes(modules, mapping, dry_run: bool = False):
    """Globally replace all project #include statements."""
    print(f"\n[Include Map] {len(mapping)} entries")
    for k, v in sorted(mapping.items()):
        print(f"  '{k}' -> '{v}'")

    total_replaced = 0
    files_scanned = 0

    # Module source files (including tests)
    for module_dir in modules:
        for pattern in ["**/*.cpp", "**/*.h", "**/*.hpp", "**/*.c"]:
            for f in module_dir.rglob(pattern[3:]):
                if not f.is_file():
                    continue
                if f.name == "CMakeLists.txt":
                    continue
                files_scanned += 1
                total_replaced += replace_includes_in_file(f, mapping, dry_run)

    # launch/templates/main.cpp.in
    main_cpp_in = PROJECT_ROOT / "launch/templates/main.cpp.in"
    if main_cpp_in.exists():
        files_scanned += 1
        total_replaced += replace_includes_in_file(main_cpp_in, mapping, dry_run)

    print(f"\n[Include Replace] Scanned {files_scanned} files, replaced {total_replaced} includes")


def main():
    dry_run = "--dry-run" in sys.argv

    modules = find_modules()
    print(f"Found {len(modules)} modules:")
    for m in modules:
        print(f"  - {m.name}")

    # Safety check: refuse to run if any module already has public/ or private/
    dirty = False
    for module_dir in modules:
        if (module_dir / "public").exists() or (module_dir / "private").exists():
            print(f"  [!] {module_dir.name} already has public/ or private/ — clean up before running")
            dirty = True
    if dirty:
        print("\nAborting. Run this to clean up residual directories:")
        print("  find _engine/source _game/source -type d \\( -name public -o -name private \\) -prune -exec rm -rf {} +")
        sys.exit(1)

    # Discover files BEFORE migration (for include map)
    file_manifest = {}
    for module_dir in modules:
        headers, sources = discover_files(module_dir)
        file_manifest[module_dir] = (headers, sources)

    # Build include map from pre-migration discovery
    include_map = build_include_map(file_manifest)

    # Step 1: Migrate files
    print("\n" + "=" * 60)
    print("STEP 1: File Migration")
    print("=" * 60)
    for module_dir in modules:
        headers, sources = file_manifest[module_dir]
        migrate_module(module_dir, headers, sources, dry_run)

    # Step 2: Update CMakeLists.txt
    print("\n" + "=" * 60)
    print("STEP 2: CMakeLists.txt Update")
    print("=" * 60)
    for module_dir in modules:
        update_module_cmake(module_dir, dry_run)

    # Step 3: Replace includes
    print("\n" + "=" * 60)
    print("STEP 3: Global Include Replacement")
    print("=" * 60)
    replace_all_includes(modules, include_map, dry_run)

    print("\n" + "=" * 60)
    if dry_run:
        print("DRY RUN complete. No files were modified.")
        print("Run without --dry-run to apply changes.")
    else:
        print("Migration complete!")
        print("Next: verify with 'python scripts/build/build.py --debug --build'")
    print("=" * 60)


if __name__ == "__main__":
    main()
