#coding:utf-8

"""
ID:          table.alter-08
TITLE:       ALTER TABLE - DROP column
DESCRIPTION:
FBTEST:      functional.table.alter.08
NOTES:
    [12.07.2025] pzotov
    Removed 'SHOW' command.
    Statement 'ALTER TABLE...' followed by commit must allow further actions related to changed DDL.
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.949; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set count on;
    create table test(id int default 1, f01 int default 2 unique, f02 int default 3 references test(f01));
    commit;

    alter table test drop f01; -- must fail because f02 references on it
    alter table test drop f02, drop f01; -- must pass
    commit;

    insert into test default values; -- must pass
    select * from test; -- only one column remains now
"""

substitutions = [('[ \t]+', ' '), (r'cancelled by trigger \(\d+\)', 'cancelled by trigger'), ('(-)?At trigger .*', 'At trigger')]
act = isql_act('db', test_script, substitutions = substitutions)

@pytest.mark.version('>=3')
def test_1(act: Action):

    expected_stdout_5x = f"""
        Statement failed, SQLSTATE = 27000
        unsuccessful metadata update
        -ALTER TABLE TEST failed
        -action cancelled by trigger (1) to preserve data integrity
        -Cannot delete PRIMARY KEY being used in FOREIGN KEY definition.
        -At trigger 'RDB$TRIGGER_23'
        Records affected: 1
        ID 1
        Records affected: 1
    """
    expected_stdout_6x = f"""
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -ALTER TABLE "PUBLIC"."TEST" failed
        -Cannot delete PRIMARY KEY being used in FOREIGN KEY definition.
        Records affected: 1
        ID 1
        Records affected: 1
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
