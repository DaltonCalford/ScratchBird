#coding:utf-8

"""
ID:          issue-965
ISSUE:       965
TITLE:       Tricky role defeats basic SQL security
DESCRIPTION:
JIRA:        CORE-606
FBTEST:      bugs.core_0606
FBTEST:      bugs.core_0521
    [23.06.2025] pzotov
    Expected output was separated depending on FB version: we have to show SCHEMA name as prefix for DB object (since 6.0.0.834).
    Reimplemented: removed usage of hard-coded values for user and role name. Added substitutions to reduce irrelevant lines.

    Checked on 6.0.0.853; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

substitutions = [
    ('[ \t]+', ' '),
    ('.* Grant permissions .*', ''),
    ('Statement failed, SQLSTATE = HY000', ''),
    ('record not found for user:.*', ''), ('read/select', 'SELECT'),
    ('Data source : Firebird::.*', 'Data source : Firebird::'),
    ('(-)?At block line: [\\d]+, col: [\\d]+', ''),
    ('335545254 : Effective user is.*', '')
]

db = db_factory()
for_cvc_role = role_factory('db', name='"FOR CVC"')
for_role = role_factory('db', name='"FOR"')
cvc_user = user_factory('db', name='cvc', password='pw')

act = isql_act('db', substitutions = substitutions)

expected_stdout_5x = """
    GRANT SELECT ON t t TO ROLE FOR
    GRANT FOR CVC TO CVC
    WHO_AM_I                        CVC
    I_M_PLAYING_ROLE                FOR CVC
    Statement failed, SQLSTATE = 42000
    Execute statement error at isc_dsql_prepare :
    335544352 : no permission for SELECT access to TABLE t t
    Statement : select data from "t t"
    Data source : Firebird::
"""
expected_stdout_6x = """
    GRANT SELECT ON PUBLIC."t t" TO ROLE "FOR"
    GRANT "FOR CVC" TO CVC
    GRANT USAGE ON SCHEMA PUBLIC TO PUBLIC
    WHO_AM_I CVC
    I_M_PLAYING_ROLE FOR CVC
    Statement failed, SQLSTATE = 42000
    Execute statement error at isc_dsql_prepare :
    335544352 : no permission for SELECT access to TABLE "PUBLIC"."t t"
    Statement : select data from "t t"
    Data source : Firebird::
"""

@pytest.mark.es_eds
@pytest.mark.version('>=3')
def test_1(act: Action, cvc_user: User, for_role: Role, for_cvc_role: Role):

    test_sql = f"""
        recreate table "t t"(data int);
        commit;
        insert into "t t" values(123456);
        commit;

        grant {for_cvc_role.name} to user {cvc_user.name};
        grant select on table "t t" to {for_role.name};
        commit;

        show grants;
        commit;

        set list on;
        set term ^;
        execute block returns(who_am_i varchar(31), i_m_playing_role varchar(31)) as
        begin
          for
             execute statement 'select current_user, current_role from rdb$database'
             on external 'localhost:' || rdb$get_context('SYSTEM','DB_NAME')
             as user '{cvc_user.name}' password '{cvc_user.password}' role '{for_cvc_role.name}'
             into who_am_i, i_m_playing_role
          do
             suspend;
        end
        ^

        execute block returns(data int) as
        begin
          for
             execute statement 'select data from "t t"'
             on external 'localhost:' || rdb$get_context('SYSTEM','DB_NAME')
             as user '{cvc_user.name}' password '{cvc_user.password}' role '{for_cvc_role.name}'
             into data
          do
             suspend;
        end
        ^
        set term ;^
        commit;
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.isql(switches = ['-q'], input = test_sql, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

