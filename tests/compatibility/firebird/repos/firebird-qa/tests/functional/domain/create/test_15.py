#coding:utf-8

"""
ID:          domain.create-15
FBTEST:      functional.domain.create.15
TITLE:       CREATE DOMAIN - CHAR
DESCRIPTION: Simple domain creation based CHAR datatype
NOTES:
    [06.10.2023] pzotov
    1. Removed SHOW command for check result because its output often changes.
       It is enough for this test to obtain similar data from RDB tables.
    2. Made example more complex: create domain with charset differ than default one for DB, and collate differ than default for domain.
"""

import pytest
from firebird.qa import *

db = db_factory(charset = 'win1250')

test_script = """
    set bail on;
    set list on;
    alter character set win1252 set default collation pxw_swedfin;
    create view v_domain_info as
    select
        f.rdb$field_name as dm_name
        ,f.rdb$field_length as dm_size
        ,f.rdb$field_scale
        ,f.rdb$character_length as dm_char_len
        --,f.rdb$character_set_id as dm_cset_id
        --,f.rdb$collation_id as dm_coll_id
        ,f.rdb$default_source as dm_default
        ,f.rdb$null_flag as dm_not_null
        ,f.rdb$validation_source as dm_check_expr
        ,c.rdb$character_set_name as dm_cset_name
        --,c.rdb$default_collate_name as dm_default_coll_name
        --,k.rdb$base_collation_name
        ,k.rdb$collation_name as dm_coll_name
    from rdb$fields f
    join rdb$character_sets c on f.rdb$character_set_id = c.rdb$character_set_id
    join rdb$collations k on c.rdb$character_set_id = k.rdb$character_set_id and f.rdb$collation_id = k.rdb$collation_id
    where f.rdb$field_name = upper('dm_test')
    ;
    commit;
    create domain dm_test char(300) character set win1252 collate win_ptbr;
    commit;
    select v.* from v_domain_info v;

"""

act = isql_act('db', test_script)

expected_stdout = """
    DM_NAME                         DM_TEST
    DM_SIZE                         300
    RDB$FIELD_SCALE                 0
    DM_CHAR_LEN                     300
    DM_DEFAULT                      <null>
    DM_NOT_NULL                     <null>
    DM_CHECK_EXPR                   <null>
    DM_CSET_NAME                    WIN1252
    DM_COLL_NAME                    WIN_PTBR
"""

@pytest.mark.skip("Covered by 'test_all_datatypes_basic.py'")
@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
