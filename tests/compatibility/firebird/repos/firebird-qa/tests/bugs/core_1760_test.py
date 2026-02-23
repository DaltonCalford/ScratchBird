#coding:utf-8

"""
ID:          issue-2185
ISSUE:       2185
TITLE:       Support hex numeric and string literals
DESCRIPTION: See doc\\sql.extensions\\README.hex_literals.txt
JIRA:        CORE-1760
FBTEST:      bugs.core_1760
NOTES:
    [26.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.863; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    -- binary literal ::=  { x | X } <quote> [ { <hexit> <hexit> }... ] <quote>

    select x'1' from rdb$database; -- raises: token unknown because length is odd

    select x'11' from rdb$database; -- must raise: token unknown because length is odd

    select x'0123456789' from rdb$database;

    select x'01234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789' from rdb$database;

    -- must raise: token unknown because last char is not hexit
    select x'0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678x' from rdb$database;

    select uuid_to_char(x'BA1749B583BF9146B360F54E25FE583E') from rdb$database;


    -- ##############################################################################

    -- Numeric literal: { 0x | 0X } <hexit> [ <hexit>... ]
    -- https://firebirdsql.org/refdocs/langrefupd25-bigint.html
    recreate view v_test as
    select
        +-0x1              "-1(a)"
       ,-+-0xf             "+15"
       ,0x7FFF             "32767"
       ,0x8000             "32768"
       ,0xFFFF             "65535"
       ,0x10000            "65536(a)"
       ,0x000000000010000  "65536(b)"
       ,0x80000000         "-2147483648"
       ,0x080000000        "+2147483648(a)"
       ,0x000000080000000  "+2147483648(b)"
       ,0XFFFFFFFF         "-1(b)"
       ,0X0FFFFFFFF        "+4294967295"
       ,0x100000000        "+4294967296(a)"
       ,0x0000000100000000 "+4294967296(b)"
       ,0X7FFFFFFFFFFFFFFF "9223372036854775807"
       ,0x8000000000000000 "-9223372036854775808"
       ,0x8000000000000001 "-9223372036854775807"
       ,0x8000000000000002 "-9223372036854775806"
       ,0xffffffffffffffff "-1(c)"
    from rdb$database;

    select * from v_test;

    -- If the number of <hexit> is greater than 8, the constant data type is a signed BIGINT
    -- If it's less or equal than 8, the data type is a signed INTEGER
    set sqlda_display on;
    select * from v_test rows 0;
    set sqlda_display off;
"""

act = isql_act('db', test_script,
               substitutions=[('.*At line.*', ''), ('-Token unknown.*', '-Token unknown')])

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    if act.is_version('<6'):
        expected_sqlda = """
            INPUT message field count: 0
            OUTPUT message field count: 19
            01: sqltype: 496 LONG Nullable scale: 0 subtype: 0 len: 4
              :  name: -1(a)  alias: -1(a)
              : table: V_TEST  owner: SYSDBA
            02: sqltype: 496 LONG Nullable scale: 0 subtype: 0 len: 4
              :  name: +15  alias: +15
              : table: V_TEST  owner: SYSDBA
            03: sqltype: 496 LONG Nullable scale: 0 subtype: 0 len: 4
              :  name: 32767  alias: 32767
              : table: V_TEST  owner: SYSDBA
            04: sqltype: 496 LONG Nullable scale: 0 subtype: 0 len: 4
              :  name: 32768  alias: 32768
              : table: V_TEST  owner: SYSDBA
            05: sqltype: 496 LONG Nullable scale: 0 subtype: 0 len: 4
              :  name: 65535  alias: 65535
              : table: V_TEST  owner: SYSDBA
            06: sqltype: 496 LONG Nullable scale: 0 subtype: 0 len: 4
              :  name: 65536(a)  alias: 65536(a)
              : table: V_TEST  owner: SYSDBA
            07: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
              :  name: 65536(b)  alias: 65536(b)
              : table: V_TEST  owner: SYSDBA
            08: sqltype: 496 LONG Nullable scale: 0 subtype: 0 len: 4
              :  name: -2147483648  alias: -2147483648
              : table: V_TEST  owner: SYSDBA
            09: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
              :  name: +2147483648(a)  alias: +2147483648(a)
              : table: V_TEST  owner: SYSDBA
            10: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
              :  name: +2147483648(b)  alias: +2147483648(b)
              : table: V_TEST  owner: SYSDBA
            11: sqltype: 496 LONG Nullable scale: 0 subtype: 0 len: 4
              :  name: -1(b)  alias: -1(b)
              : table: V_TEST  owner: SYSDBA
            12: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
              :  name: +4294967295  alias: +4294967295
              : table: V_TEST  owner: SYSDBA
            13: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
              :  name: +4294967296(a)  alias: +4294967296(a)
              : table: V_TEST  owner: SYSDBA
            14: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
              :  name: +4294967296(b)  alias: +4294967296(b)
              : table: V_TEST  owner: SYSDBA
            15: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
              :  name: 9223372036854775807  alias: 9223372036854775807
              : table: V_TEST  owner: SYSDBA
            16: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
              :  name: -9223372036854775808  alias: -9223372036854775808
              : table: V_TEST  owner: SYSDBA
            17: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
              :  name: -9223372036854775807  alias: -9223372036854775807
              : table: V_TEST  owner: SYSDBA
            18: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
              :  name: -9223372036854775806  alias: -9223372036854775806
              : table: V_TEST  owner: SYSDBA
            19: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
              :  name: -1(c)  alias: -1(c)
              : table: V_TEST  owner: SYSDBA
        """
    else:
        expected_sqlda = """
            INPUT message field count: 0
            OUTPUT message field count: 19
            01: sqltype: 496 LONG Nullable scale: 0 subtype: 0 len: 4
            :  name: -1(a)  alias: -1(a)
            : table: V_TEST  schema: PUBLIC  owner: SYSDBA
            02: sqltype: 496 LONG Nullable scale: 0 subtype: 0 len: 4
            :  name: +15  alias: +15
            : table: V_TEST  schema: PUBLIC  owner: SYSDBA
            03: sqltype: 496 LONG Nullable scale: 0 subtype: 0 len: 4
            :  name: 32767  alias: 32767
            : table: V_TEST  schema: PUBLIC  owner: SYSDBA
            04: sqltype: 496 LONG Nullable scale: 0 subtype: 0 len: 4
            :  name: 32768  alias: 32768
            : table: V_TEST  schema: PUBLIC  owner: SYSDBA
            05: sqltype: 496 LONG Nullable scale: 0 subtype: 0 len: 4
            :  name: 65535  alias: 65535
            : table: V_TEST  schema: PUBLIC  owner: SYSDBA
            06: sqltype: 496 LONG Nullable scale: 0 subtype: 0 len: 4
            :  name: 65536(a)  alias: 65536(a)
            : table: V_TEST  schema: PUBLIC  owner: SYSDBA
            07: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
            :  name: 65536(b)  alias: 65536(b)
            : table: V_TEST  schema: PUBLIC  owner: SYSDBA
            08: sqltype: 496 LONG Nullable scale: 0 subtype: 0 len: 4
            :  name: -2147483648  alias: -2147483648
            : table: V_TEST  schema: PUBLIC  owner: SYSDBA
            09: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
            :  name: +2147483648(a)  alias: +2147483648(a)
            : table: V_TEST  schema: PUBLIC  owner: SYSDBA
            10: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
            :  name: +2147483648(b)  alias: +2147483648(b)
            : table: V_TEST  schema: PUBLIC  owner: SYSDBA
            11: sqltype: 496 LONG Nullable scale: 0 subtype: 0 len: 4
            :  name: -1(b)  alias: -1(b)
            : table: V_TEST  schema: PUBLIC  owner: SYSDBA
            12: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
            :  name: +4294967295  alias: +4294967295
            : table: V_TEST  schema: PUBLIC  owner: SYSDBA
            13: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
            :  name: +4294967296(a)  alias: +4294967296(a)
            : table: V_TEST  schema: PUBLIC  owner: SYSDBA
            14: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
            :  name: +4294967296(b)  alias: +4294967296(b)
            : table: V_TEST  schema: PUBLIC  owner: SYSDBA
            15: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
            :  name: 9223372036854775807  alias: 9223372036854775807
            : table: V_TEST  schema: PUBLIC  owner: SYSDBA
            16: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
            :  name: -9223372036854775808  alias: -9223372036854775808
            : table: V_TEST  schema: PUBLIC  owner: SYSDBA
            17: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
            :  name: -9223372036854775807  alias: -9223372036854775807
            : table: V_TEST  schema: PUBLIC  owner: SYSDBA
            18: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
            :  name: -9223372036854775806  alias: -9223372036854775806
            : table: V_TEST  schema: PUBLIC  owner: SYSDBA
            19: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
            :  name: -1(c)  alias: -1(c)
            : table: V_TEST  schema: PUBLIC  owner: SYSDBA
        """

    expected_stdout = f"""
        CONSTANT                        11
        CONSTANT                        0123456789
        CONSTANT                        01234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789
        UUID_TO_CHAR                    BA1749B5-83BF-9146-B360-F54E25FE583E
        -1(a)                           -1
        +15                             15
        32767                           32767
        32768                           32768
        65535                           65535
        65536(a)                        65536
        65536(b)                        65536
        -2147483648                     -2147483648
        +2147483648(a)                  2147483648
        +2147483648(b)                  2147483648
        -1(b)                           -1
        +4294967295                     4294967295
        +4294967296(a)                  4294967296
        +4294967296(b)                  4294967296
        9223372036854775807             9223372036854775807
        -9223372036854775808            -9223372036854775808
        -9223372036854775807            -9223372036854775807
        -9223372036854775806            -9223372036854775806
        -1(c)                           -1

        {expected_sqlda}
    """

    expected_stderr = """
        Statement failed, SQLSTATE = 42000
        Dynamic SQL Error
        -SQL error code = -104
        -Token unknown - line 1, column 9
        -'1'

        Statement failed, SQLSTATE = 42000
        Dynamic SQL Error
        -SQL error code = -104
        -Token unknown - line 1, column 9
        -'0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678x'
    """

    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stdout == act.clean_expected_stdout and
            act.clean_stderr == act.clean_expected_stderr)


