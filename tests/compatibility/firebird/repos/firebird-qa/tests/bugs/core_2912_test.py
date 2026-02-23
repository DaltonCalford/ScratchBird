#coding:utf-8

"""
ID:          issue-3296
ISSUE:       3296
TITLE:       Exception when upper casing string with 'ÿ' (lowercase y trema, code 0xFF in ISO8859_1)
DESCRIPTION:
JIRA:        CORE-2912
FBTEST:      bugs.core_2912
NOTES:
    [31.10.2024] pzotov
    Bug was fixed for too old FB (2.1.6; 2.5.3; 3.0 Alpha 1), firebird-driver and/or QA-plugin
    will not able to run on this version in order to reproduce problem.
    Checked on 6.0.0.511 (Windows/Linux); 5.0.2.1551; 4.0.6.3165; 3.0.13.33794

    [27.06.2025] pzotov
    Removed 'set plan on' as irrelevant to this test.
"""
from pathlib import Path

import pytest
from firebird.qa import *

db = db_factory(charset='ISO8859_1')

act = isql_act('db', substitutions=[('[ \\t]+', ' ')])
tmp_sql = temp_file('tmp_core_2912.sql')

@pytest.mark.intl
@pytest.mark.version('>=3.0.0')
def test_1(act: Action, tmp_sql: Path):

    test_script = """
        create table test(c varchar(10));
        commit;
        insert into test(c) values('ÿ');
        insert into test(c) values('Faÿ');
        commit;
        create index test_cu on test computed by (upper (c collate iso8859_1));
        commit;
        set list on;
        select upper('aÿb') au from rdb$database;
        select c, upper(c) cu from test where c starting with upper('ÿ');
        select c, upper(c) cu from test where c containing 'Faÿ';
        select c, upper(c) cu from test where c starting with 'Faÿ';
        select c, upper(c) cu from test where c like 'Faÿ%';
        select c, upper(c) cu from test where c similar to '[[:alpha:]]{1,}ÿ%';
        select c from test where upper (c collate iso8859_1) =  upper('ÿ');
        select c, upper(c) cu from test where upper (c collate iso8859_1) starting with upper('Faÿ');
    """

    # ::: NB :::
    # For proper output of test, input script must be encoded in iso8859_1.
    #
    tmp_sql.write_text(test_script, encoding = 'iso8859_1')

    act.expected_stdout = """
        AU                              AÿB
        C                               ÿ
        CU                              ÿ
        C                               Faÿ
        CU                              FAÿ
        C                               Faÿ
        CU                              FAÿ
        C                               Faÿ
        CU                              FAÿ
        C                               Faÿ
        CU                              FAÿ
        C                               ÿ
        C                               Faÿ
        CU                              FAÿ
    """

    act.isql(switches = ['-q'], input_file = tmp_sql, charset = 'iso8859_1', combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

