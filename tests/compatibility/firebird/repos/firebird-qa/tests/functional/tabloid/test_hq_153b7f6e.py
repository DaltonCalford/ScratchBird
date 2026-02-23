#coding:utf-8

"""
ID:          n/a
TITLE:       Don't set rdb$relation_fields.rdb$collation_id if field uses explicit domain with the same charset and collation id
DESCRIPTION: 
    [17.02.2026] pzotov
    No explicitly specified ticket exists for this test.
    Regression was found occasionally in FB fork after message from one of customers.
    It relates to the functionality that [currently] was not yet merged in the vanilla FB.
    By a strange coincidence, the bug did not appeared during common QA runs (i.e. in any other tests).
    Discussed with Vlad, mailbox pz@ibase.ru, 17.02.2026.
    Checked on 6.0.0.1454, 5.0.4.1765.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create domain dm_user_u8ci as varchar(10) character set utf8 collate unicode_ci;
    create domain dm_info_utf8 as varchar(80) character set utf8 collate unicode;
    create domain dm_info_1250 as varchar(80) character set win1250 collate win_cz_ci_ai;
    create table test (
        usr_name_ini1 dm_user_u8ci,
        usr_info_ini1 dm_info_utf8,
        usr_info_1250 dm_info_1250 
    ); 
    commit;

    set list on;
    set count on;
    set width rdb$field_name 16;
    select r.rdb$field_name,r.rdb$collation_id from rdb$relation_fields r where r.rdb$relation_name='TEST' order by 1;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    RDB$FIELD_NAME       USR_INFO_1250
    RDB$COLLATION_ID     <null>
    RDB$FIELD_NAME       USR_INFO_INI1
    RDB$COLLATION_ID     <null>
    RDB$FIELD_NAME       USR_NAME_INI1
    RDB$COLLATION_ID     <null>

    Records affected: 3
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
