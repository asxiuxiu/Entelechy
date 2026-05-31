#!/usr/bin/env python3
"""
Source Forest generator (Entelechy lightweight edition)
DEPRECATED: CMake now handles module discovery and main.cpp generation directly.
This script is kept as a fallback for standalone main.cpp generation.
"""

import json
import os
import sys
import argparse
from pathlib import Path


def parse_args():
    parser = argparse.ArgumentParser(description='Entelechy Source Forest Generator (deprecated)')
    parser.add_argument('--config', required=True, help='Path to build config JSON')
    parser.add_argument('--output', default='build/generated', help='Output directory for main.cpp')
    parser.add_argument('--source-root', default='.', help='Root directory containing source repos')
    return parser.parse_args()


def load_json(path):
    with open(path, 'r', encoding='utf-8-sig') as f:
        return json.load(f)


def generate_main_cpp(output_dir, solution_name, enabled_modules):
    template_path = Path(__file__).parent / 'templates' / 'main.cpp.in'
    with open(template_path, 'r', encoding='utf-8') as f:
        template = f.read()

    forward_decls = []
    init_calls = []

    for module in enabled_modules:
        init_func = module.get('init_function')
        namespace = module.get('namespace', 'Entelechy')
        if init_func:
            forward_decls.append(f"namespace {namespace} {{ void {init_func}(); }}")
            init_calls.append(f"    {namespace}::{init_func}();")

    # Deduplicate while preserving order
    forward_decls = list(dict.fromkeys(forward_decls))

    main_cpp = template.replace('@SOLUTION_NAME@', solution_name)
    main_cpp = main_cpp.replace('@FORWARD_DECLS@', '\n'.join(forward_decls))
    main_cpp = main_cpp.replace('@INIT_CALLS@', '\n'.join(init_calls))

    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)
    with open(output_path / 'main.cpp', 'w', encoding='utf-8') as f:
        f.write(main_cpp)


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

    print("[Generator] WARNING: generator.py is deprecated. CMake now handles module discovery.")
    config = load_json(str(config_path))
    solution_name = config['solution_name']
    source_tree = config['source_tree']

    enabled_modules = []
    for repo_config_path in source_tree:
        full_repo_config = source_root / repo_config_path
        if not full_repo_config.exists():
            print(f"[Warning] Config not found: {full_repo_config}")
            continue
        repo_config = load_json(str(full_repo_config))
        repo_root = repo_config['root_path']
        for module in repo_config['modules']:
            if not module.get('enabled', True):
                continue
            module['real_path'] = str((source_root / repo_root / module['path']).resolve())
            enabled_modules.append(module)

    generate_main_cpp(output_path, solution_name, enabled_modules)
    print(f"[Generator] Generated main.cpp -> {output_path / 'main.cpp'}")
    print(f"[Generator] {len(enabled_modules)} modules processed")


if __name__ == '__main__':
    main()
