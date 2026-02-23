#coding:utf-8

"""
ID:          issue-5834
ISSUE:       5834
TITLE:       Direct system table modifications are not completely prohibited
DESCRIPTION:
JIRA:        CORE-5567
FBTEST:      bugs.core_5567
NOTES:
    [01.07.2025] pzotov
    Refactored: suppressed name of system table as it has no matter for this test.
    Added appropriate substitutions.
    Checked on 6.0.0.881; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^;
    execute block as
    begin
      execute statement 'drop domain dm_test';
      when any do begin end
    end
    ^
    set term ;^
    commit;

    create domain dm_test numeric(18, 2);
    commit;

    set term ^;
    execute block as
        declare procedure hack as
        begin
            update rdb$fields set rdb$field_scale = -3 where rdb$field_name = upper('dm_test');
        end
    begin
        execute procedure hack;
    end
    ^
    set term ;^
    commit;

    set list on;
    select ff.rdb$field_scale domain_precision
    from rdb$fields ff
    where ff.rdb$field_name = upper('dm_test')
    ;

"""

substitutions = [('[ \t]+', ' '), (r'line(:)?\s+\d+', ''), ('.*At block.*', ''), ('(-)?At sub procedure.*', ''), ('for system table .*', 'for system table')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    Statement failed, SQLSTATE = 42000
    UPDATE operation is not allowed for system table
    DOMAIN_PRECISION -2
"""

@pytest.mark.version('>=3.0.3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

