#coding:utf-8

"""
ID:          issue-4585
ISSUE:       4585
TITLE:       Wrong result of join when joined fields are created via row_number() function
DESCRIPTION:
JIRA:        CORE-4261
FBTEST:      bugs.core_4261b
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table bbb(id int); commit;
    recreate table yyy(id int primary key, c varchar(3)); commit;
    recreate table bbb(tm timestamp, qi int, vi int references yyy, bv int, constraint bbb_pk primary key (tm, qi, vi));
    commit;
    insert into yyy (id, c) values ( 1, 'rio');
    insert into yyy (id, c) values ( 2, 'rio');
    insert into yyy (id, c) values ( 3, 'rio');
    insert into yyy (id, c) values ( 4, 'foo');
    insert into yyy (id, c) values ( 5, 'foo');
    insert into yyy (id, c) values ( 6, 'foo');
    insert into yyy (id, c) values ( 7, 'bar');
    insert into yyy (id, c) values ( 8, 'bar');
    insert into yyy (id, c) values ( 9, 'bar');
    insert into yyy (id, c) values (10, 'rio');
    insert into yyy (id, c) values (11, 'rio');
    insert into yyy (id, c) values (12, 'rio');
    insert into yyy (id, c) values (13, 'foo');
    insert into yyy (id, c) values (14, 'foo');
    insert into yyy (id, c) values (15, 'bar');
    insert into yyy (id, c) values (16, 'bar');
    insert into yyy (id, c) values (17, 'rio');
    insert into yyy (id, c) values (18, 'foo');
    insert into yyy (id, c) values (19, 'bar');
    insert into yyy (id, c) values (20, 'rio');
    insert into yyy (id, c) values (21, 'foo');
    insert into yyy (id, c) values (22, 'bar');
    insert into yyy (id, c) values (23, 'rio');
    insert into yyy (id, c) values (24, 'foo');
    insert into yyy (id, c) values (25, 'bar');
    insert into yyy (id, c) values (26, 'bar');
    insert into yyy (id, c) values (27, 'rio');
    insert into yyy (id, c) values (28, 'foo');
    insert into yyy (id, c) values (29, 'rio');
    insert into yyy (id, c) values (30, 'foo');
    insert into yyy (id, c) values (31, 'rio');
    insert into yyy (id, c) values (32, 'foo');
    insert into yyy (id, c) values (33, 'bar');
    insert into yyy (id, c) values (34, 'rio');
    insert into yyy (id, c) values (35, 'foo');
    insert into yyy (id, c) values (36, 'bar');
    insert into yyy (id, c) values (37, 'rio');
    insert into yyy (id, c) values (38, 'foo');
    insert into yyy (id, c) values (39, 'bar');
    insert into yyy (id, c) values (40, 'rio');
    insert into yyy (id, c) values (41, 'rio');
    insert into yyy (id, c) values (42, 'foo');
    insert into yyy (id, c) values (43, 'bar');
    insert into yyy (id, c) values (44, 'rio');
    insert into yyy (id, c) values (45, 'foo');
    insert into yyy (id, c) values (46, 'bar');
    insert into yyy (id, c) values (47, 'bar');
    insert into yyy (id, c) values (48, 'foo');
    insert into yyy (id, c) values (49, 'rio');
    insert into yyy (id, c) values (50, 'foo');
    insert into yyy (id, c) values (51, 'bar');
    insert into yyy (id, c) values (52, 'rio');
    insert into yyy (id, c) values (53, 'foo');
    insert into yyy (id, c) values (54, 'bar');
    commit;
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:01', 1, 1, 155);
    insert into bbb (tm, qi, vi, bv) values ('23-jun-2003 01:12:02', 1, 1, 100);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:03', 2, 2, 255);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:04', 3, 3, 255);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:05', 1, 4, 255);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:06', 2, 5, 255);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:07', 3, 6, 255);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:08', 1, 7, 255);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:09', 2, 8, 255);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:10', 3, 9, 255);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:11', 4, 10, 50);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:12', 5, 11, 100);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:13', 5, 12, 155);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:14', 5, 13, 155);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:15', 5, 14, 100);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:16', 5, 15, 50);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:17', 5, 16, 205);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:18', 6, 10, 155);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:19', 6, 17, 100);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:20', 6, 18, 255);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:21', 6, 19, 255);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:22', 7, 17, 155);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:23', 7, 20, 100);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:24', 7, 21, 255);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:25', 7, 22, 255);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:26', 8, 10, 50);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:27', 9, 23, 255);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:28', 9, 24, 255);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:29', 9, 25, 100);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:30', 9, 26, 155);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:31', 10, 25, 155);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:31', 10, 26, 100);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:33', 10, 27, 10);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:34', 10, 28, 10);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:35', 10, 29, 245);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:36', 10, 30, 245);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:37', 11, 31, 100);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:38', 11, 32, 100);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:39', 11, 33, 100);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:40', 11, 34, 155);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:41', 11, 35, 155);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:42', 11, 36, 155);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:43', 12, 31, 155);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:44', 12, 32, 155);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:45', 12, 33, 155);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:46', 12, 34, 100);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:47', 12, 35, 100);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:12:48', 12, 36, 100);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:13:01', 4, 37, 20);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:13:02', 8, 38, 20);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:13:03', 13, 39, 123);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:13:04', 14, 39, 111);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:13:05', 14, 40, 50);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:13:06', 15, 41, 50);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:13:07', 15, 41, 50);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:13:08', 15, 42, 50);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:13:09', 15, 42, 50);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:13:10', 16, 42, 50);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:13:11', 16, 42, 50);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:13:12', 16, 43, 50);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:13:13', 16, 43, 50);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:13:14', 16, 47, 50);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:13:15', 17, 44, 10);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:13:16', 17, 44, 10);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:13:17', 17, 45, 10);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:13:18', 17, 45, 10);
    insert into bbb (tm, qi, vi, bv) values ('1-feb-2003 01:13:19', 18, 45, 10);
    insert into bbb (tm, qi, vi, bv) values ('1-mar-2003 01:13:20', 18, 45, 10);
    insert into bbb (tm, qi, vi, bv) values ('1-apr-2003 01:13:21', 18, 46, 10);
    insert into bbb (tm, qi, vi, bv) values ('1-may-2003 01:13:22', 18, 46, 10);
    insert into bbb (tm, qi, vi, bv) values ('11-jun-2003 01:13:23', 19, 44, 10);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:13:24', 19, 44, 10);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:13:25', 19, 45, 10);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:13:26', 19, 45, 10);
    insert into bbb (tm, qi, vi, bv) values ('1-feb-2003 01:13:27', 20, 45, 10);
    insert into bbb (tm, qi, vi, bv) values ('1-mar-2003 01:13:28', 20, 45, 10);
    insert into bbb (tm, qi, vi, bv) values ('1-apr-2003 01:13:29', 20, 46, 10);
    insert into bbb (tm, qi, vi, bv) values ('1-may-2003 01:13:30', 20, 46, 10);
    insert into bbb (tm, qi, vi, bv) values ('1-feb-2003 01:13:31', 21, 49, 50);
    insert into bbb (tm, qi, vi, bv) values ('2-feb-2003 01:13:32', 21, 49, 50);
    insert into bbb (tm, qi, vi, bv) values ('3-feb-2003 01:13:33', 21, 50, 50);
    insert into bbb (tm, qi, vi, bv) values ('4-feb-2003 01:13:34', 21, 50, 50);
    insert into bbb (tm, qi, vi, bv) values ('5-feb-2003 01:13:35', 21, 48, 1);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2000 01:13:36', 22, 50, 50);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2001 01:13:37', 22, 50, 50);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2002 01:13:38', 22, 51, 50);
    insert into bbb (tm, qi, vi, bv) values ('1-jun-2002 01:13:39', 22, 51, 50);
    insert into bbb (tm, qi, vi, bv) values ('1-jan-2003 01:13:05', 4, 37, 185);
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    with
    c as(
      select
          b.qi, v.c
          ,row_number()over(order by b.qi, b.tm, b.vi) r
      from bbb b join yyy v on v.id = b.vi
    )
    select t1.r r1, t2.r r2, t3.r r3
    from c t1
    left join c t2 on t1.qi = t2.qi and t1.r = t2.r-1
    left join c t3 on t2.qi = t3.qi and t1.r = t3.r-2
    where t1.c = 'rio' and t2.c = 'foo' and t3.c = 'bar'
    order by 1,2,3;
"""

act = isql_act('db', test_script, substitutions=[('=.*', '')])

expected_stdout = """
                   R1                    R2                    R3
===================== ===================== =====================
                    1                     2                     3
                    5                     6                     7
                    8                     9                    10
                   21                    22                    23
                   25                    26                    27
                   30                    31                    32
                   40                    41                    42
                   43                    44                    45
                   46                    47                    48
                   49                    50                    51
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
