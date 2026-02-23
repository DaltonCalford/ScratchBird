#coding:utf-8

"""
ID:          table.alter-04
TITLE:       ALTER TABLE - ADD CONSTRAINT - UNIQUE
DESCRIPTION:
FBTEST:      functional.table.alter.04
NOTES:
    [12.07.2025] pzotov
    Removed 'SHOW' command.
    Statement 'ALTER TABLE...' followed by commit must allow further actions related to changed DDL.
    Added 'SQL_SCHEMA_PREFIX' and variable to be substituted in expected_* on FB 6.x
    Checked on 6.0.0.949; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table test( id int);
    insert into test(id) values(null);
    insert into test(id) values(-2);
    insert into test(id) values(-2);
    insert into test(id) values(null);
    commit;
    alter table test add constraint test_unq unique(id); -- must fail because duplicates exist
    commit;
    delete from test where id is not null;
    commit;
    alter table test add constraint test_unq unique(id); -- must pass
    commit;
    insert into test(id) values(1);
    insert into test(id) values(1); -- must fail
    select id, count(*) from test group by id order by id; -- must issue null:2, 1:1
"""

substitutions = [('[ \t]+', ' '), ('(-)?Problematic key value is.*', 'Problematic key value is')]
act = isql_act('db', test_script, substitutions = substitutions)

@pytest.mark.version('>=3')
def test_1(act: Action):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    TEST_TABLE_NAME = '"TEST"' if act.is_version('<6') else f'{SQL_SCHEMA_PREFIX}"TEST"'
    expected_stdout = f"""
        Statement failed, SQLSTATE = 23000
        violation of PRIMARY or UNIQUE KEY constraint "TEST_UNQ" on table {TEST_TABLE_NAME}
        -Problematic key value is ("ID" = -2)

        Statement failed, SQLSTATE = 23000
        violation of PRIMARY or UNIQUE KEY constraint "TEST_UNQ" on table {TEST_TABLE_NAME}
        -Problematic key value is ("ID" = 1)

        ID                              <null>
        COUNT                           2
        ID                              1
        COUNT                           1
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
