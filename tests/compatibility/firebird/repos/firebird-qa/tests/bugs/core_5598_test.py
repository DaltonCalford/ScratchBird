#coding:utf-8

"""
ID:          issue-5864
ISSUE:       5864
TITLE:       Error "block size exceeds implementation restriction" while inner joining large datasets with a long key using the HASH JOIN plan
DESCRIPTION:
  Hash join have to operate with keys of total length >= 1 Gb if we want to reproduce runtime error
  "Statement failed, SQLSTATE = HY001 / unable to allocate memory from operating system"
  If test table that serves as the source for HJ has record length about 65 Kb than not less than 16K records must be added there.
  If we use charset UTF8 than record length in bytes will be 8 times of declared field_len, so we declare field with len = 8191 charactyer
  (and this is current implementation limit).
  Than we add into this table >= 16Kb rows of unicode (NON-ascii!) characters.
  Finally, we launch query against this table and this query will use hash join because of missed indices.
  We have to check that NO errors occured during this query.

  Discussed with dimitr: letters 08-jan-2018 .. 06-feb-2018.
JIRA:        CORE-5598
FBTEST:      bugs.core_5598
NOTES:
    [07.04.2022] pzotov
    FB 5.0.0.455 and later: data sources with equal cardinality now present in the HASH plan in order they are specified in the query.
    Reversed order was used before this build. Because of this, two cases of expected stdout must be taken in account, see variables
    'fb3x_checked_stdout' and 'fb5x_checked_stdout'.

    [02.05.202] pzotov
    Test requires that presense of FIREBIRD_TMP pointing to accessible directory.
    If this directory missed then execution fails with messsage that can be LOCALIZED:
    ========
        (Captured ISQL stderr call):
        Statement failed, SQLSTATE = 08001
        I/O error during "CreateFile (create)" operation for file "<path>\fb_recbuf_py6oyh"
        -Error while trying to create file
        -[ The system cannot find the path specified ] -- CAN BE LOCALIZED.
    ========
    One need to specify 'io_enc=locale.getpreferredencoding()' when invoke ISQL
    if we want this message to appear at console in readable view:
        act.isql(switches = ..., input = ..., charset = ..., io_enc=locale.getpreferredencoding())
    NOTE: specifying 'encoding_errors = ignore' in the DEFAULT section of firebird-driver.conf
    does not prevent from UnicodeDecode error in this case.

    [01.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.881; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
import locale
from firebird.qa import *

substitutions = [('[ \t]+', ' '), ('.*RECORD LENGTH:[ \t]+[\\d]+[ \t]*\\)', ''),
                 ('.*COUNT[ \t]+[\\d]+', ''), ('(?m)DATABASE:.*\\n?', '')]

db = db_factory(charset='UTF8')

act = python_act('db', substitutions=substitutions)

fb3x_checked_stdout = """
    PLAN HASH (B NATURAL, A NATURAL)
"""

fb5x_checked_stdout = """
    PLAN HASH (A NATURAL, B NATURAL)
"""

fb6x_checked_stdout = """
    PLAN HASH ("A" NATURAL, "B" NATURAL)
"""

MIN_RECS_TO_ADD = 17000

test_script = """
    set list on; 
    set plan on;
    --set planonly;
    select count(*) from test a join test b using(id, s);
    quit;
"""

@pytest.mark.version('>=3.0.3')
def test_1(act: Action):
    with act.db.connect(charset='utf8') as con:
        con.execute_immediate('create table test(id int, s varchar(8191))')
        con.commit()
        c = con.cursor()
        c.execute(f"insert into test(id, s) select row_number()over(), lpad('', 8191, 'Алексей, Łukasz, Máté, François, Jørgen, Νικόλαος') from rdb$types,rdb$types rows {MIN_RECS_TO_ADD}")
        con.commit()

    act.expected_stdout = fb3x_checked_stdout if act.is_version('<5') else fb5x_checked_stdout if act.is_version('<6') else fb6x_checked_stdout

    # NB: FIREBIRD_TMP must point do accessible directory here!
    act.isql(switches=['-q'], input=test_script, charset='UTF8', io_enc=locale.getpreferredencoding())
    act.stdout = act.stdout.upper()
    assert act.clean_stdout == act.clean_expected_stdout
