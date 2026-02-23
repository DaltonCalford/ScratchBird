#coding:utf-8

"""
ID:          issue-2847
ISSUE:       2847
TITLE:       String values in error messages are not converted to connection charset
DESCRIPTION:
JIRA:        CORE-2431
FBTEST:      bugs.core_2431
NOTES:
    [26.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='core2431.fbk')

test_script = """
    set list on;
    select c.rdb$character_set_name as connection_cset
    from mon$attachments a
    join rdb$character_sets c on a.mon$character_set_id = c.rdb$character_set_id
    where a.mon$attachment_id = current_connection;

    set term ^;
    execute block as
    begin
      exception ex_bad_remainder using (-8);
    end
    ^
    set term ;^
    commit;
"""

substitutions = [ ('[ \t]+', ' '), ('-At block line: [\\d]+, col: [\\d]+', '-At block line')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = """
    CONNECTION_CSET WIN1251
    Statement failed, SQLSTATE = HY000
    exception 1
    -EX_BAD_REMAINDER
    -Новый остаток изделия будет отрицательным: -8
    -At block line
"""

expected_stdout_6x = """
    CONNECTION_CSET WIN1251
    Statement failed, SQLSTATE = HY000
    exception 1
    -"PUBLIC"."EX_BAD_REMAINDER"
    -Новый остаток изделия будет отрицательным: -8
    -At block line
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(charset='win1251', combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
