#coding:utf-8

"""
ID:          index.upper-bound-asc-1-segment-02
TITLE:       ASC single segment index upper bound
DESCRIPTION: Check if all 32 values are fetched with "lower than" operator.
FBTEST:      functional.arno.indices.upper_bound_asc_01_segments_02
"""

import pytest
from firebird.qa import *

init_script = """
    create table table_66 (
      id integer
    );
    
    set term ^ ;
    create procedure pr_filltable_66
    as
    declare variable fillid integer;
    begin
      fillid = 2147483647;
      while (fillid > 0) do
      begin
        insert into table_66 (id) values (:fillid);
        fillid = fillid / 2;
      end
      insert into table_66 (id) values (null);
      insert into table_66 (id) values (0);
      insert into table_66 (id) values (null);
      fillid = -2147483648;
      while (fillid < 0) do
      begin
        insert into table_66 (id) values (:fillid);
        fillid = fillid / 2;
      end
    end
    ^
    set term ; ^
    commit;
    
    execute procedure pr_filltable_66;
    commit;
    
    create asc index i_table_66_asc on table_66 (id);
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    set plan on;
    select id from table_66 t66 where t66.id < 0;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

@pytest.mark.version('>=3')
def test_1(act: Action):
    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    TABLE_TEST_NAME = 'T66' if act.is_version('<6') else '"T66"'
    INDEX_TEST_NAME = 'I_TABLE_66_ASC' if act.is_version('<6') else f'{SQL_SCHEMA_PREFIX}"I_TABLE_66_ASC"'
    expected_stdout = f"""
        PLAN ({TABLE_TEST_NAME} INDEX ({INDEX_TEST_NAME}))
        ID -2147483648
        ID -1073741824
        ID -536870912
        ID -268435456
        ID -134217728
        ID -67108864
        ID -33554432
        ID -16777216
        ID -8388608
        ID -4194304
        ID -2097152
        ID -1048576
        ID -524288
        ID -262144
        ID -131072
        ID -65536
        ID -32768
        ID -16384
        ID -8192
        ID -4096
        ID -2048
        ID -1024
        ID -512
        ID -256
        ID -128
        ID -64
        ID -32
        ID -16
        ID -8
        ID -4
        ID -2
        ID -1
    """

    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
