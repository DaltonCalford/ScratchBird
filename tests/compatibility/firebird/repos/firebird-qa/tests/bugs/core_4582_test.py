#coding:utf-8

"""
ID:          issue-4898
ISSUE:       4898
TITLE:       Within LINGER period one can not change some database properties
DESCRIPTION:
    Test verifies that after set LINGER one may still do following:
        * change buffers number in DB header (actual only for CS / SC);
        * change FW attribute to 'sync' (by default DB is created with FW = OFF);
        * change sweep interval;
        * change space usage to '-use <FULL>'
        * for FB 4.x+: change DB attribute to 'REPLICA READ_WRITE'.
        * change DB mode to read_only.

    Confirmed on WI-T3.0.0.31374 Beta 1: running GFIX -buffers N has NO affect if this is done within linger period
JIRA:        CORE-4582
FBTEST:      bugs.core_4582
NOTES:
    [21.12.2021] pcisar
        FAILs on v4.0.0.2496 and v3.0.8.33535 as database couldn't be reverted to online state
    [21.09.2022] pzotov
        It was encountered that if we change DB access to 'shutdown single maintenace' than further 'gifx -online'
        will raise message 'database db shutdown', and only 2nd call of gfix will bring DB to online.
        Report was sent to FB team, 17.09.2022 23:18, reply from Vlad got: 19.09.2022 13:06.
        It was decided to SKIP this action ('gfix -shut single -at NN') until this problem will be resolved.
    Checked on Linux and Windows: 3.0.8.33535 (SS/CS), 4.0.1.2692 (SS/CS)
"""

import pytest
from firebird.qa import *
from firebird.driver import DbWriteMode, DbAccessMode, ShutdownMode, ShutdownMethod, SrvStatFlag

substitutions = [
                 ('[\t ]+', ' '),
                 ('^((?!:::MSG:::|buffers|before|after|Sweep|Attributes).)*$', ''),
                 ('N/A', 'YES')
                ]

init_script = """
    -- Result of this test on WI-T3.0.0.31374 Beta 1:
    -- Expected standard output from Python does not match actual output.
    -- - GFIX could change buffers ? =>  YES
    -- ?                                 ^^^
    -- + GFIX could change buffers ? =>  NO!
    -- ?                                 ^^^

    set term ^;
    -- Aux procedure that determines: is current connection to SuperServer ?
    -- If yes, returns current value of page buffers, otherwise (SC/CS) always return -1, in order to
    -- make final output = 'N/A' and give 'substitutions' section handle this case as acceptable.
    create or alter procedure sp_get_buff returns(mon_buffers int) as
    begin
        mon_buffers = -1;
        if (exists(select * from mon$attachments where mon$user containing 'cache writer' and mon$system_flag = 1))
        then
            select mon$page_buffers from mon$database into mon_buffers;

        suspend;

    end
    ^
    set term ;^
    commit;
    recreate table log(buf_before int, buf_after int);
    commit;
"""

db = db_factory(init=init_script)
#db = db_factory(filename = '#tmp_core_4582_alias', init=init_script)

act = python_act('db', substitutions=substitutions)

@pytest.mark.version('>=3.0')
def test_1(act: Action, capsys):

    if act.is_version('>=4'):
        ADDED_REPLICA_ATTR = ", read-write replica"
    else:
        ADDED_REPLICA_ATTR = ''

    expected_stdout = f"""
        Page buffers 3791
        Attributes force write, no reserve, read only{ADDED_REPLICA_ATTR}
        Sweep interval: 5678
        GFIX could change buffers ? =>  YES
    """
    
    script = """
        insert into log(buf_before) select mon_buffers from sp_get_buff;
        commit;
        alter database set linger to 15;
        commit;
    """
    act.isql(switches=[], input=script)

    #with act.connect_server() as srv:
        #srv.database.set_default_cache_size(database=act.db.db_path, size=3791)
        #srv.database.set_write_mode(database=act.db.db_path, mode=DbWriteMode.ASYNC)
        #srv.database.set_access_mode(database=act.db.db_path, mode=DbAccessMode.READ_ONLY)
        #srv.database.shutdown(database=act.db.db_path, mode=ShutdownMode.SINGLE,
                              #method=ShutdownMethod.DENNY_ATTACHMENTS, timeout=20)
        #srv.database.get_statistics(database=act.db.db_path, flags=SrvStatFlag.HDR_PAGES,
                                    #callback=print)
        #srv.database.bring_online(database=act.db.db_path)
        #srv.database.set_access_mode(database=act.db.db_path, mode=DbAccessMode.READ_WRITE)
    act.reset()
    act.gfix(switches=['-buffers', '3791', act.db.dsn])
    act.reset()
    act.gfix(switches=['-write','sync', act.db.dsn])
    act.reset()
    act.gfix(switches=['-housekeeping','5678', act.db.dsn])
    act.reset()
    act.gfix(switches=['-use','full', act.db.dsn])
    act.reset()
    
    if act.is_version('>=4'):
        act.gfix(switches=['-replica','read_write', act.db.dsn])
        act.reset()

    act.gfix(switches=['-mode','read_only', act.db.dsn])
    act.reset()

    
    '''
    TEMPORARY COMMENTED, SEE NOTES. DO NOT DELETE THIS TEXT!
    act.gfix(switches=['-shutdown','single', '-at', '20', act.db.dsn])
    act.reset()
    '''
    act.gstat(switches=['-h'])
    print(act.stdout)
    act.reset()

    '''
    TEMPORARY COMMENTED, SEE NOTES. DO NOT DELETE THIS TEXT!
    # [pcisar] 21.12.2021
    # FAILs on v4.0.0.2496 and v3.0.8.33535 as database couldn't be reverted to online
    # state
    act.gfix(switches=[act.db.dsn, '-online'])
    act.reset()
    '''

    act.gfix(switches=['-mode','read_write', act.db.dsn])
    script_2 = """
        set list on;
        update log set buf_after = (select mon_buffers from sp_get_buff);
        commit;
        select iif( g.buf_before > 0,
                    iif( g.buf_before is distinct from g.buf_after, 'YES', 'NO!' ),
                    'N/A'
                  ) as "GFIX could change buffers ? =>"
        from log g;
    """
    act.reset()
    act.isql(switches=[], input=script_2)
    print(act.stdout)

    # Check
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
