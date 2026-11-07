#!/usr/bin/env python3
"""Run the explicit compiler test manifest on Windows or through Wine."""

import argparse
from contextlib import contextmanager
from concurrent.futures import ThreadPoolExecutor, as_completed
import json
import os
from pathlib import Path
import shutil
import signal
import select
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]


def normalize(text):
    return text.replace('\r\n', '\n').replace('\r', '\n').rstrip()


def command(binary, *args):
    return ([] if os.name == 'nt' else ['wine']) + [str(binary), *map(str, args)]


def invoke(argv, cwd, timeout):
    # wine daemons can inherit pipes and keep communicate blocked after exit
    with tempfile.TemporaryFile() as output:
        proc = subprocess.Popen(argv, cwd=cwd, env=os.environ.copy(), stdout=output,
                                stderr=subprocess.STDOUT, start_new_session=os.name != 'nt')
        try:
            proc.wait(timeout=timeout)
        except (subprocess.TimeoutExpired, KeyboardInterrupt):
            if os.name == 'nt':
                subprocess.run(['taskkill', '/PID', str(proc.pid), '/T', '/F'],
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=10)
            else:
                try:
                    os.killpg(proc.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
            proc.wait(timeout=10)
            raise
        output.seek(0)
        text = output.read().decode('utf-8', errors='replace')
    return subprocess.CompletedProcess(argv, proc.returncode, text)


def check_crash(proc):
    code = proc.returncode
    text = proc.stdout.lower()
    if code < 0 or (code & 0xffffffff) >= 0x80000000 or any(marker in text for marker in (
            'wine: unhandled', 'unhandled exception', 'unhandled page fault',
            'unhandled stack overflow', 'could not load kernel32.dll')):
        raise AssertionError(f'process crash/launcher failure {code}\n{proc.stdout}')


@contextmanager
def isolated_runtime(workspace):
    if os.name == 'nt':
        # children inherit this; suppress windows fault dialogs, not exit status
        import ctypes
        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        previous = kernel.SetErrorMode(0x0001 | 0x0002 | 0x8000)
        try:
            yield
        finally:
            kernel.SetErrorMode(previous)
        return
    required = ('wine', 'wineboot', 'wineserver', 'Xvfb')
    missing = [tool for tool in required if not shutil.which(tool)]
    if missing:
        raise RuntimeError('headless test dependencies missing: ' + ', '.join(missing))
    keys = ('DISPLAY', 'WAYLAND_DISPLAY', 'WINEPREFIX', 'WINEDEBUG', 'WINEDLLOVERRIDES')
    previous = {key: os.environ.get(key) for key in keys}
    read_fd, write_fd = os.pipe()
    display = None
    server = None
    server_log = None
    try:
        display = subprocess.Popen(['Xvfb', '-displayfd', str(write_fd), '-nolisten', 'tcp',
                                    '-screen', '0', '1024x768x24'], pass_fds=(write_fd,),
                                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        os.close(write_fd)
        write_fd = None
        if not select.select([read_fd], [], [], 10)[0]:
            raise RuntimeError('private Xvfb display did not start')
        number = os.read(read_fd, 64).decode().strip()
        if not number.isdecimal():
            raise RuntimeError('private Xvfb display failed')
        (workspace / 'wine-prefix').mkdir()
        os.environ.update(DISPLAY=':' + number, WINEPREFIX=str(workspace / 'wine-prefix'),
                          WINEDEBUG='-all,err+seh', WINEDLLOVERRIDES='winedbg.exe=d')
        os.environ.pop('WAYLAND_DISPLAY', None)
        server_log = open(workspace / 'wineserver.log', 'w+b')
        server = subprocess.Popen(['wineserver', '-f', '-p'], env=os.environ.copy(),
                                  stdout=server_log, stderr=subprocess.STDOUT)
        boot = invoke(['wineboot', '--init'], workspace, 120)
        if boot.returncode:
            raise RuntimeError('isolated Wine initialization failed\n' + boot.stdout)
        if server.poll() is not None:
            server_log.seek(0)
            raise RuntimeError('private Wine server exited during initialization\n'
                               + server_log.read().decode('utf-8', errors='replace'))
        yield
    finally:
        # only this disposable prefix, never the users default wine session
        if os.environ.get('WINEPREFIX') == str(workspace / 'wine-prefix'):
            try:
                subprocess.run(['wineserver', '-k'], env=os.environ.copy(),
                               capture_output=True, timeout=10)
            except (OSError, subprocess.TimeoutExpired):
                pass
        if server is not None:
            try:
                server.wait(timeout=10)
            except subprocess.TimeoutExpired:
                server.kill()
                server.wait(timeout=10)
        if server_log is not None:
            server_log.close()
        if display is not None:
            display.terminate()
            try:
                display.wait(timeout=10)
            except subprocess.TimeoutExpired:
                display.kill()
                display.wait(timeout=10)
        os.close(read_fd)
        if write_fd is not None:
            os.close(write_fd)
        for key, value in previous.items():
            if value is None:
                os.environ.pop(key, None)
            else:
                os.environ[key] = value


def check_rejection(proc, expected=None):
    check_crash(proc)
    # compiler diagnostics return 1; signals, ntstatus and launcher errors fail
    if proc.returncode != 1:
        raise AssertionError(f'expected diagnostic exit 1, got {proc.returncode}\n{proc.stdout}')
    if expected and expected not in proc.stdout:
        raise AssertionError(f'missing expected diagnostic: {expected!r}\n{proc.stdout}')


def execute(case, compiler, workspace, timeout):
    start = time.perf_counter()
    result = {'name': case['Name'], 'kind': case['kind'], 'status': 'PASS', 'error': ''}
    if case.get('skip'):
        return dict(result, status='SKIP', error=case['skip'], milliseconds=0)
    try:
        kind = case['kind']
        source = workspace / case.get('ProjectFile', case.get('File', ''))
        if not source.is_file():
            raise AssertionError(f'missing test source: {source}')
        if case.get('Healing'):
            if case['Healing'] != 'auto' or kind != 'stress':
                raise AssertionError('healing is only supported for explicit auto-recovery stress cases')
            isolated = workspace / '.recovery' / source.relative_to(workspace).with_suffix('')
            isolated.mkdir(parents=True, exist_ok=True)
            copied = isolated / source.name
            shutil.copy2(source, copied)
            source = copied
            (isolated / 'sable.project').write_text(
                'project Recovery {\n'
                f'    entry: "{source.name}"\n'
                '    mode: "exe"\n    debug: true\n    healing: "auto"\n}\n')
        args = ['build', source] if 'project' in kind else ['--run', source] if kind == 'run' else [source]
        proc = invoke(command(compiler, *args), workspace, timeout)
        check_crash(proc)
        if kind.startswith('reject'):
            check_rejection(proc, case.get('ExpectedErrorContains'))
        elif kind == 'robustness':
            if proc.returncode not in (0, 1):
                raise AssertionError(f'compiler crash/abnormal exit {proc.returncode}\n{proc.stdout}')
        else:
            if proc.returncode != 0:
                raise AssertionError(f'compile/run exit {proc.returncode}\n{proc.stdout}')
            if kind == 'run':
                if proc.returncode != case['ExpectedExit'] or normalize(proc.stdout) != normalize(case['ExpectedOutput']):
                    raise AssertionError(f"expected output {case['ExpectedOutput']!r}, got {proc.stdout!r}")
            else:
                exe = workspace / case['ExeFile'] if kind == 'project' else source.with_suffix('.exe')
                if not exe.is_file():
                    raise AssertionError(f'missing output binary: {exe}')
                if kind in ('project', 'stress'):
                    runtime = invoke(command(exe), workspace, case.get('RuntimeTimeoutSeconds', timeout))
                    result.update(runtime_exit=runtime.returncode, runtime_output=runtime.stdout)
                    check_crash(runtime)
                    if kind == 'project' or 'ExpectedExit' in case:
                        expected = case['ExpectedExit']
                        if runtime.returncode != expected:
                            raise AssertionError(f'expected runtime exit {expected}, got {runtime.returncode}\n{runtime.stdout}')
                    elif runtime.returncode < 0 or (runtime.returncode & 0xffffffff) >= 0x80000000:
                        raise AssertionError(f'runtime crash {runtime.returncode}\n{runtime.stdout}')
                    for expected in case.get('ExpectedRuntimeOutputContains', []):
                        if normalize(expected) not in normalize(runtime.stdout):
                            raise AssertionError(f'missing runtime output {expected!r}\n{runtime.stdout}')
    except (AssertionError, OSError, subprocess.TimeoutExpired) as error:
        result.update(status='FAIL', error=str(error))
    result['milliseconds'] = (time.perf_counter() - start) * 1000
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    default_compiler = ROOT / 'bin/Release/sable.exe' if (ROOT / 'bin/Release/sable.exe').is_file() else ROOT / 'bin/Release/sable.exe'
    parser.add_argument('--compiler', '-c', type=Path, default=default_compiler)
    parser.add_argument('--jobs', '-j', type=int, default=1)
    parser.add_argument('--timeout', type=float, default=30)
    parser.add_argument('--filter', '-f', default='')
    parser.add_argument('--verbose', '-v', action='store_true')
    parser.add_argument('--count', action='store_true')
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    if args.jobs < 1 or args.timeout <= 0:
        parser.error('jobs and timeout must be positive')
    cases = json.loads((ROOT / 'tests/cases.json').read_text())
    cases = [c for c in cases if args.filter in c['Name']]
    if not cases:
        parser.error('no matching test cases')
    print(f'{len(cases)} cases selected', flush=True)
    if args.count:
        return 0
    compiler = args.compiler.resolve()
    if not compiler.is_file():
        parser.error(f'compiler not found: {compiler}')
    results = []
    # run only in a disposable copy; never delete outputs in the source tree
    with tempfile.TemporaryDirectory(prefix='sable-tests-') as temporary:
        workspace = Path(temporary)
        for folder in ('tests', 'stdlib'):
            shutil.copytree(ROOT / folder, workspace / folder,
                            ignore=shutil.ignore_patterns('*.exe', '*.dll', '__pycache__'))
        with isolated_runtime(workspace):
            pool = ThreadPoolExecutor(max_workers=args.jobs)
            try:
                futures = [pool.submit(execute, case, compiler, workspace, args.timeout) for case in cases]
                for future in as_completed(futures):
                    result = future.result()
                    results.append(result)
                    if args.verbose or result['status'] != 'PASS':
                        print(f"[{result['status']}] {result['name']} ({result['milliseconds']:.1f} ms)", flush=True)
                        if result['error']:
                            print(result['error'], flush=True)
            finally:
                pool.shutdown(wait=True, cancel_futures=True)
    counts = {s: sum(r['status'] == s for r in results) for s in ('PASS', 'FAIL', 'SKIP')}
    print(f"Results: {counts['PASS']} passed, {counts['FAIL']} failed, {counts['SKIP']} skipped")
    results.sort(key=lambda r: r['name'])
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps({'counts': counts, 'tests': results}, indent=2) + '\n')
    if os.environ.get('GITHUB_STEP_SUMMARY'):
        with open(os.environ['GITHUB_STEP_SUMMARY'], 'a', encoding='utf-8') as summary:
            summary.write(f"## Tests\n\n{counts['PASS']} passed, {counts['FAIL']} failed, {counts['SKIP']} skipped.\n\n")
            for result in results:
                if result['status'] == 'FAIL':
                    summary.write(f"### {result['name']}\n\n```text\n{result['error']}\n```\n\n")
    return 1 if counts['FAIL'] else 0


if __name__ == '__main__':
    sys.exit(main())
