#coding:utf-8

"""
ID:          issue-1590
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/1590
TITLE:       CHARACTER SET GBK is not installed
DESCRIPTION:
    Default character set is GBK
    Create Table T1(ID integer, FName Varchar(20); -- OK
    Commit; ---Error Message: CHARACTER SET GBK is not installed
JIRA:        CORE-1167
FBTEST:      bugs.core_1167
NOTES:
    [05.10.2023] pzotov
    Removed SHOW TABLE command for check result because its output often changes.
    Query to RDB$ tables is used instead.
"""

import pytest
from firebird.qa import *
import time

db = db_factory(charset = 'GBK')

test_script = """
    recreate table t1(
        f1 varchar(20) character set gbk
       ,f2 varchar(20) character set gbk collate gbk_unicode
    );
    commit;
    set count on;
    set list on;
    select
        rf.rdb$field_name as field_name
        ,f.rdb$character_set_id as field_cset_id
        ,f.rdb$collation_id as field_coll_id
        ,f.rdb$character_length as field_cset_len
        ,c.rdb$character_set_name as cset_name
        ,k.rdb$collation_name as field_collation
    from rdb$relation_fields rf
    join rdb$fields f on rf.rdb$field_source = f.rdb$field_name
    join rdb$character_sets c on f.rdb$character_set_id = c.rdb$character_set_id
    left join rdb$collations k on c.rdb$character_set_id = k.rdb$character_set_id and f.rdb$collation_id = k.rdb$collation_id
    where rf.rdb$relation_name = upper('T1')
    order by
         field_name
        ,field_cset_id
        ,field_coll_id
    ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    FIELD_NAME                      F1
    FIELD_CSET_ID                   67
    FIELD_COLL_ID                   0
    FIELD_CSET_LEN                  20
    CSET_NAME                       GBK
    FIELD_COLLATION                 GBK

    FIELD_NAME                      F2
    FIELD_CSET_ID                   67
    FIELD_COLL_ID                   1
    FIELD_CSET_LEN                  20
    CSET_NAME                       GBK
    FIELD_COLLATION                 GBK_UNICODE

    Records affected: 2
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
