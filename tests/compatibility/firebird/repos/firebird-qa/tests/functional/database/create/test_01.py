#coding:utf-8

"""
ID:          create-database-01
TITLE:       Create database: set names and default character set
DESCRIPTION: Check ability to specify SET NAMES and DEFAULT CHARACTER SET within one statement.
NOTES:
  name of client charset must be enclosed in apostrophes, i.e.: create database ... set names 'win1251' ...
FBTEST:      functional.database.create.01
"""

import pytest
from pathlib import Path
from firebird.qa import *

db = db_factory(do_not_create=True)

act = python_act('db')

expected_stdout = """
CLIENT_CHAR_SET                 WIN1251
DB_CHAR_SET                     UTF8
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    script = f"""
        create database '{act.db.dsn}' user '{act.db.user}'
          password '{act.db.password}' set names 'win1251' default character set utf8 ;
        set list on ;
        select c.rdb$character_set_name as client_char_set, r.rdb$character_set_name as db_char_set
        from mon$attachments a join rdb$character_sets c on a.mon$character_set_id = c.rdb$character_set_id
        cross join rdb$database r
        where a.mon$attachment_id = current_connection ;
    """
    act.expected_stdout = expected_stdout
    act.isql(switches = ['-q'], input=script, connect_db=False, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
