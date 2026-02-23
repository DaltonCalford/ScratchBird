#coding:utf-8

"""
ID:          n/a
TITLE:       CREATE DOMAIN - basic checks 
DESCRIPTION:
    Verify work for all avaliable data types, with adding [not] null, default, check clauses and collations (if applicable).
    Test creates domain for all existing data types, with adding to some of them 'NOT NULL', 'DEFAULT' and 'CHECK' clauses.
    For textual domains 'COLLATION' modifier is added. Definitions for all domains are stored in the 'dm_decl_map' dict.
    Every generated statement that creates domain must pass w/o errors.
    We check content of RDB$ tables in order to see data for just created domain(s) INSTEAD of usage 'SHOW DOMAIN' command.
    View 'v_domain_info' is used to show all data related to domains.
    Its DDL differs for FB versions prior/ since 6.x (columns related to SQL schemas present for 6.x).

    After domain will be created, we create a table with one field that has type of this domain (it also must pass ok).
    Finally, we try to insert into this table two group of records:
    1) first group contains values that are VALID for such domain (and its constraints if they are),
      i.e. every such value *must* be stored w/o problem; temporary SQL script is generated for this, see 'tmp_sql_must_pass';
    2) second group contains INVALID values for which exception must raise (its SQLSTATE depends on value);
       temporary SQL script is generated for this, see 'tmp_sql_must_fail'.
    Test verifies that no errors occur during inserting values of group-1 and no values were stored for group-2.
NOTES:
    [10.07.2025] pzotov
    This test replaces previously created ones with names:
        test_01.py  test_15.py  test_29.py
        test_02.py  test_16.py  test_30.py
        test_03.py  test_17.py  test_31.py
        test_04.py  test_18.py  test_32.py
        test_05.py  test_19.py  test_33.py
        test_06.py  test_20.py  test_34.py
        test_07.py  test_21.py  test_35.py
        test_08.py  test_22.py  test_36.py
        test_09.py  test_23.py  test_37.py
        test_10.py  test_24.py  test_38.py
        test_11.py  test_25.py  test_39.py
        test_12.py  test_26.py  test_40.py
        test_13.py  test_27.py  
        test_14.py  test_28.py  
    All these tests has been marked to be SKIPPED from execution.
    Checked on Checked on 6.0.0.909; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""
import sys
from pathlib import Path
import time
import subprocess

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

sys.stdout.reconfigure(encoding='utf-8')

db = db_factory(charset = 'utf8')

tmp_sql_must_pass = temp_file('tmp_domains_basic_check.must_pass.sql')
tmp_sql_must_fail = temp_file('tmp_domains_basic_check.must_fail.sql')
tmp_sql_fail_log = temp_file('tmp_domains_basic_check.must_fail.log')

act = python_act('db')

@pytest.mark.version('>=3')
def test_1(act: Action, capsys, tmp_sql_must_pass: Path, tmp_sql_must_fail: Path, tmp_sql_fail_log: Path):

    SQL_SCHEMA_IN_RDB_FIELDS = '' if act.is_version('<6') else ',f.rdb$schema_name as dm_itself_schema'
    SQL_SCHEMA_IN_RDB_CSET = '' if act.is_version('<6') else ',c.rdb$schema_name as dm_cset_schema'
    SQL_SCHEMA_IN_RDB_COLL = '' if act.is_version('<6') else ',k.rdb$schema_name as dm_coll_schema'

    init_script = f"""
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
                        ,'unknown'
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
            {SQL_SCHEMA_IN_RDB_FIELDS}
            ,f.rdb$field_type as dm_type
            ,upper(fn_get_type_name(f.rdb$field_type, f.rdb$field_sub_type)) as dm_type_name
            ,f.rdb$field_length as dm_size
            ,f.rdb$field_scale as dm_scale
            ,f.rdb$field_precision dm_prec
            ,f.rdb$field_sub_type as dm_subt
            ,f.rdb$dimensions as dm_dimens
            ,f.rdb$null_flag as dm_not_null
            ,f.rdb$default_source as dm_default
            ,f.rdb$validation_source as dm_check_expr
            ,f.rdb$character_length as dm_char_len
            ,f.rdb$character_set_id as dm_cset_id
            ,f.rdb$collation_id as dm_coll_id
            ,c.rdb$character_set_name as dm_cset_name
            {SQL_SCHEMA_IN_RDB_CSET}
            ,c.rdb$default_collate_name as dm_default_coll_name
            ,k.rdb$base_collation_name db_base_coll
            ,k.rdb$collation_name as dm_coll_name
            {SQL_SCHEMA_IN_RDB_COLL}
        from rdb$fields f
        left join rdb$character_sets c on f.rdb$character_set_id = c.rdb$character_set_id
        left join rdb$collations k on c.rdb$character_set_id = k.rdb$character_set_id and f.rdb$collation_id = k.rdb$collation_id
        where f.rdb$field_name starting with upper('dm_') and coalesce(f.rdb$system_flag,0) = 0
        ;
        commit;
    """
    act.isql(switches = ['-q'], charset = 'utf8', input = init_script, combine_output = True, io_enc = 'utf8')
    assert act.clean_stdout == '', f'Could not run init script:\n{act.clean_stdout}'
    act.reset()
   

    DOUBLE_CLOSEST_TO_ZERO = '1.00000000000000000-92' if act.is_version('<4') else '2.2250738585072009e-308'

    MUST_FAIL_FOR_BOOL = ("0", "1", "'QWE'", "current_date", "current_connection", "time '00:00:00'", "'ложь'", "'věrný'" )
    MUST_FAIL_FOR_INTS = ("false", "true", "date '31.12.9999'", "time '00:00:00'", "'0xZ'")
    MUST_FAIL_FOR_I128 = ("false", "true", "date '31.12.9999'", "time '00:00:00'", "'0xZ'")
    MUST_FAIL_FOR_DF16 = ("1E+385", "-1E+385")
    MUST_FAIL_FOR_DF34 = (
                             "0.100000000000000000000555000000007890000000000000000000000000000000000000000000000000000000000000999000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000010"
                            ,"-0.10000000000000000000055500000000789000000000000000000000000000000000000000000000000000000000000099900000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000010"
                            ,"12300000000000000000000555000000007890000000000000000000000000000000000000000000000000000000000000999000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000010"
                            ,"-1230000000000000000000055500000000789000000000000000000000000000000000000000000000000000000000000099900000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000010"
                            ,"1e+6145"
                            ,"-1e+6145"
                         )

    # Currently one need to enclose "almost zero" value 2.2250738585072008e-308 in order to pass it as text for rais exception
    # "SQLSTATE = 22003 / arithmetic exception, numeric overflow, or string truncation"
    # TODO: consider to add 2.2250738585072008e-308 after fix https://github.com/FirebirdSQL/firebird/issues/8647
    #
    MUST_FAIL_FOR_DBLP = ("false", "true", "date '31.12.9999'", "time '00:00:00'", "'0xZ'", "'ы'", "'€'", "1.7976931348623158e308", "'2.2250738585072008e-308'")

    MUST_FAIL_FOR_DATE = ("false", "true", "0", "-32768", "'01.01.0000'", "''", "current_time", "current_connection", "'ы'", "'€'")
    MUST_FAIL_FOR_TIME = ("false", "true", "0", "-32768", "'TODAY'", "'TOMORROW'", "'YESTERDAY'", "current_date", "current_connection", "'ы'", "'€'")
    MUST_FAIL_FOR_TMST = ("false", "true", "0", "-32768", "current_connection", "'ы'", "'€'")
    MUST_FAIL_FOR_ASCI = ("'ы'", "'€'")
    MUST_FAIL_FOR_1252 = ("'ы'",)
    MUST_FAIL_FOR_1250 = ("'ы'",)
    MUST_FAIL_FOR_NCHR = ("'ы'",)
    MUST_FAIL_FOR_BINS = () # todo later
    MUST_FAIL_FOR_TMTZ = ("time '01:02:03 Antarctica/Ananas'", "'11:12:13 +15:30'", "'11:12:13 -15:30'")
    MUST_FAIL_FOR_TSTZ = ("'11.12.13 01:02:03 Antarctica/Ananas'", "'1.1.1 1:2:3 +15:30'", "'11:12:13 -15:30'")

    # main dict: K = domain name; V = (domain_type, ( (<tuple_of_valid_values>), (<tuple_of_invalid_values>) ) )
    dm_decl_map = {
        "dm_bool"                         : ( "boolean", ("true", "false"), MUST_FAIL_FOR_BOOL ),
        "dm_i16"                          : ( "smallint", ("-32768", "32767"), ("-32769", "32768") + MUST_FAIL_FOR_INTS ),
        "dm_i32"                          : ( "int",      ("-2147483648", "2147483647"), ("-2147483649", "2147483648") + MUST_FAIL_FOR_INTS ),
        
        # do not add "-9223372036854775809" because 't0ken unknown' raises in FB 3.x for such literal:
        "dm_i64"                          : ( "bigint",   ("-9223372036854775808", "9223372036854775807"), ("9223372036854775808",) + MUST_FAIL_FOR_INTS ),

        "dm_flo"                          : ( "float",    ("1.175e-38", "3.402e38"), MUST_FAIL_FOR_DBLP ),

        # https://en.wikipedia.org/wiki/Double-precision_floating-point_format
        # 2.2250738585072009e−308 ==> largest subnormal number
        # 1.7976931348623157e+308 ==> largest normal number
        "dm_dbl"                          : ( "double precision", (DOUBLE_CLOSEST_TO_ZERO, "1.7976931348623157e308"), MUST_FAIL_FOR_DBLP ),

        "dm_dt"                           : ( "date", ("'01.01.0001'", "'31.12.9999'"), MUST_FAIL_FOR_DATE ),
        "dm_tm"                           : ( "time", ("'00:00:00.001'", "'23:59:59.999'"), MUST_FAIL_FOR_TIME ),
        "dm_ts"                           : ( "timestamp", ("'01.01.0001 00:00:00.001'", "'31.12.9999 23:59:59.999'"), MUST_FAIL_FOR_TMST ),
        "dm_dec"                          : ( "decimal(18,4)", ("0.0001", "12345678912345.6789"), MUST_FAIL_FOR_INTS ),
        "dm_num"                          : ( "numeric (18,18)", ("0.00000000000000001", "0.99999999999999999", "-0.00000000000000001", "-0.99999999999999999"), MUST_FAIL_FOR_INTS ),
        "dm_txt_char"                     : ( "char(300) character set win1252 collate win_ptbr", ("'€'", "'ÁÉÍÓÚÂÊÔÀÃÕÇ'"), MUST_FAIL_FOR_1252 ),
        "dm_txt_character"                : ( "character(32767) character set win1252 collate win_ptbr", ("'€'", "'ÁÉÍÓÚÂÊÔÀÃÕÇ'"), MUST_FAIL_FOR_1252 ),
        "dm_txt_character_var"            : ( "character varying(32765) character set win1252 collate win_ptbr", ("'€'", "'ÁÉÍÓÚÂÊÔÀÃÕÇ'"), MUST_FAIL_FOR_1252 ),
        "dm_txt_vchr"                     : ( "varchar(32765) character set win1252 collate win_ptbr", ("'€'", "'ÁÉÍÓÚÂÊÔÀÃÕÇ'"), MUST_FAIL_FOR_1252 ),
        "dm_txt_vchr_ascii"               : ( "varchar(32765) character set ascii", ("'0'", "'Z'"), MUST_FAIL_FOR_ASCI ),
        
        # ~ iso8859_1:
        "dm_txt_nchar"                    : ( "nchar(32767)", ("'µÐæ£¥Þß¿®'", "'ÁÉÍÓÚÂÊÔÀÃÕÇ'"), MUST_FAIL_FOR_NCHR ),
        "dm_txt_national_character"       : ( "national character(32767)", ("'ÿß'", "'éàäöüãñâêôç'"), MUST_FAIL_FOR_NCHR ),
        "dm_txt_national_char"            : ( "national char(32767)", ("'ÿß'", "'éàäöüãñâêôç'"), MUST_FAIL_FOR_NCHR ),
        "dm_txt_national_char_var"        : ( "national char varying(32765)", ("'ÿß'", "'éàäöüãñâêôç'"), MUST_FAIL_FOR_NCHR ),

        "dm_txt_1250_def_coll_cz"         : ( "varchar(32765) character set win1250 default 'město' collate win_cz", ("null", "'dítě'"), MUST_FAIL_FOR_1250 ),
        "dm_txt_1250_def_null_coll_cz"    : ( "varchar(32765) character set win1250 default NULL collate win_cz", ("null", "'dítě'"), MUST_FAIL_FOR_1250 ),
        "dm_txt_1250_def_cusr_coll_cz"    : ( "varchar(32765) character set win1250 default current_user collate win_cz", ("null", "'vítěz'"), MUST_FAIL_FOR_1250 ),
        "dm_txt_1250_nn_def_cusr"         : ( "varchar(32765) character set win1250 default current_user NOT NULL", ("current_user", "'vítěz'", "'učitel'"), MUST_FAIL_FOR_1250 + ("null",) ),
        "dm_txt_1250_nn_def_crol"         : ( "varchar(32765) character set win1250 default current_role collate win_cz", ("null", "current_role", "'vítěz'", "'učitel'"), MUST_FAIL_FOR_1250 ),
        "dm_txt_1250_nn_coll_cz"          : ( "varchar(32765) character set win1250 NOT NULL collate win_cz", ("'vítěz'", "'učitel'"), MUST_FAIL_FOR_1250  + ("null",) ),
        "dm_txt_1250_chk_coll_cz_ci_ai"   : ( "varchar(32765) character set win1250 check(value similar to '%město%') collate win_cz_ci_ai", ("null", "'Horní Město'"), MUST_FAIL_FOR_1250  + ("'stare misto'",) ),
        "dm_txt_1250_def_nn_chk_cz_ci_ai" : ( "varchar(32765) character set win1250 default 'město' not null check(value similar to '%město%') collate win_cz_ci_ai", ("'Horní Město'", "'Dolní Město'"), MUST_FAIL_FOR_1250  + ("'stare misto'", "null") ),

        # blobs binary
        "dm_blob_sub_bin"                 : ( "blob sub_type binary", ("0xF0000000", "0x0F0000000"), MUST_FAIL_FOR_BINS ),
        "dm_blob_sub_bin_segm"            : ( "blob sub_type binary segment size 32763", ("null", "'qwerty'"), MUST_FAIL_FOR_BINS ),
        "dm_blob_sub_0"                   : ( "blob sub_type 0", ("0xF0000000", "0x0F0000000"), MUST_FAIL_FOR_BINS ),

        # blobs textual
        "dm_blob_sub_txt"                 : ( "blob sub_type text", ("null", "'qwerty'") ),
        "dm_blob_segm_sub_type_1"         : ( "blob(12347,1)", ("null", "'qwerty'") ),
        "dm_blob_sub_1"                   : ( "blob sub_type 1", ("null", "'qwerty'") ),
        "db_blob_sub_1_1250"              : ( "blob sub_type 1 character set win1250", ("null", "'vítěz'"), MUST_FAIL_FOR_1250 ),
        "dm_blob_1250_coll_cz"            : ( "blob character set win1250 collate win_cz", ("null", "'vítěz'"), MUST_FAIL_FOR_1250 ),
        "dm_blob_sub_1_1250_coll_cz"      : ( "blob sub_type 1 character set win1250 collate win_cz", ("null", "'vítěz'"), MUST_FAIL_FOR_1250 ),
        "dm_blob_sub_txt_1250_coll_cz"    : ( "blob sub_type text character set win1250 collate win_cz", ("null", "'vítěz'"), MUST_FAIL_FOR_1250 ),
        "dm_blob_segm_1250_cz"            : ( "blob segment size 32761 character set win1250 collate win_cz", ("null", "'vítěz'"), MUST_FAIL_FOR_1250 ),

        # arrays:
        "dm_dbl_array"                    : ( "double precision [7]", () ),
        "dm_ts_array"                     : ( "timestamp[1024]", () ),
        "dm_dec_array"                    : ( "decimal(18,18) [32768]", () ),
        "dm_num_array"                    : ( "numeric(18,18)[32768]", () ),
        "dm_txt_vchr_array"               : ( "varchar(32765)[40000] character set win1252 collate win_ptbr", () ),
        "dm_txt_national_char_var_array"  : ( "national char varying(32765) [30, 30, 30]", () ),
    }

    if act.is_version("<4"):
        pass
    else:
        dm_decl_map.update(
            {
                "dm_i128"  : ( "int128", ("-170141183460469231731687303715884105728", "170141183460469231731687303715884105727"), MUST_FAIL_FOR_I128 ),
                "dm_df16"  : ( "decfloat(16)", ("1e-398", "9.9999e384"), MUST_FAIL_FOR_DF16 ),
                "dm_df34"  : ( "decfloat(34)", ("1e-6176", "9.9999e6144"), MUST_FAIL_FOR_DF34 ),
                "dm_bin"   : ( "binary(32767)", ("0xF0000000", "0x0F0000000"), MUST_FAIL_FOR_BINS ),     # ~ CHAR [(<length>)] CHARACTER SET OCTETS
                "dm_vbin"  : ( "varbinary(32765)", ("0xF0000000", "0x0F0000000"), MUST_FAIL_FOR_BINS ), # ~ VARCHAR [(<length>)] CHARACTER SET OCTETS
                "dm_tm_tz" : ( "time with time zone", ("null", "time '01:02:03 Indian/Cocos'"), MUST_FAIL_FOR_TMTZ ),
                "dm_ts_tz" : ( "timestamp with time zone", ("null", "'1.1.1 1:2:3 +06:30'"), MUST_FAIL_FOR_TSTZ ),
            }
        )

    sql_must_pass_lst = [ 'set names utf8;', f"connect {act.db.dsn} user {act.db.user} password '{act.db.password}';" ]
    sql_must_fail_lst = sql_must_pass_lst.copy()
    sql_must_fail_lst.extend( ('set list on;', 'set blob all;') )
    with act.db.connect(charset = 'utf8') as con, \
        open(tmp_sql_must_pass, 'w', encoding = 'utf8') as f_must_pass, \
        open(tmp_sql_must_fail, 'w', encoding = 'utf8') as f_must_fail:

        for dm_name, v in dm_decl_map.items():
            dm_type, must_pass_tuple = v[:2]
            must_fail_tuple = ()
            if len(v) >= 3:
                must_fail_tuple = v[2]

            dm_ddl = f"create domain {dm_name} {dm_type}"
            table_ddl = f'recreate table test(f01 {dm_name})'
            ddl_success = 0
            try:
                con.execute_immediate(dm_ddl)
                con.commit()
                con.execute_immediate(table_ddl)
                con.commit()
                ddl_success = 1
            except DatabaseError as e:
                print('Problem with creating domain ({dm_ddl}) or table ({table_ddl}):')
                print(e.__str__())
                print(e.gds_codes)

            if ddl_success:
                sql_must_pass_lst.append( f'recreate table test(f01 {dm_name});' )
                sql_must_fail_lst.append( f'recreate table test(f01 {dm_name});' )
                for i in must_pass_tuple:
                    sql_must_pass_lst.append( f'insert into test(f01) values({i});' )
                for i in must_fail_tuple:
                    sql_must_fail_lst.append( f'insert into test(f01) values({i}) returning f01 as unexpectedly_stored;' )
                sql_must_pass_lst.extend( ('commit;', 'drop table test;') )
                sql_must_fail_lst.extend( ('commit;', 'drop table test;') )

        f_must_pass.write('\n'.join(sql_must_pass_lst))
        f_must_fail.write('\n'.join(sql_must_fail_lst))

        cur = con.cursor()
        cur.execute('select * from v_domain_info order by dm_name')
        cur_cols = cur.description
        for r in cur:
            for i in range(0,len(cur_cols)):
                print( cur_cols[i][0], ':', r[i] )
            print('-' * 30)

    # Log must NOT contain 'SQLSTATE':
    #
    act.isql(switches = ['-q', '-e'], input_file = tmp_sql_must_pass, combine_output = True, connect_db = False, credentials = False, io_enc = 'utf8')
    if 'SQLSTATE' in act.clean_stdout:
        print(f'::: UNEXPECTED ERROR(s) DETECTED :::')
        for line in act.clean_stdout.splitlines():
            if (s := line.strip()):
                print(s)
    act.reset()

    # Log must NOT contain 'UNEXPECTEDLY_STORED ' (with at least one trail space after "D"):
    #
    act.isql(switches = ['-q', '-e'], charset = 'utf8', input_file = tmp_sql_must_fail, combine_output = True, connect_db = False, credentials = False, io_enc = 'utf8')
    if 'UNEXPECTEDLY_STORED ' in act.clean_stdout:
        print(f'::: UNEXPECTEDLY STORED VALUES DETECTED :::')
        for line in act.clean_stdout.splitlines():
            if (s := line.strip()):
                print(s)
    act.reset()

    expected_stdout_3x = """
        DM_NAME : DM_BLOB_1250_COLL_CZ
        DM_TYPE : 261
        DM_TYPE_NAME : BLOB SUB_TYPE TEXT
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 1
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : 51
        DM_COLL_ID : 7
        DM_CSET_NAME : WIN1250
        DM_DEFAULT_COLL_NAME : WIN1250
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_CZ
        ------------------------------
        DM_NAME : DM_BLOB_SEGM_1250_CZ
        DM_TYPE : 261
        DM_TYPE_NAME : BLOB SUB_TYPE TEXT
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 1
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : 51
        DM_COLL_ID : 7
        DM_CSET_NAME : WIN1250
        DM_DEFAULT_COLL_NAME : WIN1250
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_CZ
        ------------------------------
        DM_NAME : DM_BLOB_SEGM_SUB_TYPE_1
        DM_TYPE : 261
        DM_TYPE_NAME : BLOB SUB_TYPE TEXT
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 1
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : 4
        DM_COLL_ID : 0
        DM_CSET_NAME : UTF8
        DM_DEFAULT_COLL_NAME : UTF8
        DB_BASE_COLL : None
        DM_COLL_NAME : UTF8
        ------------------------------
        DM_NAME : DM_BLOB_SUB_0
        DM_TYPE : 261
        DM_TYPE_NAME : BLOB SUB_TYPE BINARY
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_BLOB_SUB_1
        DM_TYPE : 261
        DM_TYPE_NAME : BLOB SUB_TYPE TEXT
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 1
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : 4
        DM_COLL_ID : 0
        DM_CSET_NAME : UTF8
        DM_DEFAULT_COLL_NAME : UTF8
        DB_BASE_COLL : None
        DM_COLL_NAME : UTF8
        ------------------------------
        DM_NAME : DM_BLOB_SUB_1_1250_COLL_CZ
        DM_TYPE : 261
        DM_TYPE_NAME : BLOB SUB_TYPE TEXT
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 1
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : 51
        DM_COLL_ID : 7
        DM_CSET_NAME : WIN1250
        DM_DEFAULT_COLL_NAME : WIN1250
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_CZ
        ------------------------------
        DM_NAME : DM_BLOB_SUB_BIN
        DM_TYPE : 261
        DM_TYPE_NAME : BLOB SUB_TYPE BINARY
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_BLOB_SUB_BIN_SEGM
        DM_TYPE : 261
        DM_TYPE_NAME : BLOB SUB_TYPE BINARY
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_BLOB_SUB_TXT
        DM_TYPE : 261
        DM_TYPE_NAME : BLOB SUB_TYPE TEXT
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 1
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : 4
        DM_COLL_ID : 0
        DM_CSET_NAME : UTF8
        DM_DEFAULT_COLL_NAME : UTF8
        DB_BASE_COLL : None
        DM_COLL_NAME : UTF8
        ------------------------------
        DM_NAME : DM_BLOB_SUB_TXT_1250_COLL_CZ
        DM_TYPE : 261
        DM_TYPE_NAME : BLOB SUB_TYPE TEXT
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 1
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : 51
        DM_COLL_ID : 7
        DM_CSET_NAME : WIN1250
        DM_DEFAULT_COLL_NAME : WIN1250
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_CZ
        ------------------------------
        DM_NAME : DM_BOOL
        DM_TYPE : 23
        DM_TYPE_NAME : BOOLEAN
        DM_SIZE : 1
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : None
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_DBL
        DM_TYPE : 27
        DM_TYPE_NAME : DOUBLE PRECISION
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : None
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_DBL_ARRAY
        DM_TYPE : 27
        DM_TYPE_NAME : DOUBLE PRECISION
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : None
        DM_DIMENS : 1
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_DEC
        DM_TYPE : 16
        DM_TYPE_NAME : DECIMAL
        DM_SIZE : 8
        DM_SCALE : -4
        DM_PREC : 18
        DM_SUBT : 2
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_DEC_ARRAY
        DM_TYPE : 16
        DM_TYPE_NAME : DECIMAL
        DM_SIZE : 8
        DM_SCALE : -18
        DM_PREC : 18
        DM_SUBT : 2
        DM_DIMENS : 1
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_DT
        DM_TYPE : 12
        DM_TYPE_NAME : DATE
        DM_SIZE : 4
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : None
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_FLO
        DM_TYPE : 10
        DM_TYPE_NAME : FLOAT
        DM_SIZE : 4
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : None
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_I16
        DM_TYPE : 7
        DM_TYPE_NAME : SMALLINT
        DM_SIZE : 2
        DM_SCALE : 0
        DM_PREC : 0
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_I32
        DM_TYPE : 8
        DM_TYPE_NAME : INTEGER
        DM_SIZE : 4
        DM_SCALE : 0
        DM_PREC : 0
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_I64
        DM_TYPE : 16
        DM_TYPE_NAME : BIGINT
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : 0
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_NUM
        DM_TYPE : 16
        DM_TYPE_NAME : NUMERIC
        DM_SIZE : 8
        DM_SCALE : -18
        DM_PREC : 18
        DM_SUBT : 1
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_NUM_ARRAY
        DM_TYPE : 16
        DM_TYPE_NAME : NUMERIC
        DM_SIZE : 8
        DM_SCALE : -18
        DM_PREC : 18
        DM_SUBT : 1
        DM_DIMENS : 1
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_TM
        DM_TYPE : 13
        DM_TYPE_NAME : TIME WITHOUT TIME ZONE
        DM_SIZE : 4
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : None
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_TS
        DM_TYPE : 35
        DM_TYPE_NAME : TIMESTAMP WITHOUT TIME ZONE
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : None
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_TS_ARRAY
        DM_TYPE : 35
        DM_TYPE_NAME : TIMESTAMP WITHOUT TIME ZONE
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : None
        DM_DIMENS : 1
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_TXT_1250_CHK_COLL_CZ_CI_AI
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : check(value similar to '%m\u011bsto%')
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 51
        DM_COLL_ID : 8
        DM_CSET_NAME : WIN1250
        DM_DEFAULT_COLL_NAME : WIN1250
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_CZ_CI_AI
        ------------------------------
        DM_NAME : DM_TXT_1250_DEF_COLL_CZ
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : default 'm\u011bsto'
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 51
        DM_COLL_ID : 7
        DM_CSET_NAME : WIN1250
        DM_DEFAULT_COLL_NAME : WIN1250
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_CZ
        ------------------------------
        DM_NAME : DM_TXT_1250_DEF_CUSR_COLL_CZ
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : default current_user
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 51
        DM_COLL_ID : 7
        DM_CSET_NAME : WIN1250
        DM_DEFAULT_COLL_NAME : WIN1250
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_CZ
        ------------------------------
        DM_NAME : DM_TXT_1250_DEF_NN_CHK_CZ_CI_AI
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : 1
        DM_DEFAULT : default 'm\u011bsto'
        DM_CHECK_EXPR : check(value similar to '%m\u011bsto%')
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 51
        DM_COLL_ID : 8
        DM_CSET_NAME : WIN1250
        DM_DEFAULT_COLL_NAME : WIN1250
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_CZ_CI_AI
        ------------------------------
        DM_NAME : DM_TXT_1250_DEF_NULL_COLL_CZ
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : default NULL
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 51
        DM_COLL_ID : 7
        DM_CSET_NAME : WIN1250
        DM_DEFAULT_COLL_NAME : WIN1250
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_CZ
        ------------------------------
        DM_NAME : DM_TXT_1250_NN_COLL_CZ
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : 1
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 51
        DM_COLL_ID : 7
        DM_CSET_NAME : WIN1250
        DM_DEFAULT_COLL_NAME : WIN1250
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_CZ
        ------------------------------
        DM_NAME : DM_TXT_1250_NN_DEF_CROL
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : default current_role
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 51
        DM_COLL_ID : 7
        DM_CSET_NAME : WIN1250
        DM_DEFAULT_COLL_NAME : WIN1250
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_CZ
        ------------------------------
        DM_NAME : DM_TXT_1250_NN_DEF_CUSR
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : 1
        DM_DEFAULT : default current_user
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 51
        DM_COLL_ID : 0
        DM_CSET_NAME : WIN1250
        DM_DEFAULT_COLL_NAME : WIN1250
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN1250
        ------------------------------
        DM_NAME : DM_TXT_CHAR
        DM_TYPE : 14
        DM_TYPE_NAME : CHAR
        DM_SIZE : 300
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 300
        DM_CSET_ID : 53
        DM_COLL_ID : 6
        DM_CSET_NAME : WIN1252
        DM_DEFAULT_COLL_NAME : WIN1252
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_PTBR
        ------------------------------
        DM_NAME : DM_TXT_CHARACTER
        DM_TYPE : 14
        DM_TYPE_NAME : CHAR
        DM_SIZE : 32767
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32767
        DM_CSET_ID : 53
        DM_COLL_ID : 6
        DM_CSET_NAME : WIN1252
        DM_DEFAULT_COLL_NAME : WIN1252
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_PTBR
        ------------------------------
        DM_NAME : DM_TXT_CHARACTER_VAR
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 53
        DM_COLL_ID : 6
        DM_CSET_NAME : WIN1252
        DM_DEFAULT_COLL_NAME : WIN1252
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_PTBR
        ------------------------------
        DM_NAME : DM_TXT_NATIONAL_CHAR
        DM_TYPE : 14
        DM_TYPE_NAME : CHAR
        DM_SIZE : 32767
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32767
        DM_CSET_ID : 21
        DM_COLL_ID : 0
        DM_CSET_NAME : ISO8859_1
        DM_DEFAULT_COLL_NAME : ISO8859_1
        DB_BASE_COLL : None
        DM_COLL_NAME : ISO8859_1
        ------------------------------
        DM_NAME : DM_TXT_NATIONAL_CHARACTER
        DM_TYPE : 14
        DM_TYPE_NAME : CHAR
        DM_SIZE : 32767
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32767
        DM_CSET_ID : 21
        DM_COLL_ID : 0
        DM_CSET_NAME : ISO8859_1
        DM_DEFAULT_COLL_NAME : ISO8859_1
        DB_BASE_COLL : None
        DM_COLL_NAME : ISO8859_1
        ------------------------------
        DM_NAME : DM_TXT_NATIONAL_CHAR_VAR
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 21
        DM_COLL_ID : 0
        DM_CSET_NAME : ISO8859_1
        DM_DEFAULT_COLL_NAME : ISO8859_1
        DB_BASE_COLL : None
        DM_COLL_NAME : ISO8859_1
        ------------------------------
        DM_NAME : DM_TXT_NATIONAL_CHAR_VAR_ARRAY
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : 3
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 21
        DM_COLL_ID : 0
        DM_CSET_NAME : ISO8859_1
        DM_DEFAULT_COLL_NAME : ISO8859_1
        DB_BASE_COLL : None
        DM_COLL_NAME : ISO8859_1
        ------------------------------
        DM_NAME : DM_TXT_NCHAR
        DM_TYPE : 14
        DM_TYPE_NAME : CHAR
        DM_SIZE : 32767
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32767
        DM_CSET_ID : 21
        DM_COLL_ID : 0
        DM_CSET_NAME : ISO8859_1
        DM_DEFAULT_COLL_NAME : ISO8859_1
        DB_BASE_COLL : None
        DM_COLL_NAME : ISO8859_1
        ------------------------------
        DM_NAME : DM_TXT_VCHR
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 53
        DM_COLL_ID : 6
        DM_CSET_NAME : WIN1252
        DM_DEFAULT_COLL_NAME : WIN1252
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_PTBR
        ------------------------------
        DM_NAME : DM_TXT_VCHR_ARRAY
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : 1
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 53
        DM_COLL_ID : 6
        DM_CSET_NAME : WIN1252
        DM_DEFAULT_COLL_NAME : WIN1252
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_PTBR
        ------------------------------
        DM_NAME : DM_TXT_VCHR_ASCII
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 2
        DM_COLL_ID : 0
        DM_CSET_NAME : ASCII
        DM_DEFAULT_COLL_NAME : ASCII
        DB_BASE_COLL : None
        DM_COLL_NAME : ASCII
        ------------------------------
    """

    expected_stdout_4x = """
        DM_NAME : DM_BIN
        DM_TYPE : 14
        DM_TYPE_NAME : BINARY
        DM_SIZE : 32767
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 1
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32767
        DM_CSET_ID : 1
        DM_COLL_ID : 0
        DM_CSET_NAME : OCTETS
        DM_DEFAULT_COLL_NAME : OCTETS
        DB_BASE_COLL : None
        DM_COLL_NAME : OCTETS
        ------------------------------
        DM_NAME : DM_BLOB_1250_COLL_CZ
        DM_TYPE : 261
        DM_TYPE_NAME : BLOB SUB_TYPE TEXT
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 1
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : 51
        DM_COLL_ID : 7
        DM_CSET_NAME : WIN1250
        DM_DEFAULT_COLL_NAME : WIN1250
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_CZ
        ------------------------------
        DM_NAME : DM_BLOB_SEGM_1250_CZ
        DM_TYPE : 261
        DM_TYPE_NAME : BLOB SUB_TYPE TEXT
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 1
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : 51
        DM_COLL_ID : 7
        DM_CSET_NAME : WIN1250
        DM_DEFAULT_COLL_NAME : WIN1250
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_CZ
        ------------------------------
        DM_NAME : DM_BLOB_SEGM_SUB_TYPE_1
        DM_TYPE : 261
        DM_TYPE_NAME : BLOB SUB_TYPE TEXT
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 1
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : 4
        DM_COLL_ID : 0
        DM_CSET_NAME : UTF8
        DM_DEFAULT_COLL_NAME : UTF8
        DB_BASE_COLL : None
        DM_COLL_NAME : UTF8
        ------------------------------
        DM_NAME : DM_BLOB_SUB_0
        DM_TYPE : 261
        DM_TYPE_NAME : BLOB SUB_TYPE BINARY
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_BLOB_SUB_1
        DM_TYPE : 261
        DM_TYPE_NAME : BLOB SUB_TYPE TEXT
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 1
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : 4
        DM_COLL_ID : 0
        DM_CSET_NAME : UTF8
        DM_DEFAULT_COLL_NAME : UTF8
        DB_BASE_COLL : None
        DM_COLL_NAME : UTF8
        ------------------------------
        DM_NAME : DM_BLOB_SUB_1_1250_COLL_CZ
        DM_TYPE : 261
        DM_TYPE_NAME : BLOB SUB_TYPE TEXT
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 1
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : 51
        DM_COLL_ID : 7
        DM_CSET_NAME : WIN1250
        DM_DEFAULT_COLL_NAME : WIN1250
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_CZ
        ------------------------------
        DM_NAME : DM_BLOB_SUB_BIN
        DM_TYPE : 261
        DM_TYPE_NAME : BLOB SUB_TYPE BINARY
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_BLOB_SUB_BIN_SEGM
        DM_TYPE : 261
        DM_TYPE_NAME : BLOB SUB_TYPE BINARY
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_BLOB_SUB_TXT
        DM_TYPE : 261
        DM_TYPE_NAME : BLOB SUB_TYPE TEXT
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 1
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : 4
        DM_COLL_ID : 0
        DM_CSET_NAME : UTF8
        DM_DEFAULT_COLL_NAME : UTF8
        DB_BASE_COLL : None
        DM_COLL_NAME : UTF8
        ------------------------------
        DM_NAME : DM_BLOB_SUB_TXT_1250_COLL_CZ
        DM_TYPE : 261
        DM_TYPE_NAME : BLOB SUB_TYPE TEXT
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 1
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : 51
        DM_COLL_ID : 7
        DM_CSET_NAME : WIN1250
        DM_DEFAULT_COLL_NAME : WIN1250
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_CZ
        ------------------------------
        DM_NAME : DM_BOOL
        DM_TYPE : 23
        DM_TYPE_NAME : BOOLEAN
        DM_SIZE : 1
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : None
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_DBL
        DM_TYPE : 27
        DM_TYPE_NAME : DOUBLE PRECISION
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : None
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_DBL_ARRAY
        DM_TYPE : 27
        DM_TYPE_NAME : DOUBLE PRECISION
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : None
        DM_DIMENS : 1
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_DEC
        DM_TYPE : 16
        DM_TYPE_NAME : DECIMAL
        DM_SIZE : 8
        DM_SCALE : -4
        DM_PREC : 18
        DM_SUBT : 2
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_DEC_ARRAY
        DM_TYPE : 16
        DM_TYPE_NAME : DECIMAL
        DM_SIZE : 8
        DM_SCALE : -18
        DM_PREC : 18
        DM_SUBT : 2
        DM_DIMENS : 1
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_DF16
        DM_TYPE : 24
        DM_TYPE_NAME : DECFLOAT(16)
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : 16
        DM_SUBT : None
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_DF34
        DM_TYPE : 25
        DM_TYPE_NAME : DECFLOAT(34)
        DM_SIZE : 16
        DM_SCALE : 0
        DM_PREC : 34
        DM_SUBT : None
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_DT
        DM_TYPE : 12
        DM_TYPE_NAME : DATE
        DM_SIZE : 4
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : None
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_FLO
        DM_TYPE : 10
        DM_TYPE_NAME : FLOAT
        DM_SIZE : 4
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : None
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_I128
        DM_TYPE : 26
        DM_TYPE_NAME : INT128
        DM_SIZE : 16
        DM_SCALE : 0
        DM_PREC : 0
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_I16
        DM_TYPE : 7
        DM_TYPE_NAME : SMALLINT
        DM_SIZE : 2
        DM_SCALE : 0
        DM_PREC : 0
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_I32
        DM_TYPE : 8
        DM_TYPE_NAME : INTEGER
        DM_SIZE : 4
        DM_SCALE : 0
        DM_PREC : 0
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_I64
        DM_TYPE : 16
        DM_TYPE_NAME : BIGINT
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : 0
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_NUM
        DM_TYPE : 16
        DM_TYPE_NAME : NUMERIC
        DM_SIZE : 8
        DM_SCALE : -18
        DM_PREC : 18
        DM_SUBT : 1
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_NUM_ARRAY
        DM_TYPE : 16
        DM_TYPE_NAME : NUMERIC
        DM_SIZE : 8
        DM_SCALE : -18
        DM_PREC : 18
        DM_SUBT : 1
        DM_DIMENS : 1
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_TM
        DM_TYPE : 13
        DM_TYPE_NAME : TIME WITHOUT TIME ZONE
        DM_SIZE : 4
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : None
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_TM_TZ
        DM_TYPE : 28
        DM_TYPE_NAME : TIME WITH TIME ZONE
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : None
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_TS
        DM_TYPE : 35
        DM_TYPE_NAME : TIMESTAMP WITHOUT TIME ZONE
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : None
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_TS_ARRAY
        DM_TYPE : 35
        DM_TYPE_NAME : TIMESTAMP WITHOUT TIME ZONE
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : None
        DM_DIMENS : 1
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_TS_TZ
        DM_TYPE : 29
        DM_TYPE_NAME : TIMESTAMP WITH TIME ZONE
        DM_SIZE : 12
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : None
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        ------------------------------
        DM_NAME : DM_TXT_1250_CHK_COLL_CZ_CI_AI
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : check(value similar to '%m\u011bsto%')
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 51
        DM_COLL_ID : 8
        DM_CSET_NAME : WIN1250
        DM_DEFAULT_COLL_NAME : WIN1250
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_CZ_CI_AI
        ------------------------------
        DM_NAME : DM_TXT_1250_DEF_COLL_CZ
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : default 'm\u011bsto'
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 51
        DM_COLL_ID : 7
        DM_CSET_NAME : WIN1250
        DM_DEFAULT_COLL_NAME : WIN1250
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_CZ
        ------------------------------
        DM_NAME : DM_TXT_1250_DEF_CUSR_COLL_CZ
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : default current_user
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 51
        DM_COLL_ID : 7
        DM_CSET_NAME : WIN1250
        DM_DEFAULT_COLL_NAME : WIN1250
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_CZ
        ------------------------------
        DM_NAME : DM_TXT_1250_DEF_NN_CHK_CZ_CI_AI
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : 1
        DM_DEFAULT : default 'm\u011bsto'
        DM_CHECK_EXPR : check(value similar to '%m\u011bsto%')
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 51
        DM_COLL_ID : 8
        DM_CSET_NAME : WIN1250
        DM_DEFAULT_COLL_NAME : WIN1250
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_CZ_CI_AI
        ------------------------------
        DM_NAME : DM_TXT_1250_DEF_NULL_COLL_CZ
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : default NULL
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 51
        DM_COLL_ID : 7
        DM_CSET_NAME : WIN1250
        DM_DEFAULT_COLL_NAME : WIN1250
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_CZ
        ------------------------------
        DM_NAME : DM_TXT_1250_NN_COLL_CZ
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : 1
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 51
        DM_COLL_ID : 7
        DM_CSET_NAME : WIN1250
        DM_DEFAULT_COLL_NAME : WIN1250
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_CZ
        ------------------------------
        DM_NAME : DM_TXT_1250_NN_DEF_CROL
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : default current_role
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 51
        DM_COLL_ID : 7
        DM_CSET_NAME : WIN1250
        DM_DEFAULT_COLL_NAME : WIN1250
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_CZ
        ------------------------------
        DM_NAME : DM_TXT_1250_NN_DEF_CUSR
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : 1
        DM_DEFAULT : default current_user
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 51
        DM_COLL_ID : 0
        DM_CSET_NAME : WIN1250
        DM_DEFAULT_COLL_NAME : WIN1250
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN1250
        ------------------------------
        DM_NAME : DM_TXT_CHAR
        DM_TYPE : 14
        DM_TYPE_NAME : CHAR
        DM_SIZE : 300
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 300
        DM_CSET_ID : 53
        DM_COLL_ID : 6
        DM_CSET_NAME : WIN1252
        DM_DEFAULT_COLL_NAME : WIN1252
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_PTBR
        ------------------------------
        DM_NAME : DM_TXT_CHARACTER
        DM_TYPE : 14
        DM_TYPE_NAME : CHAR
        DM_SIZE : 32767
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32767
        DM_CSET_ID : 53
        DM_COLL_ID : 6
        DM_CSET_NAME : WIN1252
        DM_DEFAULT_COLL_NAME : WIN1252
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_PTBR
        ------------------------------
        DM_NAME : DM_TXT_CHARACTER_VAR
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 53
        DM_COLL_ID : 6
        DM_CSET_NAME : WIN1252
        DM_DEFAULT_COLL_NAME : WIN1252
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_PTBR
        ------------------------------
        DM_NAME : DM_TXT_NATIONAL_CHAR
        DM_TYPE : 14
        DM_TYPE_NAME : CHAR
        DM_SIZE : 32767
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32767
        DM_CSET_ID : 21
        DM_COLL_ID : 0
        DM_CSET_NAME : ISO8859_1
        DM_DEFAULT_COLL_NAME : ISO8859_1
        DB_BASE_COLL : None
        DM_COLL_NAME : ISO8859_1
        ------------------------------
        DM_NAME : DM_TXT_NATIONAL_CHARACTER
        DM_TYPE : 14
        DM_TYPE_NAME : CHAR
        DM_SIZE : 32767
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32767
        DM_CSET_ID : 21
        DM_COLL_ID : 0
        DM_CSET_NAME : ISO8859_1
        DM_DEFAULT_COLL_NAME : ISO8859_1
        DB_BASE_COLL : None
        DM_COLL_NAME : ISO8859_1
        ------------------------------
        DM_NAME : DM_TXT_NATIONAL_CHAR_VAR
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 21
        DM_COLL_ID : 0
        DM_CSET_NAME : ISO8859_1
        DM_DEFAULT_COLL_NAME : ISO8859_1
        DB_BASE_COLL : None
        DM_COLL_NAME : ISO8859_1
        ------------------------------
        DM_NAME : DM_TXT_NATIONAL_CHAR_VAR_ARRAY
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : 3
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 21
        DM_COLL_ID : 0
        DM_CSET_NAME : ISO8859_1
        DM_DEFAULT_COLL_NAME : ISO8859_1
        DB_BASE_COLL : None
        DM_COLL_NAME : ISO8859_1
        ------------------------------
        DM_NAME : DM_TXT_NCHAR
        DM_TYPE : 14
        DM_TYPE_NAME : CHAR
        DM_SIZE : 32767
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32767
        DM_CSET_ID : 21
        DM_COLL_ID : 0
        DM_CSET_NAME : ISO8859_1
        DM_DEFAULT_COLL_NAME : ISO8859_1
        DB_BASE_COLL : None
        DM_COLL_NAME : ISO8859_1
        ------------------------------
        DM_NAME : DM_TXT_VCHR
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 53
        DM_COLL_ID : 6
        DM_CSET_NAME : WIN1252
        DM_DEFAULT_COLL_NAME : WIN1252
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_PTBR
        ------------------------------
        DM_NAME : DM_TXT_VCHR_ARRAY
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : 1
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 53
        DM_COLL_ID : 6
        DM_CSET_NAME : WIN1252
        DM_DEFAULT_COLL_NAME : WIN1252
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_PTBR
        ------------------------------
        DM_NAME : DM_TXT_VCHR_ASCII
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 2
        DM_COLL_ID : 0
        DM_CSET_NAME : ASCII
        DM_DEFAULT_COLL_NAME : ASCII
        DB_BASE_COLL : None
        DM_COLL_NAME : ASCII
        ------------------------------
        DM_NAME : DM_VBIN
        DM_TYPE : 37
        DM_TYPE_NAME : VARBINARY
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 1
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 1
        DM_COLL_ID : 0
        DM_CSET_NAME : OCTETS
        DM_DEFAULT_COLL_NAME : OCTETS
        DB_BASE_COLL : None
        DM_COLL_NAME : OCTETS
        ------------------------------
    """

    expected_stdout_6x = """
        DM_NAME : DM_BIN
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 14
        DM_TYPE_NAME : BINARY
        DM_SIZE : 32767
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 1
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32767
        DM_CSET_ID : 1
        DM_COLL_ID : 0
        DM_CSET_NAME : OCTETS
        DM_CSET_SCHEMA : SYSTEM
        DM_DEFAULT_COLL_NAME : OCTETS
        DB_BASE_COLL : None
        DM_COLL_NAME : OCTETS
        DM_COLL_SCHEMA : SYSTEM
        ------------------------------
        DM_NAME : DM_BLOB_1250_COLL_CZ
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 261
        DM_TYPE_NAME : BLOB SUB_TYPE TEXT
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 1
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : 51
        DM_COLL_ID : 7
        DM_CSET_NAME : WIN1250
        DM_CSET_SCHEMA : SYSTEM
        DM_DEFAULT_COLL_NAME : WIN1250
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_CZ
        DM_COLL_SCHEMA : SYSTEM
        ------------------------------
        DM_NAME : DM_BLOB_SEGM_1250_CZ
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 261
        DM_TYPE_NAME : BLOB SUB_TYPE TEXT
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 1
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : 51
        DM_COLL_ID : 7
        DM_CSET_NAME : WIN1250
        DM_CSET_SCHEMA : SYSTEM
        DM_DEFAULT_COLL_NAME : WIN1250
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_CZ
        DM_COLL_SCHEMA : SYSTEM
        ------------------------------
        DM_NAME : DM_BLOB_SEGM_SUB_TYPE_1
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 261
        DM_TYPE_NAME : BLOB SUB_TYPE TEXT
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 1
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : 4
        DM_COLL_ID : 0
        DM_CSET_NAME : UTF8
        DM_CSET_SCHEMA : SYSTEM
        DM_DEFAULT_COLL_NAME : UTF8
        DB_BASE_COLL : None
        DM_COLL_NAME : UTF8
        DM_COLL_SCHEMA : SYSTEM
        ------------------------------
        DM_NAME : DM_BLOB_SUB_0
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 261
        DM_TYPE_NAME : BLOB SUB_TYPE BINARY
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_CSET_SCHEMA : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        DM_COLL_SCHEMA : None
        ------------------------------
        DM_NAME : DM_BLOB_SUB_1
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 261
        DM_TYPE_NAME : BLOB SUB_TYPE TEXT
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 1
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : 4
        DM_COLL_ID : 0
        DM_CSET_NAME : UTF8
        DM_CSET_SCHEMA : SYSTEM
        DM_DEFAULT_COLL_NAME : UTF8
        DB_BASE_COLL : None
        DM_COLL_NAME : UTF8
        DM_COLL_SCHEMA : SYSTEM
        ------------------------------
        DM_NAME : DM_BLOB_SUB_1_1250_COLL_CZ
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 261
        DM_TYPE_NAME : BLOB SUB_TYPE TEXT
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 1
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : 51
        DM_COLL_ID : 7
        DM_CSET_NAME : WIN1250
        DM_CSET_SCHEMA : SYSTEM
        DM_DEFAULT_COLL_NAME : WIN1250
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_CZ
        DM_COLL_SCHEMA : SYSTEM
        ------------------------------
        DM_NAME : DM_BLOB_SUB_BIN
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 261
        DM_TYPE_NAME : BLOB SUB_TYPE BINARY
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_CSET_SCHEMA : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        DM_COLL_SCHEMA : None
        ------------------------------
        DM_NAME : DM_BLOB_SUB_BIN_SEGM
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 261
        DM_TYPE_NAME : BLOB SUB_TYPE BINARY
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_CSET_SCHEMA : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        DM_COLL_SCHEMA : None
        ------------------------------
        DM_NAME : DM_BLOB_SUB_TXT
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 261
        DM_TYPE_NAME : BLOB SUB_TYPE TEXT
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 1
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : 4
        DM_COLL_ID : 0
        DM_CSET_NAME : UTF8
        DM_CSET_SCHEMA : SYSTEM
        DM_DEFAULT_COLL_NAME : UTF8
        DB_BASE_COLL : None
        DM_COLL_NAME : UTF8
        DM_COLL_SCHEMA : SYSTEM
        ------------------------------
        DM_NAME : DM_BLOB_SUB_TXT_1250_COLL_CZ
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 261
        DM_TYPE_NAME : BLOB SUB_TYPE TEXT
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 1
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : 51
        DM_COLL_ID : 7
        DM_CSET_NAME : WIN1250
        DM_CSET_SCHEMA : SYSTEM
        DM_DEFAULT_COLL_NAME : WIN1250
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_CZ
        DM_COLL_SCHEMA : SYSTEM
        ------------------------------
        DM_NAME : DM_BOOL
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 23
        DM_TYPE_NAME : BOOLEAN
        DM_SIZE : 1
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : None
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_CSET_SCHEMA : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        DM_COLL_SCHEMA : None
        ------------------------------
        DM_NAME : DM_DBL
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 27
        DM_TYPE_NAME : DOUBLE PRECISION
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : None
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_CSET_SCHEMA : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        DM_COLL_SCHEMA : None
        ------------------------------
        DM_NAME : DM_DBL_ARRAY
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 27
        DM_TYPE_NAME : DOUBLE PRECISION
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : None
        DM_DIMENS : 1
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_CSET_SCHEMA : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        DM_COLL_SCHEMA : None
        ------------------------------
        DM_NAME : DM_DEC
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 16
        DM_TYPE_NAME : DECIMAL
        DM_SIZE : 8
        DM_SCALE : -4
        DM_PREC : 18
        DM_SUBT : 2
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_CSET_SCHEMA : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        DM_COLL_SCHEMA : None
        ------------------------------
        DM_NAME : DM_DEC_ARRAY
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 16
        DM_TYPE_NAME : DECIMAL
        DM_SIZE : 8
        DM_SCALE : -18
        DM_PREC : 18
        DM_SUBT : 2
        DM_DIMENS : 1
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_CSET_SCHEMA : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        DM_COLL_SCHEMA : None
        ------------------------------
        DM_NAME : DM_DF16
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 24
        DM_TYPE_NAME : DECFLOAT(16)
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : 16
        DM_SUBT : None
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_CSET_SCHEMA : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        DM_COLL_SCHEMA : None
        ------------------------------
        DM_NAME : DM_DF34
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 25
        DM_TYPE_NAME : DECFLOAT(34)
        DM_SIZE : 16
        DM_SCALE : 0
        DM_PREC : 34
        DM_SUBT : None
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_CSET_SCHEMA : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        DM_COLL_SCHEMA : None
        ------------------------------
        DM_NAME : DM_DT
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 12
        DM_TYPE_NAME : DATE
        DM_SIZE : 4
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : None
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_CSET_SCHEMA : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        DM_COLL_SCHEMA : None
        ------------------------------
        DM_NAME : DM_FLO
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 10
        DM_TYPE_NAME : FLOAT
        DM_SIZE : 4
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : None
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_CSET_SCHEMA : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        DM_COLL_SCHEMA : None
        ------------------------------
        DM_NAME : DM_I128
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 26
        DM_TYPE_NAME : INT128
        DM_SIZE : 16
        DM_SCALE : 0
        DM_PREC : 0
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_CSET_SCHEMA : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        DM_COLL_SCHEMA : None
        ------------------------------
        DM_NAME : DM_I16
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 7
        DM_TYPE_NAME : SMALLINT
        DM_SIZE : 2
        DM_SCALE : 0
        DM_PREC : 0
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_CSET_SCHEMA : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        DM_COLL_SCHEMA : None
        ------------------------------
        DM_NAME : DM_I32
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 8
        DM_TYPE_NAME : INTEGER
        DM_SIZE : 4
        DM_SCALE : 0
        DM_PREC : 0
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_CSET_SCHEMA : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        DM_COLL_SCHEMA : None
        ------------------------------
        DM_NAME : DM_I64
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 16
        DM_TYPE_NAME : BIGINT
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : 0
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_CSET_SCHEMA : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        DM_COLL_SCHEMA : None
        ------------------------------
        DM_NAME : DM_NUM
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 16
        DM_TYPE_NAME : NUMERIC
        DM_SIZE : 8
        DM_SCALE : -18
        DM_PREC : 18
        DM_SUBT : 1
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_CSET_SCHEMA : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        DM_COLL_SCHEMA : None
        ------------------------------
        DM_NAME : DM_NUM_ARRAY
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 16
        DM_TYPE_NAME : NUMERIC
        DM_SIZE : 8
        DM_SCALE : -18
        DM_PREC : 18
        DM_SUBT : 1
        DM_DIMENS : 1
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_CSET_SCHEMA : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        DM_COLL_SCHEMA : None
        ------------------------------
        DM_NAME : DM_TM
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 13
        DM_TYPE_NAME : TIME WITHOUT TIME ZONE
        DM_SIZE : 4
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : None
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_CSET_SCHEMA : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        DM_COLL_SCHEMA : None
        ------------------------------
        DM_NAME : DM_TM_TZ
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 28
        DM_TYPE_NAME : TIME WITH TIME ZONE
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : None
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_CSET_SCHEMA : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        DM_COLL_SCHEMA : None
        ------------------------------
        DM_NAME : DM_TS
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 35
        DM_TYPE_NAME : TIMESTAMP WITHOUT TIME ZONE
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : None
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_CSET_SCHEMA : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        DM_COLL_SCHEMA : None
        ------------------------------
        DM_NAME : DM_TS_ARRAY
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 35
        DM_TYPE_NAME : TIMESTAMP WITHOUT TIME ZONE
        DM_SIZE : 8
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : None
        DM_DIMENS : 1
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_CSET_SCHEMA : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        DM_COLL_SCHEMA : None
        ------------------------------
        DM_NAME : DM_TS_TZ
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 29
        DM_TYPE_NAME : TIMESTAMP WITH TIME ZONE
        DM_SIZE : 12
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : None
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : None
        DM_CSET_ID : None
        DM_COLL_ID : None
        DM_CSET_NAME : None
        DM_CSET_SCHEMA : None
        DM_DEFAULT_COLL_NAME : None
        DB_BASE_COLL : None
        DM_COLL_NAME : None
        DM_COLL_SCHEMA : None
        ------------------------------
        DM_NAME : DM_TXT_1250_CHK_COLL_CZ_CI_AI
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : check(value similar to '%m\u011bsto%')
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 51
        DM_COLL_ID : 8
        DM_CSET_NAME : WIN1250
        DM_CSET_SCHEMA : SYSTEM
        DM_DEFAULT_COLL_NAME : WIN1250
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_CZ_CI_AI
        DM_COLL_SCHEMA : SYSTEM
        ------------------------------
        DM_NAME : DM_TXT_1250_DEF_COLL_CZ
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : default 'm\u011bsto'
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 51
        DM_COLL_ID : 7
        DM_CSET_NAME : WIN1250
        DM_CSET_SCHEMA : SYSTEM
        DM_DEFAULT_COLL_NAME : WIN1250
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_CZ
        DM_COLL_SCHEMA : SYSTEM
        ------------------------------
        DM_NAME : DM_TXT_1250_DEF_CUSR_COLL_CZ
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : default current_user
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 51
        DM_COLL_ID : 7
        DM_CSET_NAME : WIN1250
        DM_CSET_SCHEMA : SYSTEM
        DM_DEFAULT_COLL_NAME : WIN1250
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_CZ
        DM_COLL_SCHEMA : SYSTEM
        ------------------------------
        DM_NAME : DM_TXT_1250_DEF_NN_CHK_CZ_CI_AI
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : 1
        DM_DEFAULT : default 'm\u011bsto'
        DM_CHECK_EXPR : check(value similar to '%m\u011bsto%')
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 51
        DM_COLL_ID : 8
        DM_CSET_NAME : WIN1250
        DM_CSET_SCHEMA : SYSTEM
        DM_DEFAULT_COLL_NAME : WIN1250
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_CZ_CI_AI
        DM_COLL_SCHEMA : SYSTEM
        ------------------------------
        DM_NAME : DM_TXT_1250_DEF_NULL_COLL_CZ
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : default NULL
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 51
        DM_COLL_ID : 7
        DM_CSET_NAME : WIN1250
        DM_CSET_SCHEMA : SYSTEM
        DM_DEFAULT_COLL_NAME : WIN1250
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_CZ
        DM_COLL_SCHEMA : SYSTEM
        ------------------------------
        DM_NAME : DM_TXT_1250_NN_COLL_CZ
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : 1
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 51
        DM_COLL_ID : 7
        DM_CSET_NAME : WIN1250
        DM_CSET_SCHEMA : SYSTEM
        DM_DEFAULT_COLL_NAME : WIN1250
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_CZ
        DM_COLL_SCHEMA : SYSTEM
        ------------------------------
        DM_NAME : DM_TXT_1250_NN_DEF_CROL
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : default current_role
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 51
        DM_COLL_ID : 7
        DM_CSET_NAME : WIN1250
        DM_CSET_SCHEMA : SYSTEM
        DM_DEFAULT_COLL_NAME : WIN1250
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_CZ
        DM_COLL_SCHEMA : SYSTEM
        ------------------------------
        DM_NAME : DM_TXT_1250_NN_DEF_CUSR
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : 1
        DM_DEFAULT : default current_user
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 51
        DM_COLL_ID : 0
        DM_CSET_NAME : WIN1250
        DM_CSET_SCHEMA : SYSTEM
        DM_DEFAULT_COLL_NAME : WIN1250
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN1250
        DM_COLL_SCHEMA : SYSTEM
        ------------------------------
        DM_NAME : DM_TXT_CHAR
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 14
        DM_TYPE_NAME : CHAR
        DM_SIZE : 300
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 300
        DM_CSET_ID : 53
        DM_COLL_ID : 6
        DM_CSET_NAME : WIN1252
        DM_CSET_SCHEMA : SYSTEM
        DM_DEFAULT_COLL_NAME : WIN1252
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_PTBR
        DM_COLL_SCHEMA : SYSTEM
        ------------------------------
        DM_NAME : DM_TXT_CHARACTER
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 14
        DM_TYPE_NAME : CHAR
        DM_SIZE : 32767
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32767
        DM_CSET_ID : 53
        DM_COLL_ID : 6
        DM_CSET_NAME : WIN1252
        DM_CSET_SCHEMA : SYSTEM
        DM_DEFAULT_COLL_NAME : WIN1252
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_PTBR
        DM_COLL_SCHEMA : SYSTEM
        ------------------------------
        DM_NAME : DM_TXT_CHARACTER_VAR
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 53
        DM_COLL_ID : 6
        DM_CSET_NAME : WIN1252
        DM_CSET_SCHEMA : SYSTEM
        DM_DEFAULT_COLL_NAME : WIN1252
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_PTBR
        DM_COLL_SCHEMA : SYSTEM
        ------------------------------
        DM_NAME : DM_TXT_NATIONAL_CHAR
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 14
        DM_TYPE_NAME : CHAR
        DM_SIZE : 32767
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32767
        DM_CSET_ID : 21
        DM_COLL_ID : 0
        DM_CSET_NAME : ISO8859_1
        DM_CSET_SCHEMA : SYSTEM
        DM_DEFAULT_COLL_NAME : ISO8859_1
        DB_BASE_COLL : None
        DM_COLL_NAME : ISO8859_1
        DM_COLL_SCHEMA : SYSTEM
        ------------------------------
        DM_NAME : DM_TXT_NATIONAL_CHARACTER
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 14
        DM_TYPE_NAME : CHAR
        DM_SIZE : 32767
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32767
        DM_CSET_ID : 21
        DM_COLL_ID : 0
        DM_CSET_NAME : ISO8859_1
        DM_CSET_SCHEMA : SYSTEM
        DM_DEFAULT_COLL_NAME : ISO8859_1
        DB_BASE_COLL : None
        DM_COLL_NAME : ISO8859_1
        DM_COLL_SCHEMA : SYSTEM
        ------------------------------
        DM_NAME : DM_TXT_NATIONAL_CHAR_VAR
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 21
        DM_COLL_ID : 0
        DM_CSET_NAME : ISO8859_1
        DM_CSET_SCHEMA : SYSTEM
        DM_DEFAULT_COLL_NAME : ISO8859_1
        DB_BASE_COLL : None
        DM_COLL_NAME : ISO8859_1
        DM_COLL_SCHEMA : SYSTEM
        ------------------------------
        DM_NAME : DM_TXT_NATIONAL_CHAR_VAR_ARRAY
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : 3
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 21
        DM_COLL_ID : 0
        DM_CSET_NAME : ISO8859_1
        DM_CSET_SCHEMA : SYSTEM
        DM_DEFAULT_COLL_NAME : ISO8859_1
        DB_BASE_COLL : None
        DM_COLL_NAME : ISO8859_1
        DM_COLL_SCHEMA : SYSTEM
        ------------------------------
        DM_NAME : DM_TXT_NCHAR
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 14
        DM_TYPE_NAME : CHAR
        DM_SIZE : 32767
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32767
        DM_CSET_ID : 21
        DM_COLL_ID : 0
        DM_CSET_NAME : ISO8859_1
        DM_CSET_SCHEMA : SYSTEM
        DM_DEFAULT_COLL_NAME : ISO8859_1
        DB_BASE_COLL : None
        DM_COLL_NAME : ISO8859_1
        DM_COLL_SCHEMA : SYSTEM
        ------------------------------
        DM_NAME : DM_TXT_VCHR
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 53
        DM_COLL_ID : 6
        DM_CSET_NAME : WIN1252
        DM_CSET_SCHEMA : SYSTEM
        DM_DEFAULT_COLL_NAME : WIN1252
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_PTBR
        DM_COLL_SCHEMA : SYSTEM
        ------------------------------
        DM_NAME : DM_TXT_VCHR_ARRAY
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : 1
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 53
        DM_COLL_ID : 6
        DM_CSET_NAME : WIN1252
        DM_CSET_SCHEMA : SYSTEM
        DM_DEFAULT_COLL_NAME : WIN1252
        DB_BASE_COLL : None
        DM_COLL_NAME : WIN_PTBR
        DM_COLL_SCHEMA : SYSTEM
        ------------------------------
        DM_NAME : DM_TXT_VCHR_ASCII
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 37
        DM_TYPE_NAME : VARCHAR
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 0
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 2
        DM_COLL_ID : 0
        DM_CSET_NAME : ASCII
        DM_CSET_SCHEMA : SYSTEM
        DM_DEFAULT_COLL_NAME : ASCII
        DB_BASE_COLL : None
        DM_COLL_NAME : ASCII
        DM_COLL_SCHEMA : SYSTEM
        ------------------------------
        DM_NAME : DM_VBIN
        DM_ITSELF_SCHEMA : PUBLIC
        DM_TYPE : 37
        DM_TYPE_NAME : VARBINARY
        DM_SIZE : 32765
        DM_SCALE : 0
        DM_PREC : None
        DM_SUBT : 1
        DM_DIMENS : None
        DM_NOT_NULL : None
        DM_DEFAULT : None
        DM_CHECK_EXPR : None
        DM_CHAR_LEN : 32765
        DM_CSET_ID : 1
        DM_COLL_ID : 0
        DM_CSET_NAME : OCTETS
        DM_CSET_SCHEMA : SYSTEM
        DM_DEFAULT_COLL_NAME : OCTETS
        DB_BASE_COLL : None
        DM_COLL_NAME : OCTETS
        DM_COLL_SCHEMA : SYSTEM
        ------------------------------
    """

    act.expected_stdout = expected_stdout_3x if act.is_version('<4') else expected_stdout_4x if act.is_version('<6') else expected_stdout_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
