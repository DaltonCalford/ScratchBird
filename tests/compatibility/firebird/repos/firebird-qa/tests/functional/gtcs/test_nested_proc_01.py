#coding:utf-8

"""
ID:          n/a
TITLE:       Test of NESTED PROCEDURES functionality
DESCRIPTION:
    Original test see in:
    https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/FB_SQL_SUBPROC_1.script
NOTES:
    [21.09.2025] pzotov
    Firebird 3.x is not checked (issues "token unknown ';'").
    Checked on 6.0.0.1277 5.0.4.1713 4.0.7.3231
"""

import pytest
from firebird.qa import *

db = db_factory()

tmp_user: User = user_factory('db', name='tmp_gtcs_nested_proc_user', password='123')

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
            declare procedure sub1 returns (o1 integer)
            as
            begin
                o1 = 1;
                suspend;
                o1 = 2;
                suspend;
                o1 = 3;
                suspend;
            end
        begin
            execute procedure sub1 returning_values o1;
            suspend;

            for select o1 from sub1 into o1 do
                suspend;
        end^

        select * from p1^
        select 'point-000' as msg from rdb$database^

        --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

        execute block returns (o1 integer)
        as
            declare procedure sub1 returns (o1 integer)
            as
            begin
                o1 = 1;
                suspend;
                o1 = 2;
                suspend;
                o1 = 3;
                suspend;
            end
        begin
            execute procedure sub1 returning_values o1;
            suspend;

            for select o1 from sub1 x into o1 do
                suspend;
        end^
        select 'point-020' as msg from rdb$database^

        --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

        create function f1 returns integer
        as
            declare procedure sub1 returns (o1 integer)
            as
            begin
                o1 = 1;
                suspend;
                o1 = 2;
                suspend;
                o1 = 3;
                suspend;
            end

            declare o1 integer = 0;
            declare v integer;
        begin
            for select o1 from sub1 into v do
                o1 = o1 + v;

            return o1;
        end^

        select f1() from rdb$database^
        select 'point-040' as msg from rdb$database^

        --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

        execute block returns (o1 integer)
        as
            declare procedure sub1 (i1 integer, i2 integer = 2) returns (o1 integer)
            as
            begin
                o1 = i2;
            end
        begin
            execute procedure sub1 (0, 1) returning_values o1;
            suspend;

            execute procedure sub1 (0) returning_values o1;
            suspend;
        end^
        select 'point-050' as msg from rdb$database^

        --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

        create procedure p2 (i1 integer) returns ("ó1" integer)
        as
            declare procedure "súb1" (i1 integer) returns ("ó1" integer not null)
            as
            begin
                "ó1" = i1;
            end
        begin
            execute procedure "súb1" (i1) returning_values "ó1";
        end^

        grant execute on procedure p2 to user qa_user2^

        execute procedure p2 (1)^
        execute procedure p2 (null)^

        select 'point-060' as msg from rdb$database^

        --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

        execute block returns (o1 integer)
        as
            declare procedure sub1 (x integer) returns (x integer)
            as
            begin
            end
        begin
        end^
        select 'point-065' as msg from rdb$database^

        --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

        execute block returns (o1 integer)
        as
            declare procedure sub1
            as
            begin
            end

            declare procedure sub1
            as
            begin
            end
        begin
        end^
        select 'point-070' as msg from rdb$database^

        --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

        execute block returns (o1 integer)
        as
            declare procedure sub1
            as
                declare procedure sub11
                as
                begin
                end
            begin
            end
        begin
        end^
        select 'point-075' as msg from rdb$database^

        --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

        create domain d1 integer^
        create domain d2 integer^
        create domain d3 integer^

        create table t1 (x1 d3)^

        create procedure p3
        as
            declare procedure sub1 returns (o1 d1)
            as
                declare x1 d2;
                declare x2 type of column t1.x1;
            begin
                execute procedure p1 returning_values o1;
                suspend;
            end

            declare v integer;
        begin
            execute procedure sub1 returning_values v;
            select o1 from sub1 into v;
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
        where rdb$dependent_name = 'P3'
        order by rdb$dependent_name, rdb$depended_on_name, rdb$field_name^

        select 'point-080' as msg from rdb$database^
        --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

        grant select on table t1 to user qa_user2^

        create procedure p4
        as
            declare procedure sub1
            as
            begin
                insert into t1 values (2);
            end
        begin
            insert into t1 values (1);
            execute procedure sub1;
        end^

        grant all on table t1 to procedure p4^
        grant execute on procedure p4 to user qa_user2^

        create procedure p5
        as
            declare procedure sub1
            as
            begin
                update t1 set x1 = x1 * 10;
            end
        begin
            execute procedure sub1;
        end^

        grant execute on procedure p5 to user qa_user2^

        commit^
        select 'point-090' as msg from rdb$database^
        commit ^

        
        connect '{act.db.dsn}' user {tmp_user.name} password '{tmp_user.password}'^

        execute procedure p4^
        select * from t1 order by x1^

        execute procedure p5^
        select * from t1 order by x1^
        select 'point-100' as msg from rdb$database^

        execute block
        as
            declare procedure sub1
            as
            begin
                update t1 set x1 = x1 * 10;
            end
        begin
            execute procedure sub1;
        end^

        --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

        execute procedure p2 (1)^
        execute procedure p2 (null)^
        select 'point-110' as msg from rdb$database^

        --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
        -- disabled: output unstable or depends on ODS.
        -- select rdb$procedure_blr from rdb$procedures where rdb$procedure_name = 'P1'^
        -- select 'point-120' as msg from rdb$database^
        --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

        execute block returns (i integer, o integer)
        as
            -- Recursive procedure without forward declaration.
            declare procedure fibonacci(n integer) returns (o integer)
            as
                declare x integer;
            begin
                if (n = 0 or n = 1) then
                    o = n;
                else
                begin
                    execute procedure fibonacci(n - 1) returning_values x;
                    execute procedure fibonacci(n - 2) returning_values o;
                    o = o + x;
                end
            end
        begin
            i = 0;

            while (i < 10)
            do
            begin
                execute procedure fibonacci(i) returning_values o;
                suspend;
                i = i + 1;
            end
        end^
        select 'point-130' as msg from rdb$database^

        --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

        execute block returns (o integer)
        as
            -- Forward declaration with default parameters.
            declare procedure p2(n integer = 20) returns (o integer);

            declare procedure p1(n integer = 10) returns (o integer)
            as
            begin
                o = (select o from p2) + (select o from p2(:n));
                suspend;
            end

            declare procedure p2(n integer) returns (o integer)
            as
            begin
                o = n;
                suspend;
            end

            -- Direct declaration with default parameters.
            declare procedure p3(n integer = 10) returns (o integer)
            as
            begin
                o = (select o from p2) + (select o from p2(:n));
                suspend;
            end
        begin
            o = (select o from p1);
            suspend;

            o = (select o from p1(3));
            suspend;

            o = (select o from p2);
            suspend;

            o = (select o from p2(3));
            suspend;

            o = (select o from p3);
            suspend;

            o = (select o from p3(3));
            suspend;
        end^
        select 'point-140' as msg from rdb$database^

        --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

        -- Error: duplicate procedure P1
        execute block returns (o integer)
        as
            declare procedure p1() returns (o integer);

            declare procedure p1() returns (o integer);

            declare procedure p1() returns (o integer)
            as
            begin
                o = 2;
                suspend;
            end
        begin
            o = (select o from p1);
            suspend;
        end^
        select 'point-150' as msg from rdb$database^

        --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

        -- Error: Sub-procedure P1 was declared but not implemented
        execute block returns (o integer)
        as
            declare procedure p1(n integer = 1) returns (o integer);

            declare procedure p2() returns (o integer)
            as
            begin
                o = 2;
                suspend;
            end
        begin
            o = (select o from p2);
            suspend;

            -- Call a not-implemented procedure.
            o = (select o from p1);
            suspend;
        end^
        select 'point-160' as msg from rdb$database^

        --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

        execute block returns (o integer)
        as
            declare procedure p1(n integer = 1) returns (o integer);

            -- Error: Sub-procedure P1 has a signature mismatch with its forward declaration
            declare procedure p1() returns (o integer)
            as
            begin
                return 2;
            end
        begin
            o = (select o from p1);
            suspend;
        end^
        select 'point-170' as msg from rdb$database^

        --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

        execute block returns (o integer)
        as
            declare procedure f1(n integer) returns (o integer);

            -- Error: Default values for parameters are not allowed in definition of the previously declared sub-procedure P1
            declare procedure f1(n integer = 1) returns (o integer)
            as
            begin
                return 2;
            end
        begin
            o = (select o from p1);
            suspend;
        end^

        select 'point-999' as msg from rdb$database^
        set term ;^
    """
    
    expected_stdout = """
        O1 1
        O1 1
        O1 2
        O1 3
        MSG point-000
        O1 1
        O1 1
        O1 2
        O1 3
        MSG point-020
        F1 6
        MSG point-040
        O1 1
        O1 2
        MSG point-050
        \xf31 1
        Statement failed, SQLSTATE 42000
        validation error for variable \xf31, value *** null ***
        -At sub procedure s\xfab1
        At procedure P2
        MSG point-060
        Statement failed, SQLSTATE 42000
        Dynamic SQL Error
        -SQL error code -637
        -duplicate specification of X - not supported
        MSG point-065
        Statement failed, SQLSTATE 42000
        duplicate specification of SUB1 - not supported
        MSG point-070
        Statement failed, SQLSTATE 0A000
        feature is not supported
        -nested sub procedure
        MSG point-075
        RDB$DEPENDENT_NAME P3
        RDB$DEPENDED_ON_NAME D1
        RDB$FIELD_NAME <null>
        RDB$DEPENDENT_TYPE 5
        RDB$DEPENDED_ON_TYPE 9
        RDB$PACKAGE_NAME <null>
        RDB$DEPENDENT_NAME P3
        RDB$DEPENDED_ON_NAME D2
        RDB$FIELD_NAME <null>
        RDB$DEPENDENT_TYPE 5
        RDB$DEPENDED_ON_TYPE 9
        RDB$PACKAGE_NAME <null>
        RDB$DEPENDENT_NAME P3
        RDB$DEPENDED_ON_NAME P1
        RDB$FIELD_NAME <null>
        RDB$DEPENDENT_TYPE 5
        RDB$DEPENDED_ON_TYPE 5
        RDB$PACKAGE_NAME <null>
        RDB$DEPENDENT_NAME P3
        RDB$DEPENDED_ON_NAME T1
        RDB$FIELD_NAME X1
        RDB$DEPENDENT_TYPE 5
        RDB$DEPENDED_ON_TYPE 0
        RDB$PACKAGE_NAME <null>
        MSG point-080
        MSG point-090
        Statement failed, SQLSTATE 28000
        no permission for EXECUTE access to PROCEDURE P4
        -Effective user is TMP_GTCS_NESTED_PROC_USER
        Statement failed, SQLSTATE 28000
        no permission for SELECT access to TABLE T1
        -Effective user is TMP_GTCS_NESTED_PROC_USER
        Statement failed, SQLSTATE 28000
        no permission for SELECT access to TABLE T1
        -Effective user is TMP_GTCS_NESTED_PROC_USER
        Statement failed, SQLSTATE 28000
        no permission for SELECT access to TABLE T1
        -Effective user is TMP_GTCS_NESTED_PROC_USER
        MSG point-100
        Statement failed, SQLSTATE 28000
        no permission for SELECT access to TABLE T1
        -Effective user is TMP_GTCS_NESTED_PROC_USER
        Statement failed, SQLSTATE 28000
        no permission for EXECUTE access to PROCEDURE P2
        -Effective user is TMP_GTCS_NESTED_PROC_USER
        Statement failed, SQLSTATE 28000
        no permission for EXECUTE access to PROCEDURE P2
        -Effective user is TMP_GTCS_NESTED_PROC_USER
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
        duplicate specification of P1 - not supported
        MSG point-150
        Statement failed, SQLSTATE 42000
        Sub-procedure P1 was declared but not implemented
        MSG point-160
        Statement failed, SQLSTATE 42000
        Sub-procedure P1 has a signature mismatch with its forward declaration
        MSG point-170
        Statement failed, SQLSTATE 42000
        Default values for parameters are not allowed in definition of the previously declared sub-procedure F1
        MSG point-999
    """
    act.expected_stdout = expected_stdout
    act.isql(switches = ['-q'], input = test_script, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
