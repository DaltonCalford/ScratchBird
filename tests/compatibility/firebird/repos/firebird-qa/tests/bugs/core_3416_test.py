#coding:utf-8

"""
ID:          issue-3779
ISSUE:       3779
TITLE:       Inserting Käse into a CHARACTER SET ASCII column succeeds
DESCRIPTION:
JIRA:        CORE-3416
FBTEST:      bugs.core_3416
"""

import pytest
from pathlib import Path
from firebird.qa import *

init_script = """
    create table tascii(s_ascii varchar(10) character set ascii);
    create table tlatin(s_latin varchar(10) character set latin1);
    commit;
"""

db = db_factory(init=init_script)

act = python_act('db', substitutions=[('After line .*', ''), ('[\t ]+', ' ')])

expected_stdout = """
    insert into tascii values ('Käse');

    Records affected: 0

    select s_ascii from tascii;
    Records affected: 0

    insert into tlatin values ('Käse');
    Records affected: 1

    select s_latin from tlatin;
    S_LATIN                         Käse
    Records affected: 1
"""

expected_stderr = """
Statement failed, SQLSTATE = 22018
arithmetic exception, numeric overflow, or string truncation
-Cannot transliterate character between character sets
After line 4 in file /tmp/pytest-of-pcisar/pytest-559/test_10/test_script.sql
"""

script_file = temp_file('test_script.sql')

@pytest.mark.intl
@pytest.mark.version('>=3')
def test_1(act: Action, script_file: Path):
    script_file.write_text("""
    set list on;
    set count on;
    set echo on;
    insert into tascii values ('Käse');
    select s_ascii from tascii;
    insert into tlatin values ('Käse');
    select s_latin from tlatin;
    """, encoding='cp1252')
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.isql(switches=[], input_file=script_file, charset='WIN1252')
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)



