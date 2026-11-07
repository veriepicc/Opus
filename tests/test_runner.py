"""Harness checks that never execute Opus or Wine."""

import importlib.util
import json
from contextlib import nullcontext, redirect_stdout
import io
import os
from pathlib import Path
import subprocess
import signal
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET
from unittest.mock import MagicMock, patch

spec = importlib.util.spec_from_file_location('opus_runner', Path(__file__).with_name('run.py'))
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)

status_spec = importlib.util.spec_from_file_location(
    'ci_status', Path(__file__).resolve().parents[1] / 'tools/update-ci-status.py')
ci_status = importlib.util.module_from_spec(status_spec)
status_spec.loader.exec_module(ci_status)

benchmark_spec = importlib.util.spec_from_file_location(
    'benchmarks', Path(__file__).resolve().parents[1] / 'benchmarks/run.py')
benchmarks = importlib.util.module_from_spec(benchmark_spec)
benchmark_spec.loader.exec_module(benchmarks)


class RunnerTests(unittest.TestCase):
    def test_recovery_requires_fault_report_continuation_and_zero_exit(self):
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            (workspace / 'case.op').write_text('fixture')
            case = dict(Name='fixture', File='case.op', kind='stress', Healing='auto',
                        ExpectedExit=0, ExpectedRuntimeOutputContains=['ACCESS_VIOLATION', 'recovered'])
            output_dir = workspace / '.recovery/case'
            output_dir.mkdir(parents=True)
            (output_dir / 'case.exe').write_bytes(b'fixture')
            for code, output, status in (
                    (0, 'ACCESS_VIOLATION recovered', 'PASS'),
                    (1, 'ACCESS_VIOLATION recovered', 'FAIL'),
                    (0, 'ACCESS_VIOLATION', 'FAIL'),
                    (0, 'recovered', 'FAIL'),
                    (0xc0000005, 'ACCESS_VIOLATION recovered', 'FAIL')):
                with self.subTest(code=code, output=output), patch.object(runner, 'invoke', side_effect=[
                        subprocess.CompletedProcess([], 0, ''),
                        subprocess.CompletedProcess([], code, output)]):
                    self.assertEqual(runner.execute(case, Path('compiler'), workspace, 1)['status'], status)
            self.assertIn('healing: "auto"', (output_dir / 'opus.project').read_text())
            self.assertFalse((workspace / 'opus.project').exists())

    @unittest.skipIf(os.name == 'nt', 'posix child cleanup')
    def test_inherited_output_handle_does_not_block_parent(self):
        child_pid = None
        try:
            script = ('import subprocess,sys; '
                      'child=subprocess.Popen([sys.executable,"-c","import time; time.sleep(30)"]); '
                      'print(child.pid,flush=True)')
            proc = runner.invoke([sys.executable, '-c', script], Path.cwd(), 5)
            self.assertEqual(proc.returncode, 0)
            child_pid = int(proc.stdout.strip())
            os.kill(child_pid, 0)
        finally:
            if child_pid is not None:
                try:
                    os.kill(child_pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass

    def test_single_failure_sets_exit_code_and_report(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'tests').mkdir()
            (root / 'stdlib').mkdir()
            (root / 'compiler.exe').write_bytes(b'fixture')
            (root / 'tests/cases.json').write_text(json.dumps([{'Name': 'fixture', 'kind': 'compile', 'File': 'tests/case.op'}]))
            report = root / 'report.json'
            failure = dict(name='fixture', kind='compile', status='FAIL', error='test failure', milliseconds=1)
            with patch.object(runner, 'ROOT', root), \
                    patch.object(runner.sys, 'argv', ['run.py', '--compiler', str(root / 'compiler.exe'), '--report', str(report)]), \
                    patch.object(runner, 'isolated_runtime', return_value=nullcontext()), \
                    patch.object(runner, 'execute', return_value=failure), \
                    patch.dict(os.environ, {'GITHUB_STEP_SUMMARY': ''}), redirect_stdout(io.StringIO()):
                self.assertEqual(runner.main(), 1)
            self.assertEqual(json.loads(report.read_text())['counts']['FAIL'], 1)

    def test_gui_tests_never_launch(self):
        cases = json.loads(Path(__file__).with_name('cases.json').read_text())
        gui_names = ('test12_variadic_mismatch.op', 'test17e_msgbox_test.op',
                     'test19_str_to_c.op', 'test5_struct_layout.op')
        gui = [c for c in cases if Path(c.get('File', '')).name in gui_names]
        self.assertEqual(len(gui), 4)
        with patch.object(runner, 'invoke') as launch:
            for case in gui:
                self.assertEqual(runner.execute(case, Path('compiler'), Path('/unused'), 1)['status'], 'SKIP')
            launch.assert_not_called()

    def test_crashes_are_not_rejections(self):
        for code in (-11, 0xc0000005, 0xc00000fd, 127):
            with self.subTest(code=code), self.assertRaises(AssertionError):
                runner.check_rejection(subprocess.CompletedProcess([], code, 'expected error'), 'expected error')

    def test_wine_truncated_crash_status_is_not_a_pass(self):
        for code in (0, 1, 5):
            with self.subTest(code=code), self.assertRaises(AssertionError):
                runner.check_crash(subprocess.CompletedProcess([], code, 'wine: Unhandled page fault on read access'))

    def test_rejection_requires_diagnostic(self):
        runner.check_rejection(subprocess.CompletedProcess([], 1, 'expected error'), 'expected error')
        with self.assertRaises(AssertionError):
            runner.check_rejection(subprocess.CompletedProcess([], 1, 'wrong error'), 'expected error')

    def test_runtime_output_is_checked(self):
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            (workspace / 'case.op').write_text('fixture')
            case = dict(Name='fixture', File='case.op', kind='run', ExpectedExit=0, ExpectedOutput='expected')
            with patch.object(runner, 'invoke', return_value=subprocess.CompletedProcess([], 0, 'wrong')):
                self.assertEqual(runner.execute(case, Path('compiler'), workspace, 1)['status'], 'FAIL')

    @unittest.skipIf(os.name == 'nt', 'posix process groups')
    def test_timeout_kills_process_group(self):
        process = MagicMock(pid=1234)
        process.wait.side_effect = [subprocess.TimeoutExpired('fixture', 1), 0]
        with patch.object(runner.subprocess, 'Popen', return_value=process), patch.object(runner.os, 'killpg') as kill:
            with self.assertRaises(subprocess.TimeoutExpired):
                runner.invoke(['fixture'], '.', 1)
            kill.assert_called_once_with(1234, runner.signal.SIGKILL)

    @unittest.skipIf(os.name == 'nt', 'wine isolation')
    def test_missing_headless_dependencies_launches_nothing(self):
        with patch.object(runner.shutil, 'which', return_value=None), patch.object(runner.subprocess, 'Popen') as launch:
            with self.assertRaises(RuntimeError):
                with runner.isolated_runtime(Path('/unused')):
                    self.fail('must not start tests')
            launch.assert_not_called()

    @unittest.skipIf(os.name == 'nt', 'wine isolation')
    def test_dead_private_wine_server_fails_before_running_tests(self):
        display, server = MagicMock(), MagicMock()
        server.poll.return_value = 1
        with tempfile.TemporaryDirectory() as directory:
            with patch.object(runner.shutil, 'which', return_value='/mock'), \
                    patch.object(runner.subprocess, 'Popen', side_effect=[display, server]), \
                    patch.object(runner.select, 'select', return_value=([1], [], [])), \
                    patch.object(runner.os, 'read', return_value=b'99\n'), \
                    patch.object(runner, 'invoke', return_value=subprocess.CompletedProcess([], 0, '')), \
                    patch.object(runner.subprocess, 'run'):
                with self.assertRaisesRegex(RuntimeError, 'private Wine server exited'):
                    with runner.isolated_runtime(Path(directory)):
                        self.fail('must not execute tests without a live private server')
        server.wait.assert_called_once_with(timeout=10)
        display.terminate.assert_called_once()

    @unittest.skipIf(os.name == 'nt', 'wine isolation')
    def test_private_display_prefix_and_cleanup(self):
        display = MagicMock()
        server = MagicMock()
        server.poll.return_value = None
        original = dict(os.environ)
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            with patch.object(runner.shutil, 'which', return_value='/mock'), \
                    patch.object(runner.subprocess, 'Popen', side_effect=[display, server]) as launch, \
                    patch.object(runner.select, 'select', return_value=([1], [], [])), \
                    patch.object(runner.os, 'read', return_value=b'99\n'), \
                    patch.object(runner, 'invoke', return_value=subprocess.CompletedProcess([], 0, '')), \
                    patch.object(runner.subprocess, 'run') as cleanup:
                with runner.isolated_runtime(workspace):
                    self.assertEqual(os.environ['DISPLAY'], ':99')
                    self.assertEqual(os.environ['WINEPREFIX'], str(workspace / 'wine-prefix'))
                    self.assertNotIn('WAYLAND_DISPLAY', os.environ)
                self.assertEqual(cleanup.call_args.kwargs['env']['WINEPREFIX'], str(workspace / 'wine-prefix'))
                display.terminate.assert_called_once()
                server.wait.assert_called_once_with(timeout=10)
                self.assertEqual(launch.call_args_list[1].args[0], ['wineserver', '-f', '-p'])
        self.assertEqual(dict(os.environ), original)


class BenchmarkTests(unittest.TestCase):
    def test_runner_writes_artifacts_and_rejects_failed_builds(self):
        for code in (0, 1):
            with self.subTest(build_exit=code), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                source = root / 'fixture.op'
                source.write_text('fixture')
                fixture = dict(id='math', name='Math', desc='fixture', opus_file=source, c_macro=1)

                def compile_fixture(cmd, **kwargs):
                    if code == 0:
                        Path(cmd[-1]).with_suffix('.exe').write_bytes(b'fixture')
                    return subprocess.CompletedProcess(cmd, code, '', '')

                metrics = dict(min_ms=1, med_ms=1, avg_ms=1, exit_code=0)
                with patch.object(benchmarks, 'ROOT_DIR', root), \
                        patch.object(benchmarks, 'OUT_DIR', root / '.work'), \
                        patch.object(benchmarks, 'BENCHMARKS', [fixture]), \
                        patch.object(benchmarks, 'find_opus_bin', return_value=root / 'opus.exe'), \
                        patch.object(benchmarks, 'find_compiler', return_value=None), \
                        patch.object(benchmarks, 'ensure_tcc', return_value=None), \
                        patch.object(benchmarks, 'run_proc', side_effect=compile_fixture), \
                        patch.object(benchmarks, 'execute_binary', return_value=metrics) as execute, \
                        patch.object(sys, 'argv', ['run.py', '--iterations', '1']), \
                        patch.dict(os.environ, {'GITHUB_STEP_SUMMARY': ''}), redirect_stdout(io.StringIO()):
                    self.assertEqual(benchmarks.main(), code)
                report = json.loads((root / 'benchmark-results.json').read_text())
                self.assertEqual(bool(report['errors']), bool(code))
                self.assertTrue((root / 'benchmark-summary.md').is_file())
                self.assertTrue((root / 'BENCHMARKS.md').is_file())
                if code:
                    execute.assert_not_called()
                    self.assertIn('incomplete', (root / 'benchmark-summary.md').read_text())

    def test_publisher_generates_branch_reports_without_editing_source(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'tests').mkdir()
            (root / 'tests/cases.json').write_text('[{"Name": "fixture"}]')
            reports = root / 'reports'
            artifact = reports / 'benchmark-results'
            artifact.mkdir(parents=True)
            (artifact / 'benchmark-summary.md').write_text('## Performance\n\nmeasured summary\n')
            (artifact / 'BENCHMARKS.md').write_text('# Full measurements\n')
            readme = root / 'README.md'
            readme.write_text('# Opus\nsource README stays unchanged\n')
            output = root / 'published'
            with patch.object(ci_status, 'ROOT', root), patch.object(sys, 'argv', [
                    'update-ci-status.py', str(reports), '--output', str(output),
                    '--run-url', 'https://example.test/run', '--commit', 'abc123', '--build-result', 'success']):
                ci_status.main()
            text = (output / 'README.md').read_text()
            self.assertLess(text.index('measured summary'), text.index('Core CI Build Status'))
            self.assertIn('Source: `abc123`', text)
            self.assertEqual(readme.read_text(), '# Opus\nsource README stays unchanged\n')
            self.assertFalse((root / 'BENCHMARKS.md').exists())
            self.assertIn('# Full measurements', (output / 'BENCHMARKS.md').read_text())
            for name in ('build.svg', 'tests.svg', 'benchmarks.svg'):
                ET.fromstring((output / name).read_text())

    def report(self):
        return {
            'errors': [], 'host': 'Windows AMD64', 'iterations': 5, 'warmup': 2,
            'benchmarks': {'math': {'name': 'Math', 'results': {
                'Opus': {'compile_ms': 10, 'size_bytes': 4096, 'runtime_min_ms': 2},
                'MSVC': {'compile_ms': 100, 'size_bytes': 16384, 'runtime_min_ms': 3},
                'GCC': {'compile_ms': 20, 'size_bytes': 8192, 'runtime_min_ms': 1},
            }}},
        }

    def test_summary_uses_measurements_and_bolds_the_actual_winner(self):
        text = benchmarks.generate_readme_summary(self.report())
        self.assertIn('10.0× faster compilation', text)
        self.assertIn('4.0 KiB', text)
        self.assertIn('| math | 10.0 ms | **2.00 ms** | **1.5× faster** (3.00ms) | 2.0× slower (1.00ms) | — |', text)
        self.assertIn('5 measured runs', text)

    def test_no_speedup_claim_when_opus_is_slower_or_measurements_fail(self):
        report = self.report()
        report['benchmarks']['math']['results']['Opus']['compile_ms'] = 200
        self.assertNotIn('faster compilation', benchmarks.generate_readme_summary(report))
        report['errors'] = ['checksum mismatch']
        text = benchmarks.generate_readme_summary(report)
        self.assertIn('incomplete', text)
        self.assertNotIn('faster compilation', text)
        self.assertNotIn('KiB', text)

    def test_crashes_and_unstable_checksums_fail_measurement(self):
        for code, error in ((0xc0000005, ''), (-11, ''), (5, 'wine: Unhandled page fault')):
            with self.subTest(code=code), self.assertRaises(RuntimeError):
                benchmarks.check_execution(subprocess.CompletedProcess([], code, '', error))
        with patch.object(benchmarks, 'run_proc', side_effect=[
                subprocess.CompletedProcess([], 0, '', ''),
                subprocess.CompletedProcess([], 1, '', '')]):
            with self.assertRaises(RuntimeError):
                benchmarks.execute_binary(Path('fixture'), runs=2, warmup=0)

    def test_svg_contains_actual_benchmark_measurements(self):
        image = ci_status.svg(benchmarks.generate_readme_summary(self.report()), 'Benchmarks')
        text = ''.join(ET.fromstring(image).itertext())
        self.assertIn('10.0× faster compilation', text)
        self.assertIn('4.0 KiB', text)
        self.assertIn('1.00ms', text)


class ReadmeStatusTests(unittest.TestCase):
    def test_real_counts_and_missing_reports(self):
        with tempfile.TemporaryDirectory() as directory:
            reports = Path(directory)
            path = reports / 'tests-windows-2022/test-results.json'
            path.parent.mkdir()
            path.write_text(json.dumps({
                'counts': {'PASS': 1, 'FAIL': 1, 'SKIP': 1},
                'tests': [{'name': 'a', 'status': 'PASS'}, {'name': 'b', 'status': 'FAIL'},
                          {'name': 'c', 'status': 'SKIP'}]}))
            text = ci_status.render(reports, 'https://example.test/run', 'abc123', 'success', {'a', 'b', 'c'})
            self.assertIn('| Windows Server 2022 | Native x64 | 1 | 1 | 1 | [Failed]', text)
            self.assertIn('| Windows Server 2025 | Native x64 | — | — | — | [No report]', text)
            self.assertIn('| Release | [Passed]', text)
            self.assertIn('Source: `abc123`', text)

    def test_inconsistent_or_incomplete_reports_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            reports = Path(directory)
            path = reports / 'tests-windows-2022/test-results.json'
            path.parent.mkdir()
            for report in (
                    {'counts': {'PASS': 2, 'FAIL': 0, 'SKIP': 0},
                     'tests': [{'name': 'a', 'status': 'PASS'}]},
                    {'counts': {'PASS': 0, 'FAIL': 0, 'SKIP': 0}, 'tests': []}):
                path.write_text(json.dumps(report))
                with self.assertRaises(ValueError):
                    ci_status.render(reports, 'https://example.test/run', 'abc123', 'success', {'a'})

    def test_svg_is_valid_safe_and_preserves_counts(self):
        image = ci_status.svg('| Host | Passed | Failed | Skipped |\n| :--- | ---: | ---: | ---: |\n'
                              '| Windows <&> | 328 | 3 | 4 |', 'CI <results>')
        text = ''.join(ET.fromstring(image).itertext())
        self.assertIn('Windows <&>', text)
        self.assertEqual(ET.fromstring(image).attrib['width'], '1080')
        for count in ('328', '3', '4'):
            self.assertIn(count, text)
        self.assertNotIn('<foreignObject', image)
        self.assertNotIn('<script', image)

    def test_svg_rejects_inconsistent_table_columns(self):
        with self.assertRaises(ValueError):
            ci_status.svg('| Host | Result |\n| --- | --- |\n| Missing cell |', 'CI')


if __name__ == '__main__':
    unittest.main()
