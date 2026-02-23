#coding:utf-8

"""
ID:          issue-6565
ISSUE:       6565
TITLE:       Alter domain operation for domain with collation specified revert its collation to database default
DESCRIPTION:
  NB: current behaviour contradicts ticket notes!
  FB *must* change collation of altered domain, see issue by dimitr in the ticket 06/Jun/20 08:04 AM:
  "character set is not a part of the domain itself, it's a part of its data type".
JIRA:        CORE-6324
FBTEST:      bugs.core_6324
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

test_script = """
    set width fld_name 20;
    set width cset_name 20;
    set width coll_name 20;
    set list on;

    recreate view v_test as
    select
        f.rdb$field_name fld_name
        --,f.rdb$character_set_id cset_id
        --,f.rdb$collation_id coll_id
        ,c.rdb$character_set_name cset_name
        ,s.rdb$collation_name coll_name
    from rdb$fields f
    left join rdb$character_sets c on f.rdb$character_set_id = c.rdb$character_set_id
    left join rdb$collations s on
        f.rdb$collation_id = s.rdb$collation_id
        and c.rdb$character_set_id = s.rdb$character_set_id
    where f.rdb$field_name = upper('dm_test');

    create domain dm_test varchar(1000) character set win1250 collate pxw_plk;
    commit;

    select 'POINT-1' msg, v.* from v_test v;
    commit;

    -- Required result: collation must change from PXW_PLK to WIN1250.
    alter domain dm_test type varchar(1000) character set win1250;
    commit;

    select 'POINT-2' msg, v.* from v_test v;
    commit;

    -- Required result: collation must be REMOVED:
    alter domain dm_test type varchar(1000);
    commit;

    select 'POINT-3' msg, v.* from v_test v;

"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    MSG                             POINT-1
    FLD_NAME                        DM_TEST
    CSET_NAME                       WIN1250
    COLL_NAME                       PXW_PLK

    MSG                             POINT-2
    FLD_NAME                        DM_TEST
    CSET_NAME                       WIN1250
    COLL_NAME                       WIN1250

    MSG                             POINT-3
    FLD_NAME                        DM_TEST
    CSET_NAME                       UTF8
    COLL_NAME                       UTF8

"""

@pytest.mark.version('>=2.5.9')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
