#coding:utf-8

"""
ID:          issue-7818
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7818
TITLE:       Extend rdb$get_context('SYSTEM', '***') with client-related info
DESCRIPTION:
    Test assumes that QA plugin uses client library from the folder where testing FB instance was deployed,
    thus content of con.info.firebird_version is exactly the same as version of client library.
    This also causes value of CLIENT_OS_USER context variable to be equal to getpass.getuser().
NOTES:
    [01.11.2023] pzotov
    Checked on 6.0.0.104, 5.0.0.1259, 4.0.4.3009

    [09.05.2024] pzotov
    In firebird-driver 1.10.4+ obtaining version via 'con.info.firebird_version' returns
    MULTI-LINED data, i.e. server plus network listener plus client info, like this is done
    by 'show version' command, e.g.:
    ========
    WI-T6.0.0.348 Firebird 6.0 Initial
    WI-T6.0.0.348 Firebird 6.0 Initial/tcp (HOME-AUX2)/P19:C
    WI-T6.0.0.348 Firebird 6.0 Initial/tcp (HOME-AUX2)/P19:C
    ========
    Because of this, we have to take in account only first line of this data (split text using os.linesep).
    See letter from pcisar:
        subj: "fb_info_crypt_key: how it can be obtained using firebird-driver ? // GH-5978, 2018",
        date: 07-may-2024 13:59.
"""
import getpass
import pytest
from firebird.qa import *

db = db_factory()

#act = isql_act('db', test_script)
act = python_act('db')

@pytest.mark.version('>=4.0')
def test_1(act: Action, capsys):
    os_user = getpass.getuser()
    with act.db.connect() as con:
        fb_vers = con.info.firebird_version.split('\n')[0]
        cur = con.cursor()

        test_sql = f"""
            select
                iif( lower(system_context_client_os_user) = lower(chk_os_user)
                     ,'OK.'
                     ,q'#Unexpected: rdb$get_context('SYSTEM', 'CLIENT_OS_USER')=#'
                      || coalesce(system_context_client_os_user, '[null]')
                      || ', chk_os_user='
                      || coalesce(chk_os_user, '[null]')
                   ) as client_os_user_check
                ,
                iif( system_context_client_version = chk_fb_vers
                     ,'OK.'
                     ,q'#Unexpected: rdb$get_context('SYSTEM', 'CLIENT_VERSION')=#'
                      || coalesce(system_context_client_version, '[null]')
                      || ', chk_fb_vers='
                      || coalesce(chk_fb_vers, '[null]')
                   ) as client_version_check
            from (
                select
                     rdb$get_context('SYSTEM', 'CLIENT_OS_USER') as system_context_client_os_user
                    ,rdb$get_context('SYSTEM', 'CLIENT_VERSION') as system_context_client_version
                    ,q'*{os_user}*' as chk_os_user
                    ,q'*{fb_vers}*' as chk_fb_vers
                from rdb$database
            );
        """

        for r in cur.execute(test_sql):
            for i,col in enumerate(cur.description):
                print((col[0] +':').ljust(32), r[i])

    expected_stdout = """
        CLIENT_OS_USER_CHECK:            OK.
        CLIENT_VERSION_CHECK:            OK.
    """

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
