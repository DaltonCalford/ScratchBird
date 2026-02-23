#coding:utf-8

"""
ID:          issue-2842
ISSUE:       2842
TITLE:       Alter table not respecting collation
DESCRIPTION:
JIRA:        CORE-2426
FBTEST:      bugs.core_2426
NOTES:
    [05.10.2023] pzotov
    Removed SHOW command for check result because its output often changes. Query to RDB$ tables is used instead.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^;
    create or alter procedure sp_get_table_fields_info(a_table rdb$relation_name) returns (
         table_name type of column rdb$relations.rdb$relation_name
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
    create domain dm_test_1252 varchar(14) character set win1252;
    create domain dm_test_iso8859_1 varchar(14) character set iso8859_1 collate pt_br;

    create table tmain (
        field_a varchar(14) character set win1252 not null collate win1252
    );
    alter table tmain alter field_a type dm_test_iso8859_1;
    alter table tmain add primary key (field_a);

    create table tdetl (field_b dm_test_iso8859_1 references tmain(field_a));
    --show table t; -- colattion changes to de_de
    commit;

    set list on;
    set count on;
    select * from sp_get_table_fields_info('TMAIN');
    select * from sp_get_table_fields_info('TDETL');
"""

act = isql_act('db', test_script)

expected_stdout = """
    TABLE_NAME                      TMAIN
    FIELD_NAME                      FIELD_A
    FIELD_CSET_ID                   21
    FIELD_COLL_ID                   16
    FIELD_CSET_LEN                  14
    FIELD_CSET_NAME                 ISO8859_1
    FIELD_COLL_NAME                 PT_BR
    Records affected: 1
    TABLE_NAME                      TDETL
    FIELD_NAME                      FIELD_B
    FIELD_CSET_ID                   21
    FIELD_COLL_ID                   16
    FIELD_CSET_LEN                  14
    FIELD_CSET_NAME                 ISO8859_1
    FIELD_COLL_NAME                 PT_BR
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

