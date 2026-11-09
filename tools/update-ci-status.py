#!/usr/bin/env python3
"""Generate the reports and SVG tables published on ci-results."""

import argparse
from collections import Counter
import json
from pathlib import Path
import re
import textwrap
from xml.sax.saxutils import escape

ROOT = Path(__file__).resolve().parents[1]
PLATFORMS = (
    ('tests-windows-2022', 'Windows Server 2022', 'Native x64'),
    ('tests-windows-2025', 'Windows Server 2025', 'Native x64'),
    ('tests-windows-11-arm', 'Windows 11 on Arm', 'x64 emulation'),
    ('tests-ubuntu-wine', 'Ubuntu 24.04', 'Wine x64'),
)


def render(reports, run_url, commit, build_result, expected_names):
    build = {'success': 'Passed', 'failure': 'Failed', 'cancelled': 'Cancelled',
             'skipped': 'Not run'}.get(build_result, 'Unavailable')
    lines = [
        '## Core CI Build Status', '',
        '| Host & Toolchain | Target | Configuration | Build |',
        '| :--- | :---: | :---: | :---: |',
        f'| Windows Server 2022 · MSVC | x64 | Release | [{build}]({run_url}) |',
        '', '## Platform CI Test Status', '',
        '| Test Host | Execution | Passed | Failed | Skipped | Result |',
        '| :--- | :--- | ---: | ---: | ---: | :---: |',
    ]
    for artifact, host, execution in PLATFORMS:
        path = reports / artifact / 'test-results.json'
        if not path.is_file():
            lines.append(f'| {host} | {execution} | — | — | — | [No report]({run_url}) |')
            continue
        report = json.loads(path.read_text(encoding='utf-8'))
        tests = report['tests']
        counts = Counter(test['status'] for test in tests)
        if (len(tests) != len(expected_names) or {t['name'] for t in tests} != expected_names
                or set(counts) - {'PASS', 'FAIL', 'SKIP'}
                or any(report['counts'].get(key) != counts[key] for key in ('PASS', 'FAIL', 'SKIP'))):
            raise ValueError(f'incomplete or inconsistent report: {path}')
        result = 'Failed' if counts['FAIL'] else 'Passed' if counts['PASS'] else 'Skipped'
        lines.append(f"| {host} | {execution} | {counts['PASS']} | {counts['FAIL']} | "
                     f"{counts['SKIP']} | [{result}]({run_url}) |")
    lines.extend(['', f'Results: [CI run]({run_url}) · Source: `{commit[:12]}`.'])
    return '\n'.join(lines)


def svg(markdown, title):
    width, margin, y = 1080, 20, 18
    shapes = []

    def plain(text):
        text = re.sub(r'\[([^\]]+)\]\([^)]*\)', r'\1', text)
        return text.replace('**', '').replace('`', '')

    def label(text, x, baseline, bold=False):
        weight = ' style="font-weight:600"' if bold else ''
        shapes.append(f'<text x="{x:.1f}" y="{baseline:.1f}"{weight}>{escape(text)}</text>')

    lines = markdown.splitlines()
    i = 0
    while i < len(lines):
        line = lines[i].strip()
        if line.startswith('|'):
            rows = []
            while i < len(lines) and lines[i].strip().startswith('|'):
                cells = [plain(cell.strip()) for cell in lines[i].strip().strip('|').split('|')]
                if not all(re.fullmatch(r':?-+:?', cell) for cell in cells):
                    rows.append(cells)
                i += 1
            columns = len(rows[0])
            if any(len(row) != columns for row in rows):
                raise ValueError('inconsistent table columns')
            lengths = [max(8, min(48, max(len(row[c]) for row in rows))) for c in range(columns)]
            widths = [(width - 2 * margin) * length / sum(lengths) for length in lengths]
            for row_index, row in enumerate(rows):
                wrapped = [textwrap.wrap(cell, max(1, int((w - 20) / 7))) or ['']
                           for cell, w in zip(row, widths)]
                height = max(map(len, wrapped)) * 20 + 18
                if row_index == 0:
                    shapes.append(f'<rect class="header" x="{margin}" y="{y}" '
                                  f'width="{width - 2 * margin}" height="{height}"/>')
                else:
                    shapes.append(f'<line class="rule" x1="{margin}" y1="{y}" '
                                  f'x2="{width - margin}" y2="{y}"/>')
                x = margin
                for cell_lines, cell_width in zip(wrapped, widths):
                    cls_attr = ''
                    first_val = cell_lines[0].strip()
                    if first_val == 'Passed':
                        cls_attr = ' class="pass"'
                    elif first_val in ('Failed', 'Failure'):
                        cls_attr = ' class="fail"'
                    elif 'faster' in first_val:
                        cls_attr = ' class="fast"'
                    elif 'tied' in first_val:
                        cls_attr = ' class="tied"'
                    elif 'slower' in first_val:
                        cls_attr = ' class="slow"'

                    for n, value in enumerate(cell_lines):
                        weight = ' style="font-weight:600"' if (row_index == 0 or 'faster' in value or 'tied' in value or value in ('Passed', 'Failed')) else ''
                        shapes.append(f'<text x="{(x + 10):.1f}" y="{(y + 25 + n * 20):.1f}"{weight}{cls_attr}>{escape(value)}</text>')
                    x += cell_width
                y += height
            shapes.append(f'<line class="rule" x1="{margin}" y1="{y}" '
                          f'x2="{width - margin}" y2="{y}"/>')
            y += 14
            continue
        if line:
            heading = line.startswith('#')
            for value in textwrap.wrap(plain(line.lstrip('#').strip()), 130):
                label(value, margin, y + 18, heading)
                y += 25 if heading else 21
            y += 8
        i += 1
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{y + 8}" '
            f'viewBox="0 0 {width} {y + 8}" role="img">'
            f'<title>{escape(title)}</title><style>'
            'text{fill:#24292f;font:14px -apple-system,BlinkMacSystemFont,Segoe UI,sans-serif}'
            '.header{fill:#f6f8fa}.rule{stroke:#d0d7de;stroke-width:1}'
            '.pass{fill:#1a7f37;font-weight:600}.fail{fill:#cf222e;font-weight:600}'
            '.fast{fill:#1a7f37;font-weight:600}.slow{fill:#656d76}.tied{fill:#d97706;font-weight:600}'
            '@media(prefers-color-scheme:dark){text{fill:#e6edf3}.header{fill:#161b22}'
            '.rule{stroke:#30363d}.pass{fill:#3fb950}.fail{fill:#f85149}'
            '.fast{fill:#3fb950}.slow{fill:#8b949e}.tied{fill:#f0883e}}'
            '</style>' + ''.join(shapes) + '</svg>\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('reports', type=Path)
    parser.add_argument('--run-url', required=True)
    parser.add_argument('--commit', required=True)
    parser.add_argument('--build-result', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    manifest = json.loads((ROOT / 'tests/cases.json').read_text(encoding='utf-8'))
    content = render(args.reports, args.run_url, args.commit, args.build_result,
                     {case['Name'] for case in manifest})
    args.output.mkdir(parents=True, exist_ok=True)

    bench_dirs = sorted(args.reports.glob('benchmark-results-*'))
    if not bench_dirs:
        bench_dirs = [args.reports / 'benchmark-results'] if (args.reports / 'benchmark-results').is_dir() else []

    preferred_order = ['benchmark-results-win-2025', 'benchmark-results-win-2022', 'benchmark-results-win-arm64', 'benchmark-results-ubuntu-wine']
    bench_dirs.sort(key=lambda p: preferred_order.index(p.name) if p.name in preferred_order else 99)

    master_md = ""
    primary_summary = None

    platform_rows = []

    for bdir in bench_dirs:
        summary_file = bdir / 'benchmark-summary.md'
        details_file = bdir / 'BENCHMARKS.md'
        json_file = bdir / 'benchmark-results.json'

        if not master_md and details_file.is_file():
            master_md = details_file.read_text(encoding='utf-8')

        if not primary_summary and summary_file.is_file():
            primary_summary = summary_file.read_text(encoding='utf-8')

        if json_file.is_file():
            try:
                j = json.loads(json_file.read_text(encoding='utf-8'))
                cpu = j.get('cpu', 'Unknown')
                host = j.get('host', bdir.name)
                bdata = j.get('benchmarks', {})
                def fmt_stat(bid):
                    results_map = bdata.get(bid, {}).get('results', {})
                    res = results_map.get('Sable', results_map.get('Opus', {}))
                    c = res.get('compile_ms')
                    r = res.get('runtime_min_ms')
                    if c is None or r is None:
                        return '—'
                    return f"{c:.0f}ms / **{r:.2f}ms**"

                m_stat = fmt_stat('math')
                s_stat = fmt_stat('state')
                b_stat = fmt_stat('branch')
                g_stat = fmt_stat('general')
                h_stat = fmt_stat('hft')
                mv_stat = fmt_stat('math_vec')
                sv_stat = fmt_stat('state_vec')
                bv_stat = fmt_stat('branch_vec')
                gv_stat = fmt_stat('general_vec')
                hv_stat = fmt_stat('hft_vec')

                platform_rows.append(
                    f"| **{host}** | `{cpu}` | {m_stat} | {s_stat} | {b_stat} | {g_stat} | {h_stat} | {mv_stat} | {sv_stat} | {bv_stat} | {gv_stat} | {hv_stat} |"
                )
            except Exception:
                pass

    multi_arch_section = [
        "## Multi-Architecture Silicon Matrix (Sable: Compile / Runtime)",
        "",
        "### Scalar Benchmarks",
        "| Architecture & Host | Processor Model | Math (80M) | State (50M) | Branch (40M) | General (6M) | HFT (12M) | Math Vec (40M) | State Vec (40M) | Branch Vec (40M) | General Vec (20M) | HFT Vec (20M) |",
        "| :--- | :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |",
    ] + platform_rows + [
        "",
        f"Automated multi-host benchmarks recorded on [CI Run]({args.run_url}) · Source: `{args.commit[:12]}`.",
        "",
        "## Reproduction",
        "",
        "```powershell",
        "./bin/Release/sable.exe build benchmarks/benchmark_hft.project",
        "python benchmarks/run.py --iterations 5 --warmup 2",
        "```",
        "",
    ]

    full_benchmarks_doc = (
        master_md.strip() + "\n\n" + "\n".join(multi_arch_section)
    )

    (args.output / 'BENCHMARKS.md').write_text(full_benchmarks_doc + '\n', encoding='utf-8')

    if primary_summary:
        summary_text = primary_summary
        summary_text += f'\n[Benchmark CI run]({args.run_url}) · Source: `{args.commit[:12]}`.\n'
    else:
        summary_text = f'## Performance\n\nBenchmark measurements unavailable for [this CI run]({args.run_url}).\n'

    build, tests = content.split('## Platform CI Test Status', 1)
    images = {
        'build.svg': (build.removeprefix('## Core CI Build Status').strip(), 'Compiler build status'),
        'tests.svg': (tests.strip(), 'Passed, failed, and skipped tests by platform'),
        'benchmarks.svg': (summary_text.removeprefix('## Performance').strip(), 'Benchmark comparison'),
    }
    for filename, (body, title) in images.items():
        (args.output / filename).write_text(svg(body, title), encoding='utf-8')
    (args.output / 'README.md').write_text('# Sable CI Results\n\n' + summary_text + '\n\n' + content + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
