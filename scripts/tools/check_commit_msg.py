#!/usr/bin/env python3
"""
Entelechy Commit Message Validator

Checks commit messages against AGENTS-CODE.md commit conventions:
- Conventional Commits format: type[scope]: description
- English only
- No Co-Authored-By, Signed-off-by, or similar auto-signature footers
- No leading or trailing @ symbols

Usage:
    python scripts/tools/check_commit_msg.py <commit-msg-file>
    python scripts/tools/check_commit_msg.py --message "type: desc"

Exit codes:
    0 = message is valid
    1 = violations found
"""

import argparse
import re
import sys
from pathlib import Path

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

PROJECT_ROOT = Path(__file__).resolve().parents[2]

# Valid conventional commit types
VALID_TYPES = {
    'feat', 'fix', 'docs', 'style', 'refactor', 'perf',
    'test', 'build', 'ci', 'chore', 'revert',
}

# Forbidden auto-signature patterns
FORBIDDEN_SIGNATURES = [
    re.compile(r'\bCo-Authored-By\s*:', re.IGNORECASE),
    re.compile(r'\bSigned-off-by\s*:', re.IGNORECASE),
    re.compile(r'\bReviewed-by\s*:', re.IGNORECASE),
    re.compile(r'\bAcked-by\s*:', re.IGNORECASE),
    re.compile(r'\bTested-by\s*:', re.IGNORECASE),
    re.compile(r'\bReported-by\s*:', re.IGNORECASE),
]

# Conventional commit title pattern:
# type[optional-scope]!?: description
# description must be present and start with lowercase letter
TITLE_PATTERN = re.compile(
    r'^(?P<type>[a-z]+)'             # type (lowercase word)
    r'(?:\((?P<scope>[^)]+)\))?'     # optional (scope)
    r'(?P<breaking>!)?'              # optional breaking change !
    r':\s+'                           # colon + space separator
    r'(?P<description>[^\s].*)$'     # description (non-empty, starts with non-whitespace)
)

# Leading/trailing @ check
AT_BOUNDARY_PATTERN = re.compile(r'^\s*@|@\s*$')

# Chinese character check
CHINESE_PATTERN = re.compile(r'[一-鿿]')


def check_commit_message(message: str) -> list[str]:
    """Validate a commit message. Returns list of error messages (empty = valid)."""
    errors = []
    lines = message.split('\n')

    # ---- Title line checks ----

    # Must have at least one line
    if not lines or not lines[0].strip():
        errors.append("Commit message is empty")
        return errors

    title = lines[0]

    # No leading/trailing @ on any line
    for i, line in enumerate(lines):
        if AT_BOUNDARY_PATTERN.match(line):
            errors.append(f"Line {i + 1}: Leading or trailing '@' symbol is forbidden")
            break

    # Conventional Commits format
    match = TITLE_PATTERN.match(title)
    if not match:
        errors.append(
            "Title must follow Conventional Commits format: type[scope]: description\n"
            "  Example: feat(ecs): add entity pool\n"
            "  Example: refactor(imgui): decouple from EcsLib"
        )
        return errors

    # Type check
    commit_type = match.group('type')
    if commit_type not in VALID_TYPES:
        errors.append(
            f"Invalid type '{commit_type}'. Must be one of: {', '.join(sorted(VALID_TYPES))}"
        )

    # Description check
    description = match.group('description')
    if not description:
        errors.append("Description must not be empty")

    # English only check on all lines
    for i, line in enumerate(lines):
        if CHINESE_PATTERN.search(line):
            errors.append(
                f"Line {i + 1}: Commit message must be in English (Chinese characters found)"
            )

    # ---- Body/footer checks ----

    # Forbidden auto-signatures
    full_text = '\n'.join(lines)
    for sig_pattern in FORBIDDEN_SIGNATURES:
        for match in sig_pattern.finditer(full_text):
            line_no = full_text[:match.start()].count('\n') + 1
            errors.append(
                f"Line {line_no}: '{match.group(0)}' is forbidden. "
                "The project is signed by its actual developers only."
            )

    return errors


def main():
    parser = argparse.ArgumentParser(description="Entelechy Commit Message Validator")
    parser.add_argument(
        "msg_file", nargs="?", default=None,
        help="Path to commit message file (as passed by git commit-msg hook)"
    )
    parser.add_argument(
        "--message", "-m", default=None,
        help="Commit message to check (for testing)"
    )
    args = parser.parse_args()

    if args.message:
        message = args.message
    elif args.msg_file:
        try:
            message = Path(args.msg_file).read_text(encoding='utf-8')
        except Exception as e:
            print(f"[commit-msg] Failed to read message file: {e}")
            sys.exit(1)
    else:
        # Read from stdin (alternate hook invocation)
        message = sys.stdin.read()

    errors = check_commit_message(message)

    if errors:
        print("[commit-msg] Commit message violations:")
        for err in errors:
            print(f"  - {err}")
        print()
        print("Commit message rules (see AGENTS-CODE.md):")
        print("  1. Conventional Commits: type[scope]: description")
        print("  2. English only")
        print("  3. No Co-Authored-By / Signed-off-by / Reviewed-by / etc.")
        print("  4. No leading or trailing '@' symbols")
        sys.exit(1)

    print("[commit-msg] Commit message OK")
    sys.exit(0)


if __name__ == "__main__":
    main()
