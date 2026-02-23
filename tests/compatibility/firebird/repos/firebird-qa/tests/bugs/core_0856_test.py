#coding:utf-8

"""
ID:          issue-1246
ISSUE:       1246
TITLE:       Unable to set FName, MName, LName fields in security to blank
DESCRIPTION:
JIRA:        CORE-856
FBTEST:      bugs.core_0856
NOTES:
    [25.06.2025] pzotov
    Minimal snapshot number for 6.x: 6.0.0.863, see letter to Adriano, 24.06.2025 16:01. Fixed in commit:
    https://github.com/FirebirdSQL/firebird/commit/b3da90583735da2b01c8c8129240cfffced6c1dc

    Checked on 6.0.0.863; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create or alter user tmp$c0856 password '123'
      firstname  '....:....1....:....2....:....3..'
      middlename '....:....1....:....2....:....3..'
      lastname   '....:....1....:....2....:....3..'
    ;
    commit;
    set list on;

    select sec$user_name, sec$first_name, sec$middle_name, sec$last_name
    from sec$users where upper(sec$user_name) = upper('tmp$c0856');

    alter user tmp$c0856
    firstname ''
    middlename _ascii x'09'
    lastname _ascii x'0A'
    ;

    commit;
    select
        sec$user_name,
        octet_length(sec$first_name),
        octet_length(sec$middle_name),
        octet_length(sec$last_name),
        ascii_val(left(sec$first_name,1)),
        ascii_val(left(sec$middle_name,1)),
        ascii_val(left(sec$last_name,1))
    from sec$users where upper(sec$user_name)=upper('tmp$c0856');
    commit;

    drop user tmp$c0856;
    commit;
"""

act = isql_act('db', test_script)

expected_stdout = """
    SEC$USER_NAME                   TMP$C0856
    SEC$FIRST_NAME                  ....:....1....:....2....:....3..
    SEC$MIDDLE_NAME                 ....:....1....:....2....:....3..
    SEC$LAST_NAME                   ....:....1....:....2....:....3..

    SEC$USER_NAME                   TMP$C0856
    OCTET_LENGTH                    <null>
    OCTET_LENGTH                    1
    OCTET_LENGTH                    1
    ASCII_VAL                       <null>
    ASCII_VAL                       9
    ASCII_VAL                       10
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

