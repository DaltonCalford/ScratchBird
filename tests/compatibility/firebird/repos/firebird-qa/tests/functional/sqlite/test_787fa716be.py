#coding:utf-8

"""
ID:          787fa716be
ISSUE:       https://www.sqlite.org/src/tktview/787fa716be
TITLE:       Assertion fault when multi-use subquery implemented by co-routine
DESCRIPTION:
NOTES:
    [18.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table artists (
      id integer not null primary key,
      name varchar(255)
    );
    create table albums (
      id integer not null primary key,
      name varchar(255),
      artist_id integer references artists
    );
    insert into artists (id, name) values (1, 'ar');
    insert into albums (id, name, artist_id) values (101, 'al', 1);

    select 'test-1' as msg from rdb$database;
    set count on;
    select artists.*
    from artists
    inner join artists as b on (b.id = artists.id)
    where (artists.id in (
      select albums.artist_id
      from albums
      where ((name = 'al')
        and (albums.artist_id is not null)
        and (albums.id in (
          select id
          from (
            select albums.id,
                   row_number() over (
                     partition by albums.artist_id
                     order by name
                   ) as x
            from albums
            where (name = 'al')
          ) as t1
          where (x = 1)
        ))
        and (albums.id in (1, 2)))
      )
    );
    set count off;
    commit;
    ---------------------------------

    recreate table t1 (a int);
    recreate table t2 (b int);
    recreate table t3 (c int);
    recreate table t4 (d int);
    insert into t1 (a) values (104);
    insert into t2 (b) values (104);
    insert into t3 (c) values (104);
    insert into t4 (d) values (104);

    select 'test-2' as msg from rdb$database;
    set count on;
    select *
    from t1 cross join t2 where (t1.a = t2.b) and t2.b in (
      select t3.c
      from t3
      where t3.c in (
        select d from (select d from t4) as innermost where innermost.d=104
      )
    );
    set count off;
    commit;
    ---------------------------------

    recreate table t5(a int, b int, c int, d int);
    create index t5a on t5(a);
    create index t5b on t5(b);
    recreate table t6(e int);

    insert into t6 values(1);
    insert into t5 values(1,1,1,1);
    insert into t5 values(2,2,2,2);

    select 'test-3' as msg from rdb$database;
    set count on;
    select * from t5 where (a=1 or b=2) and c in (
      select e from (select distinct e from t6) where e=1
    );
    set count off;
    commit;
    --------------------------------

    recreate table t1 (a int); insert into t1 (a) values (104);
    recreate table t2 (b int); insert into t2 (b) values (104);
    recreate table t3 (c int); insert into t3 (c) values (104);
    recreate table t4 (d int); insert into t4 (d) values (104);

    select 'test-4' as msg from rdb$database;
    set count on;
    select *
    from t1 cross join t2 where (t1.a = t2.b) and t2.b in (
      select t3.c
      from t3
      where t3.c in (
        select d from (select distinct d from t4) as x where x.d=104
      )
    );
    set count off;
    commit;
    --------------------------------

    recreate table t1(a1 int, a2 int, a3 int);
    create index t1a2 on t1(a2, a1);
    create index t1a3 on t1(a3, a1);
    recreate table t2(d int);

    insert into t1 values(1, 1, 1);
    insert into t1 values(2, 2, 2);
    insert into t2 values(22);

    select 'test-5' as msg from rdb$database;
    set count on;
    select * from t1 where (a2=1 or a3=2) and a1 = (
      select d from (select distinct d from t2) where d=22
    );
    set count off;
    commit;
    --------------------------------

    recreate table t0 (c0 int, c1 int, primary key (c0, c1));
    recreate table t1 (c0 int);
    insert into t1 values (2);

    select 'test-6' as msg from rdb$database;
    set count on;
    select * from t0, t1 where (t0.c1 >= 1 or t0.c1 < 1) and t0.c0 in (1, t1.c0) order by 1;
    set count off;
    commit;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    MSG test-1
    Records affected: 0
    
    MSG test-2
    A 104
    B 104
    Records affected: 1
    
    MSG test-3
    A 1
    B 1
    C 1
    D 1
    Records affected: 1
    
    MSG test-4
    A 104
    B 104
    Records affected: 1
    
    MSG test-5
    Records affected: 0
    
    MSG test-6
    Records affected: 0
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
