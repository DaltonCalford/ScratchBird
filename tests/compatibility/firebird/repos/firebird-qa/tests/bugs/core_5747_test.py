#coding:utf-8

"""
ID:          issue-6010
ISSUE:       6010
TITLE:       User can grant usage privilege by himself
DESCRIPTION:
  beside generator we also have to check the same issue about grant usage on exception.
JIRA:        CORE-5747
FBTEST:      bugs.core_5747
NOTES:
    [02.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.881; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    commit;
    connect '$(DSN)' user sysdba password 'masterkey';

    create or alter user tmp$c5747 password '123';
    commit;

    recreate sequence gen_for_dba_only;
    recreate exception exc_for_dba_only 'Your names is: @1 - and you should not be able to use this exception!';
    commit;

    connect '$(DSN)' user tmp$c5747 password '123';

    grant usage on generator gen_for_dba_only to tmp$c5747;
    grant usage on exception exc_for_dba_only to tmp$c5747;
    commit;

    connect '$(DSN)' user tmp$c5747 password '123';

    set list on;

    select gen_id(gen_for_dba_only,1) as next_secret_system_sequence from rdb$database;
    set term ^;
    execute block as
    begin
        exception exc_for_dba_only using (current_user);
    end
    ^
    set term ;^

    commit;
    connect '$(DSN)' user sysdba password 'masterkey';
    drop user tmp$c5747;
    commit;

"""

substitutions = [('no G privilege with grant option on object .*',
                  'no USAGE privilege with grant option on object'),
                 ('GEN_FOR_DBA_ONLY', ''), ('EXC_FOR_DBA_ONLY', '')]

act = isql_act('db', test_script, substitutions=substitutions)

expected_stdout_3x = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -GRANT failed
    -no USAGE privilege with grant option on object
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -GRANT failed
    -no USAGE privilege with grant option on object
    Statement failed, SQLSTATE = 28000
    no permission for USAGE access to GENERATOR
    Statement failed, SQLSTATE = 28000
    no permission for USAGE access to EXCEPTION
"""

expected_stdout_5x = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -GRANT failed
    -no USAGE privilege with grant option on object
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -GRANT failed
    -no USAGE privilege with grant option on object
    Statement failed, SQLSTATE = 28000
    no permission for USAGE access to GENERATOR
    -Effective user is TMP$C5747
    Statement failed, SQLSTATE = 28000
    no permission for USAGE access to EXCEPTION
    -Effective user is TMP$C5747
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -GRANT failed
    -no USAGE privilege with grant option on object "PUBLIC".""
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -GRANT failed
    -no USAGE privilege with grant option on object "PUBLIC".""
    Statement failed, SQLSTATE = 28000
    no permission for USAGE access to GENERATOR "PUBLIC".""
    -Effective user is TMP$C5747
    Statement failed, SQLSTATE = 28000
    no permission for USAGE access to EXCEPTION "PUBLIC".""
    -Effective user is TMP$C5747
"""


@pytest.mark.version('>=3.0')
def test_2(act: Action):

    act.expected_stdout = expected_stdout_3x if act.is_version('<4') else expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
