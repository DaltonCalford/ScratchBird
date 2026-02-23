#coding:utf-8

"""
ID:          issue-6504
ISSUE:       6504
TITLE:       SHOW DOMAIN/TABLE does not display character set of system objects
DESCRIPTION:
  We gather all system domains which belongs to TEXT family by query to rdb$fields.
  Then for each record from its resulset we issue statement: 'SHOW DOMAIN ... ;'
  and write it to .SQL file. After all records will be processed, we run ISQL and
  perform this script. Every row from its output must contain phrase 'CHARACTER SET'.

  ::: NB ::: additional filtering: "where f.rdb$character_set_id > 1" is needed when
  we query rdb$fields. Otherwise we get some domains without 'CHARACTER SET' phrases
  domains definition:
    rdb$character_set_id=0:
        show domain RDB$EDIT_STRING;
        RDB$EDIT_STRING                 VARCHAR(127) Nullable
        show domain RDB$MESSAGE;
        RDB$MESSAGE                     VARCHAR(1023) Nullable
    rdb$character_set_id=1:
        RDB$SYSTEM_PRIVILEGES           BINARY(8) Nullable
JIRA:        CORE-6262
FBTEST:      bugs.core_6262
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db', substitutions=[('[ \t]+', ' ')])

sql = """
    select 'show domain '|| trim(f.rdb$field_name) ||';' as show_expr
    from rdb$fields f
    where f.rdb$character_set_id > 1
    order by f.rdb$field_name
"""

expected_stdout = """
    Number of lines with specified charset: SAME AS NUMBER OF TEXT DOMAINS
    Number of lines with missed charset: 0
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action, capsys):
    chk_script = []
    with act.db.connect() as con:
        c = con.cursor()
        text_domains_count = 0
        for row in c.execute(sql):
            chk_script.append(row[0])
            text_domains_count += 1
    act.isql(switches=[], input='\n'.join(chk_script))
    # Checks
    lines_with_charset = lines_without_charset = 0
    names_without_charset = []
    for line in act.clean_stdout.splitlines():
        if line.split():
            if 'CHARACTER SET' in line:
                lines_with_charset += 1
            else:
                lines_without_charset += 1
                names_without_charset.append(line,)
    if lines_with_charset > 0:
        result = 'SAME AS' if lines_with_charset == text_domains_count else f'{lines_with_charset} - LESS THAN'
        print(f'Number of lines with specified charset: {result} NUMBER OF TEXT DOMAINS')
        if lines_without_charset:
            print(f'Number of text domains: {text_domains_count}')
            print(f'Number of lines WITHOUT specified charset: {lines_without_charset}')
            print('\n'.join(names_without_charset))
    else:
        print('SOMETHING WAS WRONG: COULD NOT FIND ANY LINE WITH "CHARACTER SET" PHRASE')
    print('Number of lines with missed charset:', lines_without_charset)
    act.reset()
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
