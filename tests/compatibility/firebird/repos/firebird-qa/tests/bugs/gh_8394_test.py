#coding:utf-8

"""
ID:          issue-8394
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8394
TITLE:       Make Trace use HEX representation for parameter values of types [VAR]CHAR CHARACTER SET OCTETS and [VAR]BINARY
DESCRIPTION:
NOTES:
    [22.01.2025] pzotov
    Checked on 5.0.2.1601, 6.0.0.594
"""
import re

import pytest
from firebird.qa import *

db = db_factory(page_size = 8192)

substitutions = [
    ( r'Blobs:\s+\d+,\s+total\s+length:\s+\d+,\s+blob\s+pages:\s+\d+', 'Blobs: N, total length: M, blob pages: P')
   ,( r'\d+,\s+total\s+length: \d+,\s+blob\s+pages:\s+\d+', 'X, total length: M, blob pages: P')
   ,( r'Table\s+size:\s+\d+\s+bytes', 'Table size: N bytes')
]
act = python_act('db', substitutions = substitutions)

test_sql = """
    recreate sequence g;
    recreate table test(id int, b blob);

    -- https://firebirdsql.org/file/documentation/chunk/en/refdocs/fblangref30/fblangref30-datatypes-bnrytypes.html
    -- 3.7.2. BLOB Specifics / BLOB Storage
    -- * By default, a regular record is created for each BLOB and it is stored on a data page that is allocated for it.
    --   If the entire BLOB fits onto this page, it is called a level 0 BLOB.
    -- * The number of this special record is stored in the table record and occupies 8 bytes.
    --   If a BLOB does not fit onto one data page, its contents are put onto separate pages allocated exclusively to it (blob pages),
    --   while the numbers of these pages are stored into the BLOB record. This is a level 1 BLOB.
    -- * If the array of page numbers containing the BLOB data does not fit onto a data page, the array is put on separate blob pages,
    --   while the numbers of these pages are put into the BLOB record. This is a level 2 BLOB.

    set term ^;
    execute block as
        declare n int;
    begin
        insert into test(id, b) values(gen_id(g,1), gen_uuid());
        insert into test(id, b)
        values(
                gen_id(g,1)
                ,(select list(gen_uuid()) as s from rdb$types,rdb$types)
              );

        insert into test(id, b)
        values(
                gen_id(g,1)
                ,(select list(gen_uuid()) as s from (select 1 x from rdb$types,rdb$types,rdb$types rows 800000))
              );
    end
    ^
    set term ;^
    commit;
"""

@pytest.mark.version('>=5.0.2')
def test_1(act: Action, capsys):

    act.isql(switches = ['-q'], input = test_sql, combine_output = True)

    # Pipe of command to ISQL before 6.x leads to appearing of following 'noise info':
    # Database: localhost:..., User: SYSDBA
    # SQL> SQL> SQL> SQL> SQL> SQL> SQL> ...
    # We have to use 'clean_stdout' in order to ignore this:
    assert act.clean_stdout == ''
    act.reset()

    #---------------------------------------------------------------------------------

    blob_overall_info_ptn = re.compile( r'Blobs:\s+\d+,\s+total\s+length:\s+\d+,\s+blob\s+pages', re.IGNORECASE )
    blob_level_info_ptn = re.compile( r'Level\s+\d+: \d+,\s+total\s+length: \d+,\s+blob\s+pages', re.IGNORECASE )
    table_size_ptn = re.compile( r'Table\s+size:\s+\d+\s+bytes', re.IGNORECASE )

    act.gstat(switches=['-r'])
    blob_overall_found = False
    for line in act.stdout.splitlines():
        if blob_overall_info_ptn.search(line):
            blob_overall_found = True
            print(line)
        if blob_overall_found:
           if blob_level_info_ptn.search(line):
               print(line)
           if table_size_ptn.search(line):
               print(line)


    act.expected_stdout = """
        Blobs: N, total length: M, blob pages: P
        Level 0: X, total length: M, blob pages: P
        Level 1: X, total length: M, blob pages: P
        Level 2: X, total length: M, blob pages: P
        Table size: N bytes
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
