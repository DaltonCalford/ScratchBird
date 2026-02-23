#coding:utf-8

"""
ID:          issue-1175
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/1175
TITLE:       Ability to run ALTER VIEW statement without dropping dependencies
DESCRIPTION:
    Test creates a table and view ('v_words') based on it.
    Then we create several PSQL units and views which depend on that view v_words and on each other.
    After this, we run ALTER VIEW V_WORDS and change expression for one of it column - it must be performed without errors.
    Finally, we check data that is shown by each of dependent views.
JIRA:        CORE-790
FBTEST:      bugs.core_0790
NOTES:
    [04.10.2023] pzotov
    RECONNECT is required after altering V_WORDS! Otherwise PSQL objects remain show 'old' data.
    Checked on fresh 3.x, 4.x, 5.x and 6.x.
"""

import pytest
from firebird.qa import *

init_script = """
    create table words (
        id int
        ,name varchar(20)
    );
    commit;
    insert into words(id, name) values(2, 'tab');
    insert into words(id, name) values(3, 'case');
    insert into words(id, name) values(0, 'war');
    insert into words(id, name) values(1, 'flow');
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set bail on;
    set term ^;
    create view v_words as select id, name from words
    ^
    create procedure sp_get_word_by_pattern(a_name_pattern varchar(50)) returns(name type of column words.name) as
    begin
        for
            execute statement ('select v.name from v_words v where v.name similar to ?') (:a_name_pattern)
            into name
        do suspend;
    end
    ^
    create procedure sp_get_word_by_id(a_id type of column words.id) returns (name type of column words.name) as
    begin
        for select v.name from v_words v where v.id = :a_id into name do suspend;
    end
    ^
    create function fn_get_word_by_id(a_id type of column words.id) returns type of column words.name as
    begin
        return (select v.name from v_words v where v.id = :a_id);
    end
    ^
    create view v_words_similar_to_o as select p.name as name_similar_to_o from sp_get_word_by_pattern('%(wolf)%') p
    ^
    create view v_sp_get_word_by_id as select name as name_by_id_using_sp from sp_get_word_by_id(1)
    ^
    create view v_fn_get_word_by_id as select fn_get_word_by_id(1) as name_by_id_using_fn from rdb$database
    ^
    create function fn_count_words_by_pattern(a_name_pattern varchar(50)) returns int as
    begin
        return (select count(*) from sp_get_word_by_pattern(:a_name_pattern));
    end
    ^
    create view v_count_words_similar_to_a as select fn_count_words_by_pattern('%(raw|esac|bat)%') as cnt from rdb$database
    ^

    set term ;^
    commit;
    -- Result: view V_words has 5 dependencies.
    -- We can not drop columns 'id' and 'name' but we *must* have ability change expression based on them

    alter view v_words as
    select reverse(name) as name, id
    from words;

    commit;
    connect '$(DSN)'; -- ::: NB ::: this is mandatory! Otherwise PSQL objects based on this view will show 'old' data ('flow' instead of 'wolf' etc).

    set list on;
    set count on;
    set echo on;

    select * from v_count_words_similar_to_a;
    
    select * from v_words_similar_to_o order by 1;
    
    select * from v_sp_get_word_by_id;
    
    select * from v_fn_get_word_by_id;
    
    select v.name from v_words v where v.id = 1;
"""

act = isql_act('db', test_script)

expected_stdout = """
    select * from v_count_words_similar_to_a;
    CNT                             3
    Records affected: 1
    select * from v_words_similar_to_o order by 1;
    NAME_SIMILAR_TO_O               wolf
    Records affected: 1
    select * from v_sp_get_word_by_id;
    NAME_BY_ID_USING_SP             wolf
    Records affected: 1
    select * from v_fn_get_word_by_id;
    NAME_BY_ID_USING_FN             wolf
    Records affected: 1
    select v.name from v_words v where v.id = 1;
    NAME                            wolf
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

