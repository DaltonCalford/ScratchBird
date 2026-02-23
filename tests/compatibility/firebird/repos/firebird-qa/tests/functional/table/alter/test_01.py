#coding:utf-8

"""
ID:          table.alter-01
TITLE:       ALTER TABLE - ADD column
DESCRIPTION:
FBTEST:      functional.table.alter.01
NOTES:
    [06.10.2023] pzotov
    1. Removed SHOW command for check result because its output often changes.
       It is enough for this test to obtain similar data from RDB tables.
       Created view and stored function to obtain type name by rdb$fields.rdb$field_type and .rdb$field_sub_type.
    2. Made example more complex: add column with charset differ than default one for DB (collate not specified, i.e. it must be default for charset).
"""

import pytest
from firebird.qa import *

db = db_factory(charset = 'utf8')

test_script = """
    set bail on;
    set list on;

    alter character set win1252 set default collation pxw_swedfin;
    commit;
    set term ^;
    create or alter function fn_get_type_name(a_type smallint, a_subtype smallint) returns varchar(2048) as
        declare ftype varchar(2048);
    begin
        ftype = 
            decode( a_type
                    ,  7, decode(coalesce(a_subtype,0),  0, 'smallint',             1, 'numeric', 'unknown') -- 1 => small numerics [-327.68..327.67] (i.e. with mantissa that can be fit in -32768 ... 32767)
                    ,  8, decode(coalesce(a_subtype,0),  0, 'integer',              1, 'numeric', 2, 'decimal', 'unknown') -- 1: for numeric with mantissa >= 32768 and up to 9 digits, 2: for decimals up to 9 digits
                    , 10, 'float'
                    , 12, 'date'
                    , 13, 'time without time zone'
                    , 14, decode(coalesce(a_subtype,0),  0, 'char',                 1, 'binary', 'unknown')
                    , 16, decode(coalesce(a_subtype,0),  0, 'bigint',               1, 'numeric', 2, 'decimal', 'unknown')
                    , 23, 'boolean'
                    , 24, 'decfloat(16)'
                    , 25, 'decfloat(34)'
                    , 26, 'int128'
                    , 27, 'double precision' -- also for numeric and decimal, both with size >= 10, if sql_dialect = 1
                    , 28, 'time with time zone'
                    , 29, 'timestamp with time zone'
                    , 35, 'timestamp without time zone'
                    , 37, decode(coalesce(a_subtype,0),  0, 'varchar',              1, 'varbinary', 'unknown')
                    ,261, decode(coalesce(a_subtype,0),  0, 'blob sub_type binary', 1, 'blob sub_type text', 'unknown')
                  );
        if (ftype = 'unknown') then
            ftype = ftype || '__type_'  || coalesce(a_type, '[null]') || '__subtype_' || coalesce(a_subtype, '[null]');
        return ftype;
    end
    ^
    set term ;^
    commit;

    create view v_fields_info as
    select
        rf.rdb$field_name as field_name
        ,upper(fn_get_type_name(f.rdb$field_type, f.rdb$field_sub_type)) as field_type
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
    left join rdb$character_sets c on f.rdb$character_set_id = c.rdb$character_set_id
    left join rdb$collations k on c.rdb$character_set_id = k.rdb$character_set_id and f.rdb$collation_id = k.rdb$collation_id
    where rf.rdb$relation_name = upper('TEST')
    order by
         field_name
        ,field_cset_id
        ,field_coll_id
    ;
    commit;

    create table test(id int);
    commit;
    alter table test add text varchar(32) character set win1252;
    commit;

    select v.* from v_fields_info v;
"""

act = isql_act('db', test_script)

expected_stdout = """
    FIELD_NAME                      ID
    FIELD_TYPE                      INTEGER
    FIELD_CHAR_LEN                  <null>
    FIELD_CSET_ID                   <null>
    FIELD_COLL_ID                   <null>
    CSET_NAME                       <null>
    FIELD_COLLATION                 <null>

    FIELD_NAME                      TEXT
    FIELD_TYPE                      VARCHAR
    FIELD_CHAR_LEN                  32
    FIELD_CSET_ID                   53
    FIELD_COLL_ID                   5
    CSET_NAME                       WIN1252
    FIELD_COLLATION                 PXW_SWEDFIN
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
