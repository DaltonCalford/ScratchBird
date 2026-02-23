#coding:utf-8

"""
ID:          issue-3187
ISSUE:       3187
TITLE:       Problem with default value of SP parameter
DESCRIPTION:
JIRA:        CORE-2797
FBTEST:      bugs.core_2797
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create or alter procedure sp_test as begin end;
    commit;

    set term ^;
    execute block as
    begin
        begin execute statement 'drop domain dm_dts_now';  when any do begin end end
        begin execute statement 'drop domain dm_dts_cts';  when any do begin end end
        begin execute statement 'drop domain dm_int_con';  when any do begin end end
        begin execute statement 'drop domain dm_vc_user';  when any do begin end end
    end
    ^
    set term ;^
    commit;

    create domain dm_dts_now timestamp default 'now' not null ;
    create domain dm_dts_cts timestamp default current_timestamp not null ;
    create domain dm_int_con int default current_connection;
    create domain dm_vc_user int default current_user;
    commit;

    set term ^;
    create or alter procedure sp_test (
         in01 timestamp not null       =    'now'
        ,in02 dm_dts_cts               =    current_timestamp
        ,in03 type of dm_dts_now       =    current_date
        ,in04 timestamp not null       =    'now'
        ,in05 dm_dts_cts               =    current_timestamp
        ,in06 type of dm_dts_now       =    current_date
        ,in07 int not null             =    current_connection
        ,in08 dm_int_con               =    current_connection
        ,in09 dm_int_con               =    current_connection
        ,in10 varchar(31)              =    current_user
        ,in11 dm_vc_user               =    current_user
        ,in12 dm_vc_user               =    current_user
    ) as
        declare x integer;
    begin
        x = 0;
    end
    ^
    set term ;^
    commit;


    --set list on;

    set width ppar_name 15;
    set width ppar_fld_src 35;
    set width ppar_def_src 35;
    set width rdbf_fld_name 15;
    set width rdbf_def_src 35;

    select
         pp.rdb$parameter_name ppar_name
        ,replace(replace(cast( pp.rdb$field_source as varchar(35)), ascii_char(13), '<CR>'), ascii_char(10), '<LF>') ppar_fld_src
        ,replace(replace(cast( pp.rdb$default_source as varchar(35)), ascii_char(13), '<CR>'), ascii_char(10), '<LF>') ppar_def_src
        ,f.rdb$field_name rdbf_fld_name
        ,replace(replace(cast(f.rdb$default_source as varchar(35)), ascii_char(13), '<CR>'), ascii_char(10), '<LF>')  rdbf_def_src
    from rdb$procedure_parameters pp
         left join rdb$fields f on pp.rdb$field_source = f.rdb$field_name
    where upper(trim(pp.rdb$procedure_name)) = upper('sp_test');
    commit;
"""

act = isql_act('db', test_script, substitutions=[('==.*', '')])

expected_stdout = """
    PPAR_NAME       PPAR_FLD_SRC                        PPAR_DEF_SRC                        RDBF_FLD_NAME   RDBF_DEF_SRC
    IN01            RDB$1                               =    'now'                          RDB$1           <null>
    IN02            DM_DTS_CTS                          =    current_timestamp              DM_DTS_CTS      default current_timestamp
    IN03            DM_DTS_NOW                          =    current_date                   DM_DTS_NOW      default 'now'
    IN04            RDB$2                               =    'now'                          RDB$2           <null>
    IN05            DM_DTS_CTS                          =    current_timestamp              DM_DTS_CTS      default current_timestamp
    IN06            DM_DTS_NOW                          =    current_date                   DM_DTS_NOW      default 'now'
    IN07            RDB$3                               =    current_connection             RDB$3           <null>
    IN08            DM_INT_CON                          =    current_connection             DM_INT_CON      default current_connection
    IN09            DM_INT_CON                          =    current_connection             DM_INT_CON      default current_connection
    IN10            RDB$4                               =    current_user                   RDB$4           <null>
    IN11            DM_VC_USER                          =    current_user                   DM_VC_USER      default current_user
    IN12            DM_VC_USER                          =    current_user                   DM_VC_USER      default current_user
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

