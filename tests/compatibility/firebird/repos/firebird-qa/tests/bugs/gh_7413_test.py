#coding:utf-8

"""
ID:          issue-7413
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7413
TITLE:       Regression: bad plan in FB 3.0.9+ (correct in FB 3.0.8) 
NOTES:
    [01.03.2023] pzotov
        Test database was created beforehand, fulfilled with data provided in the ticket, backed up and compressed.
        Checked on 3.0.11.33665, 4.0.3.2904, 5.0.0.964
    [05.07.2025] pzotov
        Separated expected output for FB major versions prior/since 6.x.
        No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
        Checked on 6.0.0.909; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
import zipfile
from pathlib import Path
from firebird.qa import *
from firebird.driver import SrvRestoreFlag

db = db_factory()

substitutions = [('[ \t]+', ' ')]
act = python_act('db', substitutions = substitutions)

fbk_file = temp_file('gh_7413.tmp.fbk')

@pytest.mark.version('>=3.0')
def test_1(act: Action, fbk_file: Path, capsys):
    zipped_fbk_file = zipfile.Path(act.files_dir / 'gh_7413.zip', at = 'gh_7413.fbk')
    fbk_file.write_bytes(zipped_fbk_file.read_bytes())
    with act.connect_server() as srv:
        srv.database.restore(database=act.db.db_path, backup=fbk_file, flags=SrvRestoreFlag.REPLACE)
        srv.wait()

    script = """
        set list on;
        set plan on;
        select
          t1.id_x,
          t1.id_t1,
          t1.total,
          t3.invoice_no,
          t3.created_at
        from t1
        join t2 on t2.id_t2 = t1.id_t1 and t2.id_x = t1.id_x
        join t3 on t3.id_t3 = t1.id_t3 and t3.id_x = t1.id_x
        where t3.invoice_no = 1683998;
    """
    
    expected_stdout_5x = """
        PLAN JOIN (T3 INDEX (XAK2T3), T1 INDEX (R_542), T2 INDEX (XPKT2))
        ID_X                            1
        ID_T1                           6026229
        TOTAL                           30.0000
        INVOICE_NO                      1683998
        CREATED_AT                      2022-11-28
    """

    expected_stdout_6x = """
        PLAN JOIN ("PUBLIC"."T3" INDEX ("PUBLIC"."XAK2T3"), "PUBLIC"."T1" INDEX ("PUBLIC"."R_542"), "PUBLIC"."T2" INDEX ("PUBLIC"."XPKT2"))
        ID_X                            1
        ID_T1                           6026229
        TOTAL                           30.0000
        INVOICE_NO                      1683998
        CREATED_AT                      2022-11-28
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.isql(switches=[], input = script, combine_output=True)

    assert act.clean_stdout == act.clean_expected_stdout
