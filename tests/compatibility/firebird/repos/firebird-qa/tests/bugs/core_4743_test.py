#coding:utf-8

"""
ID:          issue-5048
ISSUE:       5048
TITLE:       Granted role does not work with non-ascii username
DESCRIPTION:
NOTES:
[09.02.2022] pcisar
  On Windows the act.db.connect() fails with "Your user name and password are not defined."
[08.04.2022] pzotov
  One need to specify utf8filename=True in db_factory() call if we want to establish connection as "non-ascii user".
  Specifying of this parameter in firebird-driver.conf (in the servger section) has no effect.
  Checked on 4.0.1 Release, 5.0.0.467.
  See also:
  email discusion, subject: "firebird-qa [new framework]: unable to make connection as NON-ASCII user, only on Windows (WI-V4.0.1.2692)",
  message from pcisar 08-mar-2022 13:52 ('utf8filename' parameter was added to db_factory()).

JIRA:        CORE-4743
FBTEST:      bugs.core_4743
"""

import pytest
import platform
from firebird.qa import *

db = db_factory(utf8filename=True)
non_acii_user = user_factory('db', name='"Вася Пупкин"', password= '123')
test_role = role_factory('db', name='"Старший дворник"')

act = python_act('db', substitutions=[('[\t ]+', ' ')])

ddl_script = """
    grant "Старший дворник" to "Вася Пупкин";
    commit;

    create table "Документы"(id int primary key, pid int references "Документы");
    create  exception "НЕ_число" 'Ваша строка не может быть преобразована в число.';
    create sequence "ИД_документа";
    set term ^;
    create procedure "Хранимка" as
    begin
    end
    ^
    create function "СтрВЧисло"(a_text varchar(100)) returns int as
    begin
        return 0;
    end
    ^

    create or alter package "Утилиты" as
    begin
        procedure pg_sp_worker;
    end
    ^
    recreate package body "Утилиты" as
    begin
        procedure pg_sp_worker as
        begin
        end
    end
    ^
    set term ;^
    commit;

    create or alter view v_current_privileges as
    select
         g.rdb$user as who_is_granted
        ,g.rdb$relation_name as obj_name
        ,decode( g.rdb$object_type
                 ,0,'table'
                 ,1,'view'
                 ,2,'trigger'
                 ,5,'procedure'
                 ,7,'exception'
                 ,9,'domain'
                 ,11,'charset'
                 ,13,'role'
                 ,14,'generator'
                 ,15,'function'
                 ,16,'blob filt'
                 ,18,'package'
                 ,22,'systable'
                 ,cast(g.rdb$object_type as varchar(50))
               ) as obj_type
        ,max(iif(g.rdb$privilege='S','YES',' ')) as "privilege:select"
        ,max(iif(g.rdb$privilege='I','YES',' ')) as "privilege:insert"
        ,max(iif(g.rdb$privilege='U','YES',' ')) as "privilege:update"
        ,max(iif(g.rdb$privilege='D','YES',' ')) as "privilege:delete"
        ,max(iif(g.rdb$privilege='G','YES',' ')) as "privilege:usage"
        ,max(iif(g.rdb$privilege='X','YES',' ')) as "privilege:exec"
        ,max(iif(g.rdb$privilege='R','YES',' ')) as "privilege:refer"
        ,max(iif(g.rdb$privilege='C','YES',' ')) as "privilege:create"
        ,max(iif(g.rdb$privilege='L','YES',' ')) as "privilege:alter"
        ,max(iif(g.rdb$privilege='O','YES',' ')) as "privilege:drop"
        ,max(iif(g.rdb$privilege='M','YES',' ')) as "privilege:member"
    from rdb$user_privileges g
    where g.rdb$user in( current_user, current_role )
    group by 1,2,3;

    grant select on v_current_privileges to "Старший дворник";
    grant select,insert,update,delete,references on "Документы" to "Старший дворник";
    grant usage on exception "НЕ_число" to "Старший дворник";
    grant usage on sequence "ИД_документа" to "Старший дворник";
    grant execute on procedure "Хранимка" to "Старший дворник";
    grant execute on function "СтрВЧисло" to "Старший дворник";
    grant execute on package "Утилиты" to "Старший дворник";
    grant create table to "Старший дворник";
    grant alter any table to "Старший дворник";
    grant drop any table to "Старший дворник";
    commit;
"""

expected_stdout = """
    MON$USER : Вася Пупкин
    MON$ROLE : Старший дворник

    WHO_IS_GRANTED : Вася Пупкин
    OBJ_NAME : Старший дворник
    OBJ_TYPE : role
    privilege:member : YES

    WHO_IS_GRANTED : Старший дворник
    OBJ_NAME : SQL$TABLES
    OBJ_TYPE : systable
    privilege:create : YES
    privilege:alter : YES
    privilege:drop : YES

    WHO_IS_GRANTED : Старший дворник
    OBJ_NAME : V_CURRENT_PRIVILEGES
    OBJ_TYPE : table
    privilege:select : YES

    WHO_IS_GRANTED : Старший дворник
    OBJ_NAME : Документы
    OBJ_TYPE : table
    privilege:select : YES
    privilege:insert : YES
    privilege:update : YES
    privilege:delete : YES
    privilege:refer : YES

    WHO_IS_GRANTED : Старший дворник
    OBJ_NAME : ИД_документа
    OBJ_TYPE : generator
    privilege:usage : YES

    WHO_IS_GRANTED : Старший дворник
    OBJ_NAME : НЕ_число
    OBJ_TYPE : exception
    privilege:usage : YES

    WHO_IS_GRANTED : Старший дворник
    OBJ_NAME : СтрВЧисло
    OBJ_TYPE : function
    privilege:exec : YES

    WHO_IS_GRANTED : Старший дворник
    OBJ_NAME : Утилиты
    OBJ_TYPE : package
    privilege:exec : YES

    WHO_IS_GRANTED : Старший дворник
    OBJ_NAME : Хранимка
    OBJ_TYPE : procedure
    privilege:exec : YES
"""

@pytest.mark.intl
@pytest.mark.version('>=4.0')
def test_1(act: Action, non_acii_user: User, test_role: Role, capsys):
    act.isql(switches=['-b', '-q'], input=ddl_script)
    print(act.stdout)
    with act.db.connect(user=non_acii_user.name, password=non_acii_user.password, role=test_role.name) as con:
        cur = con.cursor()
        cur.execute('select m.mon$user,m.mon$role from mon$attachments m where m.mon$attachment_id = current_connection')
        col = cur.description
        for r in cur:
            for i in range(len(col)):
                print(' '.join((col[i][0], ':', r[i])))
        cur.execute("select v.* from v_current_privileges v")
        col = cur.description
        for r in cur:
            for i in range(len(col)):
                if 'privilege:' not in col[i][0] or 'privilege:' in col[i][0] and r[i] == 'YES':
                    print(' '.join((col[i][0], ':', r[i])))
    #
    act.reset()
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
