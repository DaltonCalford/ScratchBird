#coding:utf-8

"""
ID:          build.snapshot-files-checklist
TITLE:       Check list of files in the current Firebird snapshot
DESCRIPTION:
  Get list of all files from FB_HOME and compare it with expected for each major FB version.
  Test will allert about files which existed before and now are missed.
  Test will NOT alert if new files was added to FB distributive by developers.
  In such case we have to adjust expected list manually (see variable 'check_set').

  Idea about this test originates to CORE-6424 (missed employee.fdb in some intermediate build),
  but it seems that there were several other tickets about the same (missing some of necessary files).
NOTES:
[08.02.2022] pcisar
  Manifest files are not actual. Also, they should not contain PDB files!
FBTEST:      functional.basic.build.snapshot_files_check_list
"""

from __future__ import annotations
from typing import Set
import pytest
from pathlib import Path
from firebird.qa import *

# Common code

db = db_factory()

expected_stdout = """
    OK: found all files from check set.
"""

def check_files(act: Action, expected: Set[Path]) -> None:
    actual = set()
    p: Path = None
    for p in act.home_dir.rglob('*'):
        p = p.relative_to(act.home_dir)
        if not str(p).startswith('doc') and p in expected:
            actual.add(p)
    if expected.issubset(actual):
        print('OK: found all files from check set.')
    else:
        print('ERROR! Missed some files from check set:')
        for p in (expected - actual):
            print(p)

# version: 3.0.7

act = python_act('db')

@pytest.mark.skip("FIXME: see notes")
@pytest.mark.version('>=3.0.7,<4.0')
@pytest.mark.platform('Windows')
def test_1(act: Action, capsys):
    manifest = act.files_dir / 'build-files-30.txt'
    expected_set = set([Path(s) for s in manifest.read_text().splitlines()])
    check_files(act, expected_set)
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout

# version: 4.0

@pytest.mark.skip("FIXME: see notes")
@pytest.mark.version('>=4.0,<5.0')
@pytest.mark.platform('Windows')
def test_2(act: Action, capsys):
    manifest = act.files_dir / 'build-files-40.txt'
    expected_set = set([s for s in manifest.read_text().splitlines()])
    check_files(act, expected_set)
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout

# version: 5.0

@pytest.mark.skip("FIXME: see notes")
@pytest.mark.version('>=5.0')
@pytest.mark.platform('Windows')
def test_3(act: Action, capsys):
    manifest = act.files_dir / 'build-files-50.txt'
    expected_set = set([s for s in manifest.read_text().splitlines()])
    check_files(act, expected_set)
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout

if __name__ == '__main__':
    check_list = [
         ]
    f = Path('/home/job/python/projects/firebird-qa/files/build-files-50.txt')
    f.write_text('\n'.join(check_list))
