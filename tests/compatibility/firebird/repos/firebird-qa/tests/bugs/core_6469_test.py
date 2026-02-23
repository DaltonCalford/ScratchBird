#coding:utf-8

"""
ID:          issue-2376
ISSUE:       2376
TITLE:       Provide ability to see in the trace log actions related to session management (e.g. ALTER SESSION RESET)
DESCRIPTION:
    Test verifies management statements which are specified in doc/sql.extensions/README.management_statements_psql.md
    We launch trace session before ISQL and stop it after its finish.
    Every management statement is expected to be found in the trace log.
NOTES:
    [08.04.2022] pzotov
        ATTENTION: TWO SEPARATE BRANCHES present in this test for different OS.
        1. NOTES FOR WINDOWS
        ####################
          Statement 'SET TRUSTED ROLE' is verified for appearance in the trace log.
          There are several prerequisites that must be met for check SET TRUSTED ROLE statement:
              * BOTH AuthServer and AuthClient parameters from firebird.conf contain 'Win_Sspi' as plugin, in any place;
              * current OS user has admin rights, otherwise we get "SQLSTATE = 0P000 / Your attachment has no trusted role"
              * OS environment has *no* variables ISC_USER and ISC_PASSWORD (i.e. they must be UNSET);
              * Two mappings are created (both uses plugin win_sspi):
              ** from any user to user;
              ** from predefined_group domain_any_rid_admins to role <role_to_be_trusted>

          Connect to database should be done in form: CONNECT '<computername>:<our_database>' role <role_to_be_trusted>',
          and after this we can user 'SET TRUSTED ROLE' statement (see also: core_5887-trusted_role.fbt).

          ::: NOTE :::
          We have to remove OS-veriable 'ISC_USER' before any check of trusted role.
          This variable could be set by other .fbts which was performed before current within batch mode (i.e. when fbt_run is called from <rundaily>)

        2. NOTES FOR LINUX
        ##################
          Trusted role is not verified for this case.
          Weird behaviour detected when test was ran on FB 4.0.0.2377 SuperServer: if we run this test several times (e.g. in loop) then *all*
          statements related to session management can be missed in the trace - despite the fact that they *for sure* was performed successfully
          (this can be seen in ISQL log). It seems that fail somehow related to the duration of DELAY between subsequent runs: if delay more than ~30s
          then almost no fails. But if delay is small then test can fail for almost every run.
          NO such trouble in the Classic.
          The reason currently (03-mar-2021) remains unknown.
          Sent letter to Alex et al, 03-mar-2021.
        
        [WINDOWS]
        1. The 'CONNECT ...' operator, being specified without USER/PASSWORD clauses, will take in account parameters that were specified in the command line
           of ISQL (confirmed by Alex, letter 03.04.2022 20:31).
           This means that it will use 'SYSDBA' / 'masterkey' rather than Windows trusted auth. This, in turn, leads that SYSDBA will be current user
           when following is performed:
               connect '{THIS_COMPUTER_NAME}:{act.db.db_path}' role tmp$r6469;
           - and it causes 'set trusted role' to fail (SQLSTATE = 0P000 / Your attachment has no trusted role).
           Because of this, we have to launch ISQL without using current credentials (which is True by default) - see 'credentials = False'.
        2. One need to run ISQL with requirement do NOT establish connection to the test database because this will be done in the test script itself.
           Otherwise we get 'Missing security context' *after* test finishes (the reason is unknown; instead, "Rolling back work." must be issued and redirected to STDERR).
           To prevent such error, we have to specify 'connect_db = False' in db_factory() call.
    Checked on 4.0.1 Release, 5.0.0.467.

    [02.08.2024] pzotov
        One need to check for admin rights of current OS user (noted by Dimitry Sibiryakov).
        ISQL output must be checked for matching to expected before trace log (see func run_script()).
        Replaced hard-coded name of role with 'f{tmp_role.name}' notation.
    
    Checked on Windows 6.0.0.406, 5.0.1.1469, 4.0.5.3139

JIRA:        CORE-6469
FBTEST:      bugs.core_6469
"""

import os
import ctypes
import pytest
import re
import socket
import getpass
from pathlib import Path
from firebird.qa import *
import time

try:
    del os.environ["ISC_USER"]
except KeyError as e:
    pass

db = db_factory()

act = python_act('db', substitutions = [('[ \t]+', ' '), ])

tmp_role = role_factory('db', name='TMP$R6469')
tmp_file = temp_file('c6469_tmp.sql')

################################
###       W I N D O W S      ###
################################

# version: 4.0 - Windows

trace_win = ['log_initfini = false',
             'log_statement_finish = true',
             'log_errors = true',
             'time_threshold = 0',
             ]

patterns_win =  [re.compile('alter session reset', re.IGNORECASE),
                 re.compile('set session idle timeout', re.IGNORECASE),
                 re.compile('set statement timeout', re.IGNORECASE),
                 re.compile('set bind of decfloat to double precision', re.IGNORECASE),
                 re.compile('set decfloat round ceiling', re.IGNORECASE),
                 re.compile('set decfloat traps to Division_by_zero', re.IGNORECASE),
                 re.compile('set time zone', re.IGNORECASE),
                 re.compile('set role', re.IGNORECASE),
                 re.compile('set trusted role', re.IGNORECASE)]

#----------------------------------------------------------

def is_admin():
    # https://serverfault.com/questions/29659/crossplatform-way-to-check-admin-rights-in-python-script
    # Checked on Windows 10.
    try:
        is_admin = os.getuid() == 0
    except AttributeError:
        is_admin = ctypes.windll.shell32.IsUserAnAdmin()

    return is_admin

#----------------------------------------------------------

def run_script(act: Action, tmp_role: Role, tmp_file: Path):
    #__tracebackhide__ = True
    THIS_COMPUTER_NAME = socket.gethostname()
    CURRENT_WIN_ADMIN = getpass.getuser()
    script = f"""
        set bail on;
        set list on;
        -- set echo on;
        connect '{act.db.dsn}' user '{act.db.user}' password '{act.db.password}';
        grant {tmp_role.name} to "{THIS_COMPUTER_NAME}\\{CURRENT_WIN_ADMIN}";
        commit;

        -- We have to use here "create mapping trusted_auth ... from any user to user" otherwise get
        -- Statement failed, SQLSTATE = 28000 /Missing security context for C:\\FBTESTING\\QA\\MISC\\C5887.FDB
        -- on connect statement which specifies COMPUTERNAME:USERNAME instead path to DB:
        create or alter mapping trusted_auth using plugin win_sspi from any user to user;

        -- We have to use here "create mapping win_admins ... DOMAIN_ANY_RID_ADMINS" otherwise get
        -- Statement failed, SQLSTATE = 0P000 / Your attachment has no trusted role
        create or alter mapping win_admins using plugin win_sspi from predefined_group domain_any_rid_admins to role {tmp_role.name};
        commit;

        -- We have to GRANT ROLE, even to SYSDBA. Otherwise:
        -- Statement failed, SQLSTATE = 0P000
        -- Role ... is invalid or unavailable
        grant {tmp_role.name} to sysdba;
        commit;
        --show role;
        --show grants;
        --show mapping;

        set autoddl off;
        commit;

        -- Following management statements are taken from
        -- doc/sql.extensions/README.management_statements_psql.md:
        -- ########################################################
        alter session reset;
        set session idle timeout 1800 second;
        set statement timeout 190 second;
        set bind of decfloat to double precision;
        set decfloat round ceiling;
        set decfloat traps to Division_by_zero;
        set time zone 'America/Sao_Paulo';
        set role {tmp_role.name};
        commit;

        connect '{THIS_COMPUTER_NAME}:{act.db.db_path}' role {tmp_role.name};

        select upper(mon$user) as mon_user, upper(mon$role) as mon_role, mon$auth_method as mon_auth
        from mon$attachments
        where mon$attachment_id = current_connection;
        commit;

        set trusted role;
        commit;

        connect '{act.db.dsn}' user {act.db.user} password '{act.db.password}';
        drop mapping trusted_auth;
        drop mapping win_admins;
        commit;
    """
    tmp_file.write_text(script)

    act.expected_stdout = f"""
        MON_USER {THIS_COMPUTER_NAME.upper()}\\{CURRENT_WIN_ADMIN.upper()}
        MON_ROLE {tmp_role.name.upper()}
        MON_AUTH Mapped from Win_Sspi
    """

    act.isql(switches=['-n', '-q'], input_file = tmp_file, connect_db = False, credentials = False, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

#----------------------------------------------------------

@pytest.mark.trace
@pytest.mark.version('>=4.0')
@pytest.mark.platform('Windows')
def test_1(act: Action, tmp_role: Role, tmp_file: Path,  capsys):

    if not is_admin():
        pytest.skip("Current OS user must have admin rights.")

    with act.trace(db_events=trace_win):
        run_script(act, tmp_role, tmp_file)

    # process trace
    for line in act.trace_log:
        if line.split():
            if act.match_any(line, patterns_win):
                print(' '.join(line.split()).lower())

    expected_stdout_win = f"""
        alter session reset
        set session idle timeout 1800 second
        set statement timeout 190 second
        set bind of decfloat to double precision
        set decfloat round ceiling
        set decfloat traps to division_by_zero
        set time zone 'america/sao_paulo'
        set role {tmp_role.name.lower()}
        set trusted role
    """
    
    act.expected_stdout = expected_stdout_win
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout



################################
###         L I N U X        ###
################################
# version: 4.0 - Linux

trace_lin = ['log_initfini = false',
             'log_connections = true',
             'log_statement_finish = true',
             'log_errors = true',
             'time_threshold = 0',
             ]

patterns_lin =  [re.compile('alter session reset', re.IGNORECASE),
                 re.compile('set session idle timeout', re.IGNORECASE),
                 re.compile('set statement timeout', re.IGNORECASE),
                 re.compile('set bind of decfloat to double precision', re.IGNORECASE),
                 re.compile('set decfloat round ceiling', re.IGNORECASE),
                 re.compile('set decfloat traps to Division_by_zero', re.IGNORECASE),
                 re.compile('set time zone', re.IGNORECASE),
                 re.compile('set role', re.IGNORECASE)]

@pytest.mark.version('>=4.0')
@pytest.mark.platform('Linux')
def test_2(act: Action, tmp_role: Role, capsys):

    expected_stdout_nix = f"""
        alter session reset
        set session idle timeout 1800 second
        set statement timeout 190 second
        set bind of decfloat to double precision
        set decfloat round ceiling
        set decfloat traps to division_by_zero
        set time zone 'america/sao_paulo'
        set role {tmp_role.name.lower()}
    """

    test_script_nix = f"""
        set bail on;
        set list on;

        -- We have to GRANT ROLE, even to SYSDBA. Otherwise:
        -- Statement failed, SQLSTATE = 0P000
        -- Role ... is invalid or unavailable
        grant {tmp_role.name} to sysdba;
        commit;

        select current_user as who_ami, current_role as whats_my_role from rdb$database;
        set autoddl off;
        commit;

        -- Following management statements are taken from
        -- doc/sql.extensions/README.management_statements_psql.md:
        -- ########################################################
        set echo on;
        alter session reset;
        set session idle timeout 1800 second;
        set statement timeout 190 second;
        set bind of decfloat to double precision;
        set decfloat round ceiling;
        set decfloat traps to Division_by_zero;
        set time zone 'America/Sao_Paulo';
        set role {tmp_role.name};
        commit;
        select 'Completed' as msg from rdb$database;
    """

    with act.trace(db_events=trace_lin):
        act.isql(switches=['-n', '-q'], input = test_script_nix)

    # process trace
    for line in act.trace_log:
        if line.split():
            if act.match_any(line, patterns_lin):
                print(' '.join(line.split()).lower())
    # Check
    act.expected_stdout = expected_stdout_nix
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout

