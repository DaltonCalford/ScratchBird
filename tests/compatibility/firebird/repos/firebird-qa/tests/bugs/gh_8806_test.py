#coding:utf-8

"""
ID:          n/a
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8806
TITLE:       Missing privilege checks for the COMMENT ON PARAMETER command on functions in packages
NOTES:
   [05.01.2026] pzotov
   Confirmed bug on 6.0.0.1377
   Checked on 6.0.0.1387.
"""

import pytest
from firebird.qa import *

db = db_factory()
tmp_user = user_factory('db', name='tmp$8806_junior', password='456')

act = isql_act('db')

@pytest.mark.version('>=6.0')
def test_1(act: Action, tmp_user: User):

    PACKAGE_NAME = 'pg_test'
    PG_FUNC_NAME = 'pg_fn_worker'
    PG_PROC_NAME = 'pg_sp_worker'

    test_script = f"""
        set bail on;
        set term ^;
        create or alter package {PACKAGE_NAME} as
        begin
            function {PG_FUNC_NAME}(arg1 int) returns int;
            procedure {PG_PROC_NAME}(arg1 int);
        end
        ^
        recreate package body {PACKAGE_NAME} as
        begin
            function {PG_FUNC_NAME}(arg1 int) returns int as
            begin
                return arg1 * arg1;
            end

            procedure {PG_PROC_NAME}(arg1 int) as
                declare c int;
            begin
                c = arg1 * 2;
            end

        end
        ^
        set term ;^
        commit;
        connect '{act.db.db_path}' user '{tmp_user.name}' password '{tmp_user.password}';
        set bail off;
        comment on parameter {PACKAGE_NAME}.{PG_FUNC_NAME}.arg1 is 'unauthorized comment for packaged function';
        comment on parameter {PACKAGE_NAME}.{PG_PROC_NAME}.arg1 is 'unauthorized comment for packaged procedure';
    """

    expected_stdout = f"""
        Statement failed, SQLSTATE = 28000
        unsuccessful metadata update
        -COMMENT ON "PUBLIC"."{PACKAGE_NAME.upper()}"."{PG_FUNC_NAME.upper()}".ARG1 failed
        -no permission for ALTER access to PACKAGE "PUBLIC"."{PACKAGE_NAME.upper()}"
        -Effective user is {tmp_user.name.upper()}

        Statement failed, SQLSTATE = 28000
        unsuccessful metadata update
        -COMMENT ON "PUBLIC"."{PACKAGE_NAME.upper()}"."{PG_PROC_NAME.upper()}".ARG1 failed
        -no permission for ALTER access to PACKAGE "PUBLIC"."{PACKAGE_NAME.upper()}"
        -Effective user is {tmp_user.name.upper()}
    """

    act.expected_stdout = expected_stdout
    act.isql(switches=['-q'], input = test_script, credentials = True, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
