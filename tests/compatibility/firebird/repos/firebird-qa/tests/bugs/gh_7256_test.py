#coding:utf-8

"""
ID:          issue-7256
ISSUE:       7256
TITLE:       Inconsistent conversion of non-TEXT blobs in BLOB_APPEND
DESCRIPTION:
        1. Test makes TWO subsequent connections:
            1) to write blobs in the table using charset UTF8
            2) to READ that table using charset NONE.
        This must be done in order to see how rules for BLOB_APPEND() datatype and charset work.
        Otherwise (without reconnect with charset = NONE) BLOB_APPEND() will always return blob with charset
        that equals to charset of established connection, with one exception:
        "if first non-NULL argument is [var]char with charset OCTETS, then create BLOB SUB_TYPE BINARY"
        (rules for BLOB_APPEND() result have been discussed in the fb-devel, 14.08.2022 ... 16.08.2022).

        2. Non-ascii characters are used intentionally. But they all present in both UTF8 and ISO8859_1 charsets.

        3. Statement 'select blob_append(<list of nuls>) from test' currently does not show <null> literal.
           This is a bug and must/will be fixed. After that, test will be adjusted.
NOTES:
    [22.02.2023] pzotov
        Thanks to Vlad for suggestions. Discussed 20-21 feb 2023.
        Checked on 5.0.0.958.
     [03.03.2023] pzotov
        Added 'set blob all' because result of blob_append(null, null) must be visible as literal '<null>'.
        Added substitution for suppressing 'Nullable' flags in the SQLDA output: it is sufficient for this test
        to check only datatypes of result.
        Discussed with Vlad, letters 02-mar-2023 16:01 and 03-mar-2023 14:43.
         Checked on 5.0.0.967, 4.0.3.2904 (intermediate build 03-mar-2023 12:33)
    [14.12.2023] pzotov
        Added 'SQLSTATE' in substitutions: runtime error must not be filtered out by '?!(...)' pattern
        ("negative lookahead assertion", see https://docs.python.org/3/library/re.html#regular-expression-syntax).
        Added 'combine_output = True' in order to see SQLSTATE if any error occurs.
    [04.07.2025] pzotov
        Added 'SQL_SCHEMA_PREFIX' to be substituted in expected_* on FB 6.x
        Checked on 6.0.0.894; 5.0.3.1668; 4.0.6.3214.
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table test(
        id int primary key
        ,fbo blob character set octets
        ,fbi blob character set iso8859_1
        ,fbu blob character set utf8
        ,fco varchar(8) character set octets
        ,fci varchar(8) character set iso8859_1
        ,fcu varchar(8) character set utf8
        ,num int
        ,boo boolean
    );
    recreate view v_test as select * from test;
    commit;

    insert into test (
        id
        ,fbo
        ,fbi
        ,fbu
        ,fco
        ,fci
        ,fcu
        ,num
        ,boo
    )
    values(
        1
        ,x'deadbeef' -- (select rdb$db_key from rdb$database)
        ,'ÀÆÇËÌÐÑÖ'
        ,'×ØÜÝÞßàá'
        ,x'beefdead' -- (select rdb$db_key from rdb$database)
        ,'åæçëïðñò'
        ,'øùúûüýþÿ'
        ,123456
        ,true
    );
    commit;
"""
db = db_factory(charset = 'utf8', init = init_script, utf8filename=True)

test_script = """
    set sqlda_display on;
    set blob all;
    set list on;

    set planonly;

    -- if first non-NULL argument is blob, then use its blob subtype and charset
    select blob_append(null, fbo, 'foo', 123) as blob_result_1 from test;
    -----------------------------------------------------
    -- if first non-NULL argument is [var]char with charset OCTETS, then create BLOB SUB_TYPE BINARY
    select blob_append(null, fco, 'bar', 456) as blob_result_2 from test;
    -----------------------------------------------------
    -- if first non-NULL argument is [var]char with charset other than OCTETS, then use its charset and create BLOB SUB_TYPE TEXT
    select blob_append(null, fci, num, 'iso') as blob_result_3a from test;
    select blob_append(null, fcu, num, 'utf') as blob_result_3b from test;
    -----------------------------------------------------
    -- if first non-NULL argument is not a blob nor [var]char, then create BLOB SUB_TYPE TEXT CHARACTER SET ASCII
    select blob_append(null, num, current_date, boo) as blob_result_4 from test;
    -----------------------------------------------------
    -- if all arguments is NULL, then return NULL

    set planonly;
    select blob_append(null, null, null) as blob_result_5 from test;
"""

act = isql_act('db', test_script, substitutions = [('^((?!SQLSTATE|sqltype:|BLOB_RESULT).)*$', ''), ('BLOB Nullable', 'BLOB'), ('[ \t]+', ' ')])

@pytest.mark.version('>=4.0.3')
def test_1(act: Action):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else 'SYSTEM.'
    expected_stdout = f"""
        01: sqltype: 520 BLOB scale: 0 subtype: 1 len: 8 charset: 1 {SQL_SCHEMA_PREFIX}OCTETS
        :  name: BLOB_APPEND  alias: BLOB_RESULT_1

        01: sqltype: 520 BLOB scale: 0 subtype: 0 len: 8
        :  name: BLOB_APPEND  alias: BLOB_RESULT_2

        01: sqltype: 520 BLOB scale: 0 subtype: 1 len: 8 charset: 21 {SQL_SCHEMA_PREFIX}ISO8859_1
        :  name: BLOB_APPEND  alias: BLOB_RESULT_3A

        01: sqltype: 520 BLOB scale: 0 subtype: 1 len: 8 charset: 4 {SQL_SCHEMA_PREFIX}UTF8
        :  name: BLOB_APPEND  alias: BLOB_RESULT_3B

        01: sqltype: 520 BLOB scale: 0 subtype: 1 len: 8 charset: 2 {SQL_SCHEMA_PREFIX}ASCII
        :  name: BLOB_APPEND  alias: BLOB_RESULT_4

        01: sqltype: 520 BLOB Nullable scale: 0 subtype: 0 len: 8
        :  name: BLOB_APPEND  alias: BLOB_RESULT_5

        BLOB_RESULT_5 <null>
    """

    act.expected_stdout = expected_stdout
    act.execute(combine_output = True, charset = 'None' )
    assert act.clean_stdout == act.clean_expected_stdout
    	