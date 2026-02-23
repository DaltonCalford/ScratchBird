#coding:utf-8

"""
ID:          issue-998
ISSUE:       998
TITLE:       Extend maximum database page size to 32KB
DESCRIPTION:
  We create DB with page_size = 32784, then add two table int it, both with UTF8 fields.
  First table (test_non_coll) has field which is based on trivial text domain.
  Second table (test_collated) has two 'domained' fields and both underlying domains are
  based on two collations: case_insensitive and case_insensitive + accent_insensitive.
  NOTE: we use MAXIMAL POSSIBLE length of every text field.
  Then we add to both tables some text data in order to check then correctness of queries
  which use several kinds of search (namely: starting with, like and between).
  Then we make validation, backup, restore and run again DML query and validation.
  Also, we do extraction of metadata before backup and after restore.
  Finally, we:
    1) check that all error logs are empty;
    2) compare logs of DML, metadata extraction - they should be identical.
JIRA:        CORE-2192
FBTEST:      bugs.core_2192
"""

import pytest
from firebird.qa import *
from firebird.driver import SrvRestoreFlag
from io import BytesIO

test_size = 32768 # -- Ran 1 tests in 4.504s
# test_size = 16384 # -- Ran 1 tests in 2.735s
max_indx1 = int(test_size / 4 - 9)
max_indx6 = int(max_indx1 / 6)
max_indx8 = int(max_indx1 / 8)

init_script = f"""
    set list on;
    set bail on;
    set echo on;
    create sequence g;
    commit;
    create collation utf8_ci for utf8 from unicode case insensitive;
    create collation utf8_ai_ci for utf8 from unicode accent insensitive case insensitive ;
    commit;
    create domain dm_non_coll as varchar({max_indx1});
    create domain dm_collated_ci as varchar({max_indx6}) character set utf8 collate utf8_ci;
    create domain dm_collated_ai_ci as varchar({max_indx6}) character set utf8 collate utf8_ai_ci;
    commit;
    recreate table test_non_coll(
        txt_non_coll dm_non_coll
    );
    recreate table test_collated(
        txt_ci dm_collated_ci
       ,txt_ai_ci dm_collated_ai_ci
    );
    commit;
    create index test_non_coll on test_non_coll(txt_non_coll);
    create index test_coll_ci on test_collated(txt_ci);
    create index test_coll_ai_ci on test_collated(txt_ai_ci);
    commit;
"""

db = db_factory(init=init_script, page_size=32784)

test_script = f'''
    --show version;
    delete from test_non_coll;
    delete from test_collated;
    commit;
    set count on;
    insert into test_non_coll(txt_non_coll)
    select
        rpad('', {max_indx1}, 'QWERTY' || gen_id(g,1)  )
    from
    -- rdb$types rows 10000
    (select 1 i from rdb$types rows 200), (select 1 i from rdb$types rows 5)
    rows 361
    ;
    commit;

    insert into test_collated(txt_ci, txt_ai_ci)
    select
        rpad('', {max_indx6}, 'Ещё Съешь Этих Мягких Французских Булочек Да Выпей Же Чаю')
       ,rpad('', {max_indx6}, 'Ещё Французских Булочек Этих Мягких Съешь Да Чаю Выпей Же')
    from
    (select 1 i from rdb$types rows 250), (select 1 i from rdb$types rows 2)
    ;

    commit;

    set count off;
    set list on;
    set plan on;

    select count(*)
    from test_non_coll
    where txt_non_coll starting with 'QWERTY'

    union all

    select count(*)
    from test_collated
    where txt_ci starting with 'еЩё'

    union all

    select count(*)
    from test_collated
    where txt_ai_ci starting with 'ёЩЕ'

    union all

    select count(*)
    from test_collated
    where txt_ci = lower(rpad('', {max_indx6}, 'Ещё Съешь Этих Мягких Французских Булочек Да Выпей Же Чаю'))

    union all

    select count(*)
    from test_collated
    where txt_ai_ci = rpad('', {max_indx6}, 'Ещё Французских Булочек Этих Мягких Съешь Да Чаю Выпей Же')
    ;

    select count(*)
    from test_non_coll
    where txt_non_coll like 'QWERTY%%'

    union all

    select count(*)
    from test_collated
    where txt_ci like 'еЩё%%'

    union all

    select count(*)
    from test_collated
    where txt_ai_ci like 'ёЩЕ%%'

    union all

    select count(*)
    from test_collated
    where txt_ci between
    rpad('', {max_indx6}, 'ещё Съешь ЭТИХ Мягких Французских Булочек Да Выпей Же Чаю')
    and
    rpad('', {max_indx6}, 'ЕЩЁ Съешь Этих МЯГКИХ фРанцузских Булочек Да Выпей Же Чаю')

    union all

    select count(*)
    from test_collated
    where txt_ai_ci between
    rpad('', {max_indx6}, 'ёще фРанцузских Булочек Этих Мягких Съешь Да Чаю Выпёй Же')
    and
    rpad('', {max_indx6}, 'ёщё Французских Булочёк Этих Мягких Съёшь Да Чаю Выпёй Жё')
    ;

    set plan off;
'''

act = python_act('db')

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    # 1. FIRST RUN DML_TEST
    act.script = test_script
    act.execute(charset='utf8')
    run_dml_log_1 = act.stdout
    # 2. EXTRACT METADATA-1
    act.reset()
    act.isql(switches=['-x'], charset='utf8')
    extract_meta1_sql = act.stdout
    # 3. VALIDATE DATABASE-1
    # [pcisar] I don't understand the point of validation as the original test does not check
    # that validation passed
    with act.connect_server() as srv:
        srv.database.validate(database=act.db.db_path)
        validate_log_1 = srv.readlines()
    # 4. TRY TO BACKUP AND RESTORE
    with act.connect_server() as srv:
        backup = BytesIO()
        srv.database.local_backup(database=act.db.db_path, backup_stream=backup)
        backup.seek(0)
        srv.database.local_restore(backup_stream=backup, database=act.db.db_path,
                                   flags=SrvRestoreFlag.REPLACE)
        backup.close()
    # 5. EXTRACT METADATA-2
    act.reset()
    act.isql(switches=['-x'], charset='utf8')
    extract_meta2_sql = act.stdout
    # 6. AGAIN RUN DML_TEST
    act.reset()
    act.script = test_script
    act.execute(charset='utf8')
    run_dml_log_2 = act.stdout
    # 7. VALIDATE DATABASE-2
    with act.connect_server() as srv:
        srv.database.validate(database=act.db.db_path)
        validate_log_2 = srv.readlines()
    # 8. CHECKS
    # 1) STDERR for: create DB, backup, restore, validation-1 and validation-2 - they all must be EMPTY.
    # [pcisar] This is granted as exception would be raised if there would be any error
    # 2) diff between dml_1.log and dml_2.log should be EMPTY.
    assert run_dml_log_1 == run_dml_log_2
    # 3) diff between meta1.log and meta2.log should be EMPTY.
    assert extract_meta1_sql == extract_meta2_sql
