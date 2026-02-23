#coding:utf-8

"""
ID:          issue-6521
ISSUE:       6521
TITLE:       Put options in user management statements in any order
DESCRIPTION:
  According to new syntax that is described in doc\\sql.extensions\\README.user_management, any statement that
  creates or modifies user, must now look like this:
    CREATE OR ALTER USER name [ SET ] [ options ];
  where OPTIONS is a list of following options:
    - PASSWORD 'password'
    - FIRSTNAME 'firstname'
    - MIDDLENAME 'middlename'
    - LASTNAME 'lastname'
    - ACTIVE
    - INACTIVE
    - USING PLUGIN name
    - TAGS ( tag [, tag [, tag ...]] )

  We add all options from this list, except 'INACTIVE', as separate records to the table 'TSYNTAX', field: 'token'.
  Then we generate all possible combinations of these options with requirement that each of them occurs in a generated
  record only once (see: f_generate_sql_with_combos).
  Query will contain 7 columns, one per each option, and we further concatenate them to the string.
  As result, this 'suffix part' will contain all tokens in all possible places will be created.
  We will add this 'suffix part' to 'create or alter user ...' statement.

  Finally, we redirect result of this query to a new .sql script (see: f_ddl_combinations_script) and run it.
  NOTE: total number of 'CREATE OR ALTER USER' statements in it will be 10080.

  Result of this .sql must be EMPTY: all statements have to be executed without error.

  It is crucial for this test to make .sql script run within SINGLE transaction otherwise performance will suffer.
  Also, we must inject 'SET BAIL ON;' at the start line of this script in order to make it stop when first error occurs.
JIRA:        CORE-6279
FBTEST:      bugs.core_6279
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table tsyntax( token varchar(100) );
    commit;
    insert into tsyntax( token ) values( 'password ''bar'' ' );
    insert into tsyntax( token ) values( 'firstname ''john'' ' );
    insert into tsyntax( token ) values( 'middlename ''ozzy'' ' );
    insert into tsyntax( token ) values( 'lastname ''osbourne'' ' );
    insert into tsyntax( token ) values( 'active' );
    insert into tsyntax( token ) values( 'inactive' );
    insert into tsyntax( token ) values( 'using plugin Srp' );
    insert into tsyntax( token ) values( 'tags ( foo = ''bar'', rio = ''gee'' )' );
    commit;

    set heading off;
    select 'set bail on;' from rdb$database union all
    select 'set echo off;' from rdb$database union all
    select 'commit;' from rdb$database union all
    select 'set autoddl off;' from rdb$database union all
    select 'commit;' from rdb$database
    ;

    with
    t as (
      select *
      from tsyntax x
      where x.token not in ('inactive')
    )
    ,b as (
        select trim(a.token) as a, trim(b.token) as b, trim(c.token) as c, trim(d.token) as d, trim(e.token) as e, trim(f.token) as f, trim(g.token) as g
        from t a
        left join t b on b.token not in (a.token)
        left join t c on c.token not in (a.token, b.token)
        left join t d on d.token not in (a.token, b.token, c.token)
        left join t e on e.token not in (a.token, b.token, c.token, d.token)
        left join t f on f.token not in (a.token, b.token, c.token, d.token, e.token)
        left join t g on g.token not in (a.token, b.token, c.token, d.token, e.token, f.token)
    )
    ,c as (
        select a || ' ' || b || ' ' || c || ' ' || d || ' ' || e || ' ' || f || ' ' || g || ';' as ddl_expr
        from b
    )
    select 'create or alter user tmp$c6279 ' || ddl_expr from c
    union all
    select 'create or alter user tmp$c6279 ' || replace(ddl_expr, ' active', ' inactive') from c;

    select 'rollback;' from rdb$database ;
"""

db = db_factory() # init_script is executed manually

test_user = user_factory('db', name='tmp$c6279', password='123')

act = python_act('db')

@pytest.mark.version('>=4.0')
def test_1(act: Action, test_user: User):
    act.isql(switches=['-q'], input=init_script)
    ddl_combinations_script = act.clean_stdout
    #
    act.reset()
    act.isql(switches=['-q'], input=ddl_combinations_script)
    assert act.clean_stdout == act.clean_expected_stdout # Must be ampty
