#coding:utf-8

"""
ID:          issue-536
ISSUE:       536
TITLE:       CHECK constraints fire twice
DESCRIPTION:
JIRA:        CORE-209
FBTEST:      bugs.core_0209
NOTES:
    [22.06.2025] pzotov
    ::: NB :::
    SQL schema name (introduced since 6.0.0.834), single and double quotes are suppressed in the output.
    See $QA_HOME/README.substitutions.md or https://github.com/FirebirdSQL/firebird-qa/blob/master/README.substitutions.md

    Checked on 6.0.0.853; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create domain dm_restricted_char as char(1) check (value in ('A','B','C', 'D','E'));

    recreate table test (
        id integer,
        col dm_restricted_char
    );

    recreate view v_test as select * from test;

    set term ^;
    create trigger tab_biu for test before insert or update as
    begin
        new.col = upper (new.col);
    end^

    create trigger v_test_biu for v_test before insert or update as
    begin
        -- ::: NB :::
        -- Since 2.0 trigger that belongs to updatable view MUST have DML
        -- statement that handles underlying TABLE (this was not so in 1.5).
        if ( inserting ) then
            insert into test values( new.id, new.col );
        else
            update test set col = new.col, id = new.id
            where id = old.id;
    end^

    set term ;^
    commit;

    set count on;
    set list on;
    SET ECHO ON;

    insert into v_test values (11, 'a');
    insert into v_test values (12, 'b');
    insert into v_test values (13, 'c');
    insert into v_test values (14, 'd');

    select * from test;
    commit;

    update v_test set col='e' where id=11;
    update v_test set col='e' where id=14;

    update test   set col='z' where id=12;
    update v_test set col='x' where id=13;

    select * from test;
"""

substitutions=[("-At trigger 'V_TEST_BIU' line.*", "-At trigger 'V_TEST_BIU' line")]

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

substitutions = [ ('line: \\d+, col: \\d+', '') ]
for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = isql_act('db', test_script, substitutions = substitutions)


expected_stdout = """
    insert into v_test values (11, 'a');
    Records affected: 1
    insert into v_test values (12, 'b');
    Records affected: 1
    insert into v_test values (13, 'c');
    Records affected: 1
    insert into v_test values (14, 'd');
    Records affected: 1

    select * from test;
    ID                              11
    COL                             A
    ID                              12
    COL                             B
    ID                              13
    COL                             C
    ID                              14
    COL                             D
    Records affected: 4
    commit;

    update v_test set col='e' where id=11;
    Records affected: 1
    update v_test set col='e' where id=14;
    Records affected: 1

    update test   set col='z' where id=12;
    Records affected: 0
    update v_test set col='x' where id=13;
    Records affected: 0

    select * from test;

    ID                              11
    COL                             E
    ID                              12
    COL                             B
    ID                              13
    COL                             C
    ID                              14
    COL                             E
    Records affected: 4
"""
expected_stderr = """
    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."COL", value "Z"

    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."COL", value "X"
    -At trigger 'V_TEST_BIU' line: 8, col: 5
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)

