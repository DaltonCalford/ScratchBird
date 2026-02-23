#coding:utf-8

"""
ID:          issue-5616
ISSUE:       5616
TITLE:       User collate doesn't work with blobs
DESCRIPTION:
JIRA:        CORE-5341
FBTEST:      bugs.core_5341
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^;
    execute block as
    begin
      execute statement 'drop collation pxw_cyrl_ci_ai';
      when any do begin end
    end
    ^
    set term ;^
    commit;

    create collation pxw_cyrl_ci_ai for win1251 from pxw_cyrl case insensitive accent insensitive;
    commit;

    set list on;
    set count on;

    with A as(
      select cast('update' as blob sub_type text) as blob_id from rdb$database
      union all
      select 'UPDATE' from rdb$database
    )
    select * from a
    where blob_id collate PXW_CYRL_CI_AI like '%update%';
"""

act = isql_act('db', test_script, substitutions=[('BLOB_ID.*', '')])

expected_stdout = """
    update
    UPDATE
    Records affected: 2
"""

@pytest.mark.version('>=3.0.1')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(charset='WIN1251')
    assert act.clean_stdout == act.clean_expected_stdout

