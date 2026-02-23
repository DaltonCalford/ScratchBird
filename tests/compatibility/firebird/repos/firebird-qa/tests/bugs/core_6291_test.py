#coding:utf-8

"""
ID:          issue-6533
ISSUE:       6533
TITLE:       Statement "CREATE DOMAIN [dm_name] as BIGINT" raises "numeric value is out of range"
  if its default value is -9223372036854775808
DESCRIPTION:
JIRA:        CORE-6291
FBTEST:      bugs.core_6291
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    create domain dm_bigint_absolute_max as bigint default 9223372036854775807;
    create domain dm_bigint_absolute_min as bigint default -9223372036854775808;
    create domain dm_int128_absolute_max as int128 default  170141183460469231731687303715884105727;
    create domain dm_int128_absolute_min as int128 default -170141183460469231731687303715884105728;

    create table test1(
        n_bigint_absol_max    bigint default 9223372036854775807
       ,n_bigint_absol_min    bigint default -9223372036854775808
       ,n_int128_absol_max    int128 default  170141183460469231731687303715884105727
       ,n_int128_absol_min    int128 default -170141183460469231731687303715884105728
       ,n_dm_bigint_absol_max dm_bigint_absolute_max
       ,n_dm_bigint_absol_min dm_bigint_absolute_min
       ,n_dm_int128_absol_max dm_int128_absolute_max
       ,n_dm_int128_absol_min dm_int128_absolute_min
    );
    commit;

    insert into test1 default values;
    select * from test1;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    N_BIGINT_ABSOL_MAX              9223372036854775807
    N_BIGINT_ABSOL_MIN              -9223372036854775808
    N_INT128_ABSOL_MAX                    170141183460469231731687303715884105727
    N_INT128_ABSOL_MIN                   -170141183460469231731687303715884105728
    N_DM_BIGINT_ABSOL_MAX           9223372036854775807
    N_DM_BIGINT_ABSOL_MIN           -9223372036854775808
    N_DM_INT128_ABSOL_MAX                 170141183460469231731687303715884105727
    N_DM_INT128_ABSOL_MIN                -170141183460469231731687303715884105728
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
