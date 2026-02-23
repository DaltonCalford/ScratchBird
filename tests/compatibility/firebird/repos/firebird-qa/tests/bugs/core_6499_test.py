#coding:utf-8

"""
ID:          issue-6729
ISSUE:       6729
TITLE:       Regression: gstat with switch -t executed via services fails with "found unknown switch" error
DESCRIPTION:
    Test creates several tables and request statistics for one of them usin Services API.
    Output must contain for one and only one (selected) table - TEST_01 (and its index).
    All lines from output which do not include this name are ignored (see 'subst' section).
JIRA:        CORE-6499
FBTEST:      bugs.core_6499
NOTES:
    [16.10.2025] pzotov
    Fixed in:
        4.x ('master' at that time): 10-mar-2021,
            https://github.com/FirebirdSQL/firebird/commit/6d8be2caee3c99101638bc90e5c0a13b3221dea2
        3.x (backported): 10-mar-2021,
            https://github.com/FirebirdSQL/firebird/commit/f0c193def839069459d5b12ba3dcaf65deec36a1

    Probably, the bug could be reproduced in versions of firebird-driver before 1.3.4 when it
    did not use isc_spb_sts_table tag during creation of SPB, as noted by Alex:
    https://github.com/FirebirdSQL/firebird/issues/6729#issuecomment-826248689

    Following dirrerence exists between versions 1.3.3 (08-oct-2021) and 1.3.4 (30-nov-2021)
    of firebird-driver (file core.py, class class ServerDbServices3, def get_statistics):
        index 2ecd6ea..0f74e60 100644
        --- a/core.py-1.3.3
        +++ b/core.py-1.3.4
        @@ -13,15 +13,13 @@
                 self._srv()._reset_output()
                 with a.get_api().util.get_xpb_builder(XpbKind.SPB_START) as spb:
                     spb.insert_tag(ServerAction.DB_STATS)
        -            spb.insert_string(SPBItem.DBNAME, database)
        +            spb.insert_string(SPBItem.DBNAME, database, encoding=self._srv().encoding)
                     spb.insert_int(SPBItem.OPTIONS, flags)
                     if role is not None:
        -                spb.insert_string(SPBItem.SQL_ROLE_NAME, role)
        +                spb.insert_string(SPBItem.SQL_ROLE_NAME, role, encoding=self._srv().encoding)
                     if tables is not None:
        -                cmdline = ['-t']
        -                cmdline.extend(tables)
        -                for item in cmdline:
        -                    spb.insert_string(SPBItem.COMMAND_LINE, item)
        +                for table in tables:
        +                    spb.insert_string(64, table, encoding=self._srv().encoding) # isc_spb_sts_table = 64
                     self._srv()._svc.start(spb.get_buffer())
                 if callback:
                     for line in self._srv():
    
    Bug can NOT be reproduced using current version of firebird-driver (2.0.2), checked on:
        4.0.0.2382 Release Candidate 1 // 21-feb-2021
        4.0.0.2382 Release Candidate 1 // 04-mar-2021
        3.0.7.33388 // 07-nov-2020
        3.0.8.33423 // 04-mar-2021
    Because of that, i've decided to DISABLE this test at all.
    Separate test for check gstat output will be implemented (including test of presense of SQL schemas that have been
    introduced in 6.0.0.813).
"""

import pytest
from firebird.qa import *
from firebird.driver import SrvStatFlag

substitutions = [('^((?!TEST_01\\s+\\(|TEST_01_ID\\s+\\().)*$', ''),
                 ('TEST_01\\s+\\(.*', 'TEST_01'),
                 ('Index TEST_01_ID\\s+\\(.*', 'Index TEST_01_ID'), ('[ \t]+', ' ')]

init_script = """
    recreate table test_01(id int);
    recreate table test__01(id int);
    recreate table test__011(id int);
    commit;
    insert into test_01 select row_number()over() from rdb$types;
    commit;
    create index test_01_id on test_01(id);
    commit;
"""

db = db_factory(init=init_script)

act = python_act('db', substitutions=substitutions)

expected_stdout = """
    TEST_01 (128)
    Index TEST_01_ID (0)
"""

@pytest.mark.skip('NOT REPRODUCIBLE, SEE NOTES.')
@pytest.mark.version('>=3.0.7')
def test_1(act: Action, capsys):
    with act.connect_server() as srv:
        srv.database.get_statistics(
            database=act.db.db_path
           ,tables=['TEST_01']
           ,flags=SrvStatFlag.DATA_PAGES | SrvStatFlag.IDX_PAGES
           ,callback=act.print_callback
        )
        # print( '\n'.join([x.strip() for x in srv.readlines()]) )
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
