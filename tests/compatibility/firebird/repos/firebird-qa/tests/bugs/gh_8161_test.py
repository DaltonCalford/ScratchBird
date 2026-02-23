#coding:utf-8

"""
ID:          issue-8161
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8161
TITLE:       Cardinality estimation should use primary record versions only
DESCRIPTION:
    Test must use .fbk file from core-5602. It was copied to separate file and packed into gh_8161.zip
    We check explained plan of query (making 'padding' of every line with '.' character for preserving indentation).
    Statistics is not checked.
    Numeric suffixes of index names from RDB tables are suppressed because they can change in the future.
    Also, we suppress output of rows with 'line NNN, column NNN' (FB 5.x+) because they have no matter in this test.
NOTES:
    [20.06.2024] pzotov
        Despite that we use 'clean' (i.e. just restored) DB, back-versions CAN exists there for system tables,
        particularly (for this .fbk) - in rdb$dependencies and rdb$procedures.
        For that test not only back-versions but also blobs and fragments matter.
        See letters from dimitr and hvlad: 20.06.2024 10:36, 10:39.
        Confirmed bug (regression) on 3.0.12.33735 (date of build: 09-mar-2024).
    [31.10.2024] pzotov
        Adjusted expected_out discuss with dimitr: explained plan for FB 6.x became identical to FB 5.x and earlier after
        https://github.com/FirebirdSQL/firebird/commit/e7e9e01fa9d7c13d8513fcadca102d23ad7c5e2a
        ("Rework fix for #8290: Unique scan is incorrectly reported in the explained plan for unique index and IS NULL predicate")
    [25.07.2025] pzotov
        Separated test scripts and expected output for check on versions prior/since 6.x.
        On 6.x we have to take in account indexed fields containing SCHEMA names, see below their DDL in the code.
        Thanks to dimitr for suggestion.
    [09.10.2025] pzotov
        Adjusted expected_out: explained plan for FB 6.x became identical to FB 5.x and earlier after
        3e3b75f9 ("Re-add indexes for object name without schema name, removing their uniqueness (#8664)")
        ::: NB ::: Pre-created .fbk and a query to RDB tables must be used in this test (provided in the ticket).
        Otherwise problem not reproduced.

    Checked on 6.0.0.1061; 5.0.3.1686; 4.0.6.3223; 3.0.13.33818.
"""
import zipfile
from pathlib import Path
import locale
import re
from firebird.driver import DatabaseError

import pytest
from firebird.qa import *

db = db_factory()

substitutions = [('INDEX_\\d+', 'INDEX_nn'), ('\\(?line \\d+, column \\d+\\)?', '')] 

act = python_act('db', substitutions = substitutions)
tmp_fbk = temp_file('gh_8161.tmp.fbk')

#-----------------------------------------------------------

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#-----------------------------------------------------------

@pytest.mark.version('>=3.0.12')
def test_1(act: Action, tmp_fbk: Path, capsys):
    zipped_fbk_file = zipfile.Path(act.files_dir / 'gh_8161.zip', at = 'gh_8161.fbk')
    tmp_fbk.write_bytes(zipped_fbk_file.read_bytes())

    act.gbak(switches = ['-rep', str(tmp_fbk), act.db.db_path], combine_output = True, io_enc = locale.getpreferredencoding())
    print(act.stdout) # must be empty

    test_script_5x = """
        execute block as
            declare relname varchar(32);
            declare cnt int;
        begin
            for select x.rdb$relation_name
                  from rdb$relation_fields x
                 where x.rdb$field_source = upper('bool_emul')
                into :relname
            do begin
                select count(*)
                from rdb$dependencies dep, rdb$procedures prc
                where dep.rdb$depended_on_name = :relname
                  and dep.rdb$field_name = :relname
                  and dep.rdb$depended_on_type = 0 /* obj_relation */
                  and dep.rdb$dependent_type = 5 /* obj_procedure */
                  and dep.rdb$dependent_name = prc.rdb$procedure_name
                  and prc.rdb$package_name is null
              into :cnt;
            end
        end
    """

    # ::: NB :::
    # On 6.x we have to take in account indexed fields containing SCHEMA names:
    # CREATE INDEX RDB$INDEX_3 ON RDB$RELATION_FIELDS (RDB$FIELD_SOURCE_SCHEMA_NAME, RDB$FIELD_SOURCE);
    # CREATE INDEX RDB$INDEX_4 ON RDB$RELATION_FIELDS (RDB$SCHEMA_NAME, RDB$RELATION_NAME);
    # CREATE INDEX RDB$INDEX_27 ON RDB$DEPENDENCIES (RDB$DEPENDENT_SCHEMA_NAME, RDB$DEPENDENT_NAME, RDB$DEPENDENT_TYPE);
    # CREATE INDEX RDB$INDEX_28 ON RDB$DEPENDENCIES (RDB$DEPENDED_ON_SCHEMA_NAME, RDB$DEPENDED_ON_NAME, RDB$DEPENDED_ON_TYPE, RDB$FIELD_NAME);
    # ALTER TABLE RDB$PROCEDURES ADD CONSTRAINT RDB$INDEX_21 UNIQUE (RDB$SCHEMA_NAME, RDB$PACKAGE_NAME, RDB$PROCEDURE_NAME);
    # ALTER TABLE RDB$PROCEDURES ADD CONSTRAINT RDB$INDEX_22 UNIQUE (RDB$PROCEDURE_ID);
    
    test_script_6x = """
        execute block as
            declare relname varchar(32);
            declare cnt int;
        begin
            for select x.rdb$relation_name
                  from rdb$relation_fields x
                 where
                     x.rdb$schema_name = upper('public')
                     and x.rdb$field_source = upper('bool_emul')
                into :relname
            do begin
                select count(*)
                from rdb$dependencies dep, rdb$procedures prc
                where
                  dep.rdb$depended_on_schema_name = upper('public')
                  and dep.rdb$depended_on_name = :relname
                  and dep.rdb$field_name = :relname
                  and dep.rdb$depended_on_type = 0 /* obj_relation */
                  and dep.rdb$dependent_type = 5 /* obj_procedure */
                  and dep.rdb$dependent_name = prc.rdb$procedure_name
                  and prc.rdb$schema_name = upper('public')
                  and prc.rdb$package_name is null
              into :cnt;
            end
        end
    """

    test_sql = test_script_5x if act.is_version('<6') else test_script_6x
    with act.db.connect() as con:
        cur = con.cursor()
        ps = None
        try:
            ps = cur.prepare(test_sql)
            print( '\n'.join([replace_leading(s) for s in ps.detailed_plan.split('\n')]) )
        except DatabaseError as e:
            print(e.__str__())
            print(e.gds_codes)
        finally:
            if ps:
                ps.free()

    expected_stdout_5x = """
        Select Expression
        ....-> Singularity Check
        ........-> Aggregate
        ............-> Nested Loop Join (inner)
        ................-> Filter
        ....................-> Table "RDB$DEPENDENCIES" as "DEP" Access By ID
        ........................-> Bitmap
        ............................-> Index "RDB$INDEX_nn" Range Scan (full match)
        ................-> Filter
        ....................-> Table "RDB$PROCEDURES" as "PRC" Access By ID
        ........................-> Bitmap
        ............................-> Index "RDB$INDEX_nn" Unique Scan
        Select Expression
        ....-> Filter
        ........-> Table "RDB$RELATION_FIELDS" as "X" Access By ID
        ............-> Bitmap
        ................-> Index "RDB$INDEX_nn" Range Scan (full match)
    """

    expected_stdout_6x = """
        Select Expression
        ....-> Singularity Check
        ........-> Aggregate
        ............-> Nested Loop Join (inner)
        ................-> Filter
        ....................-> Table "SYSTEM"."RDB$DEPENDENCIES" as "DEP" Access By ID
        ........................-> Bitmap
        ............................-> Index "SYSTEM"."RDB$INDEX_nn" Range Scan (full match)
        ................-> Filter
        ....................-> Table "SYSTEM"."RDB$PROCEDURES" as "PRC" Access By ID
        ........................-> Bitmap
        ............................-> Index "SYSTEM"."RDB$INDEX_nn" Unique Scan
        Select Expression
        ....-> Filter
        ........-> Table "SYSTEM"."RDB$RELATION_FIELDS" as "X" Access By ID
        ............-> Bitmap
        ................-> Index "SYSTEM"."RDB$INDEX_nn" Range Scan (full match)
    """
    
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
