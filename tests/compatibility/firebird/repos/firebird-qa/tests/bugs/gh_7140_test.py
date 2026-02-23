#coding:utf-8

"""
ID:          issue-7140
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7140
TITLE:       Wrong select result in case of special sort character
DESCRIPTION:
NOTES:
    [15.08.2023] pzotov
    Confirmed problem on 5.0.0.425.
    Checked on 5.0.0.426, 5.0.0.1163, 4.0.4.2978.
    Test fails on 3.0.12 with 'invalid collation attribute', thus min_version was set to 4.0.2.
"""

import pytest
from firebird.qa import *

db = db_factory(charset = 'utf8')

test_script = """
    create collation test_cz for UTF8 FROM UNICODE CASE INSENSITIVE ACCENT SENSITIVE 'LOCALE=cs_CZ';
    CREATE TABLE TBL_TEST (
        C1 VARCHAR(50) collate test_cz
    );
    CREATE INDEX IDX_c1 ON TBL_TEST (C1);
    CREATE DESCENDING INDEX IDX_c1_d ON TBL_TEST (C1);

    INSERT INTO TBL_TEST (C1) VALUES ('aaa');
    INSERT INTO TBL_TEST (C1) VALUES ('bbb');
    INSERT INTO TBL_TEST (C1) VALUES ('ccc');
    INSERT INTO TBL_TEST (C1) VALUES ('ddd');
    INSERT INTO TBL_TEST (C1) VALUES (NULL);
    set list on;
    select * from TBL_TEST where c1 < 'b' or c1 is null order by c1 desc;
    select * from TBL_TEST where c1 < 'c' or c1 is null order by c1 desc;
    select * from TBL_TEST where c1 < 'd' or c1 is null order by c1 desc;
"""

act = isql_act('db', test_script)

expected_stdout = """
    C1                              aaa
    C1                              <null>

    C1                              bbb
    C1                              aaa
    C1                              <null>

    C1                              ccc
    C1                              bbb
    C1                              aaa
    C1                              <null>
"""

@pytest.mark.intl
@pytest.mark.version('>=4.0.2')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
