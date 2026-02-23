#coding:utf-8

"""
ID:          issue-5498
ISSUE:       5498
TITLE:       Explicitly defined names for NOT NULL constraints are not exported into script by ISQL -x
DESCRIPTION:
JIRA:        CORE-5218
FBTEST:      bugs.core_5218
NOTES:
    [01.07.2025] pzotov
    Added 'SQL_SCHEMA_PREFIX' to be substituted in expected_* on FB 6.x
    Checked on 6.0.0.884; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table test(
       f01 int constraint f01_nn not null constraint f01_pk primary key
      ,f02 int constraint f02_nn not null constraint f02_uk unique
      -- NB: 3.0 allows to skip reference of PK column from table that
      --- is created now, i.e. one may to declare FK-field like this:
      -- ... f03 references test
      -- That's not so for 2.5.x:
      ,f03 int constraint f03_nn not null
       constraint f03_fk
       references test( f01 )
       --                ^-- this must be specified in 2.5.x
    );
"""

db = db_factory(init=init_script)

act = python_act('db')


@pytest.mark.version('>=2.5.6')
def test_1(act: Action):
    act.isql(switches=['-x'])
    assert act.return_code == 0, f'Attempt to extract metadata failed:\n{act.clean_stdout}'
    act.stdout = '\n'.join([line for line in act.stdout.splitlines() if 'CONSTRAINT' in line])

    # -----------------------------------
    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else  'PUBLIC.'
    expected_stdout = f"""
        CREATE TABLE {SQL_SCHEMA_PREFIX}TEST (F01 INTEGER CONSTRAINT F01_NN NOT NULL,
                F02 INTEGER CONSTRAINT F02_NN NOT NULL,
                F03 INTEGER CONSTRAINT F03_NN NOT NULL,
        CONSTRAINT F01_PK PRIMARY KEY (F01),
        CONSTRAINT F02_UK UNIQUE (F02));
        ALTER TABLE {SQL_SCHEMA_PREFIX}TEST ADD CONSTRAINT F03_FK FOREIGN KEY (F03) REFERENCES {SQL_SCHEMA_PREFIX}TEST (F01);
    """

    act.expected_stdout = expected_stdout
    # filter stdout
    assert act.clean_stdout == act.clean_expected_stdout


