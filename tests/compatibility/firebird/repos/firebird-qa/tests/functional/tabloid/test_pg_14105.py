#coding:utf-8

"""
ID:          tabloid.pg-14105
TITLE:       Check ability to compile query with combination of full and right join. Taken from PG bug library.
DESCRIPTION: 
  Original issue ( http://www.postgresql.org/message-id/20160420194758.22924.80319@wrigleys.postgresql.org ):
     ===
        create table a as (select 1 as id);
        select *
        from ((
               a as a1
               full join (select 1 as id) as tt
               on (a1.id = tt.id)
              )
              right join (select 1 as id) as tt2
              on (coalesce(tt.id) = tt2.id)
             )
        ;
        ERROR:  XX000: failed to build any 2-way joins
        LOCATION:  standard_join_search, allpaths.c:1832
  
  
        It works on PostgreSQL 9.2.13., returning:
         id | id | id 
        ----+----+----
          1 |  1 |  1
        (1 row)
      ===
      PS. NOTE on strange form of COALESCE: "coalesce(tt.id)" - it has only single argument.
FBTEST:      functional.tabloid.pg_14105
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table a(id int);
    commit;

    insert into a values(1);
    commit;

    set list on;

    select *
    from (
        select a1.id as a1_id, tt.id as tt_id, tt2.id as tt2_id
        from 
            (
               a as a1
               full join ( select 1 as id from rdb$database ) as tt 
                    on ( a1.id = tt.id )
            )
            right join (select 1 as id from rdb$database) as tt2
            	  -- https://www.drupal.org/node/336712
                  --  When used with one argument, it will return NULL if that argument is NULL; 
                  -- therefore, in such cases, using it is perfectly useless.
                  on ( coalesce(tt.id,tt.id) = tt2.id )
    )
    ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    A1_ID                           1
    TT_ID                           1
    TT2_ID                          1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
