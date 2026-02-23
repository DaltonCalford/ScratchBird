#coding:utf-8

"""
ID:          issue-5694
ISSUE:       5694
TITLE:       Performance degradation in FB 3.0.2 compared to FB 2.5.7
DESCRIPTION:
JIRA:        CORE-5421
FBTEST:      bugs.core_5421
NOTES:
    [01.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.881; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table tmain(id int, ekey int);
    recreate table tdetl(doc_id int, dts timestamp);

    set term ^;
    execute block as
      declare n int = 10000; -- 7600000;
      declare d int = 100; -- 5000;
      declare i int = 0;
    begin
      while ( i < n ) do
      begin
        insert into tdetl(doc_id, dts) values( :i, current_date-rand()*:d )
        returning :i+1 into i;
      end
      insert into tmain(id, ekey) values(0, 100);
    end^
    set term ;^
    commit;

    create index c5421_tmain_id on tmain(id);
    create index c5421_tmain_ekey on tmain(ekey);

    create index c5421_tdetl_doc_id on tdetl(doc_id);
    create descending index c5421_tdetl_dts on tdetl(dts);
    commit;

    --set width rel_name 8;
    --set width idx_name 30;
    --select ri.rdb$relation_name rel_name, ri.rdb$index_name idx_name, ri.rdb$statistics
    --from rdb$indices ri where ri.rdb$index_name starting with 'C5421';

    set list on;
    set plan on;
    --set stat on;
    set count on;
    select first 1 d.doc_id --, d.dts
    from tmain c
    join tdetl d ON d.doc_id = c.id
    where
        c.ekey = 100 and
        d.dts <= 'tomorrow'
    order by
        d.dts desc
    ;

"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = """
    PLAN SORT (JOIN (C INDEX (C5421_TMAIN_EKEY), D INDEX (C5421_TDETL_DOC_ID)))
    DOC_ID 0
    Records affected: 1
"""

expected_stdout_6x = """
    PLAN SORT (JOIN ("C" INDEX ("PUBLIC"."C5421_TMAIN_EKEY"), "D" INDEX ("PUBLIC"."C5421_TDETL_DOC_ID")))
    DOC_ID 0
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
