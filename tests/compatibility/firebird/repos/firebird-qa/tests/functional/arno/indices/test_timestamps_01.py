#coding:utf-8

"""
ID:          index.timestamps
TITLE:       TIMESTAMP in index with values below julian date
DESCRIPTION:
  Datetime values below the julian date (firebird base date '1858-11-17') should be stored
  in correct order.
FBTEST:      functional.arno.indices.timestamps_01
"""

import pytest
from firebird.qa import *

init_script = """
    create table era (
      begindatetime timestamp not null,
      enddatetime timestamp not null
    );
    commit;

    insert into era (begindatetime, enddatetime) values ('1500-01-01', '1550-12-31');
    insert into era (begindatetime, enddatetime) values ('1858-11-17', '1858-11-17');
    insert into era (begindatetime, enddatetime) values ('1858-11-15 18:00', '1858-11-15 20:00');
    insert into era (begindatetime, enddatetime) values ('1858-11-16 12:00', '1858-11-16 13:00');
    insert into era (begindatetime, enddatetime) values ('1858-11-18 16:00', '1858-11-18 17:00');
    insert into era (begindatetime, enddatetime) values ('2004-04-08 02:00', '2004-04-08 02:09');
    commit;

    create unique asc index pk_begindatetime on era (begindatetime);
    create unique asc index pk_enddatetime on era (enddatetime);
    commit;
  """

db = db_factory(init=init_script)

test_script = """
    -- Queries with RANGE index scan now have in the plan only "ORDER"
    -- clause (index navigation) without bitmap building.
    -- See: http://tracker.firebirdsql.org/browse/CORE-1550
    -- ("the same index should never appear in both ORDER and INDEX parts of the same plan item")
    set list on;
    set plan on;
    select
      e.begindatetime,
      e.enddatetime
    from
      era e
    where
      e.begindatetime >= '1700-01-01'
    order by
      begindatetime asc;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)


@pytest.mark.version('>=3.0')
def test_1(act: Action):
    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    TABLE_TEST_NAME = 'E' if act.is_version('<6') else '"E"'
    INDEX_TEST_NAME = 'PK_BEGINDATETIME' if act.is_version('<6') else f'{SQL_SCHEMA_PREFIX}"PK_BEGINDATETIME"'
    expected_stdout = f"""
        PLAN ({TABLE_TEST_NAME} ORDER {INDEX_TEST_NAME})
        BEGINDATETIME                   1858-11-15 18:00:00.0000
        ENDDATETIME                     1858-11-15 20:00:00.0000
        BEGINDATETIME                   1858-11-16 12:00:00.0000
        ENDDATETIME                     1858-11-16 13:00:00.0000
        BEGINDATETIME                   1858-11-17 00:00:00.0000
        ENDDATETIME                     1858-11-17 00:00:00.0000
        BEGINDATETIME                   1858-11-18 16:00:00.0000
        ENDDATETIME                     1858-11-18 17:00:00.0000
        BEGINDATETIME                   2004-04-08 02:00:00.0000
        ENDDATETIME                     2004-04-08 02:09:00.0000
    """

    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
