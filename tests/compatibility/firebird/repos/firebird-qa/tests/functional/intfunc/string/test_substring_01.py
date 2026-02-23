#coding:utf-8

"""
ID:          intfunc.string.left
TITLE:       Positional SUBSTRING function
DESCRIPTION: https://www.firebirdsql.org/file/documentation/html/en/refdocs/fblangref50/firebird-50-language-reference.html#fblangref50-scalarfuncs-substring-pos
NOTES:
    [20.08.2025] pzotov
    NB: 3.x raises "SQLSTATE = 22011 / Invalid offset parameter ... to SUBSTRING. Only positive integers are allowed."
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set blob all;
    create table test(v varchar(6), b blob);
    insert into test(v,b) values('abcdef','abcdef');

    select substring(v from 1 for 2) as subs_vchr_01, substring(b from 1 for 2) as subs_blob_01 from test;
    -- result: 'ab'

    select substring(v from 2) as subs_vchr_02, substring(b from 2) as subs_blob_01 from test;
    -- result: 'bcdef'

    select substring(v from 0 for 2) as subs_vchr_03, substring(b from 0 for 2) as subs_blob_03 from test;
    -- result: 'a'
    -- and NOT 'ab', because there is "nothing" at position 0

    select '>' || substring(v from -5 for 2) || '<' as subs_vchr_04, '>' || substring(b from -5 for 2) || '<' as subs_blob_04 from test;
    -- result: ''
    -- length ends before the actual start of the string

    select '>' || substring(v from 6 for 2) || '<' as subs_vchr_05, '>' || substring(b from 6 for 2) || '<' as subs_blob_05 from test;

    select '>' || substring(v from 7 for 2) || '<' as subs_vchr_06, '>' || substring(b from 7 for 2) || '<' as subs_blob_06 from test;

    select '>' || substring(v from -2147483648 for 2) || '<' as subs_vchr_07, '>' || substring(b from -2147483648 for 2) || '<' as subs_blob_07 from test;

    select '>' || substring(v from 2147483648 for 2147483647) || '<' as subs_vchr_08, '>' || substring(b from 2147483648 for 2147483647) || '<' as subs_blob_08 from test;

    select '>' || substring(v from 2147483649 for 2) || '<' as subs_vchr_09, '>' || substring(b from 2147483649 for 2) || '<' as subs_blob_09 from test;

    select '>' || substring(v from -2147483648 for 2147483647) || '<' as subs_vchr_10, '>' || substring(b from -2147483648 for 2147483647) || '<' as subs_blob_10 from test;
"""

substitutions = [('[ \t]+', ' '), ('SUBS_BLOB_\\d+ .*', '')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    SUBS_VCHR_01 ab
    ab
    SUBS_VCHR_02 bcdef
    bcdef
    SUBS_VCHR_03 a
    a
    SUBS_VCHR_04 ><
    ><
    SUBS_VCHR_05 >f<
    >f<
    SUBS_VCHR_06 ><
    ><
    Statement failed, SQLSTATE = 22003
    arithmetic exception, numeric overflow, or string truncation
    -numeric value is out of range
    SUBS_VCHR_08 ><
    ><
    Statement failed, SQLSTATE = 22003
    arithmetic exception, numeric overflow, or string truncation
    -numeric value is out of range
    Statement failed, SQLSTATE = 22003
    arithmetic exception, numeric overflow, or string truncation
    -numeric value is out of range
"""

@pytest.mark.version('>=4')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
