#coding:utf-8

"""
ID:          issue-5116
ISSUE:       5116
TITLE:       EXECUTE PROCEDURE's RETURNING_VALUES and EXECUTE STATEMENT's INTO does not check validity of assignments targets leading to bugcheck
DESCRIPTION:
JIRA:        CORE-4819
FBTEST:      bugs.core_4819
NOTES:
    [30.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.881; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create or alter procedure sp_test(a_x int) as begin end;
    recreate table test(id int primary key, x bigint, y bigint);

    insert into test values(1,  3,  7);

    set list on;

    ---------------------
    select 'initial state' msg, t.* from test t;
    ---------------------
    commit;

    set term ^;
    create or alter procedure sp_test(a_x int) returns(o_y int) as
    begin
        o_y = 7 * a_x;
        suspend;
    end
    ^
    commit
    ^
    create or alter trigger test_aud1 for test active after insert or update or delete as
    begin
        -- Since WI-T3.0.0.31846 following must produce:
        -- Statement failed, SQLSTATE = 42000
        -- attempted update of read-only column

        if (not inserting) then
            begin
                execute procedure sp_test(old.x) returning_values(old.x);
                execute procedure sp_test(old.x) returning_values(old.y);
            end
        else
            begin
                execute procedure sp_test(new.x) returning_values(new.x);
                execute procedure sp_test(new.x) returning_values(new.y);
            end

        rdb$set_context( 'USER_SESSION', 'X_TRG_AIUD1', iif( inserting, new.x, old.x ) );
        rdb$set_context( 'USER_SESSION', 'Y_TRG_AIUD1', iif( inserting, new.y, old.y ) );

    end
    ^

    create or alter trigger test_aud2 for test active after insert or update or delete as
    begin
        -- Since WI-T3.0.0.31846 following must produce:
        -- Statement failed, SQLSTATE = 42000
        -- attempted update of read-only column
        if (not inserting) then
            begin
                execute statement ('execute procedure sp_test( ? )') ( 2 * old.x ) into old.x;
                execute statement ('execute procedure sp_test( ? )') ( 2 * old.x ) into old.y;
            end
        else
            begin
                execute statement ('execute procedure sp_test( ? )') ( 2 * new.x ) into new.x;
                execute statement ('execute procedure sp_test( ? )') ( 2 * new.x ) into new.y;
            end

        rdb$set_context( 'USER_SESSION', 'X_TRG_AIUD2', iif( inserting, new.x, old.x ) );
        rdb$set_context( 'USER_SESSION', 'Y_TRG_AIUD2', iif( inserting, new.y, old.y ) );
    end
    ^
    set term ;^
    commit;

    -- ######################################################################
    -- 1. Check when 'execute proc returning_values' is called from trigger,
    --    making invalid update of old. & new. variables:

    delete from test where id = 1
    returning 'delete-returning:' as msg,
              x as old_x,
              y as old_y;
    select
        'after  delete' msg,
        cast(rdb$get_context('USER_SESSION', 'X_TRG_AIUD1') as int) as X_TRG_AIUD1,
        cast(rdb$get_context('USER_SESSION', 'Y_TRG_AIUD1') as int) as Y_TRG_AIUD1,
        cast(rdb$get_context('USER_SESSION', 'X_TRG_AIUD2') as int) as X_TRG_AIUD2,
        cast(rdb$get_context('USER_SESSION', 'Y_TRG_AIUD2') as int) as Y_TRG_AIUD2
    from rdb$database;
    rollback;

    update test set x = 17 where id = 1
    returning 'update-returning:' as msg,
              old.x as old_x,
              new.x as new_x,
              old.y as old_y,
              new.y as new_y;

    select
        'after update' msg,
        cast(rdb$get_context('USER_SESSION', 'X_TRG_AIUD1') as int) as X_TRG_AIUD1,
        cast(rdb$get_context('USER_SESSION', 'Y_TRG_AIUD1') as int) as Y_TRG_AIUD1,
        cast(rdb$get_context('USER_SESSION', 'X_TRG_AIUD2') as int) as X_TRG_AIUD2,
        cast(rdb$get_context('USER_SESSION', 'Y_TRG_AIUD2') as int) as Y_TRG_AIUD2,
        t.*
    from test t;
    rollback;

    ---------------------

    insert into test(id, x) values(2, 19)
    returning 'insert-returning:' as msg, x as new_x, y as new_y;

    select
        'after  insert' msg,
        cast(rdb$get_context('USER_SESSION', 'X_TRG_AIUD1') as int) as X_TRG_AIUD1,
        cast(rdb$get_context('USER_SESSION', 'Y_TRG_AIUD1') as int) as Y_TRG_AIUD1,
        cast(rdb$get_context('USER_SESSION', 'X_TRG_AIUD2') as int) as X_TRG_AIUD2,
        cast(rdb$get_context('USER_SESSION', 'Y_TRG_AIUD2') as int) as Y_TRG_AIUD2,
        t.*
    from test t where id=2;
    rollback;

    -- ######################################################################
    -- 2. Check when call 'execute proc returning_values' inside implicit cursor:
    set term ^;
    execute block returns(msg varchar(20), old_y int, new_y int) as
    begin
      msg='within cursor-1:';
      for
          select x, y from test
          as cursor ce
      do begin
          old_y = ce.y;

          -- WI-T3.0.0.31845. Following leads to:
          -- Statement failed, SQLSTATE = XX000
          -- internal Firebird consistency check (EVL_assign_to: invalid operation (229), file: evl.cpp line: 205)
          -- Since WI-T3.0.0.31846 must produce:
          -- Statement failed, SQLSTATE = 42000
          -- attempted update of read-only column

          execute procedure sp_test(ce.x) returning_values(ce.y);
          new_y = ce.y;
          suspend;
      end
    end
    ^
    set term ;^
    rollback;

    -- ######################################################################
    -- 3. Check when call 'execute statement' inside implicit cursor:
    set term ^;
    execute block returns(msg varchar(20), old_y int, new_y int) as
    begin
      msg='within cursor-2:';
      for
          select x, y from test
          as cursor ce
      do begin
          old_y = ce.y;

          -- Since WI-T3.0.0.31846 following must produce:
          -- Statement failed, SQLSTATE = 42000
          -- attempted update of read-only column
          execute statement ('execute procedure sp_test( ? )') ( ce.x ) into ce.y;
          new_y = ce.y;
          suspend;
      end
    end
    ^
    set term ;^
    rollback;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

fb3x_checked_stdout = """
    MSG                             initial state
    ID                              1
    X                               3
    Y                               7
    Statement failed, SQLSTATE = 42000
    attempted update of read-only column
    Statement failed, SQLSTATE = 42000
    attempted update of read-only column
    Statement failed, SQLSTATE = 42000
    attempted update of read-only column
    MSG                             after  delete
    X_TRG_AIUD1                     <null>
    Y_TRG_AIUD1                     <null>
    X_TRG_AIUD2                     <null>
    Y_TRG_AIUD2                     <null>
    Statement failed, SQLSTATE = 42000
    attempted update of read-only column
    MSG                             after update
    X_TRG_AIUD1                     <null>
    Y_TRG_AIUD1                     <null>
    X_TRG_AIUD2                     <null>
    Y_TRG_AIUD2                     <null>
    ID                              1
    X                               3
    Y                               7
    Statement failed, SQLSTATE = 42000
    attempted update of read-only column
    Statement failed, SQLSTATE = 42000
    attempted update of read-only column
    Statement failed, SQLSTATE = 42000
    attempted update of read-only column
"""

fb5x_checked_stdout = """
    MSG                             initial state
    ID                              1
    X                               3
    Y                               7
    Statement failed, SQLSTATE = 42000
    attempted update of read-only column TEST.X
    Statement failed, SQLSTATE = 42000
    attempted update of read-only column TEST.X
    Statement failed, SQLSTATE = 42000
    attempted update of read-only column TEST.X
    MSG                             after  delete
    X_TRG_AIUD1                     <null>
    Y_TRG_AIUD1                     <null>
    X_TRG_AIUD2                     <null>
    Y_TRG_AIUD2                     <null>
    Statement failed, SQLSTATE = 42000
    attempted update of read-only column TEST.X
    MSG                             after update
    X_TRG_AIUD1                     <null>
    Y_TRG_AIUD1                     <null>
    X_TRG_AIUD2                     <null>
    Y_TRG_AIUD2                     <null>
    ID                              1
    X                               3
    Y                               7
    Statement failed, SQLSTATE = 42000
    attempted update of read-only column TEST.X
    Statement failed, SQLSTATE = 42000
    attempted update of read-only column CE.Y
    Statement failed, SQLSTATE = 42000
    attempted update of read-only column CE.Y
"""

fb6x_checked_stdout = """
    MSG                             initial state
    ID                              1
    X                               3
    Y                               7
    Statement failed, SQLSTATE = 42000
    attempted update of read-only column "PUBLIC"."TEST"."X"
    Statement failed, SQLSTATE = 42000
    attempted update of read-only column "PUBLIC"."TEST"."X"
    Statement failed, SQLSTATE = 42000
    attempted update of read-only column "PUBLIC"."TEST"."X"
    MSG                             after  delete
    X_TRG_AIUD1                     <null>
    Y_TRG_AIUD1                     <null>
    X_TRG_AIUD2                     <null>
    Y_TRG_AIUD2                     <null>
    Statement failed, SQLSTATE = 42000
    attempted update of read-only column "PUBLIC"."TEST"."X"
    MSG                             after update
    X_TRG_AIUD1                     <null>
    Y_TRG_AIUD1                     <null>
    X_TRG_AIUD2                     <null>
    Y_TRG_AIUD2                     <null>
    ID                              1
    X                               3
    Y                               7
    Statement failed, SQLSTATE = 42000
    attempted update of read-only column "PUBLIC"."TEST"."X"
    Statement failed, SQLSTATE = 42000
    attempted update of read-only column "CE"."Y"
    Statement failed, SQLSTATE = 42000
    attempted update of read-only column "CE"."Y"
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = fb3x_checked_stdout if act.is_version('<4') else fb5x_checked_stdout if act.is_version('<6') else fb6x_checked_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

