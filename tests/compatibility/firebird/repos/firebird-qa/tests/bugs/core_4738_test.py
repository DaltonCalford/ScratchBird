#coding:utf-8

"""
ID:          issue-5043
ISSUE:       5043
TITLE:       Command "Alter table <T> alter <C> type <domain_>" does not work: "BLR syntax error: expected valid BLR code ..."
DESCRIPTION:
JIRA:        CORE-4738
FBTEST:      bugs.core_4738
NOTES:
    [30.06.2025] pzotov
    This issue caused by regression when core-4733 ( https://github.com/FirebirdSQL/firebird/issues/5039 ) was fixed.
    Commit for core-4733 (to check regression): http://sourceforge.net/p/firebird/code/61241 2015-04-05 02:24:40 +0000
    Confirmed bug on 3.0.0.31771.

    Replaced 'SHOW TABLE' command with qery to RDB tables that displays field_type and field_length instead.
    Checked on 6.0.0.881; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create domain dm_id bigint;
    commit;

    create table test(num smallint);
    commit;

    create view v_info as
    select
        rf.rdb$field_name fld_name
        ,f.rdb$field_type fld_type
        ,f.rdb$field_length fld_length
        ,f.rdb$field_scale fld_scale
    from rdb$relation_fields rf
    left join rdb$fields f on rf.rdb$field_source = f.rdb$field_name
    where rf.rdb$relation_name = 'TEST';
    commit;

    select 'point-1' msg, v.* from v_info v;

    alter table test alter num type dm_id;
    commit;

    select 'point-2' msg, v.* from v_info v;

    insert into test(num) values(-2147483648);
    select * from test;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    MSG                             point-1
    FLD_NAME                        NUM
    FLD_TYPE                        7
    FLD_LENGTH                      2
    FLD_SCALE                       0

    MSG                             point-2
    FLD_NAME                        NUM
    FLD_TYPE                        16
    FLD_LENGTH                      8
    FLD_SCALE                       0

    NUM                             -2147483648
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
