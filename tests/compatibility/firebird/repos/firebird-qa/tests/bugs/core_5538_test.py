#coding:utf-8

"""
ID:          issue-5806
ISSUE:       5806
TITLE:       Add ability to backup/restore only those (several) tables which are enumerated
  as command line argument (pattern)
DESCRIPTION:
  We create several tables and add single row to each of them. Row contains name of corresponding table.
  Then we create view that based on UNIONED-query to all of these tables.
  After this, we handle list of PATTERNS and pass each of its elements (herteafter its name is: <P>) to
  '-include_data' gbak command switch.
  Further we RESTORE from this .fbk to temporary DB. This new database which contain only those tables
  which names matched to '-include_data <P>' pattern on previous step.
  We also must check joint usage of '-include_data' and (old) '-skip_data' command switches.
  For this purpose we create single pattern for EXCLUDING some tables (see 'skip_ptn' variable) and use
  this pattern together with elements from patterns list for tables which data must be included in .fbk.
JIRA:        CORE-5538
FBTEST:      bugs.core_5538
"""

import pytest
from pathlib import Path
from firebird.qa import *

init_script = """
    recreate view v_test as select 1 x from rdb$database;
    commit;
    recreate table test_anna( s varchar(20) default 'anna' );
    recreate table test_beta( s varchar(20) default 'beta' );
    recreate table test_ciao( s varchar(20) default 'ciao' );
    recreate table test_cola( s varchar(20) default 'cola' );
    recreate table test_dina( s varchar(20) default 'dina' );
    recreate table test_doca( s varchar(20) default 'doca' );
    recreate table test_docb( s varchar(20) default 'docb' );
    recreate table test_docc( s varchar(20) default 'docc' );
    recreate table test_dora( s varchar(20) default 'dora' );
    recreate table test_dura( s varchar(20) default 'dura' );
    recreate table test_mail( s varchar(20) default 'mail' );
    recreate table test_omen( s varchar(20) default 'omen' );
    recreate table test_opel( s varchar(20) default 'opel' );
    recreate table test_rose( s varchar(20) default 'rose' );
    recreate table test_win1( s varchar(20) default 'win1' );
    recreate table test_won2( s varchar(20) default 'won2' );
    recreate table test_w_n3( s varchar(20) default 'w_n3' );
    commit;

    recreate view v_test as
    select v.s
    from rdb$database r
    left join (
        select s from test_anna union all
        select s from test_beta union all
        select s from test_ciao union all
        select s from test_cola union all
        select s from test_dina union all
        select s from test_doca union all
        select s from test_docb union all
        select s from test_docc union all
        select s from test_dora union all
        select s from test_dura union all
        select s from test_mail union all
        select s from test_omen union all
        select s from test_opel union all
        select s from test_rose union all
        select s from test_win1 union all
        select s from test_won2 union all
        select s from test_w_n3
    ) v on 1=1
    ;
    commit;

    insert into test_anna default values;
    insert into test_beta default values;
    insert into test_ciao default values;
    insert into test_cola default values;
    insert into test_dina default values;
    insert into test_doca default values;
    insert into test_docb default values;
    insert into test_docc default values;
    insert into test_dora default values;
    insert into test_dura default values;
    insert into test_mail default values;
    insert into test_omen default values;
    insert into test_opel default values;
    insert into test_rose default values;
    insert into test_win1 default values;
    insert into test_won2 default values;
    insert into test_w_n3 default values;
    commit;
"""

db = db_factory(init=init_script)

act = python_act('db', substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    0 test_doc% 								doca
    0 test_doc% 								docb
    0 test_doc% 								docc
    1 test_d(o|u)ra 							dora
    1 test_d(o|u)ra 							dura
    2 %_w(i|o|_)n[[:DIGIT:]] 					win1
    2 %_w(i|o|_)n[[:DIGIT:]] 					won2
    2 %_w(i|o|_)n[[:DIGIT:]] 					w_n3
    3 test_a[[:ALPHA:]]{1,}a 					anna

    0 test_d%     test_d(o|u)% 					dina
    1 test_(a|b)[[:ALPHA:]]+a test_d(o|u)% 		anna
    1 test_(a|b)[[:ALPHA:]]+a test_d(o|u)% 		beta
"""

fbk_file = temp_file('core_5538.fbk')
fdb_file = temp_file('core_5538.fdb')

@pytest.mark.version('>=4.0')
def test_1(act: Action, fbk_file: Path, fdb_file: Path, capsys):
    # 1. Check that we can use patterns for include data only from several selected tables:
    for i, p in enumerate(['test_doc%', 'test_d(o|u)ra', '%_w(i|o|_)n[[:DIGIT:]]', 'test_a[[:ALPHA:]]{1,}a']):
        act.reset()
        act.gbak(switches=['-b', act.db.dsn, str(fbk_file), '-include', p])
        act.reset()
        act.gbak(switches=['-rep', str(fbk_file), act.get_dsn(fdb_file)])
        act.reset()
        act.isql(switches=[act.get_dsn(fdb_file)], connect_db=False,
                 input=f"set heading off; select {i} ptn_indx, q'{{{p}}}' as ptn_text, v.* from v_test v;")
        print(act.stdout)
    # 2. Check interaction between -INCLUDE_DATA and -SKIP_DATA switches for a table:
    # We must check only conditions marked by '**':
    # +--------------------------------------------------+
    # |           |             INCLUDE_DATA             |
    # |           |--------------------------------------|
    # | SKIP_DATA |  NOT SET   |   MATCH    | NOT MATCH  |
    # +-----------+------------+------------+------------+
    # |  NOT SET  |  included  |  included  |  excluded  | <<< these rules can be skipped  in this test
    # |   MATCH   |  excluded  |**excluded**|**excluded**|
    # | NOT MATCH |  included  |**included**|**excluded**|
    # +-----------+------------+------------+------------+
    skip_ptn = 'test_d(o|u)%'
    for i, p in enumerate(['test_d%', 'test_(a|b)[[:ALPHA:]]+a']):
        act.reset()
        act.gbak(switches=['-b', act.db.dsn, str(fbk_file), '-include_data', p, '-skip_data', skip_ptn])
        act.reset()
        act.gbak(switches=['-rep', str(fbk_file), act.get_dsn(fdb_file)])
        act.reset()
        act.isql(switches=[act.get_dsn(fdb_file)], connect_db=False,
                 input=f"set heading off; select {i} ptn_indx, q'{{{p}}}' as include_ptn, q'{{{skip_ptn}}}' as exclude_ptn, v.* from v_test v;")
        print(act.stdout)
    # Check
    act.reset()
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
