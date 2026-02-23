#coding:utf-8

"""
ID:          issue-3723
ISSUE:       3723
TITLE:       Generators are set to 0 after restore
DESCRIPTION:
    FB 4.x has incompatible behaviour with all previous versions since build 4.0.0.2131 (06-aug-2020):
    statement 'alter sequence <seq_name> restart with 0' changes rdb$generators.rdb$initial_value to -1 thus
    next call of gen_id(<seq_name>,1) will return 0 (ZERO!) rather than 1.
    See also CORE-6084 and related commit:
    https://github.com/FirebirdSQL/firebird/commit/23dc0c6297825b2e9006f4d5a2c488702091033d
    This is considered as *expected* and is noted in doc/README.incompatibilities.3to4.txt
JIRA:        CORE-3357
FBTEST:      bugs.core_3357
NOTES:
    [27.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from io import BytesIO
from firebird.qa import *
from firebird.driver import SrvRestoreFlag

init_script = """
    recreate sequence g1 start with 9223372036854775807 increment by -2147483647;
    recreate sequence g2 start with -9223372036854775808 increment by 2147483647;
    commit;
"""

db = db_factory(init=init_script)

act = python_act('db')

expected_out_3x = """
    Generator G1, current value: 9223372036854775807, initial value: 9223372036854775807, increment: -2147483647
    Generator G2, current value: -9223372036854775808, initial value: -9223372036854775808, increment: 2147483647
"""

expected_out_5x = """
    Generator G1, current value: -9223372034707292162, initial value: 9223372036854775807, increment: -2147483647
    Generator G2, current value: 9223372034707292161, initial value: -9223372036854775808, increment: 2147483647
"""

expected_out_6x = """
    Generator PUBLIC.G1, current value: -9223372034707292162, initial value: 9223372036854775807, increment: -2147483647
    Generator PUBLIC.G2, current value: 9223372034707292161, initial value: -9223372036854775808, increment: 2147483647
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    with act.connect_server() as srv:
        backup = BytesIO()
        srv.database.local_backup(database=act.db.db_path, backup_stream=backup)
        backup.seek(0)
        srv.database.local_restore(backup_stream=backup, database=act.db.db_path,
                                   flags=SrvRestoreFlag.REPLACE)

    act.expected_stdout = expected_out_3x if act.is_version('<4') else expected_out_5x if act.is_version('<6') else expected_out_6x
    act.isql(switches = ['-q'], input="show sequ g1; show sequ g2;", combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
