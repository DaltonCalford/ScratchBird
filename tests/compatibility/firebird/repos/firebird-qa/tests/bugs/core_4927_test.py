#coding:utf-8

"""
ID:          issue-5218
ISSUE:       5218
TITLE:       IIF function prevents the condition from being pushed into the union for better optimization
DESCRIPTION:
JIRA:        CORE-4927
FBTEST:      bugs.core_4927
NOTES:
    [30.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.881; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

init_script = """
    create or alter procedure sp_test as begin end;
    recreate view vd_union as select 1 id from rdb$database;
    recreate table header_2100(dd_id int, ware_id int, snd_optype_id int);
    recreate table detail_1000 (ware_id int,snd_optype_id int,rcv_optype_id int,snd_id int);
    recreate table detail_1200 (ware_id int,snd_optype_id int,rcv_optype_id int,snd_id int);
    recreate table detail_2000 (ware_id int,snd_optype_id int,rcv_optype_id int,snd_id int);
    recreate table detail_2100 (ware_id int,snd_optype_id int,rcv_optype_id int,snd_id int);
    recreate table detail_3300 (ware_id int,snd_optype_id int,rcv_optype_id int,snd_id int);
    recreate view vd_union as
    select 'd1000' src,q.*
    from detail_1000 q
    union all
    select 'd1200', q.*
    from detail_1200 q
    union all
    select 'd2000', q.*
    from detail_2000 q
    union all
    select 'd2100', q.*
    from detail_2100 q
    union all
    select 'd3300', q.*
    from detail_3300 q
    ;
    commit;

    set term ^;
    create or alter procedure sp_test returns(result int) as
    begin
        for
            select count(*)
            from (
                select
                    d.dd_id,
                    d.ware_id,
                    iif(1 = 0, 3300, 2100) as snd_optype_id -- this caused engine to unnecessary scans of tables which did not contain data searched for
                from header_2100 d
            ) d
            left join vd_union qd on
                qd.ware_id = d.ware_id
                and qd.snd_optype_id = d.snd_optype_id
                and qd.rcv_optype_id is not distinct from 3300
                and qd.snd_id = d.dd_id
            into result
        do
           suspend;
    end
    ^
    set term ;^
    commit;

    insert into header_2100(dd_id, ware_id, snd_optype_id) values(1, 11, 2100);
    commit;

    insert into detail_1000 (ware_id,snd_optype_id,rcv_optype_id,snd_id) values( 11, 1000, 1200, 1);
    insert into detail_1200 (ware_id,snd_optype_id,rcv_optype_id,snd_id) values( 11, 1200, 2000, 1);
    insert into detail_2000 (ware_id,snd_optype_id,rcv_optype_id,snd_id) values( 11, 2000, 2100, 1);
    insert into detail_2100 (ware_id,snd_optype_id,rcv_optype_id,snd_id) values( 11, 2100, 3300, 1);
    insert into detail_3300 (ware_id,snd_optype_id,rcv_optype_id,snd_id) values( 11, 3300, null, 1);
    commit;

    create index d1000_wsrs on detail_1000 (ware_id,snd_optype_id,rcv_optype_id,snd_id);
    create index d1200_wsrs on detail_1200 (ware_id,snd_optype_id,rcv_optype_id,snd_id);
    create index d2000_wsrs on detail_2000 (ware_id,snd_optype_id,rcv_optype_id,snd_id);
    create index d2100_wsrs on detail_2100 (ware_id,snd_optype_id,rcv_optype_id,snd_id);
    create index d3300_wsrs on detail_3300 (ware_id,snd_optype_id,rcv_optype_id,snd_id);
    commit;
"""

db = db_factory(init=init_script)

substitutions = [
    ('^((?!HEADER_|DETAIL_).)*$', ''),
    ('(")?HEADER_2100(")?.*', 'HEADER_2100'),
    ('(")?DETAIL_2100(")?.*', 'DETAIL_2100'),
    ('"PUBLIC"', 'PUBLIC')
]

act = python_act('db', substitutions = substitutions)

trace = ['time_threshold = 0',
         'log_initfini = false',
         'log_statement_finish = true',
         'print_perf = true',
         ]

@pytest.mark.trace
@pytest.mark.version('>=3.0')
def test_1(act: Action, capsys):
    with act.trace(db_events=trace):
        act.isql(switches=[], input='set list on; select result from sp_test;')

    expected_stdout_5x = """
        HEADER_2100
        DETAIL_2100
    """

    expected_stdout_6x = """
        PUBLIC.HEADER_2100
        PUBLIC.DETAIL_2100
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.trace_to_stdout()
    assert act.clean_stdout == act.clean_expected_stdout
