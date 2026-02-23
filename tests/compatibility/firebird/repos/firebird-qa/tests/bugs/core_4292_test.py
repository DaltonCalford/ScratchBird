#coding:utf-8

"""
ID:          issue-4615
ISSUE:       4615
TITLE:       Server ignores asynchronous (monitoring or cancellation) requests while
  preparing a query with lot of windowed functions
DESCRIPTION:
  Preparing stage of test query will last lot of time even on power host.
  We launch separate (child) process with ISQL and allow him to start preparing this query during several
  seconds. Then we launch second child process with ISQL and try to kill first one.
  Before this ticket was fixed it was NOT possible neither to cancel it by using MON$ATTACHMENTS nor even
  query MON$ tables at all (until this 'complex query' preparing finish).
JIRA:        CORE-4292
FBTEST:      bugs.core_4292
"""

import pytest
import subprocess
import time
from pathlib import Path
from firebird.qa import *

db = db_factory()
heavy_script = temp_file('heavy_script.sql')
heavy_output = temp_file('heavy_script.out')

act = python_act('db', substitutions=[('.*line.*', ''), ('[\t ]+', ' '),
                                        ('.*Killed by database administrator.*', '')])

test_script = """
    set bail on;
    set list on;
    set count on;
    with a as (
        select rf.rdb$field_id fid, rr.rdb$relation_id rid, rr.rdb$relation_name rnm
        from rdb$relation_fields rf join rdb$relations rr on rf.rdb$relation_name=rr.rdb$relation_name
    )
    ,b as (
        select fid, rnm, rid, iif(rid is null, 1, r) r
        from (--f
            select fid, rnm, rid,
                    iif(lag(fid) over(partition by rid order by fid) is not null
                        and lag(r) over(partition by rid order by fid) >= r
                        , r + 1, r) r
            from (--e
                select fid, rnm, rid,
                        iif(lag(fid) over(partition by rid order by fid) is not null
                            and lag(r) over(partition by rid order by fid) >= r
                            , r + 1, r) r
                from (--d
                    select fid, rnm, rid,
                            iif(lag(fid) over(partition by rid order by fid) is not null
                                and lag(r) over(partition by rid order by fid) >= r
                                , r + 1, r) r
                    from (--c
                        select fid, rnm, rid,
                                iif(lag(fid) over(partition by rid order by fid) is not null
                                    and lag(r) over(partition by rid order by fid) >= r
                               ,r + 1, r) r
                        from (--b
                            select fid, rnm, rid,
                                    iif( lag(fid) over(partition by rid order by fid) is not null
                                          and lag(r) over(partition by rid order by fid) >= r
                                         ,r + 1, r) r
                            from (
                                    select a.*, 1 r
                                    from a
                                  ) a
                            ) b
                      ) c
                  ) d
              ) e
          ) f
    )
    -- select max(r) r from b group by fid having max(r) < 6; -- ok

    ,c
    as (
        select fid, rnm, rid, iif(rid is null, 1, r) r
        from (--f
            select fid, rnm, rid,
                    iif(lag(fid) over(partition by rid order by fid) is not null
                         and lag(r) over(partition by rid order by fid) >= r
                        , r + 1, r) r
            from (--e
                select fid, rnm, rid,
                        iif(lag(fid) over(partition by rid order by fid) is not null
                             and lag(r) over(partition by rid order by fid) >= r
                            , r + 1, r) r
                from (--d
                    select fid, rnm, rid,
                            iif(lag(fid) over(partition by rid order by fid) is not null
                                 and lag(r) over(partition by rid order by fid) >= r
                                , r + 1, r) r
                    from (--c
                        select fid, rnm, rid,
                                iif(lag(fid) over(partition by rid order by fid) is not null
                                     and lag(r) over(partition by rid order by fid) >= r
                               ,r + 1, r) r
                        from (--b
                            select fid, rnm, rid,
                                    iif( lag(fid) over(partition by rid order by fid) is not null
                                          and lag(r) over(partition by rid order by fid) >= r
                                         ,r + 1, r) r
                            from (
                                    select fid, rnm, rid, max(r) over(partition by fid) r from b
                                  ) a
                            ) b
                      ) c
                  ) d
              ) e
          ) f
    )
    -- select * from c -- ok

    ,d
    as (
        select fid, rnm, rid, iif(rid is null, 1, r) r
        from (--f
            select fid, rnm, rid,
                    iif( lag(fid) over(partition by rid order by fid) is not null
                         and lag(r) over(partition by rid order by fid) >= r
                        , r + 1, r) r
            from (--e
                select fid, rnm, rid,
                        iif( lag(fid) over(partition by rid order by fid) is not null
                             and lag(r) over(partition by rid order by fid) >= r
                            , r + 1, r) r
                from (--d
                    select fid, rnm, rid,
                            iif( lag(fid) over(partition by rid order by fid) is not null
                                 and lag(r) over(partition by rid order by fid) >= r
                                , r + 1, r) r
                    from (--c
                        select fid, rnm, rid,
                                iif( lag(fid) over(partition by rid order by fid) is not null
                                     and lag(r) over(partition by rid order by fid) >= r
                               ,r + 1, r) r
                        from (--b
                            select fid, rnm, rid,
                                    iif( lag(fid) over(partition by rid order by fid) is not null
                                          and lag(r) over(partition by rid order by fid) >= r
                                         ,r + 1, r) r
                            from (
                                    select fid, rnm, rid, max(r) over(partition by fid) r from c
                                  ) a
                            ) b
                      ) c
                  ) d
              ) e
          ) f
    )
    select * from d rows 0;
    set count off;
    select 'WORKER FINISHED TOO FAST! DELAY IN TEST MUST BE REDUCED!' msg from rdb$database;
"""

killer_sql = """
    set list on;
    /*
    select a.*, s.*
    from mon$attachments a
    left join mon$statements s on a.mon$attachment_id = s.mon$attachment_id
    where
        a.mon$attachment_id <> current_connection
        and a.mon$system_flag is distinct from 1
    ;
    */
    select count(*) as "Found worker ?"
    from mon$attachments a
    where
        a.mon$attachment_id <> current_connection
        and a.mon$system_flag is distinct from 1
    ;

    delete from mon$attachments
    where
        mon$attachment_id <> current_connection
        and mon$system_flag is distinct from 1
    returning
      sign(mon$attachment_id) as deleted_mon_att_id,
      mon$user as deleted_mon_user,
      iif(mon$remote_protocol containing 'tcp', 'tcp', null) as deleted_mon_protocol,
      iif(mon$remote_process containing 'isql', 'isql', null) as deleted_mon_process,
      mon$system_flag as deleted_mon_sys_flag
    ;
"""

expected_stdout_killer = """
Found worker ?                  1
DELETED_MON_ATT_ID              1
DELETED_MON_USER                SYSDBA
DELETED_MON_PROTOCOL            tcp
DELETED_MON_PROCESS             isql
DELETED_MON_SYS_FLAG            0
"""

expected_stdout_worker = """
Statement failed, SQLSTATE = 08003
connection shutdown
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action, heavy_script: Path, heavy_output: Path):
    heavy_script.write_text(test_script)
    with open(heavy_output, mode='w') as heavy_out:
        p_heavy_sql = subprocess.Popen([act.vars['isql'], '-i', str(heavy_script),
                                       '-user', act.db.user,
                                       '-password', act.db.password, act.db.dsn],
                                       stdout=heavy_out, stderr=subprocess.STDOUT)
        try:
            time.sleep(4)
            act.expected_stdout = expected_stdout_killer
            act.isql(switches=[], input=killer_sql)
        finally:
            p_heavy_sql.terminate()
    #
    assert act.clean_stdout == act.clean_expected_stdout
    # And worker...
    act.reset()
    act.expected_stdout = expected_stdout_worker
    act.stdout = heavy_output.read_text()
    assert act.clean_stdout == act.clean_expected_stdout
