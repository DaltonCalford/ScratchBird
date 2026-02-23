#coding:utf-8

"""
ID:          issue-5306
ISSUE:       5306
TITLE:       Regression: Non-indexed predicates may not be applied immediately after
  retrieval when tables are being joined
DESCRIPTION:
JIRA:        CORE-5018
FBTEST:      bugs.core_5018
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='mon-stat-gathering-3_0.fbk')

test_script = """
    recreate table zf (
      id int primary key,
      kont_id int not null
    );

    recreate table u (
      id int primary key,
      kont_id int not null
    );

    recreate table k (
      id int primary key
    );

    commit;

    insert into zf (id, kont_id) values ('1', '1');
    insert into zf (id, kont_id) values ('2', '7');
    insert into zf (id, kont_id) values ('3', '3');
    insert into zf (id, kont_id) values ('4', '5');
    insert into zf (id, kont_id) values ('5', '5');
    insert into zf (id, kont_id) values ('6', '1');
    insert into zf (id, kont_id) values ('7', '4');
    insert into zf (id, kont_id) values ('8', '2');
    insert into zf (id, kont_id) values ('9', '9');
    insert into zf (id, kont_id) values ('10', '1');


    insert into k (id) values ('1');
    insert into k (id) values ('2');
    insert into k (id) values ('3');
    insert into k (id) values ('4');
    insert into k (id) values ('5');
    insert into k (id) values ('6');
    insert into k (id) values ('7');
    insert into k (id) values ('8');
    insert into k (id) values ('9');
    insert into k (id) values ('10');

    insert into u (id, kont_id) values ('1', '4');
    insert into u (id, kont_id) values ('2', '6');
    insert into u (id, kont_id) values ('3', '3');
    insert into u (id, kont_id) values ('4', '2');
    insert into u (id, kont_id) values ('5', '5');
    insert into u (id, kont_id) values ('6', '2');
    insert into u (id, kont_id) values ('7', '9');
    insert into u (id, kont_id) values ('8', '2');
    insert into u (id, kont_id) values ('9', '10');
    insert into u (id, kont_id) values ('10', '1');

    commit;

    execute procedure sp_truncate_stat;
    commit;
    execute procedure sp_gather_stat;
    commit;

    set term ^;
    execute block as
        declare c int;
    begin
        select count(*)
        from zf
        inner join u on zf.id=u.id
        left join k kzf on zf.kont_id=kzf.id
        left join k kum on u.kont_id=kum.id
        where zf.kont_id<>u.kont_id
        into c;

        if ( rdb$get_context('SYSTEM','ENGINE_VERSION') starting with '4.0' ) then
            rdb$set_context('USER_SESSION', 'MAX_IDX_READS', '45'); -- 27.07.2016
        else
            rdb$set_context('USER_SESSION', 'MAX_IDX_READS', '30');
            --                                                 ^
            --                                                 |
            --                 ### T H R E S H O L D ###-------+
    end
    ^
    set term ;^

    execute procedure sp_gather_stat;
    commit;

    set list on;
    select iif( indexed_reads <= c_max_idx_reads,
                'ACCEPTABLE',
                'FAILED, TOO BIG: ' || indexed_reads || ' > ' || c_max_idx_reads
              ) as idx_reads_estimation
    from (
        select indexed_reads, cast(rdb$get_context('USER_SESSION', 'MAX_IDX_READS') as int) as c_max_idx_reads
        from v_agg_stat_main
    );
    -- WI-V2.5.5.26952 IR=22
    -- WI-V3.0.0.32140 IR=33
    -- WI-V3.0.0.32179 IR=25
    -- WI-T4.0.0.313:  IR=39
"""

act = isql_act('db', test_script)

expected_stdout = """
    IDX_READS_ESTIMATION            ACCEPTABLE
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

