#coding:utf-8

"""
ID:          issue-1574
ISSUE:       1574
TITLE:       Activating index change "STARTING" working as "LIKE" in join condition
DESCRIPTION:
JIRA:        CORE-1153
FBTEST:      bugs.core_1153
NOTES:
    [24.06.2025] pzotov
    Separated execution plans for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema name and quotes to enclosing object names.
    Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.858; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table tdetl (
        sid  varchar(40)
    );
    create table tmain (
        sid  varchar(40)
    );

    insert into tdetl(sid) values('AAA');
    insert into tdetl(sid) values('aaa');
    insert into tdetl(sid) values('Aaa Aaa');
    insert into tdetl(sid) values('BBB');
    insert into tdetl(sid) values('BBB');
    insert into tdetl(sid) values('CCC');
    commit;

    insert into tmain(sid) values ('AAA Aaa');
    insert into tmain(sid) values ('AAA Bbb');
    insert into tmain(sid) values ('DDD Ddd');
    insert into tmain(sid) values ('Bbb Aaa');
    insert into tmain(sid) values ('Bbb Bbb');
    commit;

    create index d_idx1 on tdetl computed by (upper(sid));
    create index m_idx1 on tmain computed by (upper(sid));
    commit;

    set list on;
    set plan on;
    alter index d_idx1 inactive;

    select distinct m.sid as m_sid, d.sid as d_did
    from tmain m
    left outer join tdetl d
      on upper(m.sid) starting upper(d.sid)
    order by m.sid;

    alter index d_idx1 active;

    select distinct m.sid as m_sid, d.sid as d_sid
    from tmain m
    left outer join tdetl d
      on upper(m.sid) starting upper(d.sid)
    order by m.sid;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

@pytest.mark.version('>=3')
def test_1(act: Action):

    if act.is_version('<6'):
        qry_plan = 'PLAN SORT (JOIN (M NATURAL, D NATURAL))'
    else:
        qry_plan = 'PLAN SORT (JOIN ("M" NATURAL, "D" NATURAL))'

    expected_stdout = f"""
        {qry_plan}
        M_SID AAA Aaa
        D_DID AAA
        M_SID AAA Aaa
        D_DID Aaa Aaa
        M_SID AAA Aaa
        D_DID aaa
        M_SID AAA Bbb
        D_DID AAA
        M_SID AAA Bbb
        D_DID aaa
        M_SID Bbb Aaa
        D_DID BBB
        M_SID Bbb Bbb
        D_DID BBB
        M_SID DDD Ddd
        D_DID <null>

        {qry_plan}
        M_SID AAA Aaa
        D_SID AAA
        M_SID AAA Aaa
        D_SID Aaa Aaa
        M_SID AAA Aaa
        D_SID aaa
        M_SID AAA Bbb
        D_SID AAA
        M_SID AAA Bbb
        D_SID aaa
        M_SID Bbb Aaa
        D_SID BBB
        M_SID Bbb Bbb
        D_SID BBB
        M_SID DDD Ddd
        D_SID <null>
    """

    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

