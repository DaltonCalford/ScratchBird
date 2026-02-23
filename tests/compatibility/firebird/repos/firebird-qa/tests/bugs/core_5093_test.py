#coding:utf-8

"""
ID:          issue-5378
ISSUE:       5378
TITLE:       Alter computed field type does not work
DESCRIPTION:
    Test creates table with fields of (almost) all possible datatypes.
    Then we apply "ALTER TABLE ALTER FIELD ..., ALTER FIELD ..." so that every field is changed,
    either by updating its computed-by value or type (for text fields - also add/remove charset).
    Expression for ALTER TABLE - see literal "alter_table_ddl", encoded in UTF8.
    NB: changing character set should NOT be reflected on SQLDA output (at least for current FB builds).
JIRA:        CORE-5093
FBTEST:      bugs.core_5093
NOTES:
    [23.01.2024] pzotov
    Adjusted output after fixed gh-7924: column 'b_added_charset' character set must be changed to utf8.

    [24.01.2024] pzotov
    Currently gh-7924 fixed only for FB 6.x, thus charsets for FB 3.x ... 5.x will not be changed.
    Because of that, expected_output depends on major FB version, see its definition in 'blob_new_cset'.
    Checked on 6.0.0.223, 5.0.1.1322

    [01.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    
    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

substitutions = [('DATABASE: LOCALHOST.*TEST.FDB.*', ''),
                 ('.*TABLE: T1.*', ''), ('.*MESSAGE FIELD COUNT.*', ''),
                 ('  ', ' ')]

init_script = """
    recreate table t1 (
         n0 int

        ,si smallint computed by(32767)
        ,bi bigint computed by (2147483647)
        ,s2 smallint computed by ( mod(bi, nullif(si,0)) )

        ,dx double precision computed by (pi())
        ,fx float computed by (dx*dx)
        ,nf numeric(3,1) computed by (fx)

        ,dt date computed by ('now')
        ,tm time computed by ('now')

        ,c_change_cb_value char character set win1251 computed by ('ы') -- for this field we only will change value inside its COMPUTED_BY clause; this should NOT bring any affect on SQLDA output.
        ,c_change_charset char character set win1252 computed by ('å') -- for this field we'll only change CHARACTER SET, but this will not has effect (at least on current FB builds)
        ,c_change_length char character set utf8 computed by ('∑') -- for this field we'll only increase its length

        ,b_change_cb_value blob character set win1251 computed by ('ы') -- for this field we only will change value inside its COMPUTED_BY clause; this should NOT bring any affect on SQLDA output.
        ,b_change_charset blob character set win1252 computed by ('å') -- for this field we'll only change CHARACTER SET, but this will not has effect (at least on current FB builds)
        ,b_remove_charset blob character set win1252 computed by ('ä') -- for this field we'll only REMOVE its CHARACTER SET; this should change this blob subtype from 1 to 0
        ,b_added_charset blob computed by ('∑') -- for this field we'll only ADD definition of CHARACTER SET; this should change this blob subtype from 0 to 1
    );
    commit;
"""

db = db_factory(charset='UTF8', init=init_script)

act = python_act('db', substitutions=substitutions)

sql_script = """
    alter table t1
        alter si type int computed by (32767) -- LONG
       ,alter bi type int computed by (2147483647) -- LONG
       ,alter s2 type smallint computed by ( 1 + mod(bi, nullif(si,0)) ) -- SHORT

       ,alter dx type float computed by( pi()/2 ) -- FLOAT
       ,alter fx type float computed by (dx*dx*dx) -- FLOAT
       ,alter nf type bigint computed by (fx * fx) -- INT64

       ,alter dt type date computed by ('today') -- DATE
       ,alter tm type timestamp computed by ('now') -- TIMESTAMP

       ,alter c_change_cb_value type char character set win1251 computed by ('Ё') -- TEXT
       ,alter c_change_charset type char character set utf8 computed by ('Æ') -- TEXT
       ,alter c_change_length type char(2) computed by ('∑∞')    -- TEXT

        -- All these fields, of course, should remain in type = BLOB,
        -- but when charset is removed (field "b_remove_charset") then blob subtype has to be changed to 0,
        -- and when we ADD charset (field "b_added_charset") then blob subtype has to be changed to 1.
       ,alter b_change_cb_value type blob character set win1251 computed by ('Ё') -- BLOB
       ,alter b_change_charset type blob character set iso8859_1 computed by ('å') -- BLOB
       ,alter b_remove_charset type blob /*character set win1252 */ computed by ('Æ') -- BLOB
       ,alter b_added_charset type blob character set utf8 computed by ('∞')    -- BLOB
    ;
    commit;
    set sqlda_display on;
    select * from t1;
"""

BLOB_NEW_CSET_5X = 'CHARSET: 0 NONE'
BLOB_NEW_CSET_6X = 'CHARSET: 4 UTF8'

@pytest.mark.version('>=3.0')
def test_1(act: Action):

    expected_out_5x_a = """
        01: SQLTYPE: 496 LONG NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 4
        : NAME: N0 ALIAS: N0
        02: SQLTYPE: 500 SHORT NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 2
        : NAME: SI ALIAS: SI
        03: SQLTYPE: 580 INT64 NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 8
        : NAME: BI ALIAS: BI
        04: SQLTYPE: 500 SHORT NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 2
        : NAME: S2 ALIAS: S2
        05: SQLTYPE: 480 DOUBLE NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 8
        : NAME: DX ALIAS: DX
        06: SQLTYPE: 482 FLOAT NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 4
        : NAME: FX ALIAS: FX
        07: SQLTYPE: 500 SHORT NULLABLE SCALE: -1 SUBTYPE: 1 LEN: 2
        : NAME: NF ALIAS: NF
        08: SQLTYPE: 570 SQL DATE NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 4
        : NAME: DT ALIAS: DT
        09: SQLTYPE: 560 TIME NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 4
        : NAME: TM ALIAS: TM
        10: SQLTYPE: 452 TEXT NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 4 CHARSET: 4 UTF8
        : NAME: C_CHANGE_CB_VALUE ALIAS: C_CHANGE_CB_VALUE
        11: SQLTYPE: 452 TEXT NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 4 CHARSET: 4 UTF8
        : NAME: C_CHANGE_CHARSET ALIAS: C_CHANGE_CHARSET
        12: SQLTYPE: 452 TEXT NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 4 CHARSET: 4 UTF8
        : NAME: C_CHANGE_LENGTH ALIAS: C_CHANGE_LENGTH
        13: SQLTYPE: 520 BLOB NULLABLE SCALE: 0 SUBTYPE: 1 LEN: 8 CHARSET: 4 UTF8
        : NAME: B_CHANGE_CB_VALUE ALIAS: B_CHANGE_CB_VALUE
        14: SQLTYPE: 520 BLOB NULLABLE SCALE: 0 SUBTYPE: 1 LEN: 8 CHARSET: 4 UTF8
        : NAME: B_CHANGE_CHARSET ALIAS: B_CHANGE_CHARSET
        15: SQLTYPE: 520 BLOB NULLABLE SCALE: 0 SUBTYPE: 1 LEN: 8 CHARSET: 4 UTF8
        : NAME: B_REMOVE_CHARSET ALIAS: B_REMOVE_CHARSET
        16: SQLTYPE: 520 BLOB NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 8
        : NAME: B_ADDED_CHARSET ALIAS: B_ADDED_CHARSET
    """

    expected_out_6x_a = """
        01: SQLTYPE: 496 LONG NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 4
        : NAME: N0 ALIAS: N0
        02: SQLTYPE: 500 SHORT NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 2
        : NAME: SI ALIAS: SI
        03: SQLTYPE: 580 INT64 NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 8
        : NAME: BI ALIAS: BI
        04: SQLTYPE: 500 SHORT NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 2
        : NAME: S2 ALIAS: S2
        05: SQLTYPE: 480 DOUBLE NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 8
        : NAME: DX ALIAS: DX
        06: SQLTYPE: 482 FLOAT NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 4
        : NAME: FX ALIAS: FX
        07: SQLTYPE: 500 SHORT NULLABLE SCALE: -1 SUBTYPE: 1 LEN: 2
        : NAME: NF ALIAS: NF
        08: SQLTYPE: 570 SQL DATE NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 4
        : NAME: DT ALIAS: DT
        09: SQLTYPE: 560 TIME NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 4
        : NAME: TM ALIAS: TM
        10: SQLTYPE: 452 TEXT NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 4 CHARSET: 4 SYSTEM.UTF8
        : NAME: C_CHANGE_CB_VALUE ALIAS: C_CHANGE_CB_VALUE
        11: SQLTYPE: 452 TEXT NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 4 CHARSET: 4 SYSTEM.UTF8
        : NAME: C_CHANGE_CHARSET ALIAS: C_CHANGE_CHARSET
        12: SQLTYPE: 452 TEXT NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 4 CHARSET: 4 SYSTEM.UTF8
        : NAME: C_CHANGE_LENGTH ALIAS: C_CHANGE_LENGTH
        13: SQLTYPE: 520 BLOB NULLABLE SCALE: 0 SUBTYPE: 1 LEN: 8 CHARSET: 4 SYSTEM.UTF8
        : NAME: B_CHANGE_CB_VALUE ALIAS: B_CHANGE_CB_VALUE
        14: SQLTYPE: 520 BLOB NULLABLE SCALE: 0 SUBTYPE: 1 LEN: 8 CHARSET: 4 SYSTEM.UTF8
        : NAME: B_CHANGE_CHARSET ALIAS: B_CHANGE_CHARSET
        15: SQLTYPE: 520 BLOB NULLABLE SCALE: 0 SUBTYPE: 1 LEN: 8 CHARSET: 4 SYSTEM.UTF8
        : NAME: B_REMOVE_CHARSET ALIAS: B_REMOVE_CHARSET
        16: SQLTYPE: 520 BLOB NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 8
        : NAME: B_ADDED_CHARSET ALIAS: B_ADDED_CHARSET
    """

    act.expected_stdout = expected_out_5x_a if act.is_version('<6') else expected_out_6x_a
    act.isql(switches=['-q', '-m'], input='set sqlda_display on; select * from t1;')
    act.stdout = act.stdout.upper()
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    ####################################################
    # ::: NB :::
    # We have to separate result for B_ADDED_CHARSET because it differs in FB 6.x and older versions
    #
    blob_new_cset = BLOB_NEW_CSET_5X if act.is_version('<6') else BLOB_NEW_CSET_6X
    ####################################################

    expected_out_5x_b = f"""
        01: SQLTYPE: 496 LONG NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 4
        : NAME: N0 ALIAS: N0
        02: SQLTYPE: 496 LONG NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 4
        : NAME: SI ALIAS: SI
        03: SQLTYPE: 496 LONG NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 4
        : NAME: BI ALIAS: BI
        04: SQLTYPE: 500 SHORT NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 2
        : NAME: S2 ALIAS: S2
        05: SQLTYPE: 482 FLOAT NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 4
        : NAME: DX ALIAS: DX
        06: SQLTYPE: 482 FLOAT NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 4
        : NAME: FX ALIAS: FX
        07: SQLTYPE: 580 INT64 NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 8
        : NAME: NF ALIAS: NF
        08: SQLTYPE: 570 SQL DATE NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 4
        : NAME: DT ALIAS: DT
        09: SQLTYPE: 510 TIMESTAMP NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 8
        : NAME: TM ALIAS: TM
        10: SQLTYPE: 452 TEXT NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 4 CHARSET: 4 UTF8
        : NAME: C_CHANGE_CB_VALUE ALIAS: C_CHANGE_CB_VALUE
        11: SQLTYPE: 452 TEXT NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 4 CHARSET: 4 UTF8
        : NAME: C_CHANGE_CHARSET ALIAS: C_CHANGE_CHARSET
        12: SQLTYPE: 452 TEXT NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 8 CHARSET: 4 UTF8
        : NAME: C_CHANGE_LENGTH ALIAS: C_CHANGE_LENGTH
        13: SQLTYPE: 520 BLOB NULLABLE SCALE: 0 SUBTYPE: 1 LEN: 8 CHARSET: 4 UTF8
        : NAME: B_CHANGE_CB_VALUE ALIAS: B_CHANGE_CB_VALUE
        14: SQLTYPE: 520 BLOB NULLABLE SCALE: 0 SUBTYPE: 1 LEN: 8 CHARSET: 4 UTF8
        : NAME: B_CHANGE_CHARSET ALIAS: B_CHANGE_CHARSET
        15: SQLTYPE: 520 BLOB NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 8
        : NAME: B_REMOVE_CHARSET ALIAS: B_REMOVE_CHARSET
        16: SQLTYPE: 520 BLOB NULLABLE SCALE: 0 SUBTYPE: 1 LEN: 8 {blob_new_cset}
        : NAME: B_ADDED_CHARSET ALIAS: B_ADDED_CHARSET
    """

    expected_out_6x_b = f"""
        01: SQLTYPE: 496 LONG NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 4
        : NAME: N0 ALIAS: N0
        02: SQLTYPE: 496 LONG NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 4
        : NAME: SI ALIAS: SI
        03: SQLTYPE: 496 LONG NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 4
        : NAME: BI ALIAS: BI
        04: SQLTYPE: 500 SHORT NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 2
        : NAME: S2 ALIAS: S2
        05: SQLTYPE: 482 FLOAT NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 4
        : NAME: DX ALIAS: DX
        06: SQLTYPE: 482 FLOAT NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 4
        : NAME: FX ALIAS: FX
        07: SQLTYPE: 580 INT64 NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 8
        : NAME: NF ALIAS: NF
        08: SQLTYPE: 570 SQL DATE NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 4
        : NAME: DT ALIAS: DT
        09: SQLTYPE: 510 TIMESTAMP NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 8
        : NAME: TM ALIAS: TM
        10: SQLTYPE: 452 TEXT NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 4 CHARSET: 4 SYSTEM.UTF8
        : NAME: C_CHANGE_CB_VALUE ALIAS: C_CHANGE_CB_VALUE
        11: SQLTYPE: 452 TEXT NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 4 CHARSET: 4 SYSTEM.UTF8
        : NAME: C_CHANGE_CHARSET ALIAS: C_CHANGE_CHARSET
        12: SQLTYPE: 452 TEXT NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 8 CHARSET: 4 SYSTEM.UTF8
        : NAME: C_CHANGE_LENGTH ALIAS: C_CHANGE_LENGTH
        13: SQLTYPE: 520 BLOB NULLABLE SCALE: 0 SUBTYPE: 1 LEN: 8 CHARSET: 4 SYSTEM.UTF8
        : NAME: B_CHANGE_CB_VALUE ALIAS: B_CHANGE_CB_VALUE
        14: SQLTYPE: 520 BLOB NULLABLE SCALE: 0 SUBTYPE: 1 LEN: 8 CHARSET: 4 SYSTEM.UTF8
        : NAME: B_CHANGE_CHARSET ALIAS: B_CHANGE_CHARSET
        15: SQLTYPE: 520 BLOB NULLABLE SCALE: 0 SUBTYPE: 0 LEN: 8
        : NAME: B_REMOVE_CHARSET ALIAS: B_REMOVE_CHARSET
        16: SQLTYPE: 520 BLOB NULLABLE SCALE: 0 SUBTYPE: 1 LEN: 8 CHARSET: 4 SYSTEM.UTF8
        : NAME: B_ADDED_CHARSET ALIAS: B_ADDED_CHARSET
    """

    act.expected_stdout = expected_out_5x_b if act.is_version('<6') else expected_out_6x_b
    act.isql(switches=['-q', '-m'], input=sql_script)
    act.stdout = act.stdout.upper()
    assert act.clean_stdout == act.clean_expected_stdout
