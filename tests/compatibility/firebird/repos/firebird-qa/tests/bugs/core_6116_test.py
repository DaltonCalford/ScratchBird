#coding:utf-8

"""
ID:          issue-6365
ISSUE:       6365
TITLE:       The Metadata script extracted using ISQL of a database restored from a Firebird
  2.5.9 Backup is invalid/incorrect when table has COMPUTED BY field
DESCRIPTION:
  Test uses backup of preliminary created database in FB 2.5.9, DDL is the same as in the ticket.
  This .fbk is restored and we launch ISQL -X in order to get metadata. Then we check that two
  in this script with "COMPUTED BY" phrase contain non zero number as width of this field:
  1) line that belongs to CREATE TABLE statement:
     FULL_NAME VARCHAR(100) ... COMPUTED BY ...
  2) line with ALTER COLUMN statement:
     ALTER FULL_NAME TYPE VARCHAR(100) ... COMPUTED BY ...
JIRA:        CORE-6116
FBTEST:      bugs.core_6116
"""

import pytest
import re
from firebird.qa import *

db = db_factory(from_backup='core6116-25.fbk')

act = python_act('db')

expected_stdout = """
    Length in "CREATE TABLE" statement: 100
    Length in "ALTER COLUMN" statement: 100
"""

@pytest.mark.version('>=3.0.6')
def test_1(act: Action, capsys):
     comp_field_initial_ptn = re.compile( 'FULL_NAME\\s+VARCHAR\\(\\d+\\).*COMPUTED BY', re.IGNORECASE )
     comp_field_altered_ptn = re.compile( 'ALTER\\s+FULL_NAME\\s+TYPE\\s+VARCHAR\\(\\d+\\).*COMPUTED BY', re.IGNORECASE )
     #
     act.isql(switches=['-x'])
     for line in act.stdout.splitlines():
          if comp_field_initial_ptn.search(line):
               words = line.replace('(',' ').replace(')',' ').split() # ['FULL_NAME', 'VARCHAR', '0', ... , 'COMPUTED', 'BY']
               print(f'Length in "CREATE TABLE" statement: {words[2]}')
          if comp_field_altered_ptn.search(line):
               words = line.replace('(',' ').replace(')',' ').split() # ['ALTER', 'FULL_NAME', 'TYPE', 'VARCHAR', '0', ... , 'COMPUTED', 'BY']
               print(f'Length in "ALTER COLUMN" statement: {words[4]}')
     # Check
     act.reset()
     act.expected_stdout = expected_stdout
     act.stdout = capsys.readouterr().out
     assert act.clean_stdout == act.clean_expected_stdout
