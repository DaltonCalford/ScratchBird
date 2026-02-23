#coding:utf-8

"""
ID:          issue-383
ISSUE:       383
TITLE:       WHERE CURRENT OF doesn't work
DESCRIPTION:
NOTES:
    [13.06.2025] pzotov
    1. Increased the 'subsitutions' list to suppress "PUBLIC" schema prefix and remove single/double quotes from object names. Need since 6.0.0.834.
       ::: NB :::
       File act.files_dir/'test_config.ini' must contain section:
           [schema_n_quotes_suppress]
           addi_subst="PUBLIC". " '
       (thi file is used in qa/plugin.py, see QA_GLOBALS dictionary).

       Value of parameter 'addi_subst' is splitted on tokens using space character and we add every token to 'substitutions' list which
       eventually will be like this:
           substitutions = [ ( <previous tuple(s)>, ('"PUBLIC".', ''), ('"', ''), ("'", '') ]

    2. Adjusted expected output: removed single quotes from DB object name(s).

    Checked on 6.0.0.835; 5.0.3.1661; 4.0.6.3207; 3.0.13.33807.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- NB: changed expected value of SQLSTATE to actual. See comment in git:
    -- "Prevent stack trace (line/column info) from overriding the real error's SQLSTATE", 30-apr-2016
    -- https://github.com/FirebirdSQL/firebird/commit/d1d8b36a07d4f11d98d2c8ec16fb8ec073da442b // FB 4.0
    -- https://github.com/FirebirdSQL/firebird/commit/849bfac745bc9158e9ef7990f5d52913f8b72f02 // FB 3.0
    -- https://github.com/FirebirdSQL/firebird/commit/b9d4142c4ed1fdf9b7c633edc7b2425f7b93eed0 // FB 2.5
    -- See also letter from dimitr, 03-may-2016 19:24.

    recreate table test (a integer not null, constraint test_pk primary key (a));
    insert into test (a) values (1);
    insert into test (a) values (2);
    insert into test (a) values (3);
    insert into test (a) values (4);
    commit;

    set term ^;
    create procedure test_upd(d integer) as
        declare c cursor for (
            select a from test
        );
    begin
        open c;
        update test set a = a + :d
        where current of c;
        close c;
    end
    ^
    set term ;^
    commit;

    execute procedure test_upd (2);
"""

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

substitutions = [ ('line: \\d+, col: \\d+', '') ]
for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = isql_act('db', test_script, substitutions=substitutions)

expected_stderr = """
    Statement failed, SQLSTATE = 22000
    no current record for fetch operation
    -At procedure TEST_UPD
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr

