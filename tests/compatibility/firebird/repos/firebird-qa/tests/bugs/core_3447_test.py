#coding:utf-8

"""
ID:          issue-3808
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/3808
TITLE:       Collation is not installed with icu > 4.2
DESCRIPTION:
JIRA:        CORE-3447
FBTEST:      bugs.core_3447
NOTES:
    [05.10.2023] pzotov
    1. Removed SHOW command for check result because its output often changes. Query to RDB$ tables is used instead.
    2. Added creation of custom utf8-based collation (among others)
    3. Increased min_version to 4.0 because FB 3.x can not create collation with provided attributes and raise error:
        Statement failed, SQLSTATE = HY000
        unsuccessful metadata update
        -CREATE COLLATION CUSTOM_COLL_FRFR_CI failed
        -Invalid collation attributes
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set term ^;
    create or alter procedure sp_get_table_fields_info(a_table rdb$relation_name) returns (
         table_name type of column rdb$relations.rdb$relation_name
        ,field_pos type of column rdb$relation_fields.rdb$field_position
        ,field_name type of column rdb$relation_fields.rdb$field_name
        ,field_cset_id type of column rdb$fields.rdb$character_set_id
        ,field_coll_id type of column rdb$fields.rdb$collation_id
        ,field_cset_len type of column rdb$fields.rdb$character_length
        ,field_cset_name type of column rdb$character_sets.rdb$character_set_name
        ,field_coll_name type of column rdb$collations.rdb$collation_name
    ) as
    begin
        for
            select
                 :a_table
                ,rf.rdb$field_position as field_pos
                ,rf.rdb$field_name as field_name
                ,f.rdb$character_set_id as field_cset_id
                ,f.rdb$collation_id as field_coll_id
                ,f.rdb$character_length as field_cset_len
                ,c.rdb$character_set_name as cset_name
                ,k.rdb$collation_name as field_coll_name
            from rdb$relation_fields rf
            join rdb$fields f on rf.rdb$field_source = f.rdb$field_name
            join rdb$character_sets c on f.rdb$character_set_id = c.rdb$character_set_id
            join rdb$collations k on c.rdb$character_set_id = k.rdb$character_set_id and f.rdb$collation_id = k.rdb$collation_id
            where rf.rdb$relation_name = upper(:a_table)
            order by
                 field_name
                ,field_cset_id
                ,field_coll_id
            into
                 table_name
                ,field_pos
                ,field_name
                ,field_cset_id
                ,field_coll_id
                ,field_cset_len
                ,field_cset_name
                ,field_coll_name
        do begin
            suspend;
        end
    end ^
    set term ;^
    commit;

    create collation custom_coll_frfr_ci for utf8 from unicode case insensitive 'LOCALE=fr_FR';
    commit;
    recreate table test(
        name1 varchar(32) character set utf8 collate ucs_basic,
        name2 varchar(32) character set utf8 collate unicode,
        name3 varchar(32) character set utf8 collate unicode_ci,
        name4 varchar(32) character set utf8 collate unicode_ci_ai,
        name5 varchar(32) character set utf8 collate custom_coll_frfr_ci
    );
    commit;
    set list on;
    select 
         table_name
        ,field_pos
        ,field_name
        ,field_cset_name
        ,field_coll_name
    from sp_get_table_fields_info('test')
    order by table_name, field_pos
    ;
    -- Passed on: WI-V2.5.5.26871, WI-T3.0.0.31844; LI-V2.5.3.26788, LI-T3.0.0.31842
"""

act = isql_act('db', test_script)

expected_stdout = """
    TABLE_NAME                      test
    FIELD_POS                       0
    FIELD_NAME                      NAME1
    FIELD_CSET_NAME                 UTF8
    FIELD_COLL_NAME                 UCS_BASIC
    TABLE_NAME                      test
    FIELD_POS                       1
    FIELD_NAME                      NAME2
    FIELD_CSET_NAME                 UTF8
    FIELD_COLL_NAME                 UNICODE
    TABLE_NAME                      test
    FIELD_POS                       2
    FIELD_NAME                      NAME3
    FIELD_CSET_NAME                 UTF8
    FIELD_COLL_NAME                 UNICODE_CI
    TABLE_NAME                      test
    FIELD_POS                       3
    FIELD_NAME                      NAME4
    FIELD_CSET_NAME                 UTF8
    FIELD_COLL_NAME                 UNICODE_CI_AI
    TABLE_NAME                      test
    FIELD_POS                       4
    FIELD_NAME                      NAME5
    FIELD_CSET_NAME                 UTF8
    FIELD_COLL_NAME                 CUSTOM_COLL_FRFR_CI
"""

@pytest.mark.version('>=4')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
