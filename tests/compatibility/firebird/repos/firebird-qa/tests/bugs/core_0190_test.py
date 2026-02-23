#coding:utf-8

"""
ID:          issue-517
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/517
TITLE:       trigger with except-s on view with union
NOTES:
    Test should be added during initial migration from fbtest but did not, the reason is unknown.
    Noted by Anton Zuev: https://github.com/FirebirdSQL/firebird-qa/pull/5
    Checked on 3.0.11.33665, 4.0.3.2904, 5.0.0.970
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table a (id integer);

    create view v1 (vid) as
    select id from a
    union all
    select id from a;

    create exception exa 'foo!..';
    create exception exb 'bar!..';
    commit;

    set term ^;
    create trigger tv1 for v1 active before update as
    begin
        if (new.vid = 0) then
            exception exa;
        else
            exception exb;
    end
    ^
    set term ;^
    commit;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = ''
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
