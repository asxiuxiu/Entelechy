#!/usr/bin/env python3
"""
Entelechy Code Style Linter

Checks C++ source files against AGENTS-CODE.md naming conventions and style rules.

Usage:
    python scripts/tools/lint.py                    # Check all source files
    python scripts/tools/lint.py --staged           # Check only git staged files
    python scripts/tools/lint.py --fix              # Auto-fix line endings (LF)
    python scripts/tools/lint.py --fix --staged     # Fix only staged files
    python scripts/tools/lint.py path/to/file.cpp   # Check specific file(s)

Exit codes:
    0 = no violations
    1 = violations found (or fix applied with remaining issues)
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path
from dataclasses import dataclass
from typing import List, Tuple, Optional

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

PROJECT_ROOT = Path(__file__).resolve().parents[2]

# Directories to exclude from scanning
EXCLUDE_DIRS = {
    'build',
    'scripts',
    '.claude',
    '.git',
    '.conan',
    'third_party',      # external deps
    'external',
    'vendor',
}

# File extensions to check
SOURCE_EXTS = {'.h', '.hpp', '.c', '.cpp'}

# Third-party modules whose internal API we don't enforce
# (but code *calling* them from project modules still gets checked)
THIRD_PARTY_MODULES = {
    'imgui',
}

# ---------------------------------------------------------------------------
# Violation reporting
# ---------------------------------------------------------------------------

@dataclass
class Violation:
    file: Path
    line: int
    col: int
    rule: str          # e.g. "naming.member-variable"
    message: str
    severity: str = "error"   # "error" or "warning"

    def __str__(self):
        rel = self.file.relative_to(PROJECT_ROOT).as_posix()
        return f"{rel}:{self.line}:{self.col} [{self.severity}] {self.rule}: {self.message}"


class LintContext:
    def __init__(self):
        self.violations: List[Violation] = []

    def add(self, file: Path, line: int, col: int, rule: str, message: str, severity: str = "error"):
        self.violations.append(Violation(file, line, col, rule, message, severity))

    def has_errors(self) -> bool:
        return any(v.severity == "error" for v in self.violations)

    def print_report(self):
        if not self.violations:
            print("[Lint] No violations found.")
            return

        errors = [v for v in self.violations if v.severity == "error"]
        warnings = [v for v in self.violations if v.severity == "warning"]

        for v in errors:
            print(f"  ERROR   {v}")
        for v in warnings:
            print(f"  WARNING {v}")

        print(f"\n[Lint] {len(errors)} error(s), {len(warnings)} warning(s)")


# ---------------------------------------------------------------------------
# File discovery
# ---------------------------------------------------------------------------

def should_exclude(path: Path) -> bool:
    """Check if a path should be excluded from scanning."""
    parts = path.parts
    for part in parts:
        if part in EXCLUDE_DIRS:
            return True
    return False


def is_third_party_file(path: Path) -> bool:
    """Check if a file belongs to a third-party module whose internal API we skip."""
    parts = path.parts
    for tp in THIRD_PARTY_MODULES:
        if tp in parts:
            return True
    return False


def get_source_files(paths: Optional[List[Path]] = None) -> List[Path]:
    """Discover source files to check."""
    if paths:
        files = []
        for p in paths:
            if p.is_file():
                if p.suffix in SOURCE_EXTS:
                    files.append(p.resolve())
            elif p.is_dir():
                for ext in SOURCE_EXTS:
                    files.extend(p.rglob(f"*{ext}"))
        # Filter out excluded paths
        files = [f for f in files if not should_exclude(f)]
        return sorted(set(files))

    # Default: scan _engine and _game
    files = []
    for root_name in ('_engine', '_game'):
        root = PROJECT_ROOT / root_name
        if root.exists():
            for ext in SOURCE_EXTS:
                files.extend(root.rglob(f"*{ext}"))

    return sorted([f for f in files if not should_exclude(f)])


def get_staged_files() -> List[Path]:
    """Get C++ source files that are currently git staged."""
    result = subprocess.run(
        ["git", "diff", "--cached", "--name-only", "--diff-filter=ACM"],
        capture_output=True, text=True, cwd=PROJECT_ROOT
    )
    if result.returncode != 0:
        print(f"[Lint] Warning: failed to get staged files: {result.stderr}")
        return []

    files = []
    for line in result.stdout.strip().split('\n'):
        if not line:
            continue
        p = PROJECT_ROOT / line
        if p.suffix in SOURCE_EXTS and not should_exclude(p):
            files.append(p)
    return files


# ---------------------------------------------------------------------------
# Line ending fixer
# ---------------------------------------------------------------------------

def fix_line_endings(file: Path) -> bool:
    """Convert CRLF to LF in a file. Returns True if changes were made."""
    content = file.read_bytes()
    if b'\r\n' not in content:
        return False
    fixed = content.replace(b'\r\n', b'\n')
    file.write_bytes(fixed)
    return True


# ---------------------------------------------------------------------------
# Regex patterns for style checks
# ---------------------------------------------------------------------------

# Member variable: must be m_snake_case (m_ followed by lowercase + underscores)
# Violations: m_camelCase, m_PascalCase, m_snake_case_ (trailing underscore)
MEMBER_VAR_BAD = re.compile(
    r'\b(m_[a-z]+[A-Z][a-zA-Z0-9]*)'   # m_somethingLikeThis (camelCase after m_)
    r'|\b(m_[a-z_]*[A-Z][a-zA-Z0-9_]*)'  # m_Something (PascalCase after m_)
    r'|\b(m_[a-z_]+_)\b'               # m_something_ (trailing underscore)
)

# Also catch non-m_ prefixed member variables in classes (heuristic)
# This is a weaker check — we look for common patterns in class bodies
MEMBER_VAR_MISSING_PREFIX = re.compile(
    r'^\s+(?!//)(?!/\*)(?!\*)(?!\s*//)'
    r'(?!const\s+)(?!static\s+)(?!inline\s+)(?!virtual\s+)(?!explicit\s+)'
    r'(?!friend\s+)(?!using\s+)(?!typedef\s+)(?!template\s+)'
    r'(?!public\s*:|private\s*:|protected\s*:)'
    r'(\w+(?:<[^>]+>)?)\s+(?!m_)([a-z][a-zA-Z0-9_]*)\s*[;=]'
)

# Function names: must be camelCase
# Violations: snake_case functions, PascalCase functions (that aren't constructors)
# We skip lines that look like macros, template specializations, or type definitions
FUNCTION_NAME_BAD = re.compile(
    r'^\s*(?!//)(?!/\*)(?!\*)(?!\s*#)'
    r'(?!\s*template)(?!\s*using)(?!\s*typedef)'
    r'(?!\s*namespace)(?!\s*class)(?!\s*struct)(?!\s*enum)'
    r'(?:\w+(?:<[^>]+>)?\s+)*'  # return type(s), optional
    r'(~?[a-z][a-zA-Z0-9]*)\s*\('
)

# Class/struct names: must be PascalCase
# Violations: snake_case class names
CLASS_NAME_BAD = re.compile(
    r'\b(?:class|struct)\s+([a-z][a-z0-9_]*[a-z0-9])\b'
)

# Enum values: must be PascalCase (not UPPER_SNAKE_CASE)
ENUM_VALUE_BAD = re.compile(
    r'\b([A-Z][A-Z0-9_]+)\s*(?:=|,|$)'
)

# Bare assert() — should use CHECK/VERIFY/ENSURE/STATIC_ASSERT
BARE_ASSERT = re.compile(
    r'\bassert\s*\('
)

# Chinese characters in code comments (not in string literals)
# We allow Chinese in TODO.md and AGENTS.md, but not in C++ comments
CHINESE_CHARS = re.compile(r'[一-鿿]')

# Macros/constants: should be UPPER_SNAKE_CASE
# Violations: kCamelCase constants
CONSTANT_BAD = re.compile(
    r'\b(k[A-Z][a-zA-Z0-9]*)\b'
)

# ---------------------------------------------------------------------------
# Checkers
# ---------------------------------------------------------------------------

def check_member_variables(ctx: LintContext, file: Path, lines: List[str], in_third_party: bool):
    """Check member variable naming (m_snake_case)."""
    if in_third_party:
        return

    for i, line in enumerate(lines, 1):
        # Skip comments, strings (simplistic)
        code = line.split('//')[0]

        # Check for m_camelCase violations
        for match in MEMBER_VAR_BAD.finditer(code):
            name = match.group(0)
            # Skip if it's actually valid (m_ + all lowercase + no trailing _)
            if re.match(r'^m_[a-z][a-z0-9_]*[a-z0-9]$', name):
                continue
            # Skip common false positives from standard library
            if name in ('m_',):
                continue
            ctx.add(file, i, match.start() + 1, "naming.member-variable",
                    f"Member variable '{name}' must use m_snake_case (m_ + lowercase + underscores)")


def check_function_names(ctx: LintContext, file: Path, lines: List[str], in_third_party: bool):
    """Check function naming (camelCase)."""
    if in_third_party:
        return

    for i, line in enumerate(lines, 1):
        code = line.split('//')[0]

        for match in FUNCTION_NAME_BAD.finditer(code):
            name = match.group(1)
            if not name:
                continue

            # Skip destructors (~ClassName)
            if name.startswith('~'):
                continue

            # Skip if already camelCase
            if re.match(r'^[a-z][a-z0-9]*([A-Z][a-z0-9]*)*$', name):
                continue

            # Skip operator overloads
            if name.startswith('operator'):
                continue

            # Skip main
            if name == 'main':
                continue

            # Skip common macro-generated names
            if name.upper() == name:
                continue

            ctx.add(file, i, match.start() + 1, "naming.function",
                    f"Function '{name}' must use camelCase")


def check_class_names(ctx: LintContext, file: Path, lines: List[str], in_third_party: bool):
    """Check class/struct naming (PascalCase)."""
    if in_third_party:
        return

    for i, line in enumerate(lines, 1):
        code = line.split('//')[0]

        for match in CLASS_NAME_BAD.finditer(code):
            name = match.group(1)
            if not name:
                continue

            # Skip if already PascalCase
            if re.match(r'^[A-Z][a-zA-Z0-9]*$', name):
                continue

            ctx.add(file, i, match.start() + 1, "naming.class",
                    f"Class/struct '{name}' must use PascalCase")


def check_bare_assert(ctx: LintContext, file: Path, lines: List[str], in_third_party: bool):
    """Check for bare assert() usage."""
    if in_third_party:
        return

    for i, line in enumerate(lines, 1):
        code = line.split('//')[0]

        for match in BARE_ASSERT.finditer(code):
            ctx.add(file, i, match.start() + 1, "assert.bare",
                    "Use CHECK(), VERIFY(), ENSURE(), or STATIC_ASSERT() instead of bare assert()")


def check_chinese_comments(ctx: LintContext, file: Path, lines: List[str], in_third_party: bool):
    """Check for Chinese characters in code comments."""
    if in_third_party:
        return

    for i, line in enumerate(lines, 1):
        # Check comments only (both // and /* styles)
        comment = ""
        if '//' in line:
            comment = line.split('//', 1)[1]
        elif '/*' in line and '*/' not in line:
            comment = line.split('/*', 1)[1]
        elif '/*' in line and '*/' in line:
            parts = line.split('/*')
            for part in parts[1:]:
                if '*/' in part:
                    comment += part.split('*/')[0] + " "

        if not comment:
            continue

        for match in CHINESE_CHARS.finditer(comment):
            ctx.add(file, i, match.start() + 1, "language.chinese-comment",
                    "Code comments must be in English", severity="warning")
            break  # One violation per line is enough


def check_line_endings(ctx: LintContext, file: Path, content: bytes, in_third_party: bool):
    """Check for CRLF line endings."""
    if in_third_party:
        return

    if b'\r\n' in content:
        ctx.add(file, 1, 1, "format.line-ending",
                "File uses CRLF line endings; must use LF", severity="warning")


def check_file_encoding(ctx: LintContext, file: Path, content: bytes, in_third_party: bool):
    """Check for UTF-8 BOM."""
    if in_third_party:
        return

    # Skip empty files
    if not content:
        return

    # Check for BOM
    if not content.startswith(b'\xef\xbb\xbf'):
        ctx.add(file, 1, 1, "format.encoding",
                "File must use UTF-8 with BOM encoding", severity="warning")


def check_cmake_relative_paths(ctx: LintContext, file: Path, lines: List[str]):
    """Check CMakeLists.txt for relative path usage."""
    if file.name != 'CMakeLists.txt':
        return

    for i, line in enumerate(lines, 1):
        # Skip comments
        code = line.split('#')[0]

        # Check for relative paths in add_library/add_executable/include
        # Match patterns like "../" or "./" in quoted paths
        if re.search(r'["\']\.\./', code) or re.search(r'["\']\./', code):
            ctx.add(file, i, 1, "cmake.relative-path",
                    "CMakeLists.txt must use ${CMAKE_CURRENT_LIST_DIR} instead of relative paths")


# ---------------------------------------------------------------------------
# Main lint runner
# ---------------------------------------------------------------------------

def lint_file(ctx: LintContext, file: Path, fix: bool = False) -> bool:
    """Lint a single file. Returns True if fixes were applied."""
    in_third_party = is_third_party_file(file)
    rel = file.relative_to(PROJECT_ROOT).as_posix()

    try:
        content = file.read_bytes()
    except Exception as e:
        ctx.add(file, 1, 1, "io.read-error", f"Failed to read file: {e}", severity="warning")
        return False

    # Fix line endings if requested
    fixed = False
    if fix and not in_third_party:
        if fix_line_endings(file):
            print(f"  [Fixed] {rel}: converted CRLF -> LF")
            fixed = True
            # Re-read after fix
            content = file.read_bytes()

    # Decode for line-based checks
    # Try UTF-8 first, fall back to latin-1
    try:
        text = content.decode('utf-8-sig')
    except UnicodeDecodeError:
        text = content.decode('latin-1')

    lines = text.split('\n')

    # Run all checkers
    check_line_endings(ctx, file, content, in_third_party)
    check_file_encoding(ctx, file, content, in_third_party)
    check_member_variables(ctx, file, lines, in_third_party)
    check_function_names(ctx, file, lines, in_third_party)
    check_class_names(ctx, file, lines, in_third_party)
    check_bare_assert(ctx, file, lines, in_third_party)
    check_chinese_comments(ctx, file, lines, in_third_party)
    check_cmake_relative_paths(ctx, file, lines)

    return fixed


def main():
    parser = argparse.ArgumentParser(description="Entelechy Code Style Linter")
    parser.add_argument("paths", nargs="*", help="Specific files or directories to check")
    parser.add_argument("--staged", action="store_true", help="Check only git staged files")
    parser.add_argument("--fix", action="store_true", help="Auto-fix line endings")
    parser.add_argument("--verbose", "-v", action="store_true", help="Print checked files")
    args = parser.parse_args()

    # Determine files to check
    if args.staged:
        files = get_staged_files()
        if not files:
            print("[Lint] No staged C++ source files to check.")
            sys.exit(0)
    elif args.paths:
        files = get_source_files([Path(p) for p in args.paths])
    else:
        files = get_source_files()

    if not files:
        print("[Lint] No files to check.")
        sys.exit(0)

    print(f"[Lint] Checking {len(files)} file(s)...")

    ctx = LintContext()
    total_fixed = 0

    for file in files:
        if args.verbose:
            rel = file.relative_to(PROJECT_ROOT).as_posix()
            print(f"  Checking {rel}")

        if lint_file(ctx, file, fix=args.fix):
            total_fixed += 1

    # Print summary
    print()
    ctx.print_report()

    if total_fixed > 0:
        print(f"\n[Lint] Fixed line endings in {total_fixed} file(s).")

    if ctx.has_errors():
        sys.exit(1)
    sys.exit(0)


if __name__ == "__main__":
    main()
