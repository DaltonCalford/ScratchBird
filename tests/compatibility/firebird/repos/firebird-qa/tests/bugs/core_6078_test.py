#coding:utf-8

"""
ID:          issue-6328
ISSUE:       6328
TITLE:       Permissions for create or alter statements are not checked
DESCRIPTION:
  The problem occured for all kind of DB objects which allow 'create OR ALTER' statement to be applied against themselves.
  Test creates non-privileged user and checks for all such objects that this user can NOT create any object because missing
  privilege to do this.
JIRA:        CORE-6078
FBTEST:      bugs.core_6078
NOTES:
    [02.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.889; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813
"""

import pytest
from pathlib import Path
from firebird.qa import *

db = db_factory()

user_0 = user_factory('db', name='tmp$c6078_0', password='123')
user_1 = user_factory('db', name='tmp$c6078_1', password='123')
user_2 = user_factory('db', name='tmp$c6078_2', password='456')

act = python_act('db', substitutions=[('.*After line \\d+.*', ''),
                                      ('ALTERED_TRIGGER_SOURCE.*', 'ALTERED_TRIGGER_SOURCE')])

# version: 3.0

expected_stdout_1 = """
    Statement failed, SQLSTATE = 28000
    modify record error
    -no permission for UPDATE access to COLUMN PLG$SRP_VIEW.PLG$ACTIVE

    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -ALTER DATABASE failed
    -no permission for ALTER access to DATABASE

    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -ALTER DOMAIN DM_TEST failed
    -no permission for ALTER access to DOMAIN DM_TEST

    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -ALTER TABLE TEST failed
    -no permission for ALTER access to TABLE TEST

    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -ALTER TABLE TEST failed
    -no permission for ALTER access to TABLE TEST

    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -ALTER INDEX TEST_UID failed
    -no permission for ALTER access to TABLE TEST

    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -COMMENT ON TEST failed
    -no permission for ALTER access to TABLE TEST

    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -CREATE OR ALTER TRIGGER TEST_BI failed
    -no permission for ALTER access to TABLE TEST

    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -CREATE OR ALTER TRIGGER TRG$START failed
    -no permission for ALTER access to DATABASE

    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -CREATE OR ALTER TRIGGER TRIG_DDL_SP failed
    -no permission for ALTER access to DATABASE


    ALTERED_TRIGGER_NAME            TEST_BI
    ALTERED_TRIGGER_SOURCE          c:3d0
    as
        begin
           new.uid = gen_uuid();
        end


    Records affected: 1
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -CREATE OR ALTER PACKAGE PKG_TEST failed
    -There is no privilege for this operation

    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -RECREATE PACKAGE BODY PKG_TEST failed
    -There is no privilege for this operation


    ALTERED_PKG_NAME                <null>


    Records affected: 1
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -CREATE OR ALTER FUNCTION FN_C6078 failed
    -There is no privilege for this operation


    ALTERED_STANDALONE_FUNC         <null>


    Records affected: 1
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -CREATE OR ALTER PROCEDURE SP_C6078 failed
    -There is no privilege for this operation


    ALTERED_STANDALONE_PROC         <null>


    Records affected: 1
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -CREATE OR ALTER VIEW V_C6078 failed
    -There is no privilege for this operation


    ALTERED_VIEW_NAME               <null>


    Records affected: 1
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -ALTER SEQUENCE SQ_C6078 failed
    -There is no privilege for this operation


    ALTERED_SEQUENCE_NAME           <null>


    Records affected: 1
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -CREATE OR ALTER EXCEPTION EX_C6078 failed
    -There is no privilege for this operation


    ALTERED_EXCEPTION_NAME          <null>


    Records affected: 1
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -CREATE OR ALTER FUNCTION WAIT_EVENT failed
    -There is no privilege for this operation


    ALTERED_UDR_BASED_FUNC          <null>


    Records affected: 1
    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -ALTER CHARACTER SET UTF8 failed
    -no permission for ALTER access to CHARACTER SET UTF8

    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -ALTER MAPPING LOCAL_MAP_C6078 failed
    -Unable to perform operation.  You must be either SYSDBA or owner of the database

    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -ALTER MAPPING GLOBAL_MAP_C6078 failed
    -Unable to perform operation.  You must be either SYSDBA or owner of the database
"""

@pytest.mark.version('>=3.0.5,<4')
def test_1(act: Action, user_0: User, user_1: User, user_2: User):
    script_vars = {'dsn': act.db.dsn,
                   'user_name': act.db.user,
                   'user_password': act.db.password,}
    script_file = act.files_dir / 'core_6078.sql'
    script = script_file.read_text() % script_vars
    act.expected_stdout = expected_stdout_1
    act.isql(switches=['-q'], input=script, combine_output=True)
    assert act.clean_stdout == act.clean_expected_stdout

############################################################################

# version: 4.0+

expected_stdout_5x = """
    Statement failed, SQLSTATE = 28000
    modify record error
    -no permission for UPDATE access to COLUMN PLG$SRP_VIEW.PLG$ACTIVE
    -Effective user is TMP$C6078_0

    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -ALTER DATABASE failed
    -no permission for ALTER access to DATABASE

    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -ALTER DOMAIN DM_TEST failed
    -no permission for ALTER access to DOMAIN DM_TEST
    -Effective user is TMP$C6078_0

    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -ALTER TABLE TEST failed
    -no permission for ALTER access to TABLE TEST
    -Effective user is TMP$C6078_0

    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -ALTER TABLE TEST failed
    -no permission for ALTER access to TABLE TEST
    -Effective user is TMP$C6078_0

    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -ALTER INDEX TEST_UID failed
    -no permission for ALTER access to TABLE TEST
    -Effective user is TMP$C6078_0

    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -COMMENT ON TEST failed
    -no permission for ALTER access to TABLE TEST
    -Effective user is TMP$C6078_0

    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -CREATE OR ALTER TRIGGER TEST_BI failed
    -no permission for ALTER access to TABLE TEST
    -Effective user is TMP$C6078_0

    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -CREATE OR ALTER TRIGGER TRG$START failed
    -no permission for ALTER access to DATABASE

    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -CREATE OR ALTER TRIGGER TRIG_DDL_SP failed
    -no permission for ALTER access to DATABASE


    ALTERED_TRIGGER_NAME            TEST_BI
    ALTERED_TRIGGER_SOURCE          c:3cc
    as
        begin
           new.uid = gen_uuid();
        end


    Records affected: 1
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -CREATE OR ALTER PACKAGE PKG_TEST failed
    -No permission for CREATE PACKAGE operation

    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -RECREATE PACKAGE BODY PKG_TEST failed
    -No permission for CREATE PACKAGE operation


    ALTERED_PKG_NAME                <null>


    Records affected: 1
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -CREATE OR ALTER FUNCTION FN_C6078 failed
    -No permission for CREATE FUNCTION operation

    ALTERED_STANDALONE_FUNC         <null>


    Records affected: 1
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -CREATE OR ALTER PROCEDURE SP_C6078 failed
    -No permission for CREATE PROCEDURE operation

    ALTERED_STANDALONE_PROC         <null>


    Records affected: 1
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -CREATE OR ALTER VIEW V_C6078 failed
    -No permission for CREATE VIEW operation

    ALTERED_VIEW_NAME               <null>


    Records affected: 1
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -ALTER SEQUENCE SQ_C6078 failed
    -No permission for CREATE GENERATOR operation

    ALTERED_SEQUENCE_NAME           <null>


    Records affected: 1
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -CREATE OR ALTER EXCEPTION EX_C6078 failed
    -No permission for CREATE EXCEPTION operation

    ALTERED_EXCEPTION_NAME          <null>


    Records affected: 1
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -CREATE OR ALTER FUNCTION WAIT_EVENT failed
    -No permission for CREATE FUNCTION operation

    ALTERED_UDR_BASED_FUNC          <null>


    Records affected: 1
    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -ALTER CHARACTER SET UTF8 failed
    -no permission for ALTER access to CHARACTER SET UTF8
    -Effective user is TMP$C6078_0

    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -ALTER MAPPING LOCAL_MAP_C6078 failed
    -Unable to perform operation
    -System privilege CHANGE_MAPPING_RULES is missing

    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -ALTER MAPPING GLOBAL_MAP_C6078 failed
    -Unable to perform operation
    -System privilege CHANGE_MAPPING_RULES is missing
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 28000
    modify record error
    -no permission for UPDATE access to COLUMN "PLG$SRP"."PLG$SRP_VIEW"."PLG$ACTIVE"
    -Effective user is TMP$C6078_0
    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -ALTER DATABASE failed
    -no permission for ALTER access to DATABASE
    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -ALTER DOMAIN "PUBLIC"."DM_TEST" failed
    -no permission for ALTER access to DOMAIN "PUBLIC"."DM_TEST"
    -Effective user is TMP$C6078_0
    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -ALTER TABLE "PUBLIC"."TEST" failed
    -no permission for ALTER access to TABLE "PUBLIC"."TEST"
    -Effective user is TMP$C6078_0
    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -ALTER TABLE "PUBLIC"."TEST" failed
    -no permission for ALTER access to TABLE "PUBLIC"."TEST"
    -Effective user is TMP$C6078_0
    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -ALTER INDEX "PUBLIC"."TEST_UID" failed
    -no permission for ALTER access to TABLE "PUBLIC"."TEST"
    -Effective user is TMP$C6078_0
    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -COMMENT ON "PUBLIC"."TEST" failed
    -no permission for ALTER access to TABLE "PUBLIC"."TEST"
    -Effective user is TMP$C6078_0
    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -CREATE OR ALTER TRIGGER "PUBLIC"."TEST_BI" failed
    -no permission for ALTER access to TABLE "PUBLIC"."TEST"
    -Effective user is TMP$C6078_0
    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -CREATE OR ALTER TRIGGER "PUBLIC"."TRG$START" failed
    -no permission for ALTER access to DATABASE
    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -CREATE OR ALTER TRIGGER "PUBLIC"."TRIG_DDL_SP" failed
    -no permission for ALTER access to DATABASE
    ALTERED_TRIGGER_NAME            TEST_BI
    ALTERED_TRIGGER_SOURCE
    as
    begin
    new.uid = gen_uuid();
    end
    Records affected: 1
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -CREATE OR ALTER PACKAGE "PUBLIC"."PKG_TEST" failed
    -No permission for CREATE PACKAGE operation
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -RECREATE PACKAGE BODY "PUBLIC"."PKG_TEST" failed
    -No permission for CREATE PACKAGE operation
    ALTERED_PKG_NAME                <null>
    Records affected: 1
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -CREATE OR ALTER FUNCTION "PUBLIC"."FN_C6078" failed
    -No permission for CREATE FUNCTION operation
    ALTERED_STANDALONE_FUNC         <null>
    Records affected: 1
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -CREATE OR ALTER PROCEDURE "PUBLIC"."SP_C6078" failed
    -No permission for CREATE PROCEDURE operation
    ALTERED_STANDALONE_PROC         <null>
    Records affected: 1
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -CREATE OR ALTER VIEW "PUBLIC"."V_C6078" failed
    -No permission for CREATE VIEW operation
    ALTERED_VIEW_NAME               <null>
    Records affected: 1
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -ALTER SEQUENCE "PUBLIC"."SQ_C6078" failed
    -No permission for CREATE GENERATOR operation
    ALTERED_SEQUENCE_NAME           <null>
    Records affected: 1
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -CREATE OR ALTER EXCEPTION "PUBLIC"."EX_C6078" failed
    -No permission for CREATE EXCEPTION operation
    ALTERED_EXCEPTION_NAME          <null>
    Records affected: 1
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -CREATE OR ALTER FUNCTION "PUBLIC"."WAIT_EVENT" failed
    -No permission for CREATE FUNCTION operation
    ALTERED_UDR_BASED_FUNC          <null>
    Records affected: 1
    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -ALTER CHARACTER SET "SYSTEM"."UTF8" failed
    -no permission for ALTER access to CHARACTER SET "SYSTEM"."UTF8"
    -Effective user is TMP$C6078_0
    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -ALTER MAPPING LOCAL_MAP_C6078 failed
    -Unable to perform operation
    -System privilege CHANGE_MAPPING_RULES is missing
    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -ALTER MAPPING GLOBAL_MAP_C6078 failed
    -Unable to perform operation
    -System privilege CHANGE_MAPPING_RULES is missing
"""

@pytest.mark.version('>=4.0')
def test_2(act: Action, user_0: User, user_1: User, user_2: User):
    script_vars = {'dsn': act.db.dsn,
                   'user_name': act.db.user,
                   'user_password': act.db.password,}
    script_file = act.files_dir / 'core_6078.sql'
    script = script_file.read_text() % script_vars

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.isql(switches=['-q'], input = script, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
