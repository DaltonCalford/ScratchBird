#coding:utf-8

"""
ID:          issue-5056
ISSUE:       5056
TITLE:       EXECUTE STATEMENT using BLOB parameters results in "Invalid BLOB ID" error
DESCRIPTION:
JIRA:        CORE-4752
FBTEST:      bugs.core_4752
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table tb(id int, b1 blob);
    commit;
    insert into tb(id) values(1);
    insert into tb(id) values(2);
    insert into tb(id) values(3);
    commit;

    set term ^;
    execute block as
        declare v_stt varchar(255);
        declare v_blob_a blob = 'qwertyuioplkjhgfdsazxcvbnm';
        declare v_blob_b blob = '1234567890asdfghjklmnbvcxz';
    begin
        v_stt = 'update tb set b1 = case when id in (1,2) then cast(? as blob) else cast(? as blob) end';
        execute statement ( v_stt ) ( v_blob_a, v_blob_b );
    end
    ^
    set term ;^
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()
