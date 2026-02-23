#coding:utf-8

"""
ID:          issue-7398
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7398
TITLE:       Worst plan sort created to execute an indexed tables
DESCRIPTION:
NOTES:
    [29.09.2024] pzotov
        1. Ineffective execution plan was up to 4.0.3.2840.
           Since 4.0.3.2843 plan changed and is the same for all subsequent FB-4.x snapshots.
           Commit: https://github.com/FirebirdSQL/firebird/commit/1b192404d43a15d403b5ff92760bc5df9d3c89c3
           (13.09.2022 19:17, "More complete solution for #3357 and #7118")

        2. Database provided in the ticket has too big size (~335 Mb).
           Test uses much smaller DB that was created on basis of original one by
           extraction of small portions of data from tables PCP_TIN_REC_MAT and INV_ETQ_MAT.
           These tables in original DB have 114115 and 1351211 rows.
           In DB that is used here these tables have 15000 and 30000 rows corresp.
           NOT all constraints are used in the test DB. Particularly, following DDL were abandoned:
               ALTER TABLE PCP_TIN_REC ADD CONSTRAINT FK_PCP_TIN_REC_EMP FOREIGN KEY (ID_EMP) REFERENCES SYS_EMP (ID_EMP);
               ALTER TABLE PCP_TIN_REC ADD CONSTRAINT FK_PCP_TIN_REC_OP FOREIGN KEY (ID_OP) REFERENCES PCP_OP (ID_OP);
               ALTER TABLE PCP_TIN_REC_MAT ADD CONSTRAINT FK_PCP_TIN_REC_MAT_MAT FOREIGN KEY (ID_MAT) REFERENCES INV_MAT (ID_MAT);
           Test database have been backed up using 4.0.3.2840 and compressed to .zip file.
        3. Because of missed valuable part of source data, I'm not sure that this test verifies exactly ticket issue.
           But in any case, using this test one may see difference in execution plan that is produced in 4.0.3.2840 and 4.0.3.2843.
           And such difference also can be seen on original DB (although plans there differ from those which are in test DB).

        Checked on 6.0.0.471, 5.0.2.1519, 4.0.6.3157.
    [05.07.2025] pzotov
        Added substitution to suppress all except sqltype and fields name from SQLDA output.
        Checked on 6.0.0.892; 5.0.3.1668.
    [15.01.2026] pzotov
        Execution plan has changed since 6.0.0.1393-f7068cd.
        See  e8de18c2, "Some adjustments to the selectivity factors".
        New plan can be used instead of old one (letter from dimitr, 14.01.2026 13:05).
        NOTE. This test may be changed soon again.
        Checked on 6.0.0.1393-f7068cd.
"""

import locale
import re
import zipfile
from pathlib import Path
from firebird.driver import SrvRestoreFlag, DatabaseError
import time

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

check_sql = """
    select r.id_op, r.id_rec, sum(m.q_mat * cus.cus_med)
    from pcp_tin_rec r
    join pcp_tin_rec_mat m on r.id_rec = m.id_rec
    join inv_etq_mat cus on cus.id_mat = m.id_mat and cus.anomes = r.am_bai
    join inv_etq_nat nat on nat.id_nat = cus.id_nat
    where
        nat.cml_stat = 1 and r.id_op = 216262
    group by r.id_op, r.id_rec
"""

fbk_file = temp_file('gh_7398.tmp.fbk')

#-----------------------------------------------------------

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#-----------------------------------------------------------

expected_out_4x = """
    Select Expression
    ....-> Aggregate
    ........-> Sort (record length: 148, key length: 16)
    ............-> Nested Loop Join (inner)
    ................-> Filter
    ....................-> Table "PCP_TIN_REC" as "R" Full Scan
    ................-> Filter
    ....................-> Table "PCP_TIN_REC_MAT" as "M" Access By ID
    ........................-> Bitmap
    ............................-> Index "FK_PCP_TIN_REC_MAT_REC" Range Scan (full match)
    ................-> Filter
    ....................-> Table "INV_ETQ_MAT" as "CUS" Access By ID
    ........................-> Bitmap
    ............................-> Index "IDX_INV_ETQ_MAT_ANOMES" Range Scan (full match)
    ................-> Filter
    ....................-> Table "INV_ETQ_NAT" as "NAT" Access By ID
    ........................-> Bitmap
    ............................-> Index "PK_INV_ETQ_NAT" Unique Scan
"""
expected_out_5x = """
    Select Expression
    ....-> Aggregate
    ........-> Sort (record length: 148, key length: 16)
    ............-> Filter
    ................-> Hash Join (inner)
    ....................-> Nested Loop Join (inner)
    ........................-> Filter
    ............................-> Table "PCP_TIN_REC" as "R" Full Scan
    ........................-> Filter
    ............................-> Table "PCP_TIN_REC_MAT" as "M" Access By ID
    ................................-> Bitmap
    ....................................-> Index "FK_PCP_TIN_REC_MAT_REC" Range Scan (full match)
    ........................-> Filter
    ............................-> Table "INV_ETQ_MAT" as "CUS" Access By ID
    ................................-> Bitmap
    ....................................-> Index "IDX_INV_ETQ_MAT_ANOMES" Range Scan (full match)
    ....................-> Record Buffer (record length: 33)
    ........................-> Filter
    ............................-> Table "INV_ETQ_NAT" as "NAT" Access By ID
    ................................-> Bitmap
    ....................................-> Index "IDX_INV_ETQ_NAT_CML_STAT" Range Scan (full match)
"""

expected_out_6x = """
    Select Expression
    ....-> Aggregate
    ........-> Sort (record length: 148, key length: 16)
    ............-> Filter
    ................-> Hash Join (inner) (keys: 1, total key length: 4)
    ....................-> Nested Loop Join (inner)
    ........................-> Hash Join (inner) (keys: 1, total key length: 4)
    ............................-> Table "PUBLIC"."PCP_TIN_REC_MAT" as "M" Full Scan
    ............................-> Record Buffer (record length: 33)
    ................................-> Filter
    ....................................-> Table "PUBLIC"."PCP_TIN_REC" as "R" Full Scan
    ........................-> Filter
    ............................-> Table "PUBLIC"."INV_ETQ_MAT" as "CUS" Access By ID
    ................................-> Bitmap
    ....................................-> Index "PUBLIC"."IDX_INV_ETQ_MAT_ANOMES" Range Scan (full match)
    ....................-> Record Buffer (record length: 33)
    ........................-> Filter
    ............................-> Table "PUBLIC"."INV_ETQ_NAT" as "NAT" Access By ID
    ................................-> Bitmap
    ....................................-> Index "PUBLIC"."IDX_INV_ETQ_NAT_CML_STAT" Range Scan (full match)
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action, fbk_file: Path, capsys):
    zipped_fbk_file = zipfile.Path(act.files_dir / 'gh_7398.zip', at = 'gh_7398.fbk')
    fbk_file.write_bytes(zipped_fbk_file.read_bytes())

    with act.connect_server(encoding=locale.getpreferredencoding()) as srv:
        srv.database.restore(database = act.db.db_path, backup = fbk_file, flags = SrvRestoreFlag.REPLACE)
        restore_log = srv.readlines()
        assert restore_log == []


    with act.db.connect() as con:
        chk_sql = 'select 1 from test order by id'
        cur = con.cursor()
        ps = None
        try:
            ps = cur.prepare(check_sql)
            # Print explained plan with padding eash line by dots in order to see indentations:
            print( '\n'.join([replace_leading(s) for s in ps.detailed_plan.split('\n')]) )
        except DatabaseError as e:
            print( e.__str__() )
            print(e.gds_codes)
        finally:
            if ps:
                ps.free()
    
    act.expected_stdout = expected_out_4x if act.is_version('<5') else expected_out_5x if act.is_version('<6') else expected_out_6x

    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
