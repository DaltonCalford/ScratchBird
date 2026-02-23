#coding:utf-8

"""
ID:          issue-6112
ISSUE:       6112
TITLE:       There is no check of existance generator and exception when privileges are granted
DESCRIPTION:
JIRA:        CORE-5852
FBTEST:      bugs.core_5852
NOTES:
    [02.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.889; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate view v_grants as
    select
        p.rdb$user_type       as usr_type
       ,p.rdb$user            as usr_name
       ,p.rdb$grantor         as who_gave
       ,p.rdb$privilege       as what_can
       ,p.rdb$grant_option    as has_grant
       ,p.rdb$object_type     as obj_type
       ,p.rdb$relation_name   as rel_name
       ,p.rdb$field_name      as fld_name
    from rdb$database r left join rdb$user_privileges p on 1=1
    where p.rdb$user in( upper('tmp$c5852') )
    order by 1,2,3,4,5,6,7,8
    ;

    create or alter user tmp$c5852 password '123';
    commit;

    grant usage on exception no_such_exc to user tmp$c5852;
    grant usage on generator no_such_gen to user tmp$c5852;
    grant usage on sequence no_such_seq to user tmp$c5852;

    --grant execute on procedure no_such_proc to user tmp$c5852;
    --grant execute on function no_such_func to user tmp$c5852;
    --grant execute on package no_such_pkg to user tmp$c5852;

    commit;

    set list on;
    set count on;

    select
         usr_type
        ,usr_name
        ,what_can
        ,has_grant
        ,obj_type
        ,rel_name
    from v_grants;
    commit;

    drop user tmp$c5852;
    commit;
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -GRANT failed
    -Exception NO_SUCH_EXC does not exist
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -GRANT failed
    -Generator/Sequence NO_SUCH_GEN does not exist
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -GRANT failed
    -Generator/Sequence NO_SUCH_SEQ does not exist
    Records affected: 0
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -GRANT failed
    -Exception "PUBLIC"."NO_SUCH_EXC" does not exist
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -GRANT failed
    -Generator/Sequence "PUBLIC"."NO_SUCH_GEN" does not exist
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -GRANT failed
    -Generator/Sequence "PUBLIC"."NO_SUCH_SEQ" does not exist
    Records affected: 0
"""

@pytest.mark.version('>=3.0.4')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
