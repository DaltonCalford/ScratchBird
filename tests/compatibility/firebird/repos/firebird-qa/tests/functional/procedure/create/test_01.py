#coding:utf-8

"""
ID:          procedure.create-01
TITLE:       CREATE PROCEDURE
DESCRIPTION: Create trivial SP and check SHOW PROCEDURE output for it.
FBTEST:      functional.procedure.create.01
NOTES:
    [11.07.2025] pzotov
    Removed 'show procedure' because its output can be frequently changed in master branch.
    Checked on 6.0.0.949; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

SP_BODY = """
    begin
      post_event 'test';
    end
"""

test_script = f"""
    set list on;
    set blob all;
    set term ^;
    create procedure sp_test as
    {SP_BODY}
    ^
    set term ;^
    commit;
    select
        p.rdb$procedure_source as blob_proc_source
       ,p.rdb$valid_blr
    from rdb$procedures p where p.rdb$procedure_name = upper('sp_test');
"""

substitutions = [('[ \t]+', ' '), ('BLOB_PROC_SOURCE .*', '')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = f"""
    BLOB_PROC_SOURCE                1a:4e0
    {SP_BODY}
    RDB$VALID_BLR                   1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
