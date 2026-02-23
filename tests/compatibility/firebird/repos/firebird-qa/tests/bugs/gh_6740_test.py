#coding:utf-8

"""
ID:          issue-6740
ISSUE:       6740
TITLE:       Allow parenthesized query expression for standard-compliance
DESCRIPTION:
    Queries which do not use `WITH` clause now can be enclosed in parenthesis,
    but this leads to reduced number of max parts of UNIONed query, from 255 to 128.
JIRA:        CORE-6511
FBTEST:      bugs.gh_6740
NOTES:
    [30.06.2022] pzotov
    Error message in case when NUM_OF_UNIONED_PARTS >= 129 can obfuscate:
    "Too many Contexts of Relation/Procedure/Views. Maximum allowed is 256"
    Max limit of unioned-parts is 128, it was explained by Adriano in the ticket.
    Checked on 5.0.0.509.
"""

import pytest
from firebird.qa import *

db = db_factory()
act = python_act('db')

NUM_OF_UNIONED_PARTS = 128
unioned_query = '('
for i in range(0,NUM_OF_UNIONED_PARTS):
  unioned_query = ''.join( (unioned_query, 'select %d ' % (i+1) + ('as i ' if i==0 else '') + 'from rdb$database') )
  if i < NUM_OF_UNIONED_PARTS-1:
      unioned_query = ''.join( (unioned_query, ' union all (') )

unioned_query = ''.join( (unioned_query, ')' * NUM_OF_UNIONED_PARTS) )


expected_stdout = "8256"

@pytest.mark.version('>=5.0')
def test_1(act: Action, capsys):


    with act.db.connect() as con:
        cur = con.cursor()
        cur.execute(unioned_query)
        print( sum([x[0] for x in cur.fetchall()]) )

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
