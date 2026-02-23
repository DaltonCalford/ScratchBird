#coding:utf-8

"""
ID:          issue-3012
ISSUE:       3012
TITLE:       Attachments using NONE charset may cause reads from MON$ tables to fail
DESCRIPTION:
JIRA:        CORE-2602
FBTEST:      bugs.core_2602
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

act = python_act('db')

expected_stdout = """
    Attach: r2none. othr ; sql_text_hash:  98490476833044645 ; charset_name NONE
    Attach: r2utf8. othr ; sql_text_hash:  98490476833044645 ; charset_name NONE
    Attach: r2none. othr ; sql_text_hash:  97434734411675813 ; charset_name UTF8
    Attach: r2utf8. othr ; sql_text_hash:  97434734411675813 ; charset_name UTF8
"""

sql = """
    select
         iif(s.mon$attachment_id = current_connection, 'this', 'othr') as attach
         -- do NOT return SQL text of query with non-ascii characters: it can be displayed differently
         -- depending on machine codepage (or OS?):
         -- >>> disabled 05-feb-2018 >>> , iif(s.mon$sql_text like '%123%456%', s.mon$sql_text, '') as sql_text
         -- Use HASH(sql_text) instead. Bug (in 2.1.3) still can be reproduces with hash() also:
        ,hash(s.mon$sql_text) as sql_text_hash
        ,trim(c.rdb$character_set_name) as charset_name
    from mon$statements s
    join mon$attachments a on s.mon$attachment_id = a.mon$attachment_id
    join rdb$character_sets c on a.mon$character_set_id = c.rdb$character_set_id
    where
        s.mon$attachment_id != current_connection
        and s.mon$sql_text NOT containing 'mon$statements'
        and s.mon$sql_text NOT containing 'rdb$auth'
        and a.mon$remote_protocol is not null
"""

@pytest.mark.version('>=2.5')
def test_1(act: Action, capsys):
    with act.db.connect(charset='UTF8') as con1_utf8, \
         act.db.connect(charset='NONE') as con2_none, \
         act.db.connect(charset='UTF8') as con2_utf8:
        with act.db.connect(charset='NONE') as con1_none:
            r1none = con1_none.cursor()
            r1utf8 = con1_utf8.cursor()
            r2none = con2_none.cursor()
            r2utf8 = con2_utf8.cursor()
            # In attachment with charset = NONE we start query to mon$database which includes two non-ascii characters:
            # 1) Unicode Character 'LATIN SMALL LETTER A WITH ACUTE' (U+00E1)
            # 2) Unicode Character 'LATIN SMALL LETTER E WITH ACUTE' (U+00E9)
            r1none.execute("select '123áé456' from mon$database")
            r2none.execute(sql)
            for r in r2none:
                print(' '.join(('Attach: r2none.', r[0], '; sql_text_hash: ', str(r[1]), '; charset_name', r[2])))

            r2utf8.execute(sql)
            for r in r2utf8:
                print(' '.join(('Attach: r2utf8.', r[0], '; sql_text_hash: ', str(r[1]), '; charset_name', r[2])))

        r1utf8.execute("select '123áé456' from mon$database")
        con2_none.commit()
        con2_utf8.commit()
        r2none.execute(sql)
        for r in r2none:
            print(' '.join(('Attach: r2none.', r[0], '; sql_text_hash: ', str(r[1]), '; charset_name', r[2])))
        r2utf8.execute(sql)
        for r in r2utf8:
            print(' '.join(('Attach: r2utf8.', r[0], '; sql_text_hash: ', str(r[1]), '; charset_name', r[2])))
        # Check
        act.expected_stdout = expected_stdout
        act.stdout = capsys.readouterr().out
        assert act.clean_stdout == act.clean_expected_stdout
