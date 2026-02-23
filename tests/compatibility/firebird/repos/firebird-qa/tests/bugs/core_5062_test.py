#coding:utf-8

"""
ID:          issue-5349
ISSUE:       5349
TITLE:       CHAR_TO_UUID on column with index throws expression evaluation not supported. Human readable UUID argument for CHAR_TO_UUID must be of exact length 36
DESCRIPTION:
JIRA:        CORE-5062
NOTES:
    [18.01.2025] pzotov
    Resultset of cursor that executes using instance of selectable PreparedStatement must be stored
    in some variable in order to have ability close it EXPLICITLY (before PS will be freed).
    Otherwise access violation raises during Python GC and pytest hangs at final point (does not return control to OS).
    This occurs at least for: Python 3.11.2 / pytest: 7.4.4 / firebird.driver: 1.10.6 / Firebird.Qa: 0.19.3
    The reason of that was explained by Vlad, 26.10.24 17:42 ("oddities when use instances of selective statements").
"""

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

init_script = """
    recreate table test_uuid(
        datavalue int,
        uuid char(16) character set octets,
        constraint test_uuid_unq unique(uuid)
    );
    commit;
    insert into test_uuid(datavalue, uuid) values( 1, char_to_uuid('57F2B8C7-E1D8-4B61-9086-C66D1794F2D9') );
    commit;
"""

db = db_factory(init=init_script)

act = python_act('db')

@pytest.mark.version('>=3.0')
def test_1(act: Action, capsys):
    with act.db.connect() as con:
        ps, rs = None, None
        cur = con.cursor()
        try:
            ps = cur.prepare("select datavalue from test_uuid where uuid = char_to_uuid(?)")
            print(ps.plan)

            # ::: NB ::: 'ps' returns data, i.e. this is SELECTABLE expression.
            # We have to store result of cur.execute(<psInstance>) in order to
            # close it explicitly.
            # Otherwise AV can occur during Python garbage collection and this
            # causes pytest to hang on its final point.
            # Explained by hvlad, email 26.10.24 17:42
            rs = cur.execute(ps, ['57F2B8C7-E1D8-4B61-9086-C66D1794F2D9'])
            for r in rs:
                print(r[0])
        except DatabaseError as e:
            print( e.__str__() )
            print(e.gds_codes)
        finally:
            if rs:
                rs.close() # <<< EXPLICITLY CLOSING CURSOR RESULTS
            if ps:
                ps.free()

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    TABLE_NAME = 'TEST_UUID' if act.is_version('<6') else '"TEST_UUID"'
    INDEX_NAME = 'TEST_UUID_UNQ' if act.is_version('<6') else '"TEST_UUID_UNQ"'
    expected_stdout = f"""
        PLAN ({SQL_SCHEMA_PREFIX}{TABLE_NAME} INDEX ({SQL_SCHEMA_PREFIX}{INDEX_NAME}))
        1
    """

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()
