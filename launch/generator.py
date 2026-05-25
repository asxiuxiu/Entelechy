#!/usr/bin/env python3
"""
Source Forest generator (Entelechy lightweight edition)
Simulates the engine's chaos_launch_generator.exe

Functions:
1. Parse configs/*.json (build configuration)
2. Read each repo's cmake_projects.json (module configuration)
3. Generate proxy CMakeLists.txt in build/sourcetree/
4. Generate the final main.cpp entry point
"""

import json
import os
import sys
import argparse
from pathlib import Path
import shutil


def parse_args():
    parser = argparse.ArgumentParser(description='Entelechy Source Forest Generator')
    parser.add_argument('--config', required=True, help='Path to build config JSON')
    parser.add_argument('--output', default='build/sourcetree', help='Output directory for sourcetree')
    parser.add_argument('--source-root', default='.', help='Root directory containing source repos')
    return parser.parse_args()


def load_json(path):
    with open(path, 'r', encoding='utf-8-sig') as f:
        return json.load(f)


def generate_proxy_cmake(proxy_path, real_source_path):
    cmake_path = str(real_source_path).replace('\\', '/')
    proxy_content = f"""# Proxy CMakeLists.txt
# This is the "pass-through window": includes the real source directory's CMakeLists.txt

set(REAL_SOURCE_PATH "{cmake_path}")
message(STATUS "[Proxy] Including: ${{REAL_SOURCE_PATH}}")

# Include the real source build configuration
include(${{REAL_SOURCE_PATH}}/CMakeLists.txt)
"""
    proxy_path.parent.mkdir(parents=True, exist_ok=True)
    with open(proxy_path, 'w', encoding='utf-8') as f:
        f.write(proxy_content)


def generate_main_cpp(sourcetree_dir, solution_name, enabled_modules):
    template_path = Path(__file__).parent / 'templates' / 'main.cpp'
    with open(template_path, 'r', encoding='utf-8') as f:
        template = f.read()

    forward_decls = []
    init_calls = []

    for module in enabled_modules:
        module_name = module['name']
        module_type = module.get('type', 'unknown')

        if module_type == 'engine':
            if 'core' in module_name.lower():
                forward_decls.append("namespace Entelechy { void initCore(); }")
                init_calls.append("    Entelechy::initCore();")
            elif 'bridge' in module_name.lower():
                forward_decls.append("namespace Entelechy { void initBridge(); }")
                init_calls.append("    Entelechy::initBridge();")
            elif 'window' in module_name.lower():
                forward_decls.append("namespace Entelechy { void initWindow(); }")
                init_calls.append("    Entelechy::initWindow();")
            elif 'imgui' in module_name.lower():
                forward_decls.append("namespace Entelechy { void initImGui(); }")
                init_calls.append("    Entelechy::initImGui();")
        elif module_type == 'game':
            if 'runtime' in module_name.lower():
                forward_decls.append("namespace game { void initRuntime(); }")
                init_calls.append("    game::initRuntime();")

    forward_decls = list(dict.fromkeys(forward_decls))

    main_cpp = template.replace('@SOLUTION_NAME@', solution_name)
    main_cpp = main_cpp.replace('@FORWARD_DECLS@', '\n'.join(forward_decls))
    main_cpp = main_cpp.replace('@INIT_CALLS@', '\n'.join(init_calls))

    with open(sourcetree_dir / 'main.cpp', 'w', encoding='utf-8') as f:
        f.write(main_cpp)


def generate_modules_cmake(sourcetree_dir, enabled_modules):
    modules_file = sourcetree_dir / 'generated' / 'modules.cmake'
    modules_file.parent.mkdir(parents=True, exist_ok=True)

    content = "# Auto-generated module list\n"
    content += "set(ENABLED_MODULES\n"

    for module in enabled_modules:
        module_dir = module['proxy_dir'].replace(os.sep, '/')
        content += f"    {module_dir}\n"

    content += ")\n"

    with open(modules_file, 'w', encoding='utf-8') as f:
        f.write(content)


def generate_sourcetree(config_path, output_dir, source_root):
    print(f"[Generator] Loading config: {config_path}")
    config = load_json(config_path)

    solution_name = config['solution_name']
    source_tree = config['source_tree']

    print(f"[Generator] Solution: {solution_name}")
    print(f"[Generator] Repositories: {len(source_tree)}")

    sourcetree_dir = Path(output_dir)
    sourcetree_dir.mkdir(parents=True, exist_ok=True)

    (sourcetree_dir / 'generated').mkdir(exist_ok=True)
    (sourcetree_dir / 'cmake').mkdir(exist_ok=True)

    template_dir = Path(__file__).parent / 'templates'
    shutil.copy(template_dir / 'cmake' / 'json_parser.cmake',
                sourcetree_dir / 'cmake' / 'json_parser.cmake')

    enabled_modules = []

    for repo_config_path in source_tree:
        full_repo_config = Path(source_root) / repo_config_path
        print(f"[Generator] Processing repo: {full_repo_config}")

        if not full_repo_config.exists():
            print(f"[Warning] Config not found: {full_repo_config}")
            continue

        repo_config = load_json(full_repo_config)
        repo_name = repo_config['name']
        repo_root = repo_config['root_path']
        modules = repo_config['modules']

        repo_type = 'unknown'
        if 'engine' in repo_name.lower():
            repo_type = 'engine'
        elif 'game' in repo_name.lower():
            repo_type = 'game'
        elif 'plugin' in repo_name.lower():
            repo_type = 'plugin'

        for module in modules:
            if not module.get('enabled', True):
                print(f"  [Skip] {module['name']} (disabled)")
                continue

            module_name = module['name']
            module_path = module['path']

            real_source_path = (Path(source_root) / repo_root / module_path).resolve()
            proxy_dir = sourcetree_dir / repo_root / module_path
            proxy_cmake = proxy_dir / 'CMakeLists.txt'

            print(f"  [Generate] {repo_name}/{module_name} -> {proxy_cmake}")

            generate_proxy_cmake(proxy_cmake, real_source_path)

            enabled_modules.append({
                'name': module_name,
                'type': repo_type,
                'proxy_dir': str(proxy_dir.relative_to(sourcetree_dir)),
                'real_path': str(real_source_path)
            })

    print("[Generator] Creating root CMakeLists.txt")
    with open(template_dir / 'CMakeLists.txt', 'r', encoding='utf-8') as f:
        root_cmake_template = f.read()

    root_cmake = root_cmake_template.replace('@SOLUTION_NAME@', solution_name)
    root_cmake = root_cmake.replace('@CONFIG_PATH@', str(Path(config_path).resolve()).replace('\\', '/'))
    root_cmake = root_cmake.replace('@CMAKE_CURRENT_SOURCE_DIR@', str(sourcetree_dir.resolve()).replace('\\', '/'))

    link_libs = []
    for m in enabled_modules:
        if 'core' in m['name'].lower():
            link_libs.append('CoreLib')
        elif 'motor' in m['name'].lower():
            link_libs.append('MotorLib')
        elif 'bridge' in m['name'].lower():
            link_libs.append('BridgeLib')
        elif 'math' in m['name'].lower():
            link_libs.append('MathLib')
        elif 'memory' in m['name'].lower():
            link_libs.append('MemoryLib')
        elif 'window' in m['name'].lower():
            link_libs.append('WindowLib')
        elif 'log' in m['name'].lower():
            link_libs.append('LogLib')
        elif 'render' in m['name'].lower():
            link_libs.append('RenderLib')
        elif 'imgui' in m['name'].lower():
            link_libs.append('ImGuiLib')
        elif 'vfs' in m['name'].lower():
            link_libs.append('VFSLib')
        elif 'thread_pool' in m['name'].lower():
            link_libs.append('ThreadPoolLib')
        elif 'test' == m['name'].lower():
            link_libs.append('TestFrameworkLib')
        elif 'runtime' in m['name'].lower():
            link_libs.append('RuntimeLib')

    link_libs_str = '\n'.join([f'    {lib}' for lib in link_libs])
    root_cmake = root_cmake.replace('@LINK_LIBS@', link_libs_str)

    with open(sourcetree_dir / 'CMakeLists.txt', 'w', encoding='utf-8') as f:
        f.write(root_cmake)

    generate_modules_cmake(sourcetree_dir, enabled_modules)
    generate_main_cpp(sourcetree_dir, solution_name, enabled_modules)

    print(f"\n[Generator] Successfully generated {len(enabled_modules)} modules")
    print(f"[Generator] Output: {sourcetree_dir.absolute()}")
    print(f"[Generator] Next step: cmake -S {output_dir} -B build")


def main():
    args = parse_args()

    script_dir = Path(__file__).parent.parent

    config_path = Path(args.config)
    if not config_path.is_absolute():
        config_path = script_dir / config_path

    source_root = Path(args.source_root)
    if not source_root.is_absolute():
        source_root = script_dir / source_root

    output_path = Path(args.output)
    if not output_path.is_absolute():
        output_path = script_dir / output_path

    generate_sourcetree(config_path, output_path, source_root)


if __name__ == '__main__':
    main()
