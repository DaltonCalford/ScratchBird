#coding:utf-8

"""
ID:          domain.create-27
FBTEST:      functional.domain.create.27
TITLE:       CREATE DOMAIN. Specifying BLOB SUB_TYPE <N>, where N = 0 or 1.
DESCRIPTION: Domain creation based on BLOB datatype with SUBTYPE <NUM> specification, where NUM = 0 or 1.
NOTES:
    [06.10.2023] pzotov
    1. Removed SHOW command for check result because its output often changes.
       It is enough for this test to obtain similar data from RDB tables.
       Created view and stored function to obtain type name by rdb$fields.rdb$field_type and .rdb$field_sub_type.
    2. Made example more complex: create domain with charset differ than default one for DB, and collate differ than default for domain.
    3. Ensure that we can use just created domain w/o problem (create table with column based on domain and add record).
"""

import pytest
from firebird.qa import *

db = db_factory(charset = 'utf8')

test_script = f"""
    set bail on;
    set list on;
    alter character set win1252 set default collation pxw_swedfin;

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

    create view v_domain_info as
    select
        f.rdb$field_name as dm_name
        ,upper(fn_get_type_name(f.rdb$field_type, f.rdb$field_sub_type)) as dm_type
        ,f.rdb$segment_length as blob_segm_size
        ,f.rdb$field_length as dm_size
        ,f.rdb$field_scale as dm_scale
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
    left join rdb$character_sets c on f.rdb$character_set_id = c.rdb$character_set_id
    left join rdb$collations k on c.rdb$character_set_id = k.rdb$character_set_id and f.rdb$collation_id = k.rdb$collation_id
    where f.rdb$field_name = upper('dm_test')
    ;
    commit;

    create domain dm_test blob
        sub_type 1                            -- sub_type = 1 => text
        character set win1250 collate WIN_CZ
    ;
    recreate table test(s dm_test);
    commit;

    select v.* from v_domain_info v;
	-- Ensure that we can use this domain: create table and write string with 64 characters from win1250 charset.
	-- This must complete successful:
    insert into test(s) values('ŔÁÂĂÄĹĆÇČÉĘËĚÍÎĎĐŃŇÓÔŐÖ×ŘŮÚŰÜÝŢßŕáâăäĺćçčéęëěíîďđńňóôőö÷řůúűüýţ˙') returning char_length(s) as blob_text_char_len;
    commit;


    drop table test;
    drop domain dm_test;
    commit;

    create domain dm_test blob sub_type 0;    -- sub_type 0 => binary
    recreate table test(s dm_test);
    commit;
    select v.* from v_domain_info v;
	-- Ensure that we can use this domain: create table and write binary data in it
	-- This must complete successful:
	insert into test(s) values(gen_uuid()) returning octet_length(s) as blob_bin_octet_len;
    commit;
"""

act = isql_act('db', test_script)

@pytest.mark.skip("Covered by 'test_all_datatypes_basic.py'")
@pytest.mark.version('>=3')
def test_1(act: Action):
    expected_stdout = f"""
        DM_NAME                         DM_TEST
        DM_TYPE                         BLOB SUB_TYPE TEXT
        BLOB_SEGM_SIZE                  80
        DM_SIZE                         8
        DM_SCALE                        0
        DM_CHAR_LEN                     <null>
        DM_DEFAULT                      <null>
        DM_NOT_NULL                     <null>
        DM_CHECK_EXPR                   <null>
        DM_CSET_NAME                    WIN1250
        DM_COLL_NAME                    WIN_CZ
        BLOB_TEXT_CHAR_LEN              64

        DM_NAME                         DM_TEST
        DM_TYPE                         BLOB SUB_TYPE BINARY
        BLOB_SEGM_SIZE                  80
        DM_SIZE                         8
        DM_SCALE                        0
        DM_CHAR_LEN                     <null>
        DM_DEFAULT                      <null>
        DM_NOT_NULL                     <null>
        DM_CHECK_EXPR                   <null>
        DM_CSET_NAME                    <null>
        DM_COLL_NAME                    <null>
        BLOB_BIN_OCTET_LEN              16
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
