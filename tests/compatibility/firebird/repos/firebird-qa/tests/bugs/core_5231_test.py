#coding:utf-8

"""
ID:          issue-5510
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/5510
TITLE:       EXECUTE STATEMENT: BLR error if more than 256 output parameters exist
DESCRIPTION:
    We define here number of output args for which one need to made test - see var 'sp_args_count'.
    Then we open .sql file and GENERATE it content based on value of 'sp_args_count' (procedure will
    have header and body with appropriate number of arguments and statement to be executed).
    Finally, we run ISQL subprocess with giving to it for execution just generated .sql script.
    ISQL should _not_ issue any error and all lines of its STDOUT should start from the names of
    output arguments (letter 'O': O1, O2, ... O5000).
JIRA:        CORE-5231
FBTEST:      bugs.core_5231
"""

import pytest
from pathlib import Path
from firebird.qa import *

db = db_factory()

act = python_act('db', substitutions=[('^O.*', '')])

SP_ARGS_COUNT = 5000

ddl_script = temp_file('core_5231.sql')

def build_script(ddl_script: Path):
    with open(ddl_script, 'w') as ddl_file:
        ddl_file.write("""
        set term ^;
        execute block as
        begin
            execute statement 'drop procedure sp_test';
        when any do begin end
        end ^
        commit ^
        create or alter procedure sp_test returns (
        """)
        delimiter = ''
        for i in range(SP_ARGS_COUNT):
            ddl_file.write(f'{delimiter}o{i} int')
            delimiter = ','
        ddl_file.write(
        """) as begin
        for execute statement 'select
        """)

        delimiter = ''
        for i in range(SP_ARGS_COUNT):
            ddl_file.write(f'{delimiter}{i}')
            delimiter = ','
        ddl_file.write(" from rdb$database'\ninto ")

        delimiter = ''
        for i in range(SP_ARGS_COUNT):
            ddl_file.write(f'{delimiter}o{i}')
            delimiter = ','

        ddl_file.write("""
        do suspend;
        end^
        set term ;^
        commit;
        set list on;
        select * from sp_test;
        """)

@pytest.mark.version('>=3.0')
def test_1(act: Action, ddl_script: Path):
    build_script(ddl_script)
    act.isql(switches=[], input_file=ddl_script, charset='NONE', combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
