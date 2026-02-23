#coding:utf-8

"""
ID:          issue-4097
ISSUE:       4097
TITLE:       The trigger together with the operator MERGE if in a condition of connection ON contains new is not compiled
DESCRIPTION:
JIRA:        CORE-3753
FBTEST:      bugs.core_3753
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^;
    execute block as
    begin
        begin execute statement 'drop trigger horse_ai'; when any do begin end end
    end
    ^
    set term ;^

    recreate table cover(id int);
    commit;

    recreate table horse (
        code_horse integer constraint pk_horse primary key,
        code_father integer default -2 not null,
        code_mother integer default -3 not null,
        name varchar(50)
    );

    recreate table cover (
        code_cover integer constraint pk_cover primary key
        ,code_father integer not null
        ,code_mother integer not null
        ,code_horse integer not null
        ,constraint fk_cover_ref_father foreign key (code_father) references horse (code_horse)
        ,constraint fk_cover_ref_horse foreign key (code_horse) references horse (code_horse)
        ,constraint fk_cover_ref_mother foreign key (code_mother) references horse (code_horse)
    );
    commit;

    set term ^;
    create or alter trigger horse_ai for horse active after insert position 0 as
    begin
        merge into cover
        using rdb$database as tbl
        on cover.code_father = new.code_father and
           cover.code_mother = new.code_mother
        when matched then
          update set
          code_horse = new.code_horse
        when not matched then
          insert (code_horse,
                  code_father,
                  code_mother)
          values (new.code_horse,
                  new.code_father,
                  new.code_mother);
    end
    ^
    set term ;^
    commit;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()
