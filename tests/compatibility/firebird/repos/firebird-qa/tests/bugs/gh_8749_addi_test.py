#coding:utf-8

"""
ID:          n/a
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8749
TITLE:       RDB$RECORD_VERSION has wrong value when is used in BEFORRE trigger and RETURNING INTO.
DESCRIPTION:
NOTES:
    [17.02.2026] pzotov
    Thanks to Vlad for suggestions.
    Confirmed bug on 6.0.0.1458-0-6a76c1d; 5.0.4.1762-7c85eae.
    Checked on 6.0.0.1458-c3778e0; 5.0.4.1767-0-52823f5.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate table test(x int);
    commit;

    set term ^;
    create trigger test_bi for test active before insert position 0 as
    begin
        rdb$set_context('USER_SESSION', 'TRG_1_BI', new.rdb$record_version);
    end
    ^
    create trigger test_bu for test active before update position 0 as
    begin
        rdb$set_context('USER_SESSION', 'TRG_2_BU', new.rdb$record_version);
    end
    ^
    create trigger test_bd for test active before delete position 0 as
    begin
        rdb$set_context('USER_SESSION', 'TRG_3_BD', old.rdb$record_version);
    end
    ^
    set term ;^
    commit;
    insert into test(x) values(1);
    update test set x = 2 where x = 1;
    delete from test where x = 2;
    select
        mon$variable_name as ctx_var_name,
        sign(mon$variable_value) as rec_version_sign
    from mon$context_variables
    order by 1;
    commit;
    -------------------------------------------------------------------------
    recreate table test(x int default current_transaction);
    commit;

    set term ^;
    execute block as
        declare init_rec_vers bigint;
    begin
        insert into test default values returning rdb$record_version into init_rec_vers; 
        rdb$set_context('USER_SESSION', 'TX_INIT', init_rec_vers);
    end
    ^
    commit
    ^
    execute block returns (check_01 varchar(255), check_02 varchar(255) ) as
        declare old_rec_vers bigint;
        declare new_rec_vers bigint;
    begin
        update test set x = -x rows 1 returning old.rdb$record_version, new.rdb$record_version into old_rec_vers, new_rec_vers;
        rdb$set_context('USER_SESSION', 'TX_CURR', current_transaction);
        check_01 = iif( cast(rdb$get_context('USER_SESSION', 'TX_INIT') as bigint) = old_rec_vers
                        ,'OK'
                        ,q'#FAILED: context variable 'TX_INIT' <> old_rec_vers#'
                      );
        check_02 = iif( old_rec_vers < new_rec_vers
                        ,'OK'
                        ,'FAILED: old_rec_vers >= new_rec_vers or some of them is null'
                      );
        suspend;
    end
    ^
    set term ;^
    commit;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    CTX_VAR_NAME                    TRG_1_BI
    REC_VERSION_SIGN                1
    CTX_VAR_NAME                    TRG_2_BU
    REC_VERSION_SIGN                1
    CTX_VAR_NAME                    TRG_3_BD
    REC_VERSION_SIGN                1
    CHECK_01                        OK
    CHECK_02                        OK
"""

@pytest.mark.version('>=5.0.4')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
