#coding:utf-8

"""
ID:          issue-6490
ISSUE:       6490
TITLE:       Problem with too many number of columns in resultset
DESCRIPTION:
  We create .sql with 32767 columns and run it with requirement to display SQLDA.
  All lines in produced output with 'charset: ' substring must contain only one value:
  * '3' for FB 3.x; '4' for FB 4.x.
  If some charset ID differs from expected, we raise error and terminate check furter lines.

  Confirmed bug on 3.0.6.33272: first 32108 fields are shown in SQLDA with 'charset: 0 NONE'.
  String 'charset: 3 UNICODE_FSS' appeared only since 32109-th column and up to the end.
JIRA:        CORE-6246
FBTEST:      bugs.core_6246
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

MAX_FOR_PASS = 32767

test_script = f"""
    set sqlda_display on;
    set planonly;
    select
        {','.join(['x1.rdb$field_name' for i in range(MAX_FOR_PASS)])}
    from rdb$fields as x1 rows 1;
"""

@pytest.mark.version('>=3.0.6')
def test_1(act: Action):
    act.isql(switches=['-q'], input=test_script)
    # 1. For FB 3.x: only "charset: 3" must present in any string that describes column:
    #     NN: sqltype: 452 TEXT Nullable scale: 0 subtype: 0 len: 93 charset:   3 UNICODE_FSS
    #                                                                           ^
    #      0     1      2    3     4       5    6    7     8   9  10    11     12      13
    # 2. For FB 4.x: only "charset: 4" must present in any string that describes column:
    #     NN: sqltype: 452 TEXT Nullable scale: 0 subtype: 0 len: 252 charset:  4     UTF8
    #                                                                           ^
    #      0     1      2    3     4       5    6    7     8   9   10   11     12      13
    #                                                                           ^
    #                                                    we must check this token
    # where 'NN:' is '01:', '02:', '03:', ... '32767:'
    expected_charset_id = '3' if act.is_version('<4') else '4'
    charset_id_position = -1
    i = 0
    for line in act.stdout.splitlines():
        i += 1
        if 'sqltype:' in line:
            elements = line.split()
            if charset_id_position < 0:
                charset_id_position = [(n, w) for n, w in enumerate(elements) if w.lower() == 'charset:'][0][0] + 1
            # charset_id = elements[-2]
            charset_id = elements[charset_id_position]
            if charset_id != expected_charset_id:
                print(f'At least one UNEXPECTED charset in SQLDA at position: {charset_id_position}. Line No: {i}, charset_id: {charset_id}')
                print(line)
                pytest.fail("UNEXPECTED charset in SQLDA")
    if charset_id_position < 0:
        # ISQL log is empty or not contains 'sqltype:' in any line.
        pytest.fail('No lines with expected pattern found.')
