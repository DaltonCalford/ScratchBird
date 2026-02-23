#coding:utf-8

"""
ID:          issue-5219
ISSUE:       5219
TITLE:       It is not possible to save the connection information in the ON CONNECT trigger,
  if the connection is created by the gbak
DESCRIPTION:
JIRA:        CORE-4928
FBTEST:      bugs.core_4928
"""

import pytest
from pathlib import Path
from firebird.qa import *

init_script = """
    recreate table att_log (
        att_id int,
        att_name varchar(255),
        att_user varchar(255),
        att_addr varchar(255),
        att_prot varchar(255),
        att_auth varchar(255),
        att_proc varchar(255),
        att_dts timestamp default 'now'
    );

    commit;

    set term ^;
    create or alter trigger trg_connect active on connect as
    begin
      in autonomous transaction do
      insert into att_log(att_id, att_name, att_user, att_addr, att_prot, att_auth, att_proc)
      select
           mon$attachment_id
          ,mon$attachment_name
          ,mon$user
          ,mon$remote_address
          ,mon$remote_protocol
          ,mon$auth_method
          ,mon$remote_process
      from mon$attachments
      where
          mon$remote_protocol starting with upper('TCP')
          and mon$user = upper('SYSDBA')
          and lower(mon$remote_process) similar to '%[\\/]gbak(.exe)?'
      ;
    end
    ^
    set term ;^
    commit;
"""

db = db_factory(init=init_script)

act = python_act('db')

expected_stdout = """
    IS_ATT_ID_OK                    1
    IS_ATT_NAME_OK                  1
    IS_ATT_ADDR_OK                  1
    IS_ATT_AUTH_OK                  1
    IS_ATT_DTS_OK                   1
    IS_ATT_PROC_OK                  1
"""

fbk_file = temp_file('tmp_core_4928.fbk')

@pytest.mark.version('>=3.0')
def test_1(act: Action, fbk_file: Path):
    act.gbak(switches=['-b', act.db.dsn, str(fbk_file)])
    act.reset()
    # This was in original test, but it makes no sense as it overwites att_log content
    # from backup that does not contain any data on v4.0.0.2496
    # It's IMHO not important to test the issue anyway
    #act.gbak(switches=['-rep', str(fbk_file), act.db.dsn])
    #act.reset()
    # Check
    act.expected_stdout = expected_stdout
    act.script = """
    set list on;
    select 
        --/*
        iif( att_id > 0, 1, 0) is_att_id_ok
       ,iif(att_name containing 'test.fdb', 1, 0) is_att_name_ok
       ,iif( att_addr is not null, 1, 0) is_att_addr_ok
       ,iif( att_auth is not null, 1, 0) is_att_auth_ok
       ,iif( att_dts is not null, 1, 0) is_att_dts_ok
       ,iif( att_proc similar to '%[\\/]gbak(.exe)?', 1, 0) is_att_proc_ok
       -- */
       -- a.*
    from rdb$database
    left join att_log a on  a.att_id <> current_connection;
    """
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
