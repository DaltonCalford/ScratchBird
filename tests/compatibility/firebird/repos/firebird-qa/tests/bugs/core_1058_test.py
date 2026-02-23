#coding:utf-8

"""
ID:          issue-1479
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/1479
TITLE:       ALTER DOMAIN and ALTER TABLE don't allow to change character set and/or collation
DESCRIPTION:
JIRA:        CORE-1058
FBTEST:      bugs.core_1058
NOTES:
    [05.10.2023] pzotov
    1. Removed SHOW command for check result because its output often changes.
       It is enough for this test to verify just absense of any error messages.
    2. There was issue about wrong change of columns collation, see:
       https://github.com/FirebirdSQL/firebird/issues/7924
       Fixed in 6.0.0.219, commit:
       https://github.com/FirebirdSQL/firebird/commit/11dec10f9fc079ed74d623211e01f465e45d6a7c

    [25.06.2025] pzotov
    Minimal snapshot number for 6.x: 6.0.0.863, see letter to Adriano, 24.06.2025 11:05. Fixed in commit:
    https://github.com/FirebirdSQL/firebird/commit/cbbbf3b94e7508806142eea0cd330ed4eedbbcdc

    Checked on 6.0.0.863; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory(charset = 'win1251')

test_script = """
    set bail on;
    set list on;
    create view v_domain_info as
    select
        f.rdb$field_name as dm_name
        ,f.rdb$field_length as dm_size
        ,f.rdb$character_length as dm_char_len
        ,f.rdb$character_set_id as dm_cset_id
        ,f.rdb$collation_id as dm_coll_id
        ,c.rdb$character_set_name as dm_cset_name
        --,c.rdb$default_collate_name as dm_default_coll_name
        --,k.rdb$base_collation_name
        ,k.rdb$collation_name as dm_coll_name
    from rdb$fields f
    join rdb$character_sets c on f.rdb$character_set_id = c.rdb$character_set_id
    join rdb$collations k on c.rdb$character_set_id = k.rdb$character_set_id and f.rdb$collation_id = k.rdb$collation_id
    where f.rdb$field_name = upper('dm_test')
    ;

    create view v_fields_info as
    select
        rf.rdb$field_name as field_name
        -- ,rf.rdb$field_source
        -- ,rf.rdb$field_position
        ,f.rdb$character_length as field_char_len
        ,f.rdb$character_set_id as field_cset_id
        ,f.rdb$collation_id as field_coll_id
        ,c.rdb$character_set_name as cset_name
        --,c.rdb$default_collate_name
        --,k.rdb$base_collation_name
        ,k.rdb$collation_name as field_collation
        --,k.rdb$collation_id
    from rdb$relation_fields rf
    join rdb$fields f on rf.rdb$field_source = f.rdb$field_name
    join rdb$character_sets c on f.rdb$character_set_id = c.rdb$character_set_id
    join rdb$collations k on c.rdb$character_set_id = k.rdb$character_set_id and f.rdb$collation_id = k.rdb$collation_id
    where rf.rdb$relation_name = upper('TEST')
    order by
         field_name
        ,field_cset_id
        ,field_coll_id
    ;

    ---------------------------------------------------------------
    alter character set utf8 set default collation unicode_ci;
    alter character set win1252 set default collation pxw_span;
    alter character set win1257 set default collation win1257_ee;
    commit;
     
    create domain dm_test varchar(10) character set win1252;
    create table test(
        fld_domain_defined dm_test
       ,fld_explicit_type1 varchar(10) character set win1257
       ,fld_explicit_type2 varchar(10) character set utf8
    );
    commit;

    select 'domain_info, point-1' as msg, v.* from v_domain_info v;
    select 'table info, point-1' as msg, v.* from v_fields_info v;

    ---------------------------------------------------------------

    alter character set utf8 set default collation unicode_ci_ai;
    alter character set win1252 set default collation pxw_swedfin;
    alter character set win1257 set default collation win1257_lv;
    commit;
     
    connect '$(DSN)';
     
    alter domain dm_test type varchar(10) character set utf8;
    commit;

    select 'domain_info, point-2' as msg, v.* from v_domain_info v;
    select 'table info, point-2' as msg, v.* from v_fields_info v;
"""

act = isql_act('db', test_script)

expected_stdout = """
    MSG                             domain_info, point-1
    DM_NAME                         DM_TEST
    DM_SIZE                         10
    DM_CHAR_LEN                     10
    DM_CSET_ID                      53
    DM_COLL_ID                      4
    DM_CSET_NAME                    WIN1252
    DM_COLL_NAME                    PXW_SPAN

    MSG                             table info, point-1
    FIELD_NAME                      FLD_DOMAIN_DEFINED
    FIELD_CHAR_LEN                  10
    FIELD_CSET_ID                   53
    FIELD_COLL_ID                   4
    CSET_NAME                       WIN1252
    FIELD_COLLATION                 PXW_SPAN
    
    MSG                             table info, point-1
    FIELD_NAME                      FLD_EXPLICIT_TYPE1
    FIELD_CHAR_LEN                  10
    FIELD_CSET_ID                   60
    FIELD_COLL_ID                   1
    CSET_NAME                       WIN1257
    FIELD_COLLATION                 WIN1257_EE
    
    MSG                             table info, point-1
    FIELD_NAME                      FLD_EXPLICIT_TYPE2
    FIELD_CHAR_LEN                  10
    FIELD_CSET_ID                   4
    FIELD_COLL_ID                   3
    CSET_NAME                       UTF8
    FIELD_COLLATION                 UNICODE_CI

    
    MSG                             domain_info, point-2
    DM_NAME                         DM_TEST
    DM_SIZE                         40
    DM_CHAR_LEN                     10
    DM_CSET_ID                      4
    DM_COLL_ID                      4
    DM_CSET_NAME                    UTF8
    DM_COLL_NAME                    UNICODE_CI_AI

    MSG                             table info, point-2
    FIELD_NAME                      FLD_DOMAIN_DEFINED
    FIELD_CHAR_LEN                  10
    FIELD_CSET_ID                   4
    FIELD_COLL_ID                   4
    CSET_NAME                       UTF8
    FIELD_COLLATION                 UNICODE_CI_AI

    MSG                             table info, point-2
    FIELD_NAME                      FLD_EXPLICIT_TYPE1
    FIELD_CHAR_LEN                  10
    FIELD_CSET_ID                   60
    FIELD_COLL_ID                   1
    CSET_NAME                       WIN1257
    FIELD_COLLATION                 WIN1257_EE

    MSG                             table info, point-2
    FIELD_NAME                      FLD_EXPLICIT_TYPE2
    FIELD_CHAR_LEN                  10
    FIELD_CSET_ID                   4
    FIELD_COLL_ID                   3
    CSET_NAME                       UTF8
    FIELD_COLLATION                 UNICODE_CI
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

