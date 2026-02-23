#coding:utf-8

"""
ID:          issue-6599
ISSUE:       6599
TITLE:       Adding NOT NULL column with DEFAULT value may cause default values to update
  when selecting or have the wrong charset
DESCRIPTION:
JIRA:        CORE-6358
FBTEST:      bugs.core_6358
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    -- Part 1: adding field of timestamp datatype with default value = 'now'
    -- #######

    create table tdelay(id int primary key);
    create sequence g;
    set term ^;
    create procedure sp_delay returns( dts1 timestamp, dts2 timestamp, elap_ms int ) as
        declare c int;
        declare v_id int;
    begin
        execute statement 'select count(*) from rdb$database /* do you really think that i am in work now ? */' into c;
        v_id = gen_id(g,1);
        dts1 = cast('now' as timestamp);
        insert into tdelay(id) values( :v_id );
        in autonomous transaction do
        begin
            execute statement ('insert into tdelay(id) values(?)') ( :v_id );
            when any do
            begin
                -- nop --
            end
        end
        delete from tdelay where id = :v_id;
        dts2 = cast('now' as timestamp);
        elap_ms = datediff( millisecond from dts1 to dts2);
        suspend;
    end
    ^
    set term ;^
    commit;

    -----------------------------
    recreate table t (n integer);
    insert into t values (1);
    commit;
    alter table t add dts timestamp default 'now' not null;
    commit;
    -----------------------------

    set transaction lock timeout 1;
	--                          ^^^ ### DELAY ###
    set term ^;
    execute block returns( diff_ms int ) as
        declare c int;
        declare t1 timestamp;
        declare t2 timestamp;
    begin
        select dts from t into t1;

		-- HERE WE MAKE DELAY:
		-- ###################
        select count(*) from sp_delay p into c;

        select dts from t into t2;
        diff_ms = datediff(millisecond from t1 to t2);
        suspend;
    end^
    set term ;^


    -- Part 2: adding field with charset that differs from current client connection charset.
    --##################################

    recreate table t2 (n integer);
    insert into t2 values (1);
    commit;
    alter table t2 add c1 varchar(10) character set win1252 default '123áé456' not null;
    insert into t2 (n) values (2);
    select * from t2;

"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
	DIFF_MS                         0

	N                               1
	C1                              123áé456
	N                               2
	C1                              123áé456
"""

@pytest.mark.version('>=3.0.7')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
