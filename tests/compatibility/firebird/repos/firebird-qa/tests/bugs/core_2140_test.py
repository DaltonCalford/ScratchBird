#coding:utf-8

"""
ID:          issue-2571
ISSUE:       2571
TITLE:       Error messages after parameters substitution contains '\\n' characters instead of line break
DESCRIPTION:
JIRA:        CORE-2140
FBTEST:      bugs.core_2140
NOTES:
    [26.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

substitutions = [
    ('Data source : Firebird::.*', 'Data source : Firebird::'),
    ('col(umn)?(:)?.*', 'col x'),
    ('-At block line: [\\d]+, col: [\\d]+', '')
]

act = isql_act('db', substitutions = substitutions)

@pytest.mark.es_eds
@pytest.mark.version('>=3')
def test_1(act: Action):

    # Statement that will be passed in ES/EDS (we have to check its presence in error message):
    EDS_STATEMENT = 'select rdb$relation_id from rdb$database where rdb$relation_id = :x'

    test_script = f"""
        set list on;
        set term ^ ;
        execute block returns (y int) as
        begin
          for
              execute statement  ('{EDS_STATEMENT}') (1)
              on external 'localhost:' || rdb$get_context('SYSTEM', 'DB_NAME')
              as user '{act.db.user}' password '{act.db.password}'
              with autonomous transaction
          into y
              do suspend;
        end
        ^
    """
    
    # BEFOREE fix output was:
    # Statement : select rdb$relation_id from rdb$database where rdb$relation_id = :x\nData source : Internal::.

    expected_stdout_5x = f"""
        Statement failed, SQLSTATE = 42000
        Execute statement error at isc_dsql_prepare :
        335544569 : Dynamic SQL Error
        335544436 : SQL error code = -206
        335544578 : Column unknown
        335544382 : X
        336397208 : At line 1, column 66
        Statement : {EDS_STATEMENT}
        Data source : Firebird::
        -At block line: 3, col: 11
    """

    expected_stdout_6x = f"""
        Statement failed, SQLSTATE = 42000
        Execute statement error at isc_dsql_prepare :
        335544569 : Dynamic SQL Error
        335544436 : SQL error code = -206
        335544578 : Column unknown
        335544382 : "X"
        336397208 : At line 1, column 66
        Statement : {EDS_STATEMENT}
        Data source : Firebird::
        -At block line: 3, col: 11
    """
    
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.isql(switches = ['-q'], input = test_script, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
