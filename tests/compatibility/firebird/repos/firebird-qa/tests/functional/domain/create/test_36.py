#coding:utf-8

"""
ID:          domain.create-36
FBTEST:      functional.domain.create.36
TITLE:       CREATE DOMAIN - DEFAULT CURRENT_ROLE
DESCRIPTION: Domain creation based on VARCHAR datatype with CURRENT_ROLE DEFAULT specification
NOTES:
    [06.10.2023] pzotov
    1. Removed SHOW command for check result because its output often changes.
       It is enough for this test to obtain similar data from RDB tables.
       Created view and stored function to obtain type name by rdb$fields.rdb$field_type and .rdb$field_sub_type.
    2. Made example more complex: create domain with charset differ than default one for DB, and collate differ than default for domain.
    3. Ensure that we can use just created domain w/o problem (create table with column based on domain and add record).
    4. NOTE: currently role name with ASCII-ONLY characters is checked.
       Test with non-ascii role must be implemented seperately, some troubles found with it: trace show that it was not applied to connect.
       ('NONE' was shown instead).
"""

import pytest
from firebird.qa import *

db = db_factory(charset = 'utf8')

# 'ŔÁÂĂÄĹĆÇČÉĘËĚÍÎĎĐŃŇÓÔŐÖ×ŘŮÚŰÜÝŢßŕáâăäĺćçčéęëěíîďđńňóôőö÷řůúűüýţ˙'
#tmp_user = user_factory('db', name='"ßŢŘÂŇGË ŮSĚŘ"', password = '123')
#tmp_role = role_factory('db', name='"ßŢŘÂŇGË ŔÖĹĘ"')

tmp_user = user_factory('db', name='michael_smith', password = '123')
tmp_role = role_factory('db', name='"company boss"')

act = python_act('db')
DM_SIZE = 32765

@pytest.mark.skip("Covered by 'test_all_datatypes_basic.py'")
@pytest.mark.version('>=3')
def test_1(act: Action, tmp_user: User, tmp_role: Role, capsys):

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
            ,f.rdb$field_length as dm_size
            ,f.rdb$field_scale as dm_scale
            ,f.rdb$character_length as dm_char_len
            --,f.rdb$character_set_id as dm_cset_id
            --,f.rdb$collation_id as dm_coll_id
            ,cast(f.rdb$default_source as varchar(8190)) as dm_default
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

        create domain dm_test varchar({DM_SIZE}) character set win1250 default current_role collate WIN_CZ;
        recreate table test(s dm_test);
        commit;

        grant select on v_domain_info to public;
        grant insert, select on test to user {tmp_user.name};
        grant {tmp_role.name} to {tmp_user.name};
        commit;

        connect '{act.db.dsn}' user {tmp_user.name} password '{tmp_user.password}' role {tmp_role.name};

        select v.* from v_domain_info v;
    	-- Ensure that we can use this domain: create table and write string with 64 characters from win1250 charset.
    	-- This must complete successful:
        insert into test default values returning cast(s as varchar(32)) as blob_text_data;
        commit;
    """

    expected_stdout = f"""
        DM_NAME                         DM_TEST
        DM_TYPE                         VARCHAR
        DM_SIZE                         {DM_SIZE}
        DM_SCALE                        0
        DM_CHAR_LEN                     {DM_SIZE}
        DM_DEFAULT                      default current_role
        DM_NOT_NULL                     <null>
        DM_CHECK_EXPR                   <null>
        DM_CSET_NAME                    WIN1250
        DM_COLL_NAME                    WIN_CZ
        BLOB_TEXT_DATA                  {tmp_role.name.replace('"','')}
    """
    act.expected_stdout = expected_stdout
    #act.execute(combine_output = True, input = test_script)
    act.isql(switches=['-q'], combine_output = True, input = test_script)
    assert act.clean_stdout == act.clean_expected_stdout
