#coding:utf-8

"""
ID:          gtcs.test_misplaced_collation_in_extracted_metadata
TITLE:       Misplaced collation when extracting metadata with isql
DESCRIPTION:
  bug #223126 Misplaced collation when extracting metadata with isql
  ::: NB ::: Name of original test has no relation with actual task of this test:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/CF_ISQL_20.script
FBTEST:      functional.gtcs.isql_show_command_collation
NOTES:
    [07.10.2023] pzotov.
    1. Removed SHOW commands for check result because their output often changes.
    2. It is enough to extract metadata and APPLY it to test DB (with dropping previously created objects). No error must raise.
"""

import pytest
from firebird.qa import *

init_sql = """
    create domain domain_with_collate_clause as char(1)
        character set iso8859_1
        default 'v'
        check(value >='a' and value <='z')
        collate es_es;
    commit;
    create table table_with_collated_field (
        field_01 domain_with_collate_clause
            default 'w'
            collate pt_pt
    );
    alter table table_with_collated_field add constraint f01_check check( field_01 >= 'c' );
    commit;
"""
db = db_factory(init = init_sql)
act = python_act('db')

@pytest.mark.version('>=3')
def test_1(act: Action, capsys):
    act.isql(switches=['-x'])
    init_metadata = act.stdout
    act.reset()
    #--------------------------------------------------------------------
    drop_sql = """
        drop table table_with_collated_field;
        drop domain domain_with_collate_clause;
        commit;
    """
    act.isql(switches = ['-q'], input = drop_sql, combine_output = True)
    assert act.clean_stdout == '' # no errors must occur when drop previously created table and domain
    act.reset()
    #--------------------------------------------------------------------
    # Apply extracted metadata
    #act.isql(switches = ['-q'], input = '\n'.join(init_metadata), combine_output = True)
    act.isql(switches = ['-q'], input = init_metadata, combine_output = True)
    assert act.clean_stdout == '' # no errors must occur while applying script with extracted metadata
    act.reset()

    #print(init_metadata)
    #act.stdout = capsys.readouterr().out
    #assert act.clean_stdout == act.clean_expected_stdout
