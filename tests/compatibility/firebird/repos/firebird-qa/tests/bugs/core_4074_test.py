#coding:utf-8

"""
ID:          issue-4402
ISSUE:       4402
TITLE:       Computed by columns and position function
DESCRIPTION:
JIRA:        CORE-4074
FBTEST:      bugs.core_4074
NOTES:
    [28.06.2025] pzotov
    Replaced 'SHOW' command with query to RDB tables.
    See also test for https://github.com/FirebirdSQL/firebird/issues/4357
    Bug still can be seen in 2.5.9.27156 - rdb$computed_source has weird content:
    =======
    ( 'fabio ' || position('x','schunig') ),
      f02 numeric(8,2) default 0
    )
    =======
    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

COMPUTED_EXPR = "'fabio ' || position('x','schunig')"
test_script = f"""
    set list on;
    set blob all;
    recreate table test (
      f01 computed by ( {COMPUTED_EXPR} ),
      f02 numeric(8,2) default 0
    );
    commit;
    select
        rf.rdb$field_name fld_name
        ,f.rdb$field_type fld_type
        ,f.rdb$field_length fld_length
        ,f.rdb$field_scale fld_scale
        ,f.rdb$computed_source as rdb_blob_id
    from rdb$relation_fields rf
    left join rdb$fields f on rf.rdb$field_source = f.rdb$field_name
    where rf.rdb$relation_name = upper('TEST');
"""

substitutions = [ ('[ \t]+', ' '), ('RDB_BLOB_ID.*', '') ]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    FLD_NAME                        F01
    FLD_TYPE                        37
    FLD_LENGTH                      17
    FLD_SCALE                       0
    RDB_BLOB_ID                     2:1e4
    ( 'fabio ' || position('x','schunig') )

    FLD_NAME                        F02
    FLD_TYPE                        8
    FLD_LENGTH                      4
    FLD_SCALE                       -2
    RDB_BLOB_ID                     <null>
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

