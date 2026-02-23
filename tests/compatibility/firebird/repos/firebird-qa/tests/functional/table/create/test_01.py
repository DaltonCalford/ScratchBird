#coding:utf-8

"""
ID:          table.create-01
TITLE:       CREATE TABLE - types
DESCRIPTION:
FBTEST:      functional.table.create.01
NOTES:
    [06.10.2023] pzotov
    1. Removed SHOW command. It is enough to check that we can add duplicate values in the table w/o UNQ.
    2. Script to create table with all possible datatypes is generated from list of allowable types depending on major FB version.
       Currently two lists of datatype names are used, see 'types_3x' and 'types_4x'.
"""

import pytest
from firebird.qa import *

init_sql = """
    set bail on;
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
        ,rf.rdb$field_position as field_pos
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
"""

db = db_factory(charset = 'utf8', init = init_sql)
act = python_act('db')

expected_stdout = """
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    types_3x = [
        'smallint'
        ,'int'
        ,'bigint'
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
        ,'date'
        ,'time'
        ,'timestamp'
        ,'boolean'
        ,'char(1)'
        ,'nchar(1)'
        ,'varchar(1)'
        ,'blob sub_type binary'
        ,'blob sub_type text'
        ,'blob sub_type binary segment size 12345'
        ,'blob sub_type text segment size 23451 character set win1250 collate WIN_CZ'
    ]

    types_4x = [
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
        ,'decfloat(16)'
        ,'decfloat(34)'
        ,'date'
        ,'time'
        ,'timestamp'
        ,'time with time zone'
        ,'timestamp with time zone'
        ,'boolean'
        ,'char(1)'
        ,'nchar(1)'
        ,'varchar(1)'
        ,'blob sub_type binary'
        ,'blob sub_type text'
        ,'blob sub_type binary segment size 12345'
        ,'blob sub_type text segment size 23451 character set win1250 collate WIN_CZ'
    ]    

    types_to_check = types_3x if act.is_version('<4') else types_4x

    ddl_list = [ 'create table test (', ]

    for i,ftype in enumerate(types_to_check):
        ddl_list.append( (',' if i > 0 else '') + f'fld_{i:02d} {ftype}' )
    
    ddl_list.append(');')
    
    test_sql = '\n'.join(ddl_list)
    test_sql += """
        commit;
        set list on;
        select v.* from v_fields_info v;
    """
    

    expected_stdout_3x = """
        FIELD_NAME                      FLD_00
        FIELD_TYPE                      SMALLINT
        FIELD_POS                       0
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_01
        FIELD_TYPE                      INTEGER
        FIELD_POS                       1
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_02
        FIELD_TYPE                      BIGINT
        FIELD_POS                       2
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_03
        FIELD_TYPE                      FLOAT
        FIELD_POS                       3
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_04
        FIELD_TYPE                      FLOAT
        FIELD_POS                       4
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_05
        FIELD_TYPE                      DOUBLE PRECISION
        FIELD_POS                       5
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_06
        FIELD_TYPE                      DOUBLE PRECISION
        FIELD_POS                       6
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_07
        FIELD_TYPE                      NUMERIC
        FIELD_POS                       7
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_08
        FIELD_TYPE                      DECIMAL
        FIELD_POS                       8
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_09
        FIELD_TYPE                      NUMERIC
        FIELD_POS                       9
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_10
        FIELD_TYPE                      DECIMAL
        FIELD_POS                       10
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_11
        FIELD_TYPE                      NUMERIC
        FIELD_POS                       11
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_12
        FIELD_TYPE                      DECIMAL
        FIELD_POS                       12
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_13
        FIELD_TYPE                      DATE
        FIELD_POS                       13
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_14
        FIELD_TYPE                      TIME WITHOUT TIME ZONE
        FIELD_POS                       14
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_15
        FIELD_TYPE                      TIMESTAMP WITHOUT TIME ZONE
        FIELD_POS                       15
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_16
        FIELD_TYPE                      BOOLEAN
        FIELD_POS                       16
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_17
        FIELD_TYPE                      CHAR
        FIELD_POS                       17
        FIELD_CHAR_LEN                  1
        FIELD_CSET_ID                   4
        FIELD_COLL_ID                   0
        CSET_NAME                       UTF8
        FIELD_COLLATION                 UTF8
        FIELD_NAME                      FLD_18
        FIELD_TYPE                      CHAR
        FIELD_POS                       18
        FIELD_CHAR_LEN                  1
        FIELD_CSET_ID                   21
        FIELD_COLL_ID                   0
        CSET_NAME                       ISO8859_1
        FIELD_COLLATION                 ISO8859_1
        FIELD_NAME                      FLD_19
        FIELD_TYPE                      VARCHAR
        FIELD_POS                       19
        FIELD_CHAR_LEN                  1
        FIELD_CSET_ID                   4
        FIELD_COLL_ID                   0
        CSET_NAME                       UTF8
        FIELD_COLLATION                 UTF8
        FIELD_NAME                      FLD_20
        FIELD_TYPE                      BLOB SUB_TYPE BINARY
        FIELD_POS                       20
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_21
        FIELD_TYPE                      BLOB SUB_TYPE TEXT
        FIELD_POS                       21
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   4
        FIELD_COLL_ID                   0
        CSET_NAME                       UTF8
        FIELD_COLLATION                 UTF8
        FIELD_NAME                      FLD_22
        FIELD_TYPE                      BLOB SUB_TYPE BINARY
        FIELD_POS                       22
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_23
        FIELD_TYPE                      BLOB SUB_TYPE TEXT
        FIELD_POS                       23
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   51
        FIELD_COLL_ID                   7
        CSET_NAME                       WIN1250
        FIELD_COLLATION                 WIN_CZ
    """

    expected_stdout_4x = """
        FIELD_NAME                      FLD_00
        FIELD_TYPE                      SMALLINT
        FIELD_POS                       0
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_01
        FIELD_TYPE                      INTEGER
        FIELD_POS                       1
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_02
        FIELD_TYPE                      BIGINT
        FIELD_POS                       2
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_03
        FIELD_TYPE                      INT128
        FIELD_POS                       3
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_04
        FIELD_TYPE                      FLOAT
        FIELD_POS                       4
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_05
        FIELD_TYPE                      FLOAT
        FIELD_POS                       5
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_06
        FIELD_TYPE                      DOUBLE PRECISION
        FIELD_POS                       6
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_07
        FIELD_TYPE                      DOUBLE PRECISION
        FIELD_POS                       7
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_08
        FIELD_TYPE                      NUMERIC
        FIELD_POS                       8
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_09
        FIELD_TYPE                      DECIMAL
        FIELD_POS                       9
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_10
        FIELD_TYPE                      NUMERIC
        FIELD_POS                       10
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_11
        FIELD_TYPE                      DECIMAL
        FIELD_POS                       11
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_12
        FIELD_TYPE                      NUMERIC
        FIELD_POS                       12
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_13
        FIELD_TYPE                      DECIMAL
        FIELD_POS                       13
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_14
        FIELD_TYPE                      DECFLOAT(16)
        FIELD_POS                       14
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_15
        FIELD_TYPE                      DECFLOAT(34)
        FIELD_POS                       15
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_16
        FIELD_TYPE                      DATE
        FIELD_POS                       16
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_17
        FIELD_TYPE                      TIME WITHOUT TIME ZONE
        FIELD_POS                       17
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_18
        FIELD_TYPE                      TIMESTAMP WITHOUT TIME ZONE
        FIELD_POS                       18
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_19
        FIELD_TYPE                      TIME WITH TIME ZONE
        FIELD_POS                       19
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_20
        FIELD_TYPE                      TIMESTAMP WITH TIME ZONE
        FIELD_POS                       20
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_21
        FIELD_TYPE                      BOOLEAN
        FIELD_POS                       21
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_22
        FIELD_TYPE                      CHAR
        FIELD_POS                       22
        FIELD_CHAR_LEN                  1
        FIELD_CSET_ID                   4
        FIELD_COLL_ID                   0
        CSET_NAME                       UTF8
        FIELD_COLLATION                 UTF8
        FIELD_NAME                      FLD_23
        FIELD_TYPE                      CHAR
        FIELD_POS                       23
        FIELD_CHAR_LEN                  1
        FIELD_CSET_ID                   21
        FIELD_COLL_ID                   0
        CSET_NAME                       ISO8859_1
        FIELD_COLLATION                 ISO8859_1
        FIELD_NAME                      FLD_24
        FIELD_TYPE                      VARCHAR
        FIELD_POS                       24
        FIELD_CHAR_LEN                  1
        FIELD_CSET_ID                   4
        FIELD_COLL_ID                   0
        CSET_NAME                       UTF8
        FIELD_COLLATION                 UTF8
        FIELD_NAME                      FLD_25
        FIELD_TYPE                      BLOB SUB_TYPE BINARY
        FIELD_POS                       25
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_26
        FIELD_TYPE                      BLOB SUB_TYPE TEXT
        FIELD_POS                       26
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   4
        FIELD_COLL_ID                   0
        CSET_NAME                       UTF8
        FIELD_COLLATION                 UTF8
        FIELD_NAME                      FLD_27
        FIELD_TYPE                      BLOB SUB_TYPE BINARY
        FIELD_POS                       27
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   <null>
        FIELD_COLL_ID                   <null>
        CSET_NAME                       <null>
        FIELD_COLLATION                 <null>
        FIELD_NAME                      FLD_28
        FIELD_TYPE                      BLOB SUB_TYPE TEXT
        FIELD_POS                       28
        FIELD_CHAR_LEN                  <null>
        FIELD_CSET_ID                   51
        FIELD_COLL_ID                   7
        CSET_NAME                       WIN1250
        FIELD_COLLATION                 WIN_CZ
    """

    act.expected_stdout = expected_stdout_3x if act.is_version('<4') else expected_stdout_4x

    act.isql(switches=['-q'], combine_output = True, input = test_sql)
    assert act.clean_stdout == act.clean_expected_stdout
