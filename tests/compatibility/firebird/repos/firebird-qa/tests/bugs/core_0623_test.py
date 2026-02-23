#coding:utf-8

"""
ID:          issue-985
ISSUE:       985
TITLE:       ORDER BY on a VIEW turns values in fields into NULL
DESCRIPTION:
NOTES:
[30.10.2019] NB: new datatype in FB 4.0 was introduces: numeric(38,0).
  It can lead to additional ident of values when we show them in form "SET LIST ON",
  so we have to ignore all internal spaces - see added 'substitution' section below.
JIRA:        CORE-623
FBTEST:      bugs.core_0623
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table p1 (
      x_p1 numeric(10,0),
      f_entrada date
    );

    create view vp1 (
      x_p1,
      f_entrada
    ) as
    select x_p1, f_entrada from p1;

    create table p2 (
      x_p2 numeric(10,0),
      p1_x_p1 numeric(10,0),
      n_one numeric(10,0),
      n_two numeric(10,0)
    );

    create view vp2 (
      p1_x_p1,
      n_one,
      n_two
    ) as
    select p1_x_p1, sum(n_one), sum(n_two)
    from p2 group by p1_x_p1;

    create view vvp1 (
      p1_x_p1,
      f_entrada,
      n_one,
      n_two
    ) as
    select p1.x_p1, p1.f_entrada, p2.n_one, p2.n_two
    from vp1 p1 left join vp2 p2 on p1.x_p1=p2.p1_x_p1;
    commit;

    insert into p1 values (1,'07/10/2001');
    insert into p1 values (2,'07/13/2001');
    insert into p1 values (3,'08/12/2001');

    insert into p2 values (1,1,0,1);
    insert into p2 values (2,2,1,0);
    insert into p2 values (3,1,0,1);
    commit;

    select * from vvp1;
    select * from vvp1 order by f_entrada;

    insert into p1 values (4,'08/10/2001');
    insert into p2 values (4,2,0,1);
    insert into p2 values (5,2,1,1);
    commit;
    select * from vvp1;
    select * from vvp1 order by f_entrada;
"""

act = isql_act('db', test_script, substitutions=[('=.*', ''), ('[ \t]+', ' ')])

expected_stdout = """
                  P1_X_P1   F_ENTRADA                 N_ONE                 N_TWO
    ===================== =========== ===================== =====================
                        1 2001-07-10                      0                     2
                        2 2001-07-13                      1                     0
                        3 2001-08-12                 <null>                <null>


                  P1_X_P1   F_ENTRADA                 N_ONE                 N_TWO
    ===================== =========== ===================== =====================
                        1 2001-07-10                      0                     2
                        2 2001-07-13                      1                     0
                        3 2001-08-12                 <null>                <null>


                  P1_X_P1   F_ENTRADA                 N_ONE                 N_TWO
    ===================== =========== ===================== =====================
                        1 2001-07-10                      0                     2
                        2 2001-07-13                      2                     2
                        3 2001-08-12                 <null>                <null>
                        4 2001-08-10                 <null>                <null>


                  P1_X_P1   F_ENTRADA                 N_ONE                 N_TWO
    ===================== =========== ===================== =====================
                        1 2001-07-10                      0                     2
                        2 2001-07-13                      2                     2
                        4 2001-08-10                 <null>                <null>
                        3 2001-08-12                 <null>                <null>
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

