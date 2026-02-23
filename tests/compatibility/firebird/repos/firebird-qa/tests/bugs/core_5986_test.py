#coding:utf-8

"""
ID:          issue-6238
ISSUE:       6238
TITLE:       Incorrect evaluation of NULL IS [NOT] {FALSE | TRUE}
DESCRIPTION:
  Test was implemented on the basis of 7IWD2-02-Foundation-2011-12.pdf, page 322
  (as it was suggested by Mark Rotteveel in the ticket, see his note 17/Jan/19 03:13 PM).
JIRA:        CORE-5986
FBTEST:      bugs.core_5986
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set list on;

    -- General rules: "NOT(True) is False, NOT(False) is True, and NOT(Unknown) is Unknown"
    select not true b1, not false b2, not unknown b3
    from rdb$database;

    -- Data for output in Truth-tables:
    recreate table test( x boolean, y boolean );
    insert into test(x,y) select
    true,   true    from rdb$database union all select
    true,   false   from rdb$database union all select
    true,   unknown from rdb$database union all select
    false,  true    from rdb$database union all select
    false,  false   from rdb$database union all select
    false,  unknown from rdb$database union all select
    unknown,true    from rdb$database union all select
    unknown,false   from rdb$database union all select
    unknown,unknown from rdb$database
    ;
    commit;

    -- Table 11. Truth table for the AND boolean operator.
    select x, y, x and y as "x_AND_y" from test;

    -- Table 12. Truth table for the OR boolean operator.
    select x, y, x or y as "x_OR_y" from test;

    -- Table 12. Truth table for the IS boolean operator.
    --select x, y, x is y as "x_IS_y" from test; --> token unknown; doing this using ES

    set term ^;
    execute block returns( x boolean, y boolean, "x_IS_y" boolean) as
        declare run_expr varchar(128);
    begin
        for
            select
                 x
                ,y
                ,''||coalesce(x,'unknown') || ' is ' || coalesce(y,'unknown')
            from test
        into x, y, run_expr
        do begin
            execute statement 'select ' || run_expr || ' from rdb$database' into "x_IS_y";
            suspend;
        end
    end
    ^
    set term ;^
    /*
              X       Y x_AND_y
        ======= ======= =======
        <true>  <true>  <true>
        <true>  <false> <false>
        <true>  <null>  <null>
        <false> <true>  <false>
        <false> <false> <false>
        <false> <null>  <false>
        <null>  <true>  <null>
        <null>  <false> <false>
        <null>  <null>  <null>


              X       Y  x_OR_y
        ======= ======= =======
        <true>  <true>  <true>
        <true>  <false> <true>
        <true>  <null>  <true>
        <false> <true>  <true>
        <false> <false> <false>
        <false> <null>  <null>
        <null>  <true>  <true>
        <null>  <false> <null>
        <null>  <null>  <null>


              X       Y  x_IS_y
        ======= ======= =======
        <true>  <true>  <true>
        <true>  <false> <false>
        <true>  <null>  <false>
        <false> <true>  <false>
        <false> <false> <true>
        <false> <null>  <false>
        <null>  <true>  <false>
        <null>  <false> <false>
        <null>  <null>  <true>
    */
"""

act = isql_act('db', test_script)

expected_stdout = """
    B1                              <false>
    B2                              <true>
    B3                              <null>


    X                               <true>
    Y                               <true>
    x_AND_y                         <true>

    X                               <true>
    Y                               <false>
    x_AND_y                         <false>

    X                               <true>
    Y                               <null>
    x_AND_y                         <null>

    X                               <false>
    Y                               <true>
    x_AND_y                         <false>

    X                               <false>
    Y                               <false>
    x_AND_y                         <false>

    X                               <false>
    Y                               <null>
    x_AND_y                         <false>

    X                               <null>
    Y                               <true>
    x_AND_y                         <null>

    X                               <null>
    Y                               <false>
    x_AND_y                         <false>

    X                               <null>
    Y                               <null>
    x_AND_y                         <null>


    X                               <true>
    Y                               <true>
    x_OR_y                          <true>

    X                               <true>
    Y                               <false>
    x_OR_y                          <true>

    X                               <true>
    Y                               <null>
    x_OR_y                          <true>

    X                               <false>
    Y                               <true>
    x_OR_y                          <true>

    X                               <false>
    Y                               <false>
    x_OR_y                          <false>

    X                               <false>
    Y                               <null>
    x_OR_y                          <null>

    X                               <null>
    Y                               <true>
    x_OR_y                          <true>

    X                               <null>
    Y                               <false>
    x_OR_y                          <null>

    X                               <null>
    Y                               <null>
    x_OR_y                          <null>


    X                               <true>
    Y                               <true>
    x_IS_y                          <true>

    X                               <true>
    Y                               <false>
    x_IS_y                          <false>

    X                               <true>
    Y                               <null>
    x_IS_y                          <false>

    X                               <false>
    Y                               <true>
    x_IS_y                          <false>

    X                               <false>
    Y                               <false>
    x_IS_y                          <true>

    X                               <false>
    Y                               <null>
    x_IS_y                          <false>

    X                               <null>
    Y                               <true>
    x_IS_y                          <false>

    X                               <null>
    Y                               <false>
    x_IS_y                          <false>

    X                               <null>
    Y                               <null>
    x_IS_y                          <true>
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
