#coding:utf-8

"""
ID:          gtcs.invalid-stream-when-use-trim
TITLE:       Statement with TRIM raises "bad BLR -- invalid stream"
DESCRIPTION:
  ::: NB :::
  ### Name of original test has no any relation with actual task of this test: ###
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/CF_ISQL_32.script

  Source description (Rudo Mihal, message of 2004-05-06 11:32:10; FB 1.5.1.4443):
  https://sourceforge.net/p/firebird/mailman/message/17016190/

  Example for reproducing (by N. Samofatov, with UDF usage):
  https://sourceforge.net/p/firebird/mailman/message/17017012/
FBTEST:      functional.gtcs.invalid_stream_when_use_trim
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select trim(TRAILING FROM (select max(rdb$relation_id) from rdb$database)) trim_result from rdb$database;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' '), ('TRIM_RESULT.*', 'TRIM_RESULT')])

expected_stdout = """
    TRIM_RESULT 128
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
