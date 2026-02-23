#coding:utf-8

"""
ID:          issue-5308
ISSUE:       5308
TITLE:       Regression: ORDER BY clause on compound index may disable usage of other indices
DESCRIPTION:
    Plan in 3.0.0.32179 (before fix): PLAN (ZF ORDER IXA_FK__ID__KONT_ID)
    Fixed in 3.0 since: http://sourceforge.net/p/firebird/code/62570
    Checked on 2.5.5.26952 - plans are the same now.
JIRA:        CORE-5020
FBTEST:      bugs.core_5020
NOTES:
    [17.11.2024] pzotov
    Re-implemented after https://github.com/FirebirdSQL/firebird/commit/26e64e9c08f635d55ac7a111469498b3f0c7fe81
    ( Cost-based decision between ORDER and SORT plans (#8316) ).
    Execution plan was replaced with explained. Plans are splitted for versions up to 5.x and 6.x+.
    Discussed with dimitr, letters 16.11.2024.

    Checked on 6.0.0.532; 5.0.2.1567; 4.0.6.3168; 3.0.13.33794.
"""

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

init_sql = """
    recreate table zf(
        id integer not null primary key,
        kont_id integer not null
    );

    recreate table u(
        id integer not null primary key,
        kont_id integer not null
    );

    recreate table k(
        id integer not null primary key
    );

    commit;

    insert into zf (id, kont_id) values ('1', '1');
    insert into zf (id, kont_id) values ('2', '7');
    insert into zf (id, kont_id) values ('3', '3');
    insert into zf (id, kont_id) values ('4', '5');
    insert into zf (id, kont_id) values ('5', '5');
    insert into zf (id, kont_id) values ('6', '1');
    insert into zf (id, kont_id) values ('7', '4');
    insert into zf (id, kont_id) values ('8', '2');
    insert into zf (id, kont_id) values ('9', '9');
    insert into zf (id, kont_id) values ('10', '1');


    insert into k (id) values ('1');
    insert into k (id) values ('2');
    insert into k (id) values ('3');
    insert into k (id) values ('4');
    insert into k (id) values ('5');
    insert into k (id) values ('6');
    insert into k (id) values ('7');
    insert into k (id) values ('8');
    insert into k (id) values ('9');
    insert into k (id) values ('10');

    insert into u (id, kont_id) values ('1', '4');
    insert into u (id, kont_id) values ('2', '6');
    insert into u (id, kont_id) values ('3', '3');
    insert into u (id, kont_id) values ('4', '2');
    insert into u (id, kont_id) values ('5', '5');
    insert into u (id, kont_id) values ('6', '2');
    insert into u (id, kont_id) values ('7', '9');
    insert into u (id, kont_id) values ('8', '2');
    insert into u (id, kont_id) values ('9', '10');
    insert into u (id, kont_id) values ('10', '1');

    commit;

    alter table zf add constraint fk_zf__k
    	foreign key(kont_id)
    	references k(id)
    	using index fk_zf__k
    ;
    set statistics index fk_zf__k;

    create index ixa_fk__id__kont_id on zf(id, kont_id);
    commit;
"""

db = db_factory(init = init_sql)
act = python_act('db', substitutions = [(r'record length: \d+, key length: \d+', 'record length: NN, key length: MM')])

#-----------------------------------------------------------

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#-----------------------------------------------------------

@pytest.mark.version('>=3')
def test_1(act: Action, capsys):
    test_sql = """
        select zf.*
        from zf
        where zf.kont_id=5
        order by zf.id, kont_id
    """

    with act.db.connect() as con:
        cur = con.cursor()
        ps = None
        try:
            ps = cur.prepare(test_sql)

            # Print explained plan with padding eash line by dots in order to see indentations:
            print( '\n'.join([replace_leading(s) for s in ps.detailed_plan.split('\n')]) )
            print('')
        except DatabaseError as e:
            print(e.__str__())
            print(e.gds_codes)
        finally:
            if ps:
                ps.free()


    expected_stdout_5x = """
        Select Expression
        ....-> Filter
        ........-> Table "ZF" Access By ID
        ............-> Index "IXA_FK__ID__KONT_ID" Full Scan
        ................-> Bitmap
        ....................-> Index "FK_ZF__K" Range Scan (full match)
    """

    expected_stdout_6x = """
        Select Expression
        ....-> Sort (record length: NN, key length: MM)
        ........-> Filter
        ............-> Table "PUBLIC"."ZF" Access By ID
        ................-> Bitmap
        ....................-> Index "PUBLIC"."FK_ZF__K" Range Scan (full match)
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x

    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
