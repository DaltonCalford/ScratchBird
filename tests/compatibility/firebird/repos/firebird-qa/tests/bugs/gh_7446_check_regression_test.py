#coding:utf-8

"""
ID:          issue-7446
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7446
DESCRIPTION:
    Problem was reported privately by firebird user. As a visible result there were core dump and an error in firebird.log:
        Operating system call pthread_mutex_lock failed. Error code 22

    An error happens due to an attempt to cleanup (too late attempt - when processing disconnect in CS)
    savepoint stored in metadata cache. Put attention that savepoints are always created in transaction memory pool and keeping
    them after commit/rollback in metadata cache is bad idea.

NOTES:
    [01.02.2023] pzotov.
    This test checks regression that was introduced by first attempt to solve problem described in the title.
    SQL example was provided by dimitr, see letter 31-jan-2023 13:43. Running SQL script must NOT issue any output.
    There is no similar test in old fbtest QA suite.

    Regression can be reproduced on:
        * FB 3.0.11.33654 (17-jan-2023): "SQLSTATE = 3B000 / Unable to find savepoint with name S in transaction context".
    Checked on:
        3.0.11.33658; 4.0.3.2894; 5.0.0.920 - all fine.
"""

import pytest
import locale
from firebird.qa import *

db = db_factory()
act = python_act('db')

@pytest.mark.version('>=3.0.11')
def test_1(act: Action):

    sql_chk = """
        set term ^;
        create procedure p0 returns (r int)
        as
        begin
           r = 0;
           suspend;
        end^

        create procedure p returns (r int)
        as
        begin
           in autonomous transaction do
           begin
             select p0.r from p0 into :r;
           end

           suspend;
        end^
        set term ;^

        savepoint s;
        set heading off;
        select * from p;
        release savepoint s;
    """
    act.expected_stdout = "0"
    act.isql(switches = ['-q'], input = sql_chk, combine_output = True, io_enc = locale.getpreferredencoding())
    assert act.clean_stdout == act.clean_expected_stdout
