#coding:utf-8

"""
ID:          issue-4624
ISSUE:       4624
TITLE:       Non-ASCII data in SEC$USERS is not read correctly
DESCRIPTION:
JIRA:        CORE-4301
FBTEST:      bugs.core_4301
NOTES:
    [04.09.2024] pzotov
    Added 'using plugin Srp' into 'CREATE USER' statements, in order to check 'COMMENT ON USER' with non-ascii text.
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

# Note use of "do_not_create", as we want to create user in test script
# but we want to clean it up via fixture teardown
user_a = user_factory('db', name='u30a', password='u30a', do_not_create=True)
user_b = user_factory('db', name='u30b', password='u30b', do_not_create=True)

test_script = """
    -- Field `firstname` is defined as:
    -- VARCHAR(32) CHARACTER SET UNICODE_FSS COLLATE UNICODE_FSS
    -- we can put in it max 16 non-ascii characters
    create or alter user u30a password 'u30a' firstname 'Полиграф Шариков' using plugin Srp;
    create or alter user u30b password 'u30b' firstname 'Léopold Frédéric' using plugin Srp;
    commit;
    comment on user u30a is 'это кто-то из наших';
    comment on user u30b is 'é alguém do Brasil';
    commit;
    /*
    show domain rdb$user;
    show domain SEC$NAME_PART;
    show table sec$users;
    */
    set list on;
    select
        -- 3.x:       CHAR(31) CHARACTER SET UNICODE_FSS Nullable
        -- 4.x, 5.x: (RDB$USER) CHAR(63) Nullable
        -- FB 6.x:   (RDB$USER) CHAR(63) CHARACTER SET UTF8 Nullable
        u.sec$user_name
        ,u.sec$first_name -- (SEC$NAME_PART) VARCHAR(32) Nullable
        ,u.sec$description as descr_blob_id -- (RDB$DESCRIPTION) BLOB segment 80, subtype TEXT Nullable  
    from sec$users u
    where upper(u.sec$user_name) in (upper('u30a'), upper('u30b'));
    commit;
"""

act = isql_act('db', test_script, substitutions = [ ('DESCR_BLOB_ID.*',''),('[ \t]+',' ') ] )

expected_stdout = """
    SEC$USER_NAME                   U30A
    SEC$FIRST_NAME                  Полиграф Шариков
    это кто-то из наших

    SEC$USER_NAME                   U30B
    SEC$FIRST_NAME                  Léopold Frédéric
    é alguém do Brasil
"""

@pytest.mark.intl
@pytest.mark.version('>=3.0')
def test_1(act: Action, user_a: User, user_b: User):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

