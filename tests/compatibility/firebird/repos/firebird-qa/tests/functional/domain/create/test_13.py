#coding:utf-8

"""
ID:          domain.create-13
FBTEST:      functional.domain.create.13
TITLE:       CREATE DOMAIN - NUMERIC
DESCRIPTION: Simple domain creation based NUMERIC datatype
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set count on;
    create view v_domain_info as
    select
        f.rdb$field_name as dm_name
        ,f.rdb$field_length as dm_size
        ,f.rdb$field_scale as dm_scale
        ,f.rdb$field_precision dm_prec
        ,f.rdb$field_type as dm_type
        ,f.rdb$field_sub_type as dm_subt
        ,f.rdb$dimensions as dm_dimens
        ,f.rdb$null_flag as dm_null
        ,f.rdb$validation_source as dm_check_expr
        ,f.rdb$character_length as dm_char_len
        ,f.rdb$character_set_id as dm_cset_id
        ,f.rdb$collation_id as dm_coll_id
        ,c.rdb$character_set_name as dm_cset_name
        ,c.rdb$default_collate_name as dm_default_coll_name
        ,k.rdb$base_collation_name
        ,k.rdb$collation_name as dm_coll_name
    from rdb$fields f
    left join rdb$character_sets c on f.rdb$character_set_id = c.rdb$character_set_id
    left join rdb$collations k on c.rdb$character_set_id = k.rdb$character_set_id and f.rdb$collation_id = k.rdb$collation_id
    where f.rdb$field_name = upper('dm_test')
    ;
    create domain dm_test NUMERIC(18,18);
    commit;
    select * from v_domain_info;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    DM_NAME                         DM_TEST
    DM_SIZE                         8
    DM_SCALE                        -18
    DM_PREC                         18
    DM_TYPE                         16
    DM_SUBT                         1
    DM_DIMENS                       <null>
    DM_NULL                         <null>
    DM_CHECK_EXPR                   <null>
    DM_CHAR_LEN                     <null>
    DM_CSET_ID                      <null>
    DM_COLL_ID                      <null>
    DM_CSET_NAME                    <null>
    DM_DEFAULT_COLL_NAME            <null>
    RDB$BASE_COLLATION_NAME         <null>
    DM_COLL_NAME                    <null>
    Records affected: 1
"""

@pytest.mark.skip("Covered by 'test_all_datatypes_basic.py'")
@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
