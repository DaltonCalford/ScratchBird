#coding:utf-8

"""
ID:          issue-628
ISSUE:       628
TITLE:       IB server stalled by simple script
DESCRIPTION:
  ::: NB :::
  ### Name of original test has no any relation with actual task of this test: ###
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/CF_ISQL_23.script

  Issue in original script: bug #585624 IB server stalled by simple script"
  Found in FB tracker as: http://tracker.firebirdsql.org/browse/CORE-297
  Fixed in 1.5.0
JIRA:        CORE-297
FBTEST:      bugs.core_0297
"""

import pytest
from firebird.qa import *

db = db_factory(charset='ISO8859_1')

test_script = """
    set term ^;
    create procedure group_copy (
        source integer,
        destination integer)
    as
    begin
        exit;
    end^

    create procedure insert_values (
        cont integer,
        d_group integer)
    as
    begin
        exit;
    end^
    set term ;^

    create table groups (
        gr_id integer not null,
        gr_name varchar(40) character set iso8859_1 not null
        collate de_de
    );

    create table test (
        id integer not null,
        t_group integer not null
    );

    alter table groups add constraint pk_groups primary key (gr_id);
    alter table test add constraint pk_test primary key (id, t_group);
    alter table test add constraint fk_test foreign key (t_group) references groups (gr_id);

    set term ^;
    alter procedure group_copy (
        source integer,
        destination integer)
    as
    begin
        insert into test( id, t_group )
        select a.id, :destination
        from test a
        where a.t_group = :source
            and not exists (
                select * from test b
                where b.id = a.id
                and :destination = b.t_group
            );
    end
    ^

    alter procedure insert_values (
        cont integer,
        d_group integer)
    as
        declare anz integer;
    begin
        anz = 0;

        while ( anz < cont ) do
        begin
            if ( not exists (
                                select id
                                from test where id = :anz
                                and t_group = :d_group
                            )
                ) then
                insert into test( id, t_group ) values( :anz, :d_group );

            anz = anz +1;
        end
    end
    ^
    set term ;^
    commit;


    insert into groups values ( 1 , 'Group1' );
    insert into groups values ( 2 , 'Group2' );
    commit;
    execute procedure insert_values( 3000 , 1);
    commit;

    delete from test where t_group = 2;
    execute procedure group_copy( 1 , 2 );
    commit;

    set list on;
    select count(*) from test;
    select * from groups;

"""

act = isql_act('db', test_script, substitutions=[('=', ''), ('[ \t]+', ' ')])

expected_stdout = """
    COUNT                           6000
    GR_ID                           1
    GR_NAME                         Group1
    GR_ID                           2
    GR_NAME                         Group2
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

