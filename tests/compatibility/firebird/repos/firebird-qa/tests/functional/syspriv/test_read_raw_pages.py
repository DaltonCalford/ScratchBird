#coding:utf-8

"""
ID:          syspriv.read-raw-pages
TITLE:       Check ability to get binary content of DB page by non-sysdba user who is
  granted with necessary system privilege
DESCRIPTION:
  Test uses ability to read binary content of DB page that is provided by FDB driver (see con.get_page_contents() call).
  We obtain content of page with ID=1 (this is PIP) and get its type (it must be 2).
  This action can be done by NON-dba user only if he has apropriate system privilege, otherwise FDB raises Python-related
  error. We catch this error in order to prevent failing of test with 'E' outcome and print text of exception.
FBTEST:      functional.syspriv.read_raw_pages
NOTES:
    [20.05.2022] pzotov
    Name of method to obtain raw data of page was changed in the firebird-driver: 'get_page_content()' // no tailing 's' after 't'
    Checked on 4.0.1.2692, 5.0.0.497.
"""

import pytest
from firebird.qa import *
from firebird.driver.core import DatabaseInfoProvider3 as dbnfo
from struct import unpack_from

db = db_factory()

tmp_hacker = user_factory('db', name = 'tmp_syspriv_raw_hacker', password = '123')
tmp_reader = user_factory('db', name = 'tmp_syspriv_raw_reader', password = '123')
tmp_role = role_factory('db', name='tmp_role_for_read_raw_pages')
act = python_act('db')

expected_stdout_isql = """
    WHO_AMI                         TMP_SYSPRIV_RAW_READER
    RDB$ROLE_NAME                   RDB$ADMIN
    RDB_ROLE_IN_USE                 <false>
    RDB$SYSTEM_PRIVILEGES           FFFFFFFFFFFFFFFF
    WHO_AMI                         TMP_SYSPRIV_RAW_READER
    RDB$ROLE_NAME                   TMP_ROLE_FOR_READ_RAW_PAGES
    RDB_ROLE_IN_USE                 <true>
    RDB$SYSTEM_PRIVILEGES           0400000000000000
"""

expected_stdout_main = """
    User: TMP_SYSPRIV_RAW_HACKER. Exception occured:
    InterfaceError('An error response was received')
    User: TMP_SYSPRIV_RAW_READER. Successfully get content of page, its type: 2
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action, tmp_hacker: User, tmp_reader: User, tmp_role: Role, capsys):
    init_script = f"""
        set wng off;
        set bail on;
        set list on;
        alter role {tmp_role.name} set system privileges to READ_RAW_PAGES;
        commit;
        grant default {tmp_role.name} to user {tmp_reader.name};
        commit;

        create or alter view v_check as
        select
             current_user as who_ami
            ,r.rdb$role_name
            ,rdb$role_in_use(r.rdb$role_name) as RDB_ROLE_IN_USE
            ,r.rdb$system_privileges
        from rdb$roles r
        order by r.rdb$role_name;
        commit;
        grant select on v_check to public;
        commit;

        connect '{act.db.dsn}' user {tmp_reader.name} password '{tmp_reader.password}';
        select * from v_check;
        commit;

    """
    act.isql(switches=['-q'], input=init_script, combine_output=True)
    act.expected_stdout = expected_stdout_isql
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    for u in (tmp_hacker, tmp_reader):
        with act.db.connect(user = u.name, password = u.password) as con:
            dbx = None
            try:
                dbx = dbnfo(con)
                page_buffer = dbx.get_page_content( 1 )
                (page_type,) = unpack_from('<b',page_buffer)
                print(f'User: {u.name}. Successfully get content of page, its type: %d' % page_type )
            except Exception as e:
                print(f'User: {u.name}. Exception occured:')
                print(e.__repr__())
            finally:
                if dbx:
                    dbx._close()

    act.expected_stdout = expected_stdout_main
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
