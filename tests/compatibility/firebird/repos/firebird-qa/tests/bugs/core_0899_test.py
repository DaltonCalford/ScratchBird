#coding:utf-8

"""
ID:          issue-1296
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/1296
TITLE:       Problems with explicit cursors in unwanted states
DESCRIPTION:
JIRA:        CORE-899
FBTEST:      bugs.core_0899
NOTES:
    [23.06.2025] pzotov
    ::: NB :::
    SQL schema name (introduced since 6.0.0.834), single and double quotes are suppressed in the output.
    See $QA_HOME/README.substitutions.md or https://github.com/FirebirdSQL/firebird-qa/blob/master/README.substitutions.md

    Checked on 6.0.0.853; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table T (ID integer, TXT varchar(30));
    commit;

    insert into T values (1,'Text description');
    commit;

    set term ^;

    create procedure SP_OK returns (ID integer, TXT varchar(30))
    as
      declare C cursor for ( select ID, TXT from T );
    begin
      open C;
      while (1 = 1) do
      begin
        fetch C into :ID, :TXT;
        if (ROW_COUNT = 0) then
          leave;
        update T set TXT = 'OK' where current of C;
        suspend;
      end
      close C;
    end ^

    create procedure SP_CLOSED returns (ID integer, TXT varchar(30))
    as
      declare C cursor for ( select ID, TXT from T );
    begin
      open C;
      while (1 = 1) do
      begin
        fetch C into :ID, :TXT;
        if (ROW_COUNT = 0) then
          leave;
        suspend;
      end
      close C;
      update T set TXT = 'SP_CLOSED' where current of C;
    end ^

    create procedure SP_NOTOPEN returns (ID integer, TXT varchar(30))
    as
      declare C cursor for ( select ID, TXT from T );
    begin
      update T set TXT = 'SP_NOTOPEN' where current of C;
      open C;
      while (1 = 1) do
      begin
        fetch C into :ID, :TXT;
        if (ROW_COUNT = 0) then
          leave;
        suspend;
      end
      close C;
    end ^

    create procedure SP_FETCHED returns (ID integer, TXT varchar(30))
    as
      declare C cursor for ( select ID, TXT from T );
    begin
      open C;
      while (1 = 1) do
      begin
        fetch C into :ID, :TXT;
        if (ROW_COUNT = 0) then
          leave;
        suspend;
      end
      update T set TXT = 'SP_FETCHED' where current of C;
      close C;
    end ^

    set term ; ^

    commit;

    set list on;
    select * from SP_OK;
    select * from SP_CLOSED;
    select * from SP_NOTOPEN;
    select * from SP_FETCHED;
"""

substitutions=[('[ \t]+', ' '), ('line:\\s+\\d+,', 'line: x'), ('col:\\s+\\d+', 'col: y')]

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    ID 1
    TXT Text description

    ID 1
    TXT OK

    Statement failed, SQLSTATE = 22000
    no current record for fetch operation
    -At procedure SP_CLOSED line: x col: y

    Statement failed, SQLSTATE = 22000
    no current record for fetch operation
    -At procedure SP_NOTOPEN line: x col: y

    ID 1
    TXT OK

    Statement failed, SQLSTATE = 22000
    no current record for fetch operation
    -At procedure SP_FETCHED line: x col: y
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
