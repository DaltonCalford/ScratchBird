#coding:utf-8

"""
ID:          issue-2463
ISSUE:       2463
TITLE:       Problem with a read-only marked database
DESCRIPTION:
  Since FB 2.1 engine performs transliteraion of blobs between character sets.
  In this case system blob, stored in UNICODE_FSS, transliterated into connection charset.
  To do this, temporary blob is created. Engine didn't support temporary blobs creation in
  read-only databases since read-only databases was introduced
JIRA:        CORE-2026
FBTEST:      bugs.core_2026
"""

import pytest
from firebird.qa import *
from firebird.driver import DbAccessMode

init_script = """
    recreate table test(x integer default 0);
    commit;
"""

db = db_factory(charset='ISO8859_1', init=init_script)

test_script = """
set list on;
set blob all;
select mon$read_only from mon$database;
set count on;
select RDB$FIELD_NAME, rdb$default_source from rdb$relation_fields
where rdb$default_source is not null;
"""

act = isql_act('db', test_script, substitutions=[('RDB\\$DEFAULT_SOURCE.*', '')])

expected_stdout_a = """
MON$READ_ONLY                   0
RDB$FIELD_NAME                  X
default 0
Records affected: 1
"""

expected_stdout_b = """
MON$READ_ONLY                   1
RDB$FIELD_NAME                  X
default 0
Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_a
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
    #
    with act.connect_server() as srv:
        srv.database.set_access_mode(database=act.db.db_path, mode=DbAccessMode.READ_ONLY)
    #
    act.reset()
    act.expected_stdout = expected_stdout_b
    act.isql(switches=[], charset='iso8859_1', input=act.script)
    assert act.clean_stdout == act.clean_expected_stdout


