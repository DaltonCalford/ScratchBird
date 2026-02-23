#coding:utf-8

"""
ID:          issue-827
ISSUE:       827
TITLE:       Grants overwrite previous rdb$security_classes entries
DESCRIPTION:
  Test attempts to create all kinds of objects (<K>) that can be 'target' for GRANT ON <K> statement.
  Length of  each object is equal to implementation maximum for 2.5.x, 3.0.x and 4.0.
  Pairs of objects differ only in last character.
  After all, we check that no dupicates are created in rdb$security_classes table for field rdb$security_class.
  NOTE-1: for 3.0.x and 4.0 we create objects as quoted, in UTF8, - for additional checking
          that we have no problem with non-ascii characters.
  NOTE-2: max length in 4.0 is 63 utf8 CHARACTERS (not bytes).
JIRA:        CORE-479
FBTEST:      bugs.core_0479
"""

import pytest
from firebird.qa import *

db = db_factory(charset='utf8')

# version: 3.0

test_script_1 = """
    set bail on;
    create or alter view v_check as
    select sc.rdb$security_class, r.obj_type, min(r.obj_name) as obj_1, max(r.obj_name) as obj_2
    from rdb$security_classes sc
    left join
    (

        select r.rdb$relation_name as obj_name, r.rdb$security_class as sec_class, 'table/view' as obj_type
        from rdb$relations r

        union all

        select p.rdb$procedure_name, p.rdb$security_class, 'stored proc'
        from rdb$procedures p

        union all

        select p.rdb$function_name, p.rdb$security_class, 'stored func'
        from rdb$functions p

        union all

        select p.rdb$package_name, p.rdb$security_class, 'package'
        from rdb$packages p

        union all

        select r.rdb$role_name as obj_name, r.rdb$security_class as sec_class, 'role' as obj_type
        from rdb$roles r

    ) r on sc.rdb$security_class = r.sec_class
    group by 1,2
    having count(*) > 1
    ;
    -----------------------------------------------------------------
    -- tÁÃÀÅĂÂÄĀČĒĻŅŠŪŽĪáéíóúýàèìòùâêŹ
    -- tÁÃÀÅĂÂÄĀČĒĻŅŠŪŽĪáéíóúýàèìòùâêŻ

    recreate table
    "tÁÃÀÅĂÂÄĀČĒĻŅŠŪŽ"
    (x int)
    ;

    recreate table
    "TÁÃÀÅĂÂÄĀČĒĻŅŠŪŽ"
    (x int)
    ;
    -----------------------------------------------------------------
    create view
    "vÁÃÀÅĂÂÄĀČĒĻŅŠŪŽ"
    as select * from
    "tÁÃÀÅĂÂÄĀČĒĻŅŠŪŽ"
    ;

    create view
    "VÁÃÀÅĂÂÄĀČĒĻŅŠŪŽ"
    as select * from
    "TÁÃÀÅĂÂÄĀČĒĻŅŠŪŽ"
    ;
    -----------------------------------------------------------------

    create procedure
    "pÁÃÀÅĂÂÄĀČĒĻŅŠŪŽ"
    as begin
    end;

    create procedure
    "PÁÃÀÅĂÂÄĀČĒĻŅŠŪŽ"
    as begin
    end;
    -----------------------------------------------------------------

    set term ^;
    create function
    "fÁÃÀÅĂÂÄĀČĒĻŅŠŪŽ"
    returns int
    as begin
        return 1;
    end
    ^
    create function
    "FÁÃÀÅĂÂÄĀČĒĻŅŠŪŽ"
    returns int
    as begin
        return 1;
    end
    ^
    -----------------------------------------------------------------

    create package
    "gÁÃÀÅĂÂÄĀČĒĻŅŠŪŽ"
    as begin end
    ^
    create package
    "GÁÃÀÅĂÂÄĀČĒĻŅŠŪŽ"
    as begin end
    ^
    set term ;^
    -----------------------------------------------------------------

    create role
    "rÁÃÀÅĂÂÄĀČĒĻŅŠŪŽ"
    ;

    create role
    "RÁÃÀÅĂÂÄĀČĒĻŅŠŪŽ"
    ;

    commit;

    SET LIST ON;
    SET COUNT ON;

    select * from v_check;
"""

act_1 = isql_act('db', test_script_1)

expected_stdout_1 = """
    Records affected: 0
"""

@pytest.mark.version('>=3.0,<4.0')
def test_1(act_1: Action):
    act_1.expected_stdout = expected_stdout_1
    act_1.execute()
    assert act_1.clean_stdout == act_1.clean_expected_stdout

# version: 4.0

test_script_2 = """
    set bail on;
    create or alter view v_check as
    select sc.rdb$security_class, r.obj_type, min(r.obj_name) as obj_1, max(r.obj_name) as obj_2
    from rdb$security_classes sc
    left join
    (

        select r.rdb$relation_name as obj_name, r.rdb$security_class as sec_class, 'table/view' as obj_type
        from rdb$relations r

        union all

        select p.rdb$procedure_name, p.rdb$security_class, 'stored proc'
        from rdb$procedures p

        union all

        select p.rdb$function_name, p.rdb$security_class, 'stored func'
        from rdb$functions p

        union all

        select p.rdb$package_name, p.rdb$security_class, 'package'
        from rdb$packages p

        union all

        select r.rdb$role_name as obj_name, r.rdb$security_class as sec_class, 'role' as obj_type
        from rdb$roles r

    ) r on sc.rdb$security_class = r.sec_class
    group by 1,2
    having count(*) > 1
    ;
    -----------------------------------------------------------------
    recreate table
    "tÁÃÀÅĂÂÄĀČĒĻŅŠŪŽĪáéíóúýàèìòùâêîôûãñõäëïöüÿçšδθλξσψωąęłźżњћџăşţŹ"
    (x int)
    ;

    recreate table
    "tÁÃÀÅĂÂÄĀČĒĻŅŠŪŽĪáéíóúýàèìòùâêîôûãñõäëïöüÿçšδθλξσψωąęłźżњћџăşţŻ"
    (x int)
    ;
    -----------------------------------------------------------------
    recreate view
    "vÁÃÀÅĂÂÄĀČĒĻŅŠŪŽĪáéíóúýàèìòùâêîôûãñõäëïöüÿçšδθλξσψωąęłźżњћџăşţŹ"
    as select * from
    "tÁÃÀÅĂÂÄĀČĒĻŅŠŪŽĪáéíóúýàèìòùâêîôûãñõäëïöüÿçšδθλξσψωąęłźżњћџăşţŹ"
    ;

    recreate view
    "vÁÃÀÅĂÂÄĀČĒĻŅŠŪŽĪáéíóúýàèìòùâêîôûãñõäëïöüÿçšδθλξσψωąęłźżњћџăşţŻ"
    as select * from
    "tÁÃÀÅĂÂÄĀČĒĻŅŠŪŽĪáéíóúýàèìòùâêîôûãñõäëïöüÿçšδθλξσψωąęłźżњћџăşţŻ"
    ;
    -----------------------------------------------------------------

    create procedure
    "pÁÃÀÅĂÂÄĀČĒĻŅŠŪŽĪáéíóúýàèìòùâêîôûãñõäëïöüÿçšδθλξσψωąęłźżњћџăşţŹ"
    as begin
    end;

    create procedure
    "pÁÃÀÅĂÂÄĀČĒĻŅŠŪŽĪáéíóúýàèìòùâêîôûãñõäëïöüÿçšδθλξσψωąęłźżњћџăşţŻ"
    as begin
    end;
    -----------------------------------------------------------------

    set term ^;
    create function
    "fÁÃÀÅĂÂÄĀČĒĻŅŠŪŽĪáéíóúýàèìòùâêîôûãñõäëïöüÿçšδθλξσψωąęłźżњћџăşţŹ"
    returns int
    as begin
        return 1;
    end
    ^
    create function
    "fÁÃÀÅĂÂÄĀČĒĻŅŠŪŽĪáéíóúýàèìòùâêîôûãñõäëïöüÿçšδθλξσψωąęłźżњћџăşţŻ"
    returns int
    as begin
        return 1;
    end
    ^
    -----------------------------------------------------------------

    create package
    "gÁÃÀÅĂÂÄĀČĒĻŅŠŪŽĪáéíóúýàèìòùâêîôûãñõäëïöüÿçšδθλξσψωąęłźżњћџăşţŹ"
    as begin end
    ^
    create package
    "gÁÃÀÅĂÂÄĀČĒĻŅŠŪŽĪáéíóúýàèìòùâêîôûãñõäëïöüÿçšδθλξσψωąęłźżњћџăşţŻ"
    as begin end
    ^
    set term ;^
    -----------------------------------------------------------------

    create role
    "rÁÃÀÅĂÂÄĀČĒĻŅŠŪŽĪáéíóúýàèìòùâêîôûãñõäëïöüÿçšδθλξσψωąęłźżњћџăşţŹ"
    ;

    create role
    "rÁÃÀÅĂÂÄĀČĒĻŅŠŪŽĪáéíóúýàèìòùâêîôûãñõäëïöüÿçšδθλξσψωąęłźżњћџăşţŻ"
    ;

    commit;

    SET LIST ON;
    SET COUNT ON;

    select * from v_check;
"""

act_2 = isql_act('db', test_script_2)

expected_stdout_2 = """
    Records affected: 0
"""

@pytest.mark.intl
@pytest.mark.version('>=4.0')
def test_2(act_2: Action):
    act_2.expected_stdout = expected_stdout_2
    act_2.execute()
    assert act_2.clean_stdout == act_2.clean_expected_stdout

