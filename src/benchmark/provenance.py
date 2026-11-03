#!/usr/bin/env python3
"""Capture and validate benchmark provenance without changing project inputs."""
import csv
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
DATASETS = {'patients_1K.csv': 1000, 'patients_10K.csv': 10000, 'patients_100K.csv': 100000}


def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def git(*args):
    return subprocess.check_output(['git', '-C', str(ROOT), *args])


def snapshot():
    # Include tracked and non-ignored untracked files, including dirty file contents.
    paths = sorted(set(git('ls-files', '-z', '--cached', '--others', '--exclude-standard').split(b'\0')) - {b''})
    files = {}
    for raw in paths:
        name = os.fsdecode(raw)
        path = ROOT / name
        if path.is_symlink():
            files[name] = {'symlink': os.readlink(path)}
        elif path.is_file():
            files[name] = {'sha256': sha(path)}
        else:
            files[name] = {'state': 'missing'}
    return {'commit': git('rev-parse', 'HEAD').decode().strip(),
            'status_porcelain': git('status', '--porcelain=v1', '--untracked-files=all').decode('utf-8', 'surrogateescape'),
            'files': files}


def write(path, obj):
    Path(path).write_text(json.dumps(obj, indent=2, sort_keys=True, ensure_ascii=True) + '\n', encoding='utf-8')


def main():
    action = sys.argv[1]
    if action == 'snapshot':
        write(sys.argv[2], snapshot())
        return
    stage, build_snapshot, data_dir, compiler, config, seal, cpu, executable = sys.argv[2:]
    stage, data_dir = Path(stage), Path(data_dir)
    current = snapshot()
    if current != json.loads(Path(build_snapshot).read_text(encoding='utf-8')):
        raise RuntimeError('Source snapshot differs from the build; rebuild before benchmarking')
    baseline = stage / '.provenance-baseline.json'
    datasets = {}
    for name, expected in DATASETS.items():
        path = data_dir / name
        with path.open(newline='', encoding='utf-8') as f:
            count = sum(1 for _ in csv.reader(f)) - 1
        if count != expected:
            raise RuntimeError(f'Unexpected dataset count: {name}')
        datasets[name] = {'sha256': sha(path), 'records': count}
    if action == 'start':
        write(baseline, {'snapshot': current, 'datasets': datasets, 'executable_sha256': sha(Path(executable))})
        return
    if action != 'finish':
        raise ValueError('Unknown provenance action')
    start = json.loads(baseline.read_text(encoding='utf-8'))
    if start != {'snapshot': current, 'datasets': datasets, 'executable_sha256': sha(Path(executable))}:
        raise RuntimeError('Source, executable or datasets changed during the benchmark')
    try:
        import numpy
        numpy_version = numpy.__version__
    except ImportError:
        numpy_version = 'unknown'
    csv_hashes = {f'scenario{i}.csv': sha(stage / f'scenario{i}.csv') for i in range(1, 6)}
    canonical = json.dumps(current, sort_keys=True, ensure_ascii=True, separators=(',', ':')).encode()
    manifest = {
        'source': {'git_commit': current['commit'], 'git_state': 'dirty' if current['status_porcelain'] else 'clean',
                   'snapshot_sha256': hashlib.sha256(canonical).hexdigest(), 'snapshot': current,
                   'executable_sha256': start['executable_sha256']},
        'datasets': datasets,
        'generator': {'configured_default_seed': 42, 'configured_default_master_size': 100000,
                      'historical_seed': 'unknown', 'historical_master_size': 'unknown',
                      'historical_python': 'unknown', 'historical_numpy': 'unknown',
                      'observed_python': platform.python_version(), 'observed_numpy': numpy_version},
        'environment': {'seal': seal, 'compiler': compiler, 'build_configuration': config,
                        'os': platform.system() + ' ' + platform.release(), 'architecture': platform.machine(), 'cpu': cpu or 'unknown'},
        'ckks': {'scenarios_1_2_4': {'poly_modulus_degree': 8192, 'coeff_modulus_bits': [60, 40, 40, 60], 'scale': '2^40'},
                 'scenarios_3_5': {'poly_modulus_degree': 16384, 'coeff_modulus_bits': [60, 35, 35, 35, 35, 35, 35, 35, 35, 35, 60], 'scale': '2^35'}},
        'methodology': {'warmups': 5, 'measured_runs': 30}, 'result_csv_sha256': csv_hashes}
    write(stage / 'manifest.json', manifest)


if __name__ == '__main__':
    main()
