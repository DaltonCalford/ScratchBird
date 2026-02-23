#coding:utf-8

"""
ID:          issue-8115
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8115
TITLE:       Avoid reading/hashing the inner stream(s) if the leader stream is empty
DESCRIPTION:
    Original title: "FB 5.0.0.1306 - unexpected results using LEFT JOIN with When "
NOTES:
    [16.09.2024] pzotov
        Confirmed bug in 5.0.1.1369-8c31082 (17.03.2024)
        Bug was fixed in 5.0.1.1369-bbd35ab (20.03.2024)
        Commit:
            https://github.com/FirebirdSQL/firebird/commit/bbd35ab07c129e9735f081fcd29172a8187aa8ab
            Avoid reading/hashing the inner stream(s) if the leader stream is empty
        Checked on 6.0.0.457, 5.0.2.1499
    [27.03.2025] pzotov
        Explained plan for 6.x became the same as for 5.x, see commit:
        https://github.com/FirebirdSQL/firebird/commit/6c21404c6ef800ceb7d3bb9c97dc8249431dbc5b
        Comparison of actual output must be done with single expected_out in both 5.x and 6.x.
        Plan for 5.x (with TWO 'Filter' clauses) must be considered as more effective.
        Disussed with dimitr, letter 16.09.2024 17:55.
        Checked on 6.0.0.698-6c21404.
    [06.07.2025] pzotov
        Added 'SQL_SCHEMA_PREFIX' to be substituted in expected_* on FB 6.x
        Checked on 6.0.0.914; 5.0.3.1668.
"""
import zipfile
from pathlib import Path
import locale
import re

from firebird.driver import DatabaseError
import pytest
from firebird.qa import *

db = db_factory()

substitutions = [('INDEX_\\d+', 'INDEX_nn'),] 

act = python_act('db', substitutions = substitutions)
tmp_fbk = temp_file('gh_8115.tmp.fbk')

#-----------------------------------------------------------

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#-----------------------------------------------------------

@pytest.mark.version('>=5.0.1')
def test_1(act: Action, tmp_fbk: Path, capsys):
    zipped_fbk_file = zipfile.Path(act.files_dir / 'gh_8115.zip', at = 'gh_8115.fbk')
    tmp_fbk.write_bytes(zipped_fbk_file.read_bytes())

    act.gbak(switches = ['-rep', str(tmp_fbk), act.db.db_path], combine_output = True, io_enc = locale.getpreferredencoding())
    print(act.stdout) # must be empty

    test_sql = """
        select /* trace_me */ aa.id, ab.CNP_USER, ab.ID_USER
        from sal_inperioada2('7DC51501-0DF2-45BE-93E5-382A541505DE', '15.05.2024') aa
        left join user_cnp(aa.cnp, '15.05.2024') ab on ab.CNP_USER = aa.cnp
        where ab.ID_USER = '04B23787-2C7F-451A-A12C-309F79D6F13A'
    """

    with act.db.connect() as con:
        cur = con.cursor()
        ps, rs = None, None
        try:
            ps = cur.prepare(test_sql)
            print( '\n'.join([replace_leading(s) for s in ps.detailed_plan.split('\n')]) )

            # ::: NB ::: 'ps' returns data, i.e. this is SELECTABLE expression.
            # We have to store result of cur.execute(<psInstance>) in order to
            # close it explicitly.
            # Otherwise AV can occur during Python garbage collection and this
            # causes pytest to hang on its final point.
            # Explained by hvlad, email 26.10.24 17:42
            rs = cur.execute(ps)
            for r in rs:
                for p in r:
                    print(p)
        except DatabaseError as e:
            print(e.__str__())
            print(e.gds_codes)
        except Error as x:
            print(x)
        finally:
            if rs:
                rs.close() # <<< EXPLICITLY CLOSING CURSOR RESULTS
            if ps:
                ps.free()


    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else  '"PUBLIC".'
    expected_stdout = f"""
        Select Expression
        ....-> Nested Loop Join (inner)
        ........-> Procedure {SQL_SCHEMA_PREFIX}"SAL_INPERIOADA2" as "AA" Scan
        ........-> Filter
        ............-> Filter
        ................-> Procedure {SQL_SCHEMA_PREFIX}"USER_CNP" as "AB" Scan

        000DD4E1-B4D0-4D6E-9D9F-DE9A7D0D6492
        E574F734-CECB-4A8F-B9BE-FAF51BC61FAD
        04B23787-2C7F-451A-A12C-309F79D6F13A
    """
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
