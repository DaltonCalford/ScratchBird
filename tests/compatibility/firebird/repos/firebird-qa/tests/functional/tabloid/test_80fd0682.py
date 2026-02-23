#coding:utf-8

"""
ID:          None
ISSUE:       https://github.com/FirebirdSQL/firebird/commit/80fd06828e72f9e8335150c923350730013b3b28
TITLE:       Fixed bug with index names patterns in online validation service
DESCRIPTION:
    Sources to read (19-feb-2021):
        https://sourceforge.net/p/firebird/mailman/message/37222898/
        https://sourceforge.net/p/firebird/mailman/message/37223338/
        (Firebird-devel Digest, Vol 178, Issue 34; Vol 178, Issue 35)
NOTES:
    Confirmed bug on 4.0.0.2369.
    Checked on 4.0.0.2372 -- all fine.
    Checked on 6.0.0.423, 5.0.2.1477.
"""

import re
import pytest
from firebird.qa import *

init_sql = """
    create table a(id int);
    create index a on a(id);
    create table b(id int);
    create index x on b(id);
    create table c(id int);
    create index c on c(id);
    commit;
"""
db = db_factory(init = init_sql)

substitutions = [('[ \t]+',' ')]

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = python_act('db', substitutions = substitutions)

#-------------------------------------------------------------

def clean_text(line: str):
    # Used to remove timestamp from the beginning of line,
    # then remove ID of relation / index.:
    line = re.sub(r'^\d{2}:\d{2}:\d{2}.\d{2,3}\s+', '', line)
    line = re.sub('Relation \\d+ \\(', 'Relation (', line)
    line = re.sub('Index \\d+ \\(', 'Index (', line)
    return line

#-------------------------------------------------------------

@pytest.mark.version('>=4.0')
def test_1(act: Action, capsys):

    validation_log = []
    # Run online database validation:
    with act.connect_server() as srv:
        for ix_key in ('incl', 'excl'):
            for tab_name in ('A', 'B'):
                if ix_key == 'incl':
                    srv.database.validate(database=act.db.db_path, include_table = tab_name.upper())
                else:
                    srv.database.validate(database=act.db.db_path, exclude_table = tab_name.upper())
                validation_log.append(f'{ix_key=}, {tab_name=}')
                validation_log.extend( [clean_text(line) for line in srv.readlines()] )
            for idx_name in ('A', 'X'):
                if ix_key == 'incl':
                    srv.database.validate(database=act.db.db_path, include_index = idx_name.upper())
                else:
                    srv.database.validate(database=act.db.db_path, exclude_index = idx_name.upper())
                validation_log.append(f'{ix_key=}, {idx_name=}')
                validation_log.extend( [clean_text(line) for line in srv.readlines()] )
                # validation_log.extend( [ re.sub('Index \\d+ \\(', 'Index (', re.sub(r'^\d{2}:\d{2}:\d{2}.\d{2,3}\s+', '', line)) for line in srv.readlines() ] )
                #for line in srv.readlines():
                #    validation_log.append( clean_text(line) )


    for line in validation_log:
        print(line)

    act.expected_stdout = """
        ix_key='incl', tab_name='A'
        Validation started
        Relation (A)
        process pointer page 0 of 1
        Index (A)
        Relation (A) is ok
        Validation finished

        ix_key='incl', tab_name='B'
        Validation started
        Relation (B)
        process pointer page 0 of 1
        Index (X)
        Relation (B) is ok
        Validation finished
        
        ix_key='incl', idx_name='A'
        Validation started
        Relation (A)
        process pointer page 0 of 1
        Index (A)
        Relation (A) is ok
        Relation (B)
        process pointer page 0 of 1
        Relation (B) is ok
        Relation (C)
        process pointer page 0 of 1
        Relation (C) is ok
        Validation finished
        
        ix_key='incl', idx_name='X'
        Validation started
        Relation (A)
        process pointer page 0 of 1
        Relation (A) is ok
        Relation (B)
        process pointer page 0 of 1
        Index (X)
        Relation (B) is ok
        Relation (C)
        process pointer page 0 of 1
        Relation (C) is ok
        Validation finished
        
        ix_key='excl', tab_name='A'
        Validation started
        Relation (B)
        process pointer page 0 of 1
        Index (X)
        Relation (B) is ok
        Relation (C)
        process pointer page 0 of 1
        Index (C)
        Relation (C) is ok
        Validation finished
        
        ix_key='excl', tab_name='B'
        Validation started
        Relation (A)
        process pointer page 0 of 1
        Index (A)
        Relation (A) is ok
        Relation (C)
        process pointer page 0 of 1
        Index (C)
        Relation (C) is ok
        Validation finished
        
        ix_key='excl', idx_name='A'
        Validation started
        Relation (A)
        process pointer page 0 of 1
        Relation (A) is ok
        Relation (B)
        process pointer page 0 of 1
        Index (X)
        Relation (B) is ok
        Relation (C)
        process pointer page 0 of 1
        Index (C)
        Relation (C) is ok
        Validation finished
        
        ix_key='excl', idx_name='X'
        Validation started
        Relation (A)
        process pointer page 0 of 1
        Index (A)
        Relation (A) is ok
        Relation (B)
        process pointer page 0 of 1
        Relation (B) is ok
        Relation (C)
        process pointer page 0 of 1
        Index (C)
        Relation (C) is ok
        Validation finished
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()
