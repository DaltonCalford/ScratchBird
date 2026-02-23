#coding:utf-8

"""
ID:          n/a
TITLE:       LOCAL TEMPORARY TABLE - basic test for datatypes.
DESCRIPTION:
    Test create LTT with all currently existing datatypes, including boundary sizes for some columns.
    Nulls are inserted in this table (just to check that no error occurs).
    Then mon$local_temporary_* tables are queried to show their data.
NOTES:
    [06.02.2026] pzotov
    Checked on 6.0.0.1405-761a49d.
"""
import locale
from pathlib import Path
import pytest
from firebird.qa import *

# NOTE: narrow charset required for this test!
# Otherwise test fails on attempt to create columns with len = 32765
# ("implementation limit...")
db = db_factory(charset = 'win1251')

substitutions = [('[ \t]+', ' ')]
act = python_act('db', substitutions = substitutions)
tmp_sql = temp_file('test_ltt_basic.sql')

@pytest.mark.version('>=6.0')
def test_1(act: Action, tmp_sql: Path, capsys):

    types_lst = [
        'smallint'
        ,'int'
        ,'bigint'
        ,'int128'
        ,'float'
        ,'real'
        ,'double precision'
        ,'long float'
        ,'numeric(3,2)'
        ,'decimal(3,2)'
        ,'numeric(9,2)'
        ,'decimal(9,2)'
        ,'numeric(18,2)'
        ,'decimal(18,2)'
        ,'numeric(38,38)'
        ,'decimal(38,38)'
        ,'decfloat(16)'
        ,'decfloat(34)'
        ,'date'
        ,'time'
        ,'timestamp'
        ,'time with time zone'
        ,'timestamp with time zone'
        ,'boolean'
        ,'char(1)'
        ,'char(32765)'
        ,'nchar(1)'
        ,'nchar(32765)'
        ,'varchar(1)'
        ,'varchar(32765)'
        ,'blob sub_type binary'
        ,'blob sub_type text'
        ,'blob sub_type binary segment size 12345'
        ,'blob sub_type text segment size 23451 character set win1250 collate WIN_CZ'
    ]    

    ddl_list = [ 'set bail on;', 'create local temporary table ltt_test (', ]

    for i,ftype in enumerate(types_lst):
        f_suffix = '_'.join( ftype.split(' ')[:4] )
        ddl_list.append( (',' if i > 0 else '') + f'"fld_{i:02d}_{f_suffix}" {ftype}' )
    
    ddl_list.append(');')
    ddl_list.append('commit;')

    ddl_list.append('insert into ltt_test select ' + ','.join( (['null'] * len(types_lst)) ) + ' from rdb$types;' )
    ddl_list.append('commit;')

    test_sql = '\n'.join(ddl_list)
    test_sql += """
        commit;
        set list on;
        set count on;
        select
            t.mon$table_name
            ,t.mon$schema_name
            ,t.mon$table_type
        from mon$local_temporary_tables t;

        select
             f.mon$field_name
            ,f.mon$field_type
            ,f.mon$field_precision
            ,f.mon$field_scale
            ,f.mon$field_length
            ,f.mon$field_sub_type
            ,f.mon$char_length
            ,f.mon$character_set_id
            ,f.mon$collation_id 
            ,c.rdb$character_set_name as charset_name
            ,k.rdb$collation_name as collation_name
        from mon$local_temporary_table_columns f
        left join rdb$character_sets c on f.mon$character_set_id = c.rdb$character_set_id
        left join rdb$collations k on c.rdb$character_set_id = k.rdb$character_set_id and f.mon$collation_id  = k.rdb$collation_id
        order by mon$field_position
        ;
        commit;
    """
    tmp_sql.write_text(test_sql, encoding = 'cp1251')

    expected_stdout = """
        MON$TABLE_NAME LTT_TEST
        MON$SCHEMA_NAME PUBLIC
        MON$TABLE_TYPE DELETE ROWS
        Records affected: 1

        MON$FIELD_NAME fld_00_smallint
        MON$FIELD_TYPE 8
        MON$FIELD_PRECISION 0
        MON$FIELD_SCALE 0
        MON$FIELD_LENGTH 2
        MON$FIELD_SUB_TYPE 0
        MON$CHAR_LENGTH 0
        MON$CHARACTER_SET_ID <null>
        MON$COLLATION_ID <null>
        CHARSET_NAME <null>
        COLLATION_NAME <null>
        MON$FIELD_NAME fld_01_int
        MON$FIELD_TYPE 9
        MON$FIELD_PRECISION 0
        MON$FIELD_SCALE 0
        MON$FIELD_LENGTH 4
        MON$FIELD_SUB_TYPE 0
        MON$CHAR_LENGTH 0
        MON$CHARACTER_SET_ID <null>
        MON$COLLATION_ID <null>
        CHARSET_NAME <null>
        COLLATION_NAME <null>
        MON$FIELD_NAME fld_02_bigint
        MON$FIELD_TYPE 19
        MON$FIELD_PRECISION 0
        MON$FIELD_SCALE 0
        MON$FIELD_LENGTH 8
        MON$FIELD_SUB_TYPE 0
        MON$CHAR_LENGTH 0
        MON$CHARACTER_SET_ID <null>
        MON$COLLATION_ID <null>
        CHARSET_NAME <null>
        COLLATION_NAME <null>
        MON$FIELD_NAME fld_03_int128
        MON$FIELD_TYPE 24
        MON$FIELD_PRECISION 0
        MON$FIELD_SCALE 0
        MON$FIELD_LENGTH 16
        MON$FIELD_SUB_TYPE 0
        MON$CHAR_LENGTH 0
        MON$CHARACTER_SET_ID <null>
        MON$COLLATION_ID <null>
        CHARSET_NAME <null>
        COLLATION_NAME <null>
        MON$FIELD_NAME fld_04_float
        MON$FIELD_TYPE 11
        MON$FIELD_PRECISION 0
        MON$FIELD_SCALE 0
        MON$FIELD_LENGTH 4
        MON$FIELD_SUB_TYPE 0
        MON$CHAR_LENGTH 0
        MON$CHARACTER_SET_ID <null>
        MON$COLLATION_ID <null>
        CHARSET_NAME <null>
        COLLATION_NAME <null>
        MON$FIELD_NAME fld_05_real
        MON$FIELD_TYPE 11
        MON$FIELD_PRECISION 0
        MON$FIELD_SCALE 0
        MON$FIELD_LENGTH 4
        MON$FIELD_SUB_TYPE 0
        MON$CHAR_LENGTH 0
        MON$CHARACTER_SET_ID <null>
        MON$COLLATION_ID <null>
        CHARSET_NAME <null>
        COLLATION_NAME <null>
        MON$FIELD_NAME fld_06_double_precision
        MON$FIELD_TYPE 12
        MON$FIELD_PRECISION 0
        MON$FIELD_SCALE 0
        MON$FIELD_LENGTH 8
        MON$FIELD_SUB_TYPE 0
        MON$CHAR_LENGTH 0
        MON$CHARACTER_SET_ID <null>
        MON$COLLATION_ID <null>
        CHARSET_NAME <null>
        COLLATION_NAME <null>
        MON$FIELD_NAME fld_07_long_float
        MON$FIELD_TYPE 12
        MON$FIELD_PRECISION 0
        MON$FIELD_SCALE 0
        MON$FIELD_LENGTH 8
        MON$FIELD_SUB_TYPE 0
        MON$CHAR_LENGTH 0
        MON$CHARACTER_SET_ID <null>
        MON$COLLATION_ID <null>
        CHARSET_NAME <null>
        COLLATION_NAME <null>
        MON$FIELD_NAME fld_08_numeric(3,2)
        MON$FIELD_TYPE 8
        MON$FIELD_PRECISION 3
        MON$FIELD_SCALE -2
        MON$FIELD_LENGTH 2
        MON$FIELD_SUB_TYPE 1
        MON$CHAR_LENGTH 0
        MON$CHARACTER_SET_ID <null>
        MON$COLLATION_ID <null>
        CHARSET_NAME <null>
        COLLATION_NAME <null>
        MON$FIELD_NAME fld_09_decimal(3,2)
        MON$FIELD_TYPE 9
        MON$FIELD_PRECISION 3
        MON$FIELD_SCALE -2
        MON$FIELD_LENGTH 4
        MON$FIELD_SUB_TYPE 2
        MON$CHAR_LENGTH 0
        MON$CHARACTER_SET_ID <null>
        MON$COLLATION_ID <null>
        CHARSET_NAME <null>
        COLLATION_NAME <null>
        MON$FIELD_NAME fld_10_numeric(9,2)
        MON$FIELD_TYPE 9
        MON$FIELD_PRECISION 9
        MON$FIELD_SCALE -2
        MON$FIELD_LENGTH 4
        MON$FIELD_SUB_TYPE 1
        MON$CHAR_LENGTH 0
        MON$CHARACTER_SET_ID <null>
        MON$COLLATION_ID <null>
        CHARSET_NAME <null>
        COLLATION_NAME <null>
        MON$FIELD_NAME fld_11_decimal(9,2)
        MON$FIELD_TYPE 9
        MON$FIELD_PRECISION 9
        MON$FIELD_SCALE -2
        MON$FIELD_LENGTH 4
        MON$FIELD_SUB_TYPE 2
        MON$CHAR_LENGTH 0
        MON$CHARACTER_SET_ID <null>
        MON$COLLATION_ID <null>
        CHARSET_NAME <null>
        COLLATION_NAME <null>
        MON$FIELD_NAME fld_12_numeric(18,2)
        MON$FIELD_TYPE 19
        MON$FIELD_PRECISION 18
        MON$FIELD_SCALE -2
        MON$FIELD_LENGTH 8
        MON$FIELD_SUB_TYPE 1
        MON$CHAR_LENGTH 0
        MON$CHARACTER_SET_ID <null>
        MON$COLLATION_ID <null>
        CHARSET_NAME <null>
        COLLATION_NAME <null>
        MON$FIELD_NAME fld_13_decimal(18,2)
        MON$FIELD_TYPE 19
        MON$FIELD_PRECISION 18
        MON$FIELD_SCALE -2
        MON$FIELD_LENGTH 8
        MON$FIELD_SUB_TYPE 2
        MON$CHAR_LENGTH 0
        MON$CHARACTER_SET_ID <null>
        MON$COLLATION_ID <null>
        CHARSET_NAME <null>
        COLLATION_NAME <null>
        MON$FIELD_NAME fld_14_numeric(38,38)
        MON$FIELD_TYPE 24
        MON$FIELD_PRECISION 38
        MON$FIELD_SCALE -38
        MON$FIELD_LENGTH 16
        MON$FIELD_SUB_TYPE 1
        MON$CHAR_LENGTH 0
        MON$CHARACTER_SET_ID <null>
        MON$COLLATION_ID <null>
        CHARSET_NAME <null>
        COLLATION_NAME <null>
        MON$FIELD_NAME fld_15_decimal(38,38)
        MON$FIELD_TYPE 24
        MON$FIELD_PRECISION 38
        MON$FIELD_SCALE -38
        MON$FIELD_LENGTH 16
        MON$FIELD_SUB_TYPE 2
        MON$CHAR_LENGTH 0
        MON$CHARACTER_SET_ID <null>
        MON$COLLATION_ID <null>
        CHARSET_NAME <null>
        COLLATION_NAME <null>
        MON$FIELD_NAME fld_16_decfloat(16)
        MON$FIELD_TYPE 22
        MON$FIELD_PRECISION 16
        MON$FIELD_SCALE 0
        MON$FIELD_LENGTH 8
        MON$FIELD_SUB_TYPE 0
        MON$CHAR_LENGTH 0
        MON$CHARACTER_SET_ID <null>
        MON$COLLATION_ID <null>
        CHARSET_NAME <null>
        COLLATION_NAME <null>
        MON$FIELD_NAME fld_17_decfloat(34)
        MON$FIELD_TYPE 23
        MON$FIELD_PRECISION 34
        MON$FIELD_SCALE 0
        MON$FIELD_LENGTH 16
        MON$FIELD_SUB_TYPE 0
        MON$CHAR_LENGTH 0
        MON$CHARACTER_SET_ID <null>
        MON$COLLATION_ID <null>
        CHARSET_NAME <null>
        COLLATION_NAME <null>
        MON$FIELD_NAME fld_18_date
        MON$FIELD_TYPE 14
        MON$FIELD_PRECISION 0
        MON$FIELD_SCALE 0
        MON$FIELD_LENGTH 4
        MON$FIELD_SUB_TYPE 0
        MON$CHAR_LENGTH 0
        MON$CHARACTER_SET_ID <null>
        MON$COLLATION_ID <null>
        CHARSET_NAME <null>
        COLLATION_NAME <null>
        MON$FIELD_NAME fld_19_time
        MON$FIELD_TYPE 15
        MON$FIELD_PRECISION 0
        MON$FIELD_SCALE 0
        MON$FIELD_LENGTH 4
        MON$FIELD_SUB_TYPE 0
        MON$CHAR_LENGTH 0
        MON$CHARACTER_SET_ID <null>
        MON$COLLATION_ID <null>
        CHARSET_NAME <null>
        COLLATION_NAME <null>
        MON$FIELD_NAME fld_20_timestamp
        MON$FIELD_TYPE 16
        MON$FIELD_PRECISION 0
        MON$FIELD_SCALE 0
        MON$FIELD_LENGTH 8
        MON$FIELD_SUB_TYPE 0
        MON$CHAR_LENGTH 0
        MON$CHARACTER_SET_ID <null>
        MON$COLLATION_ID <null>
        CHARSET_NAME <null>
        COLLATION_NAME <null>
        MON$FIELD_NAME fld_21_time_with_time_zone
        MON$FIELD_TYPE 25
        MON$FIELD_PRECISION 0
        MON$FIELD_SCALE 0
        MON$FIELD_LENGTH 8
        MON$FIELD_SUB_TYPE 0
        MON$CHAR_LENGTH 0
        MON$CHARACTER_SET_ID <null>
        MON$COLLATION_ID <null>
        CHARSET_NAME <null>
        COLLATION_NAME <null>
        MON$FIELD_NAME fld_22_timestamp_with_time_zone
        MON$FIELD_TYPE 26
        MON$FIELD_PRECISION 0
        MON$FIELD_SCALE 0
        MON$FIELD_LENGTH 12
        MON$FIELD_SUB_TYPE 0
        MON$CHAR_LENGTH 0
        MON$CHARACTER_SET_ID <null>
        MON$COLLATION_ID <null>
        CHARSET_NAME <null>
        COLLATION_NAME <null>
        MON$FIELD_NAME fld_23_boolean
        MON$FIELD_TYPE 21
        MON$FIELD_PRECISION 0
        MON$FIELD_SCALE 0
        MON$FIELD_LENGTH 1
        MON$FIELD_SUB_TYPE 0
        MON$CHAR_LENGTH 0
        MON$CHARACTER_SET_ID <null>
        MON$COLLATION_ID <null>
        CHARSET_NAME <null>
        COLLATION_NAME <null>
        MON$FIELD_NAME fld_24_char(1)
        MON$FIELD_TYPE 1
        MON$FIELD_PRECISION 0
        MON$FIELD_SCALE 0
        MON$FIELD_LENGTH 1
        MON$FIELD_SUB_TYPE 0
        MON$CHAR_LENGTH 1
        MON$CHARACTER_SET_ID 52
        MON$COLLATION_ID <null>
        CHARSET_NAME WIN1251
        COLLATION_NAME <null>
        MON$FIELD_NAME fld_25_char(32765)
        MON$FIELD_TYPE 1
        MON$FIELD_PRECISION 0
        MON$FIELD_SCALE 0
        MON$FIELD_LENGTH 32765
        MON$FIELD_SUB_TYPE 0
        MON$CHAR_LENGTH 32765
        MON$CHARACTER_SET_ID 52
        MON$COLLATION_ID <null>
        CHARSET_NAME WIN1251
        COLLATION_NAME <null>
        MON$FIELD_NAME fld_26_nchar(1)
        MON$FIELD_TYPE 1
        MON$FIELD_PRECISION 0
        MON$FIELD_SCALE 0
        MON$FIELD_LENGTH 1
        MON$FIELD_SUB_TYPE 0
        MON$CHAR_LENGTH 1
        MON$CHARACTER_SET_ID 21
        MON$COLLATION_ID <null>
        CHARSET_NAME ISO8859_1
        COLLATION_NAME <null>
        MON$FIELD_NAME fld_27_nchar(32765)
        MON$FIELD_TYPE 1
        MON$FIELD_PRECISION 0
        MON$FIELD_SCALE 0
        MON$FIELD_LENGTH 32765
        MON$FIELD_SUB_TYPE 0
        MON$CHAR_LENGTH 32765
        MON$CHARACTER_SET_ID 21
        MON$COLLATION_ID <null>
        CHARSET_NAME ISO8859_1
        COLLATION_NAME <null>
        MON$FIELD_NAME fld_28_varchar(1)
        MON$FIELD_TYPE 3
        MON$FIELD_PRECISION 0
        MON$FIELD_SCALE 0
        MON$FIELD_LENGTH 3
        MON$FIELD_SUB_TYPE 0
        MON$CHAR_LENGTH 1
        MON$CHARACTER_SET_ID 52
        MON$COLLATION_ID <null>
        CHARSET_NAME WIN1251
        COLLATION_NAME <null>
        MON$FIELD_NAME fld_29_varchar(32765)
        MON$FIELD_TYPE 3
        MON$FIELD_PRECISION 0
        MON$FIELD_SCALE 0
        MON$FIELD_LENGTH 32767
        MON$FIELD_SUB_TYPE 0
        MON$CHAR_LENGTH 32765
        MON$CHARACTER_SET_ID 52
        MON$COLLATION_ID <null>
        CHARSET_NAME WIN1251
        COLLATION_NAME <null>
        MON$FIELD_NAME fld_30_blob_sub_type_binary
        MON$FIELD_TYPE 17
        MON$FIELD_PRECISION 0
        MON$FIELD_SCALE 0
        MON$FIELD_LENGTH 8
        MON$FIELD_SUB_TYPE 80
        MON$CHAR_LENGTH 0
        MON$CHARACTER_SET_ID <null>
        MON$COLLATION_ID <null>
        CHARSET_NAME <null>
        COLLATION_NAME <null>
        MON$FIELD_NAME fld_31_blob_sub_type_text
        MON$FIELD_TYPE 17
        MON$FIELD_PRECISION 0
        MON$FIELD_SCALE 0
        MON$FIELD_LENGTH 8
        MON$FIELD_SUB_TYPE 80
        MON$CHAR_LENGTH 0
        MON$CHARACTER_SET_ID 52
        MON$COLLATION_ID <null>
        CHARSET_NAME WIN1251
        COLLATION_NAME <null>
        MON$FIELD_NAME fld_32_blob_sub_type_binary_segment
        MON$FIELD_TYPE 17
        MON$FIELD_PRECISION 0
        MON$FIELD_SCALE 0
        MON$FIELD_LENGTH 8
        MON$FIELD_SUB_TYPE 12345
        MON$CHAR_LENGTH 0
        MON$CHARACTER_SET_ID <null>
        MON$COLLATION_ID <null>
        CHARSET_NAME <null>
        COLLATION_NAME <null>
        MON$FIELD_NAME fld_33_blob_sub_type_text_segment
        MON$FIELD_TYPE 17
        MON$FIELD_PRECISION 0
        MON$FIELD_SCALE 0
        MON$FIELD_LENGTH 8
        MON$FIELD_SUB_TYPE 23451
        MON$CHAR_LENGTH 0
        MON$CHARACTER_SET_ID 51
        MON$COLLATION_ID 7
        CHARSET_NAME WIN1250
        COLLATION_NAME WIN_CZ
        Records affected: 34
    """

    act.expected_stdout = expected_stdout

    act.isql(switches=['-q'], charset = 'win1251', combine_output = True, input_file = tmp_sql, io_enc = locale.getpreferredencoding())
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()
