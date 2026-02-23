#coding:utf-8

"""
ID:          issue-8213
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8213
TITLE:       WHEN NOT MATCHED BY SOURCE - does not work with a direct table as source
DESCRIPTION:
NOTES:
    [20.08.2024] pzotov
    Checked on 6.0.0.438-d40d01b, 5.0.2.1479-47aa3b1
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate procedure sp_main as begin end;
    recreate table test (id smallint);
    recreate generator g;
    recreate table test (
        id  smallint primary key,
        typ smallint,
        cat smallint
    );
    commit;

    set term ^ ;
    create or alter trigger test_bi0 for test active before insert position 0 as
    begin
        new.id = coalesce(new.id, gen_id(g, 1));
    end
    ^
    set term ; ^
    commit;

    insert into test(typ, cat) values(1, 10);
    insert into test(typ, cat) values(1, 20);
    insert into test(typ, cat) values(2, 10);
    insert into test(typ, cat) values(2, 30);
    commit;

    set term ^;
    recreate procedure sp_main (
        a_insert_using_sp boolean,
        a_delete_using_sp boolean,
        a_source_typ smallint,
        a_target_typ smallint
    ) as

        declare procedure inner_sp_data_for_source_typ
        returns (
            id  smallint,
            typ smallint,
            cat smallint
        ) as
        begin
          for select t.id,
                     t.typ,
                     t.cat
          from test t
          where t.typ = :a_source_typ
          into :id,
               :typ,
               :cat
          do
          begin
            suspend;
          end
        end

    begin

      if ( a_insert_using_sp or :a_delete_using_sp ) then
          begin
            if (a_insert_using_sp) then
                merge into test t
                using inner_sp_data_for_source_typ s
                on t.typ = :a_target_typ and
                   t.cat = s.cat
                when not matched by target then
                    insert (typ, cat) values (:a_target_typ, s.cat);
            else
                merge into test t
                using test s
                on t.typ = :a_target_typ and
                   t.cat = s.cat
                when not matched by target then
                    insert (typ, cat) values (:a_target_typ, s.cat);

            if (a_delete_using_sp) then
                merge into test t
                using inner_sp_data_for_source_typ s on t.cat = s.cat
                when not matched by source and t.typ = :a_target_typ then
                    delete;
            else
                merge into test t
                using test s on t.cat = s.cat
                when not matched by source and t.typ = :a_target_typ then
                    delete;

          end
      else
          begin
            -- works as expected
            merge into test t
            using ( select t.id,
                           t.typ,
                           t.cat
                    from test t
                    where t.typ = :a_source_typ
                  ) s
            on t.typ = :a_target_typ and
               t.cat = s.cat
            when not matched by target then
                insert (typ, cat) values (:a_target_typ, s.cat);

            merge into test t
            using ( select t.id,
                           t.typ,
                           t.cat
                    from test t
                    where t.typ = :a_source_typ
                  ) s
            on t.cat = s.cat
            when not matched by source and t.typ = :a_target_typ then
                delete;
          end
    end
    ^
    set term ;^
    commit;

    -- select * from test;
    set count on;

    alter sequence g restart with 1000;
    execute procedure sp_main(true, true, 1, 10);
    select 'INS:SP, DEL:SP' msg, t.id, t.typ, t.cat from test t order by id;
    rollback;

    alter sequence g restart with 1000;
    execute procedure sp_main(true, false, 1, 10);
    select 'INS:SP, DEL:TAB' msg, t.id, t.typ, t.cat from test t order by id;
    rollback;

    alter sequence g restart with 1000;
    execute procedure sp_main(false, true, 1, 10);
    select 'INS:TAB, DEL:SP' msg, t.id, t.typ, t.cat from test t order by id;
    rollback;

    alter sequence g restart with 1000;
    execute procedure sp_main(false, false, 1, 10);
    select 'INS:TAB, DEL:TAB' msg, t.id, t.typ, t.cat from test t order by id;
    rollback;
"""

act = isql_act('db', test_script)

expected_stdout = """
    MSG                             INS:SP, DEL:SP
    ID                              1
    TYP                             1
    CAT                             10
    MSG                             INS:SP, DEL:SP
    ID                              2
    TYP                             1
    CAT                             20
    MSG                             INS:SP, DEL:SP
    ID                              3
    TYP                             2
    CAT                             10
    MSG                             INS:SP, DEL:SP
    ID                              4
    TYP                             2
    CAT                             30
    MSG                             INS:SP, DEL:SP
    ID                              1000
    TYP                             10
    CAT                             10
    MSG                             INS:SP, DEL:SP
    ID                              1001
    TYP                             10
    CAT                             20
    Records affected: 6

    MSG                             INS:SP, DEL:TAB
    ID                              1
    TYP                             1
    CAT                             10
    MSG                             INS:SP, DEL:TAB
    ID                              2
    TYP                             1
    CAT                             20
    MSG                             INS:SP, DEL:TAB
    ID                              3
    TYP                             2
    CAT                             10
    MSG                             INS:SP, DEL:TAB
    ID                              4
    TYP                             2
    CAT                             30
    MSG                             INS:SP, DEL:TAB
    ID                              1000
    TYP                             10
    CAT                             10
    MSG                             INS:SP, DEL:TAB
    ID                              1001
    TYP                             10
    CAT                             20
    Records affected: 6

    MSG                             INS:TAB, DEL:SP
    ID                              1
    TYP                             1
    CAT                             10
    MSG                             INS:TAB, DEL:SP
    ID                              2
    TYP                             1
    CAT                             20
    MSG                             INS:TAB, DEL:SP
    ID                              3
    TYP                             2
    CAT                             10
    MSG                             INS:TAB, DEL:SP
    ID                              4
    TYP                             2
    CAT                             30
    MSG                             INS:TAB, DEL:SP
    ID                              1000
    TYP                             10
    CAT                             10
    MSG                             INS:TAB, DEL:SP
    ID                              1001
    TYP                             10
    CAT                             20
    MSG                             INS:TAB, DEL:SP
    ID                              1002
    TYP                             10
    CAT                             10
    Records affected: 7

    MSG                             INS:TAB, DEL:TAB
    ID                              1
    TYP                             1
    CAT                             10
    MSG                             INS:TAB, DEL:TAB
    ID                              2
    TYP                             1
    CAT                             20
    MSG                             INS:TAB, DEL:TAB
    ID                              3
    TYP                             2
    CAT                             10
    MSG                             INS:TAB, DEL:TAB
    ID                              4
    TYP                             2
    CAT                             30
    MSG                             INS:TAB, DEL:TAB
    ID                              1000
    TYP                             10
    CAT                             10
    MSG                             INS:TAB, DEL:TAB
    ID                              1001
    TYP                             10
    CAT                             20
    Records affected: 6
"""

@pytest.mark.version('>=5.0.2')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
