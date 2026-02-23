#coding:utf-8

"""
ID:          n/a
TITLE:       SQL schemas. Name resolution. Usage of scope specifier (`%`).
DESCRIPTION:
    Original test see in:
    https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/FB_NAME_RESOLUTION_02.script

    Documentation:
    $FB_HOME/doc/sql.extensions/README.name_resolution.md
NOTES:
    [06.09.2025] pzotov
    Checked on 6.0.0.1261
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set term ^;
    create schema ambiguous1^

    set search_path to ambiguous1, public, system^

    create procedure ambiguous1.p1 returns (o varchar(50))
    as
    begin
        o = 'ambiguous1.p1';
        suspend;
    end^

    create table ambiguous1.p2 (
        o varchar(50)
    )^

    insert into ambiguous1.p2 (o) values ('ambiguous1.p2')^

    create function ambiguous1.f1 returns varchar(50)
    as
    begin
        return 'ambiguous1.f1';
    end^

    create function ambiguous1.f2 returns varchar(50)
    as
    begin
        return 'ambiguous1.f2';
    end^

    create package public.ambiguous1
    as
    begin
        procedure p1 returns (o varchar(50));
        procedure p1a returns (o varchar(50));
        procedure p1b returns (o varchar(50));
        procedure p1c returns (o varchar(50));
        procedure p1d returns (o varchar(50));
        procedure p2 returns (o varchar(50));
        procedure p2a returns (o varchar(50));
        procedure p2b returns (o varchar(50));
        procedure p2c returns (o varchar(50));
        procedure p2d returns (o varchar(50));
        function f1 returns varchar(50);
        function f1a returns varchar(50);
        function f1b returns varchar(50);
        function f1c returns varchar(50);
        function f1d returns varchar(50);
        function f2 returns varchar(50);
    end^

    create package body public.ambiguous1
    as
    begin
        procedure p1 returns (o varchar(50))
        as
        begin
            o = 'public.ambiguous1.p1';
            suspend;
        end

        procedure p1a returns (o varchar(50))
        as
        begin
            execute procedure p1 returning_values o;
            suspend;
        end

        procedure p1b returns (o varchar(50))
        as
        begin
            execute procedure ambiguous1.p1 returning_values o;
            suspend;
        end

        procedure p1c returns (o varchar(50))
        as
        begin
            execute procedure ambiguous1%package.p1 returning_values o;
            suspend;
        end

        procedure p1d returns (o varchar(50))
        as
        begin
            execute procedure ambiguous1%schema.p1 returning_values o;
            suspend;
        end

        procedure p2 returns (o varchar(50))
        as
        begin
            o = 'public.ambiguous1.p2';
            suspend;
        end

        procedure p2a returns (o varchar(50))
        as
        begin
            select * from p2 into o;
            suspend;
        end

        procedure p2b returns (o varchar(50))
        as
        begin
            select * from ambiguous1.p2 into o;
            suspend;
        end

        procedure p2c returns (o varchar(50))
        as
        begin
            select * from ambiguous1%package.p2 into o;
            suspend;
        end

        procedure p2d returns (o varchar(50))
        as
        begin
            select * from ambiguous1%schema.p2 into o;
            suspend;
        end

        function f1 returns varchar(50)
        as
        begin
            return 'public.ambiguous1.f1';
        end

        function f1a returns varchar(50)
        as
        begin
            return f1();
        end

        function f1b returns varchar(50)
        as
        begin
            return ambiguous1.f1();
        end

        function f1c returns varchar(50)
        as
        begin
            return ambiguous1%package.f1();
        end

        function f1d returns varchar(50)
        as
        begin
            return ambiguous1%schema.f1();
        end

        function f2 returns varchar(50)
        as
        begin
            return 'public.ambiguous1.f2';
        end
    end^
    set term ;^
    commit;

    set bail OFF;
    set list on;

    execute procedure ambiguous1.p1;
    execute procedure ambiguous1%package.p1;
    execute procedure ambiguous1%schema.p1;
    execute procedure public.ambiguous1.p1;

    select * from ambiguous1.p1;
    select * from ambiguous1%package.p1;
    select * from ambiguous1%schema.p1;
    select * from public.ambiguous1.p1;

    execute procedure ambiguous1.p1a;
    execute procedure ambiguous1.p1b;
    execute procedure ambiguous1.p1c;
    execute procedure ambiguous1.p1d;

    execute procedure ambiguous1.p2;
    execute procedure ambiguous1%package.p2;
    execute procedure ambiguous1%schema.p2;  -- error
    execute procedure public.ambiguous1.p2;

    select * from ambiguous1.p2;
    select * from ambiguous1%package.p2;
    select * from ambiguous1%schema.p2;
    select * from public.ambiguous1.p2;

    select * from ambiguous1.p2a;
    select * from ambiguous1.p2b;
    select * from ambiguous1.p2c;
    select * from ambiguous1.p2d;

    select ambiguous1.f1() from rdb$database;
    select ambiguous1%package.f1() from rdb$database;
    select ambiguous1%schema.f1() from rdb$database;
    select public.ambiguous1.f1() from rdb$database;

    select ambiguous1.f1a() from rdb$database;
    select ambiguous1.f1b() from rdb$database;
    select ambiguous1.f1c() from rdb$database;
    select ambiguous1.f1d() from rdb$database;

    select ambiguous1.f2() from rdb$database;
    select ambiguous1%package.f2() from rdb$database;
    select ambiguous1%schema.f2() from rdb$database;
    select public.ambiguous1.f2() from rdb$database;

"""

act = isql_act('db', test_script, substitutions=[('After line \\d+.*', ''), ('[ \t]+', ' ')])

expected_stdout = """
    O ambiguous1.p1
    O public.ambiguous1.p1
    O ambiguous1.p1
    O public.ambiguous1.p1
    O ambiguous1.p1
    O public.ambiguous1.p1
    O ambiguous1.p1
    O public.ambiguous1.p1
    O public.ambiguous1.p1
    O public.ambiguous1.p1
    O public.ambiguous1.p1
    O ambiguous1.p1
    O public.ambiguous1.p2
    O public.ambiguous1.p2
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -204
    -Procedure unknown
    -"AMBIGUOUS1"."P2"
    O public.ambiguous1.p2
    O ambiguous1.p2
    O public.ambiguous1.p2
    O ambiguous1.p2
    O public.ambiguous1.p2
    O public.ambiguous1.p2
    O public.ambiguous1.p2
    O public.ambiguous1.p2
    O ambiguous1.p2
    F1 ambiguous1.f1
    F1 public.ambiguous1.f1
    F1 ambiguous1.f1
    F1 public.ambiguous1.f1
    F1A public.ambiguous1.f1
    F1B public.ambiguous1.f1
    F1C public.ambiguous1.f1
    F1D ambiguous1.f1
    F2 ambiguous1.f2
    F2 public.ambiguous1.f2
    F2 ambiguous1.f2
    F2 public.ambiguous1.f2
"""

@pytest.mark.version('>=6')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
