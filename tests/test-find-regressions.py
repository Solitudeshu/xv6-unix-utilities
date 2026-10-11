#!/usr/bin/env python3
"""Run the unmodified course grader plus find regression tests in isolation.

Usage (from WSL): python3 tests/test-find-regressions.py --out /tmp/find-results
The real repository's source and fs.img are not modified by the test run.
"""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
from types import SimpleNamespace


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', required=True)
    parser.add_argument('--kernel-tests', action='store_true',
                        help='Also run the full kernel usertests in the temporary copy')
    args = parser.parse_args()
    repo = Path(__file__).resolve().parent.parent
    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    source_hash = hashlib.sha256((repo / 'user/find.c').read_bytes()).hexdigest()
    scratch = Path(tempfile.mkdtemp(prefix='xv6-find-regressions-'))
    data = subprocess.check_output(['git', 'archive', '--format=tar', 'HEAD'], cwd=repo)
    with tarfile.open(fileobj=io.BytesIO(data)) as archive:
        archive.extractall(scratch, filter='data')
    shutil.copy2(repo / 'user/find.c', scratch / 'user/find.c')
    os.chdir(scratch)
    print('Testing exact working-tree find.c in:', scratch, flush=True)

    with (out / 'make-grade.txt').open('w') as log:
        grade = subprocess.run(['make', '-s', 'V=@', 'grade'], stdout=log,
                               stderr=subprocess.STDOUT, timeout=180)
    grade_text = (out / 'make-grade.txt').read_text()
    print(grade_text, flush=True)
    if grade.returncode or 'Score: 156/156' not in grade_text:
        raise AssertionError('Course grader failed; see make-grade.txt')

    # Add the diagnostic driver to this temporary filesystem only.
    shutil.copy2(repo / 'tests/find-regression-driver.c', scratch / 'user/findcheck.c')
    driver = (scratch / 'user/findcheck.c').read_text()
    driver = driver.replace('#include "../user/find.c"', '#include "find.c"')
    (scratch / 'user/findcheck.c').write_text(driver)
    programs = subprocess.check_output([
        'make', '-s', '--eval', 'print-test-uprogs: ; @echo $(UPROGS)',
        'print-test-uprogs'], text=True).strip()
    with (out / 'driver-build.txt').open('w') as log:
        subprocess.run(['make', '-s', 'fs.img', 'UPROGS=' + programs + ' user/_findcheck'],
                       stdout=log, stderr=subprocess.STDOUT, check=True, timeout=90)

    sys.path.insert(0, str(scratch))
    import gradelib
    gradelib.options = SimpleNamespace(verbose=False, color='never')
    results = []

    def run_batch(name, commands, cases):
        if any(len(cmd) >= 99 for cmd in commands):
            raise AssertionError('Test command exceeds xv6 shell input limit')
        runner = gradelib.Runner()
        runner.run_qemu(gradelib.shell_script(commands), timeout=90)
        raw = runner.qemu.output.replace('\r', '')
        (out / (name + '.txt')).write_text(raw)
        if re.search(r'(?m)^usertrap\(\)|^panic:', raw):
            raise AssertionError(name + ': unexpected trap or panic')
        for label, expected in cases:
            match = re.search(r'(?ms)^CASE_' + re.escape(label) + r'\n(.*?)^END_' +
                              re.escape(label) + r'\n', raw)
            if not match:
                raise AssertionError(label + ': test did not finish')
            lines = [s for s in match.group(1).splitlines() if s and not s.startswith('$ ')]
            if callable(expected):
                expected(lines)
            elif lines != expected:
                raise AssertionError(f'{label}: expected {expected!r}, got {lines!r}')
            results.append(label)
            print('PASS', label, flush=True)

    def case(commands, cases, label, script, expected):
        commands += ['/echo CASE_' + label] + script + ['/echo END_' + label]
        cases.append((label, expected))

    commands = ['mkdir rf', 'mkdir rf/a', 'mkdir rf/b', 'echo data > rf/a/f',
                'echo fourteen > rf/a/abcdefghijklmn']
    cases = []
    for index, path in enumerate(['rf/a', 'rf/a/', 'rf/a///', './rf/a/', '/rf/a/']):
        normalized = path.rstrip('/')
        case(commands, cases, f'directory-slash-{index}', [f'find {path} a'], [normalized])
        case(commands, cases, f'exec-slash-{index}', [f'find {path} a -exec echo hit'],
             ['hit ' + normalized])
        case(commands, cases, f'child-slash-{index}', [f'find {path} f'], [normalized + '/f'])
    case(commands, cases, 'file-slash', ['find rf/a/f/// f'], ['rf/a/f'])
    case(commands, cases, 'DIRSIZ-name', ['find rf abcdefghijklmn'], ['rf/a/abcdefghijklmn'])
    case(commands, cases, 'missing-target', ['find rf missing'], [])
    case(commands, cases, 'missing-path', ['find not-present f'], ['find: cannot open not-present'])
    case(commands, cases, 'dot-prefix', ['find . f'], ['./rf/a/f'])
    case(commands, cases, 'dot-directory', ['find . .'], ['.'])
    for index, root in enumerate(['/', '///']):
        case(commands, cases, f'root-{index}', [f'find {root} README'], ['/README'])
        case(commands, cases, f'root-name-{index}', [f'find {root} /'], ['/'])

    unit_labels = ['allocation-first', 'allocation-after-first', 'read-first',
                   'read-after-entries', 'short-directory-read', 'repeat-no-leaks',
                   'main-allocation-failure', 'MAXARG-rejected', 'MAXARG-boundary',
                   'exec-failure-status']

    def check_units(lines):
        passed = [s[5:] for s in lines if s.startswith('PASS ')]
        assert passed == unit_labels, lines
        assert not any(s.startswith('FAIL ') for s in lines), lines

    case(commands, cases, 'fault-injection-and-args', ['findcheck'], check_units)
    run_batch('paths-and-errors', commands, cases)

    # xv6 sh has no PATH lookup: use absolute program names after chdir.
    commands = ['/mkdir dp', 'cd dp']
    cases = []
    paths = []
    for depth in range(1, 62):
        commands += ['/mkdir a', 'cd a']
        if depth in (10, 11, 20, 40, 50, 61):
            commands.append(f'/echo level{depth} > t')
            paths.append('/dp' + '/a' * depth + '/t')
            case(commands, cases, f'cat-depth-{depth}', ['/cat t'], [f'level{depth}'])
            case(commands, cases, f'find-depth-{depth}', ['/find /dp t'], paths.copy())
            case(commands, cases, f'exec-depth-{depth}', ['/find /dp t -exec /echo hit'],
                 ['hit ' + p for p in paths])
    case(commands, cases, 'all-deep-directories', ['/find /dp a'],
         ['/dp' + '/a' * depth for depth in range(1, 62)])
    # 127-character pathname is accepted; longer descendants fail gracefully.
    assert len(paths[-1]) == 127
    commands += ['/mkdir a', 'cd a', '/echo too-long > t']
    too_deep_dir = '/dp' + '/a' * 62
    case(commands, cases, 'MAXPATH-boundary', ['/find /dp t'],
         paths + ['find: path too long under ' + too_deep_dir])
    commands += ['cd /']
    run_batch('deep-trees', commands, cases)

    commands = ['mkdir wide']
    cases = []
    wide_paths = []
    for index in range(20):
        commands += [f'mkdir wide/n{index}', f'echo content > wide/n{index}/t']
        wide_paths.append(f'wide/n{index}/t')
    case(commands, cases, 'wide-order-and-duplicates', ['find wide t'], wide_paths)
    case(commands, cases, 'wide-exec-arguments', ['find wide t -exec echo hello world'],
         ['hello world ' + p for p in wide_paths])
    case(commands, cases, 'wide-exec-grep', ['find wide t -exec grep content'], ['content'] * 20)
    case(commands, cases, 'post-errors-shell-alive', ['echo alive'], ['alive'])
    run_batch('wide-trees', commands, cases)

    disassembly = subprocess.check_output(['riscv64-linux-gnu-objdump', '-d', 'user/_find'], text=True)
    (out / 'find-disassembly.txt').write_text(disassembly)
    match = re.search(r'<find>:\s*\n\s*[0-9a-f]+:\s*[0-9a-f]+\s+addi\s+sp,sp,-(\d+)', disassembly)
    frame = int(match.group(1)) if match else None
    assert frame is not None and frame < 128, frame
    if args.kernel_tests:
        print('Running full kernel usertests...', flush=True)
        with (out / 'kernel-usertests.txt').open('w') as log:
            kernel_tests = subprocess.run([sys.executable, '-u', 'test-xv6.py', 'usertests'],
                                          stdout=log, stderr=subprocess.STDOUT, timeout=660)
        kernel_output = (out / 'kernel-usertests.txt').read_text()
        if kernel_tests.returncode or 'ALL TESTS PASSED' not in kernel_output:
            raise AssertionError('Kernel usertests failed; see kernel-usertests.txt')
        print('Kernel usertests: ALL TESTS PASSED', flush=True)
    assert hashlib.sha256((repo / 'user/find.c').read_bytes()).hexdigest() == source_hash
    summary = {
        'grade': '156/156', 'course_tests_passed': 19,
        'regression_cases_passed': len(results) - 1,
        'unit_checks_passed': len(unit_labels),
        'additional_checks_passed': len(results) - 1 + len(unit_labels),
        'deepest_successful_subdirectories': 61, 'longest_successful_path': 127,
        'find_stack_frame_bytes': frame, 'source_sha256': source_hash,
        'kernel_usertests_passed': True if args.kernel_tests else None,
        'scratch_directory': str(scratch), 'cases': results,
    }
    (out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps(summary, indent=2), flush=True)


if __name__ == '__main__':
    main()
