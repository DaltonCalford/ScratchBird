#coding:utf-8

"""
ID:          issue-2941
ISSUE:       2941
TITLE:       The famous "cannot transliterate" error may be thrown when selecting data from the monitoring tables
DESCRIPTION:
JIRA:        CORE-2531
FBTEST:      bugs.core_2531
"""
from firebird.driver import DatabaseError

import pytest
from firebird.qa import *

init_script = """
    recreate table non_ascii(stored_sql_expr varchar(255) character set win1252);
"""

db = db_factory(init=init_script, charset='WIN1252')

test_script = """
    set count on;
    set blob all;
    set list on;
    select stored_sql_expr from non_ascii;
    select
        c.rdb$character_set_name as connection_charset
       ,s.mon$sql_text as sql_text_blob_id
    from mon$attachments a
    left join rdb$character_sets c on a.mon$character_set_id = c.rdb$character_set_id
    left join mon$statements s on a.mon$attachment_id = s.mon$attachment_id
    where
        s.mon$attachment_id <> current_connection
        and s.mon$sql_text containing 'non_ascii_literal'
    ;
"""

act = python_act('db', substitutions=[('SQL_TEXT_BLOB_ID .*', ''), ('[\t ]+', ' ')])

expected_stdout = """
    STORED_SQL_EXPR                 select 'gång' as non_ascii_literal from rdb$database
    Records affected: 1

    CONNECTION_CHARSET              WIN1252
    select 'gång' as non_ascii_literal from rdb$database
    Records affected: 1
"""

@pytest.mark.intl
@pytest.mark.version('>=3')
def test_1(act: Action):
    non_ascii_query = "select 'gång' as non_ascii_literal from rdb$database"
    non_ascii_query_inline = non_ascii_query.replace("'","''")
    act.expected_stdout = expected_stdout
    with act.db.connect(charset='WIN1252') as con:
        ps = None
        try:
            cur = con.cursor()
            cur.execute(f"insert into non_ascii(stored_sql_expr) values('{non_ascii_query_inline}')")
            con.commit()
            ps = cur.prepare(non_ascii_query)
            act.isql(switches=[], input=test_script, charset='WIN1252')
        except DatabaseError as e:
            print( e.__str__() )
            print(e.gds_codes)
        finally:
            if ps:
                ps.free()
        
    assert act.clean_stdout == act.clean_expected_stdout


