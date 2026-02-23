#coding:utf-8

"""
ID:          issue-3219
ISSUE:       3219
TITLE:       Changing data that affects an expression index that contains references to null date fields fails
DESCRIPTION:
JIRA:        CORE-2833
FBTEST:      bugs.core_2833
NOTES:
    [13.05.2023] pzotov
    Added 'combine_output = True' (otherwise error message remains unclear:
    "firebird.qa.plugin.ExecutionError: Test script execution failed").
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table policen_order (
        id integer not null,
        vstatus integer,
        ablauf date,
        vstorno integer,
        storno date
    );
    alter table policen_order add constraint pk_new_table primary key (id);

    set term ^;
    execute block as
    begin
        execute statement 'drop sequence gen_policen_order_id';
    when any do begin end
    end
    ^
    set term ;^
    commit;
    create generator gen_policen_order_id;
    commit;

    set term ^;
    create or alter trigger policen_order_bi for policen_order
    active before insert position 0 as
    begin
      if (new.id is null) then
        new.id = gen_id(gen_policen_order_id,1);
    end
    ^
    set term ;^
    commit;

    -- insert some data with null dates
    insert into policen_order (id, vstatus, ablauf, vstorno, storno)
                       values (2, 1, null, null, null);
    insert into policen_order (id, vstatus, ablauf, vstorno, storno)
                       values (3, 2, null, null, null);
    insert into policen_order (id, vstatus, ablauf, vstorno, storno)
                       values (4, 3, null, null, null);
    commit;

    -- now let's create an obscure index

    create index idx_policen_order_bit_vstatus on policen_order
    computed by
    (
        case
            when policen_order.vstatus=1 then 64 -- anbahnung
            when policen_order.vstatus=7 then 32 -- da
            when policen_order.vstatus=8 then 16 -- angebot
            when policen_order.vstatus=0 then 8 -- vertrag
            when policen_order.vstatus=3 then 8 -- vertrag
            when policen_order.vstatus=4 then 8 -- vertrag
            when policen_order.vstatus=5 then 8 -- vertrag
            when policen_order.vstatus=6 then 8 -- vertrag
            when policen_order.vstatus=2 then 4 -- fremdvertrag
            else 0
        end
        +
        case
            when
                coalesce(policen_order.ablauf, current_date+1)<=current_date
                or
                policen_order.vstorno=1
                and coalesce(policen_order.storno, current_date+1) <= current_date
            then 2
            else 0
        end
    );
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set bail on;
    set list on;

    update policen_order set vstatus = 8 where id = 2;
    commit;
    set count on;
    select * from policen_order order by id;
    set count off;

    update policen_order set vstatus = 2 where id = 2;
    commit;
    set count on;
    select * from policen_order order by id;
    set count off;
"""

act = isql_act('db', test_script)

expected_stdout = """
    ID                              2
    VSTATUS                         8
    ABLAUF                          <null>
    VSTORNO                         <null>
    STORNO                          <null>

    ID                              3
    VSTATUS                         2
    ABLAUF                          <null>
    VSTORNO                         <null>
    STORNO                          <null>

    ID                              4
    VSTATUS                         3
    ABLAUF                          <null>
    VSTORNO                         <null>
    STORNO                          <null>
    Records affected: 3


    ID                              2
    VSTATUS                         2
    ABLAUF                          <null>
    VSTORNO                         <null>
    STORNO                          <null>

    ID                              3
    VSTATUS                         2
    ABLAUF                          <null>
    VSTORNO                         <null>
    STORNO                          <null>

    ID                              4
    VSTATUS                         3
    ABLAUF                          <null>
    VSTORNO                         <null>
    STORNO                          <null>
    Records affected: 3
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

