#coding:utf-8

"""
ID:          n/a
TITLE:       Test of NESTED FUNCTIONS functionality
DESCRIPTION:
    Original test see in:
    https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/FB_SQL_SUBFUNC_1.script
NOTES:
    [21.09.2025] pzotov
    Firebird 3.x is not checked (issues "token unknown ';'").
    Checked on 6.0.0.1277 5.0.4.1713 4.0.7.3231
"""

import pytest
from firebird.qa import *

db = db_factory()

tmp_user: User = user_factory('db', name='tmp_gtcs_nested_func_user', password='123')

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

substitutions=[('=', ''), ('[ \t]+', ' '), ('line(:)?\\s+\\d+,\\s+col(umn)?(:)?\\s+\\d+', ''), ('variable (\\[)?number \\d+(\\])?', 'variable <N>')]
for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = isql_act('db', substitutions = substitutions)

#-----------------------------------------------------------

@pytest.mark.version('>=4.0')
def test_1(act: Action, tmp_user: User):

    test_script = f"""
        set list on;
        set blob all;
        set term ^;

        --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

        create procedure p1 returns (o1 integer)
        as
            declare function sub1 returns integer
            as
            begin
                return 1;
            end
        begin
            o1 = sub1();
            suspend;
        end^

        select * from p1^
        select 'point-000' as msg from rdb$database^

        --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

        create function f1 returns integer
        as
            declare function sub1 returns integer
            as
            begin
                return 1;
            end
        begin
            return sub1();
        end^

        select f1() from rdb$database^
        select 'point-020' as msg from rdb$database^

        --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

        execute block returns (o1 integer)
        as
            declare function sub1 (i1 integer, i2 integer = 2) returns integer
            as
            begin
                return i2;
            end
        begin
            o1 = sub1(0, 1);
            suspend;

            o1 = sub1(0);
            suspend;
        end^
        select 'point-040' as msg from rdb$database^

        --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

        -- non-ascii names in inner functions:
        create function f2 (i1 integer) returns integer
        as
            declare function "súb1" ("í1" integer) returns integer not null
            as
            begin
                return "í1";
            end
        begin
            return "súb1"(i1);
        end^

        grant execute on function f2 to user {tmp_user.name}^

        select f2(1) from rdb$database^

        -- must raise:
        -- SQLSTATE 42000 / validation error for variable [number 1], value *** null *** / -At sub function súb1 ...
        select f2(null) from rdb$database^

        select 'point-050' as msg from rdb$database^

        --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

        -- must raise:
        -- SQLSTATE=42000 / duplicate specification of SUB1 - not supported
        execute block returns (o1 integer)
        as
            declare function sub1 returns integer
            as
            begin
            end

            declare function sub1 returns integer
            as
            begin
            end
        begin
        end^
        select 'point-060' as msg from rdb$database^

        --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

         -- must raise: 
         -- SQLSTATE = 0A000 / feature is not supported / nested sub function
        execute block returns (o1 integer)
        as
            declare function sub1 returns integer
            as
                declare function sub11 returns integer
                as
                begin
                end
            begin
            end
        begin
        end^
        select 'point-070' as msg from rdb$database^

        --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

        create domain d1 integer^
        create domain d2 integer^
        create domain d3 integer^

        create table t1 (x1 d3)^

        create function f3 returns integer
        as
            declare function sub1 returns d1
            as
                declare x1 d2;
                declare x2 type of column t1.x1;
                declare v type of d1;
            begin
                execute procedure p1 returning_values v;
                return v;
            end
        begin
            return sub1();
        end^
        commit^

        select
            rdb$dependent_name
            ,rdb$depended_on_name
            ,rdb$field_name
            ,rdb$dependent_type
            ,rdb$depended_on_type
            ,rdb$package_name
        from rdb$dependencies
        where rdb$dependent_name = 'F3'
        order by rdb$dependent_name, rdb$depended_on_name, rdb$field_name^
        select 'point-080' as msg from rdb$database^

        --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

        grant select on table t1 to user {tmp_user.name}^

        create function f4 returns integer
        as
            declare function sub1 returns integer
            as
            begin
                insert into t1 values (2);
            end
        begin
            insert into t1 values (1);
            return sub1();
        end^

        grant all on table t1 to function f4^
        grant execute on function f4 to user {tmp_user.name}^

        create function f5 returns integer
        as
            declare function sub1 returns integer
            as
            begin
                update t1 set x1 = x1 * 10;
            end
        begin
            return sub1();
        end^

        grant execute on function f5 to user {tmp_user.name}^
        commit^
        select 'point-090' as msg from rdb$database^
        commit ^

        connect '{act.db.dsn}' user {tmp_user.name} password '{tmp_user.password}'^

        select f4() from rdb$database^
        select * from t1 order by x1^

        -- must raise:
        -- SQLSTATE 28000 / no permission for UPDATE access to TABLE T1 / Effective user is TMP_GTCS_NESTED_FUNC_USER
        select f5() from rdb$database^

        select * from t1 order by x1^
        select 'point-100' as msg from rdb$database^


        -- must raise: SQLSTATE 28000 / no permission for UPDATE access to TABLE T1 / Effective user is TMP_GTCS_NESTED_FUNC_USER
        execute block
        as
            declare function sub1 returns integer
            as
            begin
                update t1 set x1 = x1 * 10;
            end
        begin
            sub1();
        end^

        --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

        select f2(1) from rdb$database^
        
        -- must raise:
        -- SQLSTATE = 42000 / validation error for variable [number 1], value *** null *** / -At sub function súb1 / At function F2
        select f2(null) from rdb$database^
        select 'point-110' as msg from rdb$database^

        --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

        -- disabled: output unstable or depends on ODS.
        -- select rdb$function_blr from rdb$functions where rdb$function_name = 'F1'^
        -- select 'point-120' as msg from rdb$database^

        --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

        execute block returns (i integer, o integer)
        as
            -- Recursive function without forward declaration.
            declare function fibonacci(n integer) returns integer
            as
            begin
                if (n = 0 or n = 1) then
                    return n;
                else
                    return fibonacci(n - 1) + fibonacci(n - 2);
            end
        begin
            i = 0;

            while (i < 10)
            do
            begin
                o = fibonacci(i);
                suspend;
                i = i + 1;
            end
        end^
        select 'point-130' as msg from rdb$database^

        --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

        execute block returns (o integer)
        as
            -- Forward declaration with default parameters.
            declare function f2(n integer = 20) returns integer;

            declare function f1(n integer = 10) returns integer
            as
            begin
                return f2() + f2(n);
            end

            declare function f2(n integer) returns integer
            as
            begin
                return n;
            end

            -- Direct declaration with default parameters.
            declare function f3(n integer = 10) returns integer
            as
            begin
                return f2() + f2(n);
            end
        begin
            o = f1();
            suspend;

            o = f1(3);
            suspend;

            o = f2();
            suspend;

            o = f2(3);
            suspend;

            o = f3();
            suspend;

            o = f3(3);
            suspend;
        end^
        select 'point-140' as msg from rdb$database^

        --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

        -- Must raise:
        -- SQLSTATE 42000 / duplicate specification of F1 - not supported
        execute block
        as
            declare function f1() returns integer;

            -- Error: duplicate function F1
            declare function f1() returns integer;

            declare function f1() returns integer
            as
            begin
                return 2;
            end
        begin
            f1();
        end^
        select 'point-150' as msg from rdb$database^

        --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

        -- Must raise:
        -- SQLSTATE 42000 / Sub-function F1 was declared but not implemented
        execute block
        as
            -- Error: Sub-function F1 was declared but not implemented
            declare function f1(n integer = 1) returns integer;

            declare function f2() returns integer
            as
            begin
                return 2;
            end
        begin
            f2();

            -- Call a not-implemented function.
            f1();
        end^
        select 'point-160' as msg from rdb$database^

        --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

        -- Must raise:
        -- SQLSTATE 42000 / Sub-function F1 has a signature mismatch with its forward declaration
        execute block
        as
            declare function f1(n integer = 1) returns integer;

            -- Error: Sub-function F1 has a signature mismatch with its forward declaration
            declare function f1() returns integer
            as
            begin
                return 2;
            end
        begin
            f1();
        end^
        select 'point-170' as msg from rdb$database^

        --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

        -- Must raise:
        -- SQLSTATE 42000 / Default values for parameters are not allowed in definition of the previously declared sub-function F1
        execute block
        as
            declare function f1(n integer) returns integer;

            -- Error: Default values for parameters are not allowed in definition of the previously declared sub-function F1
            declare function f1(n integer = 1) returns integer
            as
            begin
                return 2;
            end
        begin
            f1();
        end^

        select 'point-999' as msg from rdb$database^
        set term ;^
    """
    
    expected_stdout = """
        O1 1
        MSG point-000
        F1 1
        MSG point-020
        O1 1
        O1 2
        MSG point-040
        F2 1
        Statement failed, SQLSTATE 42000
        validation error for variable [number 1], value *** null ***
        -At sub function s\xfab1 line: 9, col: 17
        At function F2 line: 12, col: 13
        MSG point-050
        Statement failed, SQLSTATE 42000
        duplicate specification of SUB1 - not supported
        MSG point-060
        Statement failed, SQLSTATE 0A000
        feature is not supported
        -nested sub function
        MSG point-070
        RDB$DEPENDENT_NAME F3
        RDB$DEPENDED_ON_NAME D1
        RDB$FIELD_NAME <null>
        RDB$DEPENDENT_TYPE 15
        RDB$DEPENDED_ON_TYPE 9
        RDB$PACKAGE_NAME <null>
        RDB$DEPENDENT_NAME F3
        RDB$DEPENDED_ON_NAME D2
        RDB$FIELD_NAME <null>
        RDB$DEPENDENT_TYPE 15
        RDB$DEPENDED_ON_TYPE 9
        RDB$PACKAGE_NAME <null>
        RDB$DEPENDENT_NAME F3
        RDB$DEPENDED_ON_NAME P1
        RDB$FIELD_NAME <null>
        RDB$DEPENDENT_TYPE 15
        RDB$DEPENDED_ON_TYPE 5
        RDB$PACKAGE_NAME <null>
        RDB$DEPENDENT_NAME F3
        RDB$DEPENDED_ON_NAME T1
        RDB$FIELD_NAME X1
        RDB$DEPENDENT_TYPE 15
        RDB$DEPENDED_ON_TYPE 0
        RDB$PACKAGE_NAME <null>
        MSG point-080
        MSG point-090
        F4 <null>
        X1 1
        X1 2
        Statement failed, SQLSTATE 28000
        no permission for UPDATE access to TABLE T1
        -Effective user is TMP_GTCS_NESTED_FUNC_USER
        X1 1
        X1 2
        MSG point-100
        Statement failed, SQLSTATE 28000
        no permission for UPDATE access to TABLE T1
        -Effective user is TMP_GTCS_NESTED_FUNC_USER
        F2 1
        Statement failed, SQLSTATE 42000
        validation error for variable [number 1], value *** null ***
        -At sub function s\xfab1 line: 9, col: 17
        At function F2 line: 12, col: 13
        MSG point-110
        I 0
        O 0
        I 1
        O 1
        I 2
        O 1
        I 3
        O 2
        I 4
        O 3
        I 5
        O 5
        I 6
        O 8
        I 7
        O 13
        I 8
        O 21
        I 9
        O 34
        MSG point-130
        O 30
        O 23
        O 20
        O 3
        O 30
        O 23
        MSG point-140
        Statement failed, SQLSTATE 42000
        duplicate specification of F1 - not supported
        MSG point-150
        Statement failed, SQLSTATE 42000
        Sub-function F1 was declared but not implemented
        MSG point-160
        Statement failed, SQLSTATE 42000
        Sub-function F1 has a signature mismatch with its forward declaration
        MSG point-170
        Statement failed, SQLSTATE 42000
        Default values for parameters are not allowed in definition of the previously declared sub-function F1
        MSG point-999
    """
    act.expected_stdout = expected_stdout
    act.isql(switches = ['-q'], input = test_script, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
