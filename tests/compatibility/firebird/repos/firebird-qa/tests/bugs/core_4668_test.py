#coding:utf-8

"""
ID:          issue-4979
ISSUE:       4979
TITLE:       Select from mon$table_stats doesn`t work on SC and CS
DESCRIPTION:
JIRA:        CORE-4668
FBTEST:      bugs.core_4668
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create or alter view v_stat as select 1 id from rdb$database;
    recreate table t(id int, s varchar(36)); commit;
    insert into t select row_number()over(), uuid_to_char(gen_uuid()) from rdb$types,rdb$types rows 2000;
    commit;
    create index t_s on t(s);
    commit;

    create or alter view v_stat as
    select
         r.mon$record_inserts     rec_ins
        ,r.mon$record_updates     rec_upd
        ,r.mon$record_deletes     rec_del
        ,r.mon$record_backouts    rec_bko
        ,r.mon$record_purges      rec_pur
        ,r.mon$record_expunges    rec_exp
        -----------
        ,r.mon$record_seq_reads   read_nat
        ,r.mon$record_idx_reads   read_idx
        ,r.mon$record_rpt_reads   read_rpt
        ,r.mon$backversion_reads  read_bkv
        ,r.mon$fragment_reads     read_frg
        ------------
        ,a.mon$stat_id
    from mon$record_stats r
    join mon$table_stats t on r.mon$stat_id = t.mon$record_stat_id
    join mon$attachments a on t.mon$stat_id = a.mon$stat_id
    where
        a.mon$attachment_id = current_connection
        and t.mon$table_name = upper('t')
    ;
    commit;

    set list on;

    set count on;
    select read_nat, read_idx, rec_ins, rec_upd, rec_del from v_stat; -- , rec_pur, rec_exp from v_stat;
    set count off;

    --delete from t rows 1000; commit; -- purge/expunge: can`t get exact values in SS because of background GC thread!

    select count(*) from t
    union all
    select count(*) from t where s>=''
    ;

    insert into t select row_number()over(), uuid_to_char(gen_uuid()) from rdb$types,rdb$types rows 500;
    update t set s = null rows 100;
    update t set s = null order by s rows 100;
    delete from t rows 200;
    delete from t order by s rows 200;
    commit;

    set count on;
    select read_nat, read_idx, rec_ins, rec_upd, rec_del from v_stat; -- , rec_pur, rec_exp from v_stat;

    -- Confirmed wrong result on  WI-T3.0.0.31374
    -- Records affected: 0
    -- COUNT                           2000
    -- COUNT                           2000
    -- Records affected: 0
"""

act = isql_act('db', test_script)

expected_stdout = """
    READ_NAT                        0
    READ_IDX                        0
    REC_INS                         2000
    REC_UPD                         0
    REC_DEL                         0
    Records affected: 1

    COUNT                           2000
    COUNT                           2000

    READ_NAT                        2300
    READ_IDX                        2300
    REC_INS                         2500
    REC_UPD                         200
    REC_DEL                         400
    Records affected: 1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

