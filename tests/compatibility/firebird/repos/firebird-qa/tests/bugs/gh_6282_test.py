#coding:utf-8

"""
ID:          issue-6282
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/6282
TITLE:       Add DPB properties for time zone bind and decfloat configuration [CORE6032]
DESCRIPTION:
    Test verifies:
    * ability to set appropriate DPB parameters described in the ticket;
    * actual result of their change (by performing SQL and compare output with expected);
    * surviving of changed parameters after 'slter session reset' (i.e. absence of its influence)
NOTES:
    [22.07.2024] pzotov
    Improvement appeared 07.04.2019 17:56 (2a9f8fa60b327132373cd7ee3f0a0b52e595f6b1),
    but actually this test can run only on build after 29.05.2020 (4.0.0.2011) because of
    commit https://github.com/FirebirdSQL/firebird/commit/a9cef6d9aeaabc08d8f104230a38345340edf7a2
    ("Implemented CORE-6320: Replace Util methods to get interface pointer by legacy handle with plain functions")
    Attempt to run it on earlier snapshots leads to errors:
    firebird.driver.types.DatabaseError: invalid statement handle
    or
    AttributeError: 'iUtil_v2' object has no attribute 'decode_timestamp_tz'

    [01.09.2024]
    On Linux argument of tzfile is shown with prefix ("/usr/share/zoneinfo/"), so we have to remove it:
        <class 'dateutil.tz.tz.tzfile'>:
        Windows = tzfile('Indian/Cocos')
        Linux = tzfile('/usr/share/zoneinfo/Indian/Cocos')
    This is done by extracting '_timezone_' property of this instance.

    Checked on 6.0.0.396, 5.0.1.1440, 4.0.5.3127
"""
import pytest
from firebird.qa import *
from firebird.driver import driver_config, connect, DecfloatRound, DecfloatTraps, NetProtocol, DatabaseError
import datetime

init_script = """
    set list on;
    /******
    Original: ibm.com/developerworks/ru/library/dm-0801chainani/
    See also: functional/datatypes/test_decfloat_round_modes.py:
    round-mode	12.341	12.345	12.349 	12.355 	12.405 	-12.345
    ---------------------------------------------------------------
    CEILING 	12.35 	12.35 	12.35 	12.36 	12.41 	-12.34
    UP        	12.35 	12.35 	12.35 	12.36 	12.41 	-12.35
    HALF_UP 	12.34 	12.35 	12.35 	12.36 	12.41 	-12.35
    HALF_EVEN 	12.34 	12.34 	12.35 	12.36 	12.40 	-12.34
    HALF_DOWN	12.34 	12.34 	12.35 	12.35 	12.40 	-12.34
    DOWN     	12.34 	12.34 	12.34	12.35 	12.40 	-12.34
    FLOOR    	12.34 	12.34 	12.34 	12.35 	12.40 	-12.35
    REROUND   	12.34	12.34 	12.34 	12.36 	12.41 	-12.34
    *******/

    recreate view v_test as select 1 id from rdb$database;
    commit;

    recreate table test(
         v1 decfloat
        ,v2 decfloat
        ,v3 decfloat
        ,v4 decfloat
        ,v5 decfloat
        ,v6 decfloat
        ,vc decfloat
        ,vp decfloat
        ,vd decfloat
        ,vx computed by (vc * vp / vd)
        ,vy computed by (vc * vp / vd)
    )
    ;
    commit;

    insert into test( v1,     v2,     v3,     v4,     v5,      v6,        vc,     vp,     vd)
                values(12.341, 12.345, 12.349, 12.355, 12.405, -12.345,  1608.90, 5.00, 100.00);
    commit;

    recreate view v_test as
    select
        round(v1, 2) r1, round(v2, 2) r2, round(v3, 2) r3,
        round(v4, 2) r4, round(v5, 2) r5, round(v6, 2) r6,
        round( vx, 2) as rx,
        round( -vy, 2) as ry
    from test;
    commit;
"""

db = db_factory(init = init_script)
act = python_act('db')

@pytest.mark.version('>=4.0')
def test_1(act: Action, capsys):

    srv_cfg = driver_config.register_server(name = 'srv_cfg_6282', config = '')

    db_cfg_name = f'db_cfg_6282'
    
    # DatabaseConfig; see PYTHON_HOME/Lib/site-packages/firebird\driver\config.py:
    db_cfg_object = driver_config.register_database(name = db_cfg_name)

    db_cfg_object.server.value = srv_cfg.name
    db_cfg_object.protocol.value = NetProtocol.INET
    db_cfg_object.database.value = str(act.db.db_path)
    
    ######################################################
    ###  c h e c k    s e s s i o n   t i m e z o n e  ###
    ######################################################
    #
    SELECTED_TIMEZONE = 'Indian/Cocos'
    db_cfg_object.session_time_zone.value = SELECTED_TIMEZONE
    with connect(db_cfg_name, user = act.db.user, password = act.db.password) as con:
        cur = con.cursor()
        cur.execute('select current_timestamp from rdb$database')
        for r in cur:
            print(r[0].tzinfo._timezone_)
        cur.close()

        # The value set through the DPB should survive an `alter session reset`
        con.execute_immediate('alter session reset')
        con.commit()

        cur = con.cursor()
        cur.execute('select current_timestamp from rdb$database')
        for r in cur:
            # class 'dateutil.zoneinfo.tzfile'
            tzfile_nfo = r[0].tzinfo # <class 'dateutil.tz.tz.tzfile'>: Windows = tzfile('Indian/Cocos'); Linux = tzfile('/usr/share/zoneinfo/Indian/Cocos')
            # tzfile_arg = tzfile_nfo._filename # <class 'str'>: Windows = 'Indian/Cocos'; Linux = '/usr/share/zoneinfo/Indian/Cocos'
            print(tzfile_nfo._timezone_) # Windows: 'Indian/Cocos'; Linux: 'Indian/Cocos'
            
    act.expected_stdout = f"""
        {SELECTED_TIMEZONE}
        {SELECTED_TIMEZONE}
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    # ....................................................................................

    ################################################
    ###  c h e c k    D e c f l o a t R o u n d  ###
    ################################################
    #
    # doc/sql.extensions/README.data_types.txt:
    # CEILING (towards +infinity),
    # UP (away from 0),
    # HALF_UP (to nearest, if equidistant - up),
    # HALF_EVEN (to nearest, if equidistant - ensure last digit in the result to be even),
    # HALF_DOWN (to nearest, if equidistant - down),
    # DOWN (towards 0),
    # FLOOR (towards -infinity),
    # REROUND (up if digit to be rounded is 0 or 5, down in other cases).
    #
    # Examples from functional/datatypes/test_decfloat_round_modes.py:
    #
    df_round_lst = (
        DecfloatRound.CEILING
       ,DecfloatRound.UP
       ,DecfloatRound.HALF_UP
       ,DecfloatRound.HALF_EVEN
       ,DecfloatRound.HALF_DOWN
       ,DecfloatRound.DOWN
       ,DecfloatRound.FLOOR
       ,DecfloatRound.REROUND
    )
    for r in df_round_lst:
        db_cfg_object.decfloat_round.value = r
        print(r.name)

        with connect(db_cfg_name, user = act.db.user, password = act.db.password) as con:
            try:

                # The value set through the DPB should survive an `alter session reset`
                con.execute_immediate('alter session reset')
                con.commit()

                cur = con.cursor()
                cur.execute('select v.* from v_test v')
                ccol=cur.description
                for r in cur:
                    for i in range(0,len(ccol)):
                        print( ccol[i][0],':', r[i] )
            except DatabaseError as e:
                print(e.__str__())

    # Return round option to default:
    db_cfg_object.decfloat_round.value = DecfloatRound.HALF_UP

    act.expected_stdout = """
        CEILING
        R1 : 12.35
        R2 : 12.35
        R3 : 12.35
        R4 : 12.36
        R5 : 12.41
        R6 : -12.34
        RX : 80.45
        RY : -80.44
        UP
        R1 : 12.35
        R2 : 12.35
        R3 : 12.35
        R4 : 12.36
        R5 : 12.41
        R6 : -12.35
        RX : 80.45
        RY : -80.45
        HALF_UP
        R1 : 12.34
        R2 : 12.35
        R3 : 12.35
        R4 : 12.36
        R5 : 12.41
        R6 : -12.35
        RX : 80.45
        RY : -80.45
        HALF_EVEN
        R1 : 12.34
        R2 : 12.34
        R3 : 12.35
        R4 : 12.36
        R5 : 12.40
        R6 : -12.34
        RX : 80.44
        RY : -80.44
        HALF_DOWN
        R1 : 12.34
        R2 : 12.34
        R3 : 12.35
        R4 : 12.35
        R5 : 12.40
        R6 : -12.34
        RX : 80.44
        RY : -80.44
        DOWN
        R1 : 12.34
        R2 : 12.34
        R3 : 12.34
        R4 : 12.35
        R5 : 12.40
        R6 : -12.34
        RX : 80.44
        RY : -80.44
        FLOOR
        R1 : 12.34
        R2 : 12.34
        R3 : 12.34
        R4 : 12.35
        R5 : 12.40
        R6 : -12.35
        RX : 80.44
        RY : -80.45
        REROUND
        R1 : 12.34
        R2 : 12.34
        R3 : 12.34
        R4 : 12.36
        R5 : 12.41
        R6 : -12.34
        RX : 80.44
        RY : -80.44
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    # ....................................................................................

    ################################################
    ###  c h e c k    D e c f l o a t T r a p s  ###
    ################################################
    #
    # doc/sql.extensions/README.data_types.txt:
    # SET DECFLOAT TRAPS TO <comma-separated traps list - may be empty>
    # Division_by_zero, Inexact, Invalid_operation, Overflow and Underflow
    #

    # Examples from functional/datatypes/test_decfloat_exceptions_trapping.py:
    #
    df_traps_map = {
         DecfloatTraps.DIVISION_BY_ZERO :
             ( 'select 1/1e-9999'
               ,'Decimal float divide by zero.'
             )
        ,DecfloatTraps.INEXACT :
            (  'select 1e9999 + 1e9999'
              ,'Decimal float inexact result.'
            )
        ,DecfloatTraps.INVALID_OPERATION :
            (  "select cast('34ffd' as decfloat(16))"
              ,'Decimal float invalid operation.'
            )
        ,DecfloatTraps.OVERFLOW :
            (  'select 1e9999'
              ,'Decimal float overflow.'
            )
        ,DecfloatTraps.UNDERFLOW :
            (  'select 1e-9999'
              ,'Decimal float underflow.'
            )
    }

    expected_out_lst = []
    actual_out_lst = []
    expected_iter = 'EXPECTED error message raised.'
    for k,v in df_traps_map.items():
        traps_option = k
        execute_sttm = v[0] + ' from rdb$database'
        expected_msg = v[1]
        db_cfg_object.decfloat_traps.value = [traps_option,]

        # expected_out_lst.append( str(db_cfg_object.decfloat_traps.value[0]) ) # name of option
        expected_out_lst.append(traps_option.name)
        expected_out_lst.append(expected_iter)

        actual_out_lst.append(traps_option.name)

        with connect(db_cfg_name, user = act.db.user, password = act.db.password) as con:
            try:
                # The value set through the DPB should survive an `alter session reset`
                con.execute_immediate('alter session reset')
                con.commit()

                cur = con.cursor()
                cur.execute(execute_sttm)
                for r in cur:
                    pass
            except DatabaseError as e:
                if expected_msg in e.__str__():
                    actual_out_lst.append(expected_iter)
                else:
                    actual_out_lst.append('UNEXPECTED ERROR MESSAGE:\n' + e.__str__())


    act.expected_stdout = '\n'.join(expected_out_lst)
    act.stdout = '\n'.join(actual_out_lst)
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    # Return traps option to default:
    db_cfg_object.decfloat_traps.value = []

