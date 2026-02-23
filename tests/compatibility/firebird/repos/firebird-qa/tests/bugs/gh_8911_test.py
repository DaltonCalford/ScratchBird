#coding:utf-8

"""
ID:          n/a
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8911
TITLE:       Wrong result in case of SubQueryConversion = true
DESCRIPTION:
NOTES:
    [20.02.2026] pzotov
    Custom driver config objects are created here.
    For FB 5.x we *change* SubQueryConversion: set it 'true' while default value is 'false'.
    In FB 6.x this parameter no more exists and conversion is performed always, i.e. in effect it is also 'true'.

    Confirmed bug on 5.0.4.1765 (three rows are issued instead of one).
    Checked on 5.0.4.1767-52823f5; 6.0.0.1458-c3778e0.
"""

import pytest
from firebird.qa import *
from firebird.driver import driver_config, connect, DatabaseError

init_script = """
    create table parent_table (
        id integer not null primary key,
        link_id integer
    );

    create table child_table (
        id integer not null primary key,
        parent_id integer,
        amount double precision,
        ref_id integer
    );

    insert into parent_table (id, link_id) values (1, null);

    insert into child_table (id, parent_id, amount, ref_id) values (101, 1, 100, null);
    insert into child_table (id, parent_id, amount, ref_id) values (102, 1, 100, null);
    insert into child_table (id, parent_id, amount, ref_id) values (103, 1, 100, null);
    commit;
"""

db = db_factory(init=init_script)
act = python_act('db')

#-----------------------------------------------------------

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#-----------------------------------------------------------

@pytest.mark.version('>=5.0.4')
def test_1(act: Action, capsys):

    test_sql = """
        -- expected: 1 row (ok with subqueryconversion = false)
        -- actual: 3 rows (same row repeated) with subqueryconversion = true
        select
            p.id
        from parent_table p
        where p.id = 1 and
            exists(
                select 1
                from child_table c
                where c.parent_id = p.id and
                    c.amount - coalesce(
                        (select sum(c2.amount)
                        from child_table c2
                            join parent_table p2 on p2.link_id = p.id -- correlation to outer p
                        where c2.ref_id = c.id), 0) > 0);

    """

    srv_cfg = driver_config.register_server(name = f'srv_cfg_8911', config = '')
    db_cfg_name = f'db_cfg_8911'
    db_cfg_object = driver_config.register_database(name = db_cfg_name)
    db_cfg_object.server.value = srv_cfg.name
    db_cfg_object.database.value = str(act.db.db_path)
    if act.is_version('<6'):
        db_cfg_object.config.value = f"""
            SubQueryConversion = true
        """

    with connect(db_cfg_name, user = act.db.user, password = act.db.password) as con:
        try:
            cur = con.cursor()
            cur.execute(test_sql)
            cur_cols = cur.description
            for r in cur:
                for i in range(0,len(cur_cols)):
                    print( cur_cols[i][0], ':', r[i] )
        except DatabaseError as e:
            print(e.__str__())
            print(e.gds_codes)


    expected_stdout = """
        ID : 1
    """

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
