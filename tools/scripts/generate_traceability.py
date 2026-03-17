#!/usr/bin/env python3
"""
Generate bidirectional traceability matrix from tagged source artifacts.

Scans all .c, .h, .md files for:
  - @implements [SWE-XXX-Y] tags in source code
  - @verified_by [SWE-XXX-Y] tags in test code
  - Requirement definitions in SWRS.md

Outputs: docs/traceability/full_traceability_matrix.md

@implements [SYS-077] Bidirectional Traceability
@implements [SYS-078] Traceability Matrix Automation
"""

import os
import re
import sys
from collections import defaultdict
from pathlib import Path


def find_tags(root_dir: str, tag_name: str, extensions: tuple) -> dict:
    """Find all @tag [ID] references in source files."""
    pattern = re.compile(rf'@{tag_name}\s+\[([^\]]+)\]')
    results = defaultdict(list)  # req_id -> list of (file, line_no)

    for dirpath, _, filenames in os.walk(root_dir):
        for filename in filenames:
            if filename.endswith(extensions):
                filepath = os.path.join(dirpath, filename)
                rel_path = os.path.relpath(filepath, root_dir)
                try:
                    with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
                        for line_no, line in enumerate(f, 1):
                            matches = pattern.findall(line)
                            for match in matches:
                                # Handle comma-separated IDs: [SWE-001-1, SWE-001-2]
                                for req_id in match.split(','):
                                    req_id = req_id.strip()
                                    if req_id:
                                        results[req_id].append((rel_path, line_no))
                except Exception as e:
                    print(f"Warning: Could not read {filepath}: {e}", file=sys.stderr)

    return results


def find_requirements(swrs_path: str) -> list:
    """Extract requirement IDs from SWRS.md."""
    pattern = re.compile(r'\*\*\[(SWE-\d+-\d+)\]')
    requirements = []

    try:
        with open(swrs_path, 'r', encoding='utf-8') as f:
            for line in f:
                matches = pattern.findall(line)
                requirements.extend(matches)
    except FileNotFoundError:
        print(f"Warning: SWRS not found at {swrs_path}", file=sys.stderr)

    return sorted(set(requirements))


def generate_matrix(project_root: str) -> str:
    """Generate the full traceability matrix."""
    implements = find_tags(project_root, 'implements', ('.c', '.h'))
    verified_by = find_tags(project_root, 'verified_by', ('.c', '.h'))

    swrs_path = os.path.join(project_root, 'docs/requirements/software/SWRS.md')
    all_reqs = find_requirements(swrs_path)

    lines = [
        '---',
        'title: "Full Bidirectional Traceability Matrix"',
        'document_id: "TRACE-003"',
        'project: "Sentinel Gateway"',
        'version: "1.0.0"',
        f'date: {__import__("datetime").date.today().isoformat()}',
        'status: "Generated"',
        'aspice_process: "SUP.8 Configuration Management"',
        '---',
        '',
        '# Full Bidirectional Traceability Matrix',
        '',
        '## Forward Traceability (Requirement → Code → Test)',
        '',
        '| SWE Req | Implemented In | Verified By | Status |',
        '|---------|---------------|-------------|--------|',
    ]

    orphaned_reqs = []
    untested_reqs = []

    for req_id in all_reqs:
        impl_files = implements.get(req_id, [])
        test_files = verified_by.get(req_id, [])

        impl_str = ', '.join(f'`{f}:{l}`' for f, l in impl_files) if impl_files else '❌ MISSING'
        test_str = ', '.join(f'`{f}:{l}`' for f, l in test_files) if test_files else '❌ MISSING'

        if impl_files and test_files:
            status = '✅ Complete'
        elif impl_files:
            status = '⚠️ No Tests'
            untested_reqs.append(req_id)
        elif test_files:
            status = '⚠️ No Code'
        else:
            status = '❌ Orphaned'
            orphaned_reqs.append(req_id)

        lines.append(f'| {req_id} | {impl_str} | {test_str} | {status} |')

    lines.extend([
        '',
        '## Summary',
        '',
        f'- **Total SWE requirements**: {len(all_reqs)}',
        f'- **Fully traced**: {len(all_reqs) - len(orphaned_reqs) - len(untested_reqs)}',
        f'- **Missing implementation**: {len(orphaned_reqs)}',
        f'- **Missing tests**: {len(untested_reqs)}',
        f'- **Coverage**: {((len(all_reqs) - len(orphaned_reqs)) / max(len(all_reqs), 1) * 100):.1f}%',
    ])

    if orphaned_reqs:
        lines.extend(['', '## Orphaned Requirements (no code)', ''])
        for req in orphaned_reqs:
            lines.append(f'- {req}')

    if untested_reqs:
        lines.extend(['', '## Untested Requirements (code but no tests)', ''])
        for req in untested_reqs:
            lines.append(f'- {req}')

    return '\n'.join(lines)


if __name__ == '__main__':
    project_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    matrix = generate_matrix(project_root)

    output_path = os.path.join(project_root, 'docs/traceability/full_traceability_matrix.md')
    os.makedirs(os.path.dirname(output_path), exist_ok=True)

    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(matrix)

    print(f"Traceability matrix written to: {output_path}")
