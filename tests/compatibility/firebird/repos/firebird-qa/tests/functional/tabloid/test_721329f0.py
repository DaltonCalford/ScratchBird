#coding:utf-8

"""
ID:          n/a
ISSUE:       https://github.com/FirebirdSQL/firebird/commit/721329f0d2dc011a2da149a567150e09e6dae939
TITLE:       Fix problem in gbak where a package name were being leaked to non-packaged routines.
DESCRIPTION:
    Test creates two packages and two standalone units (stored proc and function).
    Name of 1st package equals to the name of standalone SP, name of 2nd package - to the name of standalone func.
    Packages and standalone units can be independent of each other (i.e. calls between them are not mandatory).

    Then we make backup of this DB and try to restore it using three cases: via INET protocol w/o services API,
    via INET protocol using services API and via local protocol.
    In all cases we have to see in the restore log:
        gbak:adjusting system generators
        gbak:finishing, closing, and going home
        gbak:adjusting the ONLINE and FORCED WRITES flags
    (or 'gbak:ERROR' if this restore failed).
NOTES:
    [17.09.2025] pzotov
    Confirmed bug on 6.0.0.1275-402365e: server crashes.
    Checked on 6.0.0.1277-721329f (SS and CS).
"""
from pathlib import Path
import subprocess

import pytest
from firebird.qa import *

tmp_fbk = temp_file('tmp_721329f0.fbk')
tmp_res = temp_file('tmp_721329f0.fdb')
tmp_log = temp_file('tmp_721329f0.log')

init_sql = """
    set term ^;
    create package sp_factorial as
    begin
        procedure sp_factorial(i smallint) returns (o bigint);
    end
    ^
    create package fn_factorial as
    begin
        function fn_factorial(i smallint) returns bigint;
    end
    ^

    create package body sp_factorial as
    begin
        procedure sp_factorial(i smallint) returns(o bigint) as
        begin
            if ( i > 1 ) then
                o = i * (select o from sp_factorial( :i - 1 ));
            else
                o = i;
            suspend;
        end

    end
    ^

    create package body fn_factorial as
    begin
        function fn_factorial(i smallint) returns bigint as
        begin
            if ( i > 1 ) then
                return i * fn_factorial( i-1 );
            else
                return i;
        end
    end
    ^

    create procedure sp_factorial(i smallint) returns(o bigint) as
    begin
        -- select o from sp_factorial.sp_factorial(:i) into o;
        suspend;
    end
    ^

    create function fn_factorial(i smallint) returns bigint as
    begin
        return 1; -- fn_factorial.fn_factorial(:i);
    end
    ^
    set term ;^
    commit;
"""
db = db_factory(init = init_sql)

substitutions = [ ( '^((?!(iter:|(gbak:[ \t]?(ERROR:|WARNING:|finishing|adjusting)))).)*$' , '' ) ]

act = python_act('db', substitutions = substitutions)

@pytest.mark.version('>=6.0')
def test_1(act: Action, tmp_fbk: Path, tmp_res: Path, tmp_log: Path, capsys):

    act.gbak(switches=['-b', str(act.db.dsn), str(tmp_fbk)])
    assert act.stdout == '', f'Problem with backup of initially created DB:\n{act.stdout}'
    act.reset()

    for iter in ('inet_no_svc','inet_using_svc', 'local'):
        with open(tmp_log,'w') as f:
            if iter == 'embedded':
                subprocess.call( [
                                    act.vars['gbak']
                                   ,'-rep'
                                   ,'-v'
                                   ,tmp_fbk
                                   ,tmp_res
                                 ]
                                 ,stdout = f, stderr = subprocess.STDOUT
                               )
            elif iter == 'no_use_svc':
                subprocess.call( [
                                    act.vars['gbak']
                                   ,'-rep'
                                   ,'-user', act.db.user
                                   ,'-pass', act.db.password
                                   ,'-v'
                                   ,tmp_fbk
                                   ,'localhost:' + str(tmp_res)
                                 ]
                                 ,stdout = f, stderr = subprocess.STDOUT
                               )
            else:
                subprocess.call( [
                                    act.vars['gbak']
                                   ,'-rep'
                                   ,'-user', act.db.user
                                   ,'-pass', act.db.password
                                   ,'-v'
                                   ,'-se', 'localhost:service_mgr'
                                   ,tmp_fbk
                                   ,tmp_res
                                 ]
                                 ,stdout = f, stderr = subprocess.STDOUT
                               )
        print(f'iter: {iter}')
        with open(tmp_log,'r') as f:
            for line in f:
                print(line)

    act.expected_stdout = f"""
        iter: inet_no_svc
        gbak:adjusting views dbkey length
        gbak:adjusting system generators
        gbak:finishing, closing, and going home
        gbak:adjusting the ONLINE and FORCED WRITES flags

        iter: inet_using_svc
        gbak:adjusting views dbkey length
        gbak:adjusting system generators
        gbak:finishing, closing, and going home
        gbak:adjusting the ONLINE and FORCED WRITES flags

        iter: local
        gbak:adjusting views dbkey length
        gbak:adjusting system generators
        gbak:finishing, closing, and going home
        gbak:adjusting the ONLINE and FORCED WRITES flags
    """

    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

