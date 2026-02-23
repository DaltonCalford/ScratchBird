#coding:utf-8

"""
ID:          issue-8498
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8498
TITLE:       Range-based FOR statement
DESCRIPTION:
    Test check issues from doc/sql.extensions/README.range_based_for.md:
        * ability to pass parameter for <variable>;
        * ability to use values near bounds for appropriate datatype and raised error;
        * error in case if non-exact numeric types are used for `<variable>`, `<initial value>`, `<final value>` and `<by value>`;
        * error if range-based FOR BY argument not positive (and not null);
        * caching of `<initial value>`, `<final value>` and `<by value>`
        * etc
    Basic test see in functional/gtcs/test_fb_sql_for_range.py
NOTES:
    [08.04.2025] pzotov
    Checked on 6.0.0.717-f5b6b0c
"""
import pytest
from firebird.qa import *

db = db_factory()

test_script = f"""
    set list on;
    create sequence g_init;
    create sequence g_fini;
    create sequence g_step;
    create table test(id int128 primary key);
    commit;
    set term ^;

    ----------------------------------------
    -- check doc:
    -- FOR <variable> = <initial value> TO ...
    -- ...
    -- "`<variable>` also accepts parameters"
    execute block as
       declare init_value int = 11;
    begin
        execute statement
        (
            q'#
                execute block (a_param int = ?) as
                    declare x smallint;
                begin
                    for a_param = a_param downto a_param - 2 do insert into test(id) values( 2 * :a_param - 1);
                end
            #'
        ) (init_value)
        ;
    end
    ^
    -- must issue: ID_1 = 21; 19; 17
    select t.id as id_1 from test t
    ^
    rollback
    ^
    ----------------------------------------

    -- check ability to use boundary values and error in case if they exceeed appropriate datatype limit
    -- must raise (in STDOUT):
    -- counter_1 = 32767
    -- raised_gds = 335544321 // arithmetic exception, numeric overflow, or string truncation
    execute block returns(counter_1 smallint, raised_gds int) as
    begin
        begin
            raised_gds = null;
            for counter_1 = 32767 to 170141183460469231731687303715884105727 do
            begin
                -- nop --
            end
        when any do
            begin
                raised_gds = gdscode;
            end
        end
        suspend;
    end
    ^

    -- `<variable>`, `<initial value>`, `<final value>` and `<by value>`
    -- must be expressions of exact numeric types.
    -- must raise compile-time error:
    -- Statement failed, SQLSTATE = 42000
    -- Dynamic SQL Error
    -- -Arguments for range-based FOR must be exact numeric types
    -- (stdout remains empty)
    execute block returns(counter_2 smallint, raised_gds int) as
    begin
        begin
            raised_gds = null;
            for counter_2 = 3 to 4 by 1e0 do
            begin
                -- nop --
            end
        when any do
            begin
                raised_gds = gdscode;
            end
        end
        suspend;
    end
    ^

    -- `<by value>` ...  If it is zero or negative, an error is raised.
    -- Statement failed, SQLSTATE = 42000
    -- Range-based FOR BY argument must be positive
    -- Raises to STDOUT:
    -- COUNTER_3                       4
    -- RAISED_GDS                      335545314 // isc_range_for_by_should_be_positive (see src/include/gen/Firebird.pas)
    execute block returns(counter_3 smallint, raised_gds int) as
       declare x smallint = 0;
    begin
        begin
            raised_gds = null;
            for counter_3 = 4 to 3 by x do
            begin
                -- nop --
            end
        when any do
            begin
                raised_gds = gdscode;
            end
        end
        suspend;
    end
    ^

    -- `<initial value>` is evaluated and assigned to `<variable>`.
    -- If it is `NULL`, the loop is not executed.
    -- Issues to STDOUT: counter_4 <null>
    execute block returns(counter_4 smallint) as
    begin
        begin
            for counter_4 = 1 to 3 do
            begin
                counter_4 = null;
                suspend;
            end
        end
    end
    ^

    -- <final value> must be cached, so its assigning to null must not stop loop.
    -- STDOUT must contain: counter_5 = 1; 2; 3
    execute block returns(counter_5 smallint) as
       declare n smallint = 3;
    begin
        begin
            for counter_5 = 1 to n do
            begin
                n = null;
                suspend;
            end
        end
    end
    ^

    -- <by valua> must be cached, so its assigning to null must not stop loop.
    -- STDOUT must contain: counter_5 = 1; 2; 3
    execute block returns(counter_6 smallint) as
       declare x smallint = 1;
    begin
        begin
            for counter_6 = 1 to 3 by x do
            begin
                x = null;
                suspend;
            end
        end
    end
    ^

    -- must issue: 4; 10; 22
    execute block returns (counter_7 integer)
    as
    begin
        for counter_7 = 2 to 16 do
        begin
            counter_7 = counter_7 * 2;
            suspend;
        end

    end
    ^

    -- must issue: 8; 3; 1
    execute block returns (counter_8 integer)
    as
    begin
        for counter_8 = 16 downto 2 do
        begin
            counter_8 = counter_8 / 2;
            suspend;
        end
    end
    ^

    -- must issue: 1; 2; 3; 4.
    -- generators must have values: g_init = 1; g_fini = 4; g_step = 1 (because of caching <initial value>, <final value>, <by value>)
    execute block returns (counter_9 integer)
    as
    begin
        for counter_9 = gen_id(g_init,1) to gen_id(g_fini,4) by gen_id(g_step,1) do
        begin
            suspend;
        end
    end
    ^

    select
        gen_id(g_init,0) as g_init_value
       ,gen_id(g_fini,0) as g_fini_value
       ,gen_id(g_step,0) as g_step_value
    from rdb$database
    ^

"""

act = isql_act('db', test_script, substitutions=[ ('BLOB_ID .*', ''), ('[ \\t]+', ' ') ])

@pytest.mark.intl
@pytest.mark.version('>=6.0')
def test_1(act: Action):

    expected_stdout = f"""
        ID_1 21
        ID_1 19
        ID_1 17

        COUNTER_1 32767
        RAISED_GDS 335544321

        Statement failed, SQLSTATE = 42000
        Dynamic SQL Error
        -Arguments for range-based FOR must be exact numeric types

        COUNTER_3 4
        RAISED_GDS 335545314

        COUNTER_4 <null>

        COUNTER_5 1
        COUNTER_5 2
        COUNTER_5 3

        COUNTER_6 1
        COUNTER_6 2
        COUNTER_6 3

        COUNTER_7 4
        COUNTER_7 10
        COUNTER_7 22

        COUNTER_8 8
        COUNTER_8 3
        COUNTER_8 1

        COUNTER_9 1
        COUNTER_9 2
        COUNTER_9 3
        COUNTER_9 4
        
        G_INIT_VALUE 1
        G_FINI_VALUE 4
        G_STEP_VALUE 1
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
