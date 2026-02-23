#coding:utf-8

"""
ID:          n/a
TITLE:       SQL schemas. Name resolution. Usage of scope specifier (`%`).
DESCRIPTION:
    Original test see in:
    https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/FB_NAME_RESOLUTION_01.script

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
    set list on;
    set term ^;
    create package pkg1
    as
    begin
        procedure p1 returns (o varchar(50));
        procedure p1a returns (o varchar(50));
        procedure p1b returns (o varchar(50));
        procedure p1c returns (o varchar(50));
        procedure p2 returns (o varchar(50));
        procedure p2a returns (o varchar(50));
        procedure p2b returns (o varchar(50));
        procedure p2c returns (o varchar(50));
        function f1 returns varchar(50);
        function f1a returns varchar(50);
        function f1b returns varchar(50);
        function f1c returns varchar(50);
        function f2 returns varchar(50);
    end^

    create package body pkg1
    as
    begin
        procedure p1 returns (o varchar(50))
        as
        begin
            o = 'pkg1.p1';
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
            execute procedure pkg1.p1 returning_values o;
            suspend;
        end

        procedure p1c returns (o varchar(50))
        as
        begin
            execute procedure pkg1%package.p1 returning_values o;
            suspend;
        end

        procedure p2 returns (o varchar(50))
        as
        begin
            o = 'pkg1.p2';
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
            select * from pkg1.p2 into o;
            suspend;
        end

        procedure p2c returns (o varchar(50))
        as
        begin
            select * from pkg1%package.p2 into o;
            suspend;
        end

        function f1 returns varchar(50)
        as
        begin
            return 'pkg1.f1';
        end

        function f1a returns varchar(50)
        as
        begin
            return f1();
        end

        function f1b returns varchar(50)
        as
        begin
            return pkg1.f1();
        end

        function f1c returns varchar(50)
        as
        begin
            return pkg1%package.f1();
        end

        function f2 returns varchar(50)
        as
        begin
            return 'pkg1.f2';
        end
    end^
    set term ;^
    commit;

    execute procedure pkg1.p1;
    execute procedure pkg1%package.p1;

    select * from pkg1.p1;
    select * from pkg1%package.p1;

    execute procedure pkg1.p1a;
    execute procedure pkg1.p1b;
    execute procedure pkg1.p1c;

    execute procedure pkg1.p2;
    execute procedure pkg1%package.p2;

    select * from pkg1.p2a;
    select * from pkg1.p2b;
    select * from pkg1.p2c;

    select pkg1.f1() from rdb$database;

    select pkg1.f1a() from rdb$database;
    select pkg1.f1b() from rdb$database;
    select pkg1.f1c() from rdb$database;

    select pkg1%package.f1() from rdb$database;

    select pkg1.f2() from rdb$database;
    select pkg1%package.f2() from rdb$database;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    O pkg1.p1
    O pkg1.p1
    O pkg1.p1
    O pkg1.p1
    O pkg1.p1
    O pkg1.p1
    O pkg1.p1
    O pkg1.p2
    O pkg1.p2
    O pkg1.p2
    O pkg1.p2
    O pkg1.p2
    F1 pkg1.f1
    F1A pkg1.f1
    F1B pkg1.f1
    F1C pkg1.f1
    F1 pkg1.f1
    F2 pkg1.f2
    F2 pkg1.f2
"""

@pytest.mark.version('>=6')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
