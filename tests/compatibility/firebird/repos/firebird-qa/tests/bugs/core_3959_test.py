#coding:utf-8

"""
ID:          issue-4292
ISSUE:       4292
TITLE:       Repeat Temporary Table access from ReadOnly Transaction and ReadWrite transaction
  causes Internal Firebird consistency check (cannot find record back version (291), file: vio.cpp line: 4905)
DESCRIPTION:
JIRA:        CORE-3959
FBTEST:      bugs.core_3959
"""

import pytest
from firebird.qa import *
from firebird.driver import tpb, TraAccessMode, Isolation

init_script = """
    create or alter procedure fu_x1 as begin end;
    create or alter procedure save_x1 as begin end;
    commit;

    recreate global temporary table x1 (
        id integer,
        name varchar(10)
    ) on commit preserve rows;


    set term ^;
    alter procedure fu_x1 returns (
        sid integer,
        sname varchar(10)) as
    begin
        delete from x1;

        insert into x1 values (1,'1');
        insert into x1 values (2,'2');
        insert into x1 values (3,'3');
        insert into x1 values (4,'4');
        insert into x1 values (5,'5');

        for
            select x1.id, x1.name
        from x1
            into sid, sname
        do
            suspend;
    end
    ^
    alter procedure save_x1 (
        pid integer,
        pname varchar(10))
    as
    begin
        update x1 set name = :pname
        where x1.id = :pid;
        if (row_count = 0) then
            insert into x1 values( :pid, :pname);
    end
    ^
    set term ;^
    commit;
"""

db = db_factory(init=init_script)

act = python_act('db')

@pytest.mark.version('>=3')
def test_1(act: Action):
    with act.db.connect() as con:
        txparam_read = tpb(isolation=Isolation.READ_COMMITTED_RECORD_VERSION, lock_timeout=0,
                           access_mode=TraAccessMode.READ)
        txparam_write = tpb(isolation=Isolation.READ_COMMITTED_RECORD_VERSION, lock_timeout=0)

        tx_read = con.transaction_manager(txparam_read)
        cur_read = tx_read.cursor()
        cur_read.execute("select sid, sname from FU_X1")

        tx_write = con.transaction_manager(txparam_write)
        cur_write = tx_write.cursor()
        cur_write.callproc("save_x1", ['2', 'foo'])
        tx_write.commit()

        cur_read.execute("select sid, sname from FU_X1")
        cur_read.fetchall() # If this does not raises an exception, the test passes
