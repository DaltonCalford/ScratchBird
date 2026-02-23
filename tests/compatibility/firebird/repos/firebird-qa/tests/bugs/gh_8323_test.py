#coding:utf-8

"""
ID:          issue-8323
ISSUE:       https://github.com/FirebirdSQL/firebird/pull/8323
TITLE:       Add AUTO RELEASE TEMP BLOBID transaction option
NOTES:
    [28.11.2024] pzotov
    1. Test verifies only syntax extension of SET TRANSACTION, i.e. ability to use 'AUTO RELEASE TEMP BLOBID' clause.
        Handling with temporary BLOBID can not be tested in ISQL and will be checked when firebird-driver will support this.
    2. Presense of mon$transactions.mon$auto_release_temp_blobid column not checked: FB 5.x currently missed it.

    Discussed with Vlad, letters 28.11.2024.

    Checked on 6.0.0.535, 5.0.2.1569
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set list on;
    set blob all;
    recreate table test(b blob);
    commit;

    -- makes the transaction release a temporary ID of a user BLOB just after its materialization
    set transaction AUTO RELEASE TEMP BLOBID;

    insert into test values('qwerty') returning b as blob_column_id;
    -- TEMPORARY (?) DISABLED: FB 5.X HAS NO SUCH FIELD: 
    -- select mon$auto_release_temp_blobid from mon$transactions where mon$transaction_id = current_transaction;
    commit;
"""

act = isql_act('db', test_script, substitutions = [('BLOB_COLUMN_ID .*', '')])

expected_stdout = """
    qwerty
"""

@pytest.mark.version('>=5.0.2')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
