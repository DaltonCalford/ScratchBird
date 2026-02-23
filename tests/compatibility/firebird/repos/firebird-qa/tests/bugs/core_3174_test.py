#coding:utf-8

"""
ID:          issue-3548
ISSUE:       3548
TITLE:       Expression index with TRIM may lead to incorrect indexed lookup
DESCRIPTION:
JIRA:        CORE-3174
FBTEST:      bugs.core_3174
NOTES:
    [27.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

init_script = """
    create collation ps_yes for utf8 from unicode pad space;
    create collation ps_no  for utf8 from unicode no pad;

    create table t (id int, c_pad varchar(10), c_nopad varchar(10) character set utf8 collate ps_no);
    commit;
    insert into t(id, c_pad, c_nopad) values (1, '123',    '123');
    insert into t(id, c_pad, c_nopad) values (2, ' 123',   ' 123');
    insert into t(id, c_pad, c_nopad) values (3, '123  ',  '123  ');
    insert into t(id, c_pad, c_nopad) values (4, ' 123  ', ' 123  ');
    commit;

    create index t_c_pad_trim_right on t computed by (trim(trailing from c_pad));
    create index t_c_pad_trim_left on t computed by (trim(leading from c_pad));
    create index t_c_nopad_trim_right on t computed by (trim(trailing from c_nopad));
    create index t_c_nopad_trim_left on t computed by (trim(leading from c_nopad));
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set plan on;
    set list on;
    --set echo on;

    select '1.a' as test_no, id,'.' || c_pad || '.' as c_pad from t where trim(c_pad) = '123';
    select '1.b' as test_no, id,'.' || c_pad || '.' as c_pad from t where trim(trailing from c_pad) = trim(leading from c_pad);
    select '1.c' as test_no, id,'.' || c_pad || '.' as c_pad from t where trim(trailing from c_pad) = '123';
    select '1.d' as test_no, id,'.' || c_pad || '.' as c_pad from t where trim(leading from c_pad) = '123';
    select '1.e' as test_no, id,'.' || c_pad || '.' as c_pad from t where trim(trailing from c_pad) starting with '123';
    select '1.f' as test_no, id,'.' || c_pad || '.' as c_pad from t where trim(leading from c_pad) starting with '123';

    -------------------------------------------------------------------------

    select '2.a' as test_no, id,'.' || c_nopad || '.' as c_nopad from t where trim(c_nopad) = '123';
    select '2.b' as test_no, id,'.' || c_nopad || '.' as c_nopad from t where trim(trailing from c_nopad) = trim(leading from c_nopad);
    select '2.c' as test_no, id,'.' || c_nopad || '.' as c_nopad from t where trim(trailing from c_nopad) = '123';
    select '2.d' as test_no, id,'.' || c_nopad || '.' as c_nopad from t where trim(leading from c_nopad) = '123';
    select '2.f' as test_no, id,'.' || c_nopad || '.' as c_nopad from t where trim(trailing from c_nopad) starting with '123';
    select '2.g' as test_no, id,'.' || c_nopad || '.' as c_nopad from t where trim(leading from c_nopad) starting with '123';
"""

substitutions = [ ('[ \t]+', ' ') ] 
act = isql_act('db', test_script, substitutions = substitutions)


expected_out_5x = """
        PLAN (T NATURAL)
        TEST_NO 1.a
        ID 1
        C_PAD .123.
        TEST_NO 1.a
        ID 2
        C_PAD . 123.
        TEST_NO 1.a
        ID 3
        C_PAD .123 .
        TEST_NO 1.a
        ID 4
        C_PAD . 123 .
        PLAN (T NATURAL)
        TEST_NO 1.b
        ID 1
        C_PAD .123.
        TEST_NO 1.b
        ID 3
        C_PAD .123 .
        PLAN (T INDEX (T_C_PAD_TRIM_RIGHT))
        TEST_NO 1.c
        ID 1
        C_PAD .123.
        TEST_NO 1.c
        ID 3
        C_PAD .123 .
        PLAN (T INDEX (T_C_PAD_TRIM_LEFT))
        TEST_NO 1.d
        ID 1
        C_PAD .123.
        TEST_NO 1.d
        ID 2
        C_PAD . 123.
        TEST_NO 1.d
        ID 3
        C_PAD .123 .
        TEST_NO 1.d
        ID 4
        C_PAD . 123 .
        PLAN (T INDEX (T_C_PAD_TRIM_RIGHT))
        TEST_NO 1.e
        ID 1
        C_PAD .123.
        TEST_NO 1.e
        ID 3
        C_PAD .123 .
        PLAN (T INDEX (T_C_PAD_TRIM_LEFT))
        TEST_NO 1.f
        ID 1
        C_PAD .123.
        TEST_NO 1.f
        ID 2
        C_PAD . 123.
        TEST_NO 1.f
        ID 3
        C_PAD .123 .
        TEST_NO 1.f
        ID 4
        C_PAD . 123 .
        PLAN (T NATURAL)
        TEST_NO 2.a
        ID 1
        C_NOPAD .123.
        TEST_NO 2.a
        ID 2
        C_NOPAD . 123.
        TEST_NO 2.a
        ID 3
        C_NOPAD .123 .
        TEST_NO 2.a
        ID 4
        C_NOPAD . 123 .
        PLAN (T NATURAL)
        TEST_NO 2.b
        ID 1
        C_NOPAD .123.
        PLAN (T INDEX (T_C_NOPAD_TRIM_RIGHT))
        TEST_NO 2.c
        ID 1
        C_NOPAD .123.
        TEST_NO 2.c
        ID 3
        C_NOPAD .123 .
        PLAN (T INDEX (T_C_NOPAD_TRIM_LEFT))
        TEST_NO 2.d
        ID 1
        C_NOPAD .123.
        TEST_NO 2.d
        ID 2
        C_NOPAD . 123.
        PLAN (T INDEX (T_C_NOPAD_TRIM_RIGHT))
        TEST_NO 2.f
        ID 1
        C_NOPAD .123.
        TEST_NO 2.f
        ID 3
        C_NOPAD .123 .
        PLAN (T INDEX (T_C_NOPAD_TRIM_LEFT))
        TEST_NO 2.g
        ID 1
        C_NOPAD .123.
        TEST_NO 2.g
        ID 2
        C_NOPAD . 123.
        TEST_NO 2.g
        ID 3
        C_NOPAD .123 .
        TEST_NO 2.g
        ID 4
        C_NOPAD . 123 .
"""

expected_out_6x = """
        PLAN ("PUBLIC"."T" NATURAL)
        TEST_NO 1.a
        ID 1
        C_PAD .123.
        TEST_NO 1.a
        ID 2
        C_PAD . 123.
        TEST_NO 1.a
        ID 3
        C_PAD .123 .
        TEST_NO 1.a
        ID 4
        C_PAD . 123 .
        PLAN ("PUBLIC"."T" NATURAL)
        TEST_NO 1.b
        ID 1
        C_PAD .123.
        TEST_NO 1.b
        ID 3
        C_PAD .123 .
        PLAN ("PUBLIC"."T" INDEX ("PUBLIC"."T_C_PAD_TRIM_RIGHT"))
        TEST_NO 1.c
        ID 1
        C_PAD .123.
        TEST_NO 1.c
        ID 3
        C_PAD .123 .
        PLAN ("PUBLIC"."T" INDEX ("PUBLIC"."T_C_PAD_TRIM_LEFT"))
        TEST_NO 1.d
        ID 1
        C_PAD .123.
        TEST_NO 1.d
        ID 2
        C_PAD . 123.
        TEST_NO 1.d
        ID 3
        C_PAD .123 .
        TEST_NO 1.d
        ID 4
        C_PAD . 123 .
        PLAN ("PUBLIC"."T" INDEX ("PUBLIC"."T_C_PAD_TRIM_RIGHT"))
        TEST_NO 1.e
        ID 1
        C_PAD .123.
        TEST_NO 1.e
        ID 3
        C_PAD .123 .
        PLAN ("PUBLIC"."T" INDEX ("PUBLIC"."T_C_PAD_TRIM_LEFT"))
        TEST_NO 1.f
        ID 1
        C_PAD .123.
        TEST_NO 1.f
        ID 2
        C_PAD . 123.
        TEST_NO 1.f
        ID 3
        C_PAD .123 .
        TEST_NO 1.f
        ID 4
        C_PAD . 123 .
        PLAN ("PUBLIC"."T" NATURAL)
        TEST_NO 2.a
        ID 1
        C_NOPAD .123.
        TEST_NO 2.a
        ID 2
        C_NOPAD . 123.
        TEST_NO 2.a
        ID 3
        C_NOPAD .123 .
        TEST_NO 2.a
        ID 4
        C_NOPAD . 123 .
        PLAN ("PUBLIC"."T" NATURAL)
        TEST_NO 2.b
        ID 1
        C_NOPAD .123.
        PLAN ("PUBLIC"."T" INDEX ("PUBLIC"."T_C_NOPAD_TRIM_RIGHT"))
        TEST_NO 2.c
        ID 1
        C_NOPAD .123.
        TEST_NO 2.c
        ID 3
        C_NOPAD .123 .
        PLAN ("PUBLIC"."T" INDEX ("PUBLIC"."T_C_NOPAD_TRIM_LEFT"))
        TEST_NO 2.d
        ID 1
        C_NOPAD .123.
        TEST_NO 2.d
        ID 2
        C_NOPAD . 123.
        PLAN ("PUBLIC"."T" INDEX ("PUBLIC"."T_C_NOPAD_TRIM_RIGHT"))
        TEST_NO 2.f
        ID 1
        C_NOPAD .123.
        TEST_NO 2.f
        ID 3
        C_NOPAD .123 .
        PLAN ("PUBLIC"."T" INDEX ("PUBLIC"."T_C_NOPAD_TRIM_LEFT"))
        TEST_NO 2.g
        ID 1
        C_NOPAD .123.
        TEST_NO 2.g
        ID 2
        C_NOPAD . 123.
        TEST_NO 2.g
        ID 3
        C_NOPAD .123 .
        TEST_NO 2.g
        ID 4
        C_NOPAD . 123 .
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_out_5x if act.is_version('<6') else expected_out_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
