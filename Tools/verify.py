#!/usr/bin/env python3
"""
Run every static check over the project.

    python3 Tools/verify.py

Exit code is 0 when everything passes. Nothing here needs Unreal Engine, which
is the point: none of this project has ever been compiled or imported, and
these are the failures that can be found without it.
"""

import os
import subprocess
import sys

TOOLS = os.path.dirname(os.path.abspath(__file__))

CHECKS = [
    ("uht_lint.py",       "C++ reflection and include hygiene"),
    ("datatable_lint.py", "JSON tables against their row structs"),
    ("crossref_lint.py",  "IDs one table names against the table that declares them"),
]


def main():
    failed = []
    for script, description in CHECKS:
        print(f"\n{'=' * 72}\n{script} -- {description}\n{'=' * 72}")
        result = subprocess.run([sys.executable, os.path.join(TOOLS, script)])
        if result.returncode != 0:
            failed.append(script)

    print(f"\n{'=' * 72}")
    if failed:
        print(f"FAILED: {', '.join(failed)}")
        return 1
    print("All checks passed.")
    print("\nThis is not a build. UnrealHeaderTool and the compiler have still never")
    print("seen this project, and the DataTables have never been imported.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
