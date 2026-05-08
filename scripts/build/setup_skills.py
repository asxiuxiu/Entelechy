#!/usr/bin/env python3
"""
Set up .claude/skills as a link to .agents/skills so that Claude Code
and other agents share the same skills directory.

Idempotent: if the link already points to the right target, do nothing.
"""

import os
import shutil
import subprocess
import sys
from pathlib import Path


def _is_link_to(link: Path, target: Path) -> bool:
    """Return True if `link` is a symlink/junction resolving to `target`."""
    if not link.exists() and not link.is_symlink():
        return False
    try:
        resolved = link.resolve(strict=False)
        return resolved == target.resolve(strict=False)
    except OSError:
        return False


def _create_link(link: Path, target: Path) -> None:
    """Create a directory link from `link` to `target`. Cross-platform."""
    target_abs = str(target.resolve())
    if sys.platform == "win32":
        # Junction works on the same volume without admin rights.
        # /J = directory junction
        subprocess.run(
            ["cmd", "/c", "mklink", "/J", str(link), target_abs],
            check=True,
            capture_output=True,
        )
    else:
        os.symlink(target_abs, str(link), target_is_directory=True)


def setup_skills_link(project_root: Path) -> None:
    """Ensure .claude/skills points to .agents/skills.

    Behavior:
    - If .claude/skills already links to .agents/skills: no-op.
    - If .claude/skills is a wrong link or empty dir: replace it.
    - If .claude/skills is a non-empty real directory: abort with an error
      (we don't silently destroy user data).
    """
    claude_dir = project_root / ".claude"
    skills_link = claude_dir / "skills"
    skills_target = project_root / ".agents" / "skills"

    if not skills_target.exists():
        print(f"[Setup] Skills target not found: {skills_target}", file=sys.stderr)
        return

    claude_dir.mkdir(exist_ok=True)

    if _is_link_to(skills_link, skills_target):
        return  # Already correctly linked, nothing to do.

    if skills_link.exists() or skills_link.is_symlink():
        # Wrong link, or empty/leftover directory: remove and recreate.
        if skills_link.is_symlink() or not skills_link.is_dir():
            skills_link.unlink()
        else:
            try:
                # Allow removing only if empty, to avoid destroying real content.
                skills_link.rmdir()
            except OSError:
                print(
                    f"[Setup] Refuse to replace non-empty directory: {skills_link}",
                    file=sys.stderr,
                )
                sys.exit(1)

    _create_link(skills_link, skills_target)
    print(f"[Setup] Linked {skills_link} -> {skills_target}")


def main():
    project_root = Path(__file__).resolve().parents[2]
    setup_skills_link(project_root)


if __name__ == "__main__":
    main()
