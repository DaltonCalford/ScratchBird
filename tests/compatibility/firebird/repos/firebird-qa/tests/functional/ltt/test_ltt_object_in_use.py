#coding:utf-8

"""
ID:          n/a
ISSUE:       https://github.com/FirebirdSQL/firebird/commit/06d18794f7c219aa9dca05b48dabab7732c7ba37
TITLE:       Checks for 'object in use' when doing DDL changes in LTTs.
DESCRIPTION:
    DDL against LTT must behave exactly as for persistent table or GTT: 'object in use'
    must raise if changes in LTT exist and was caused by concurrent non-completed DML.
    Set of values returned by query to mon$local_temporary_table_columns gathered before
    and after such attempt must not change.
NOTES:
    [12.02.2026] pzotov
    See letter to FB team: 10-feb-2026 16:50.
    Confirmed problem on 6.0.0.1414-55ccc7a.
    Checked on 6.0.0.1428-06d1879.
"""
import pytest
import locale
from firebird.qa import *

init_sql = """
    create view v_mon_info as
    select
         f.mon$field_name
        ,f.mon$field_type
        ,f.mon$field_precision
        ,f.mon$field_scale
        ,f.mon$field_length
        ,f.mon$field_sub_type
        ,f.mon$char_length
        ,f.mon$character_set_id
        ,f.mon$collation_id 
        ,c.rdb$character_set_name as charset_name
        ,k.rdb$collation_name as collation_name
    from mon$local_temporary_table_columns f
    left join rdb$character_sets c on f.mon$character_set_id = c.rdb$character_set_id
    left join rdb$collations k on c.rdb$character_set_id = k.rdb$character_set_id and f.mon$collation_id  = k.rdb$collation_id
    order by mon$field_position
    ;
    commit;
"""
db = db_factory(init = init_sql)

substitutions = [ ('[ \t]+', ' '), ( r'(-)?At block.*', '' ) ]
act = python_act('db', substitutions = substitutions)

#-----------------------------------------------------------

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#-----------------------------------------------------------

@pytest.mark.version('>=6.0')
def test_1(act: Action, capsys):

    test_sql = """
        set bail on;
        set autoterm on;
        set list on;
        recreate local temporary table ltt(x numeric(10,2), y numeric(10,2), z int) on commit preserve rows;
        create index ltt_x on ltt(x);
        create index ltt_y on ltt(y);
        create index ltt_z on ltt(z);
        commit;
        select 'point #0' as msg, v.* from v_mon_info v;
        commit;
        set bail OFF;
        set transaction no wait;
        insert into /* trace_me: tx-1 */ ltt(x, y, z) select 12345689.01 + n-1, 12345689.01 + n, n*3 from generate_series(1, 100) as s(n);

        set plan on;
        select count(*) from ltt where x = 12345689.01;

        set bail off;

        execute block as
        begin
            execute statement 'alter table ltt drop column x'
            with autonomous transaction;
        end
        ;

        execute block as
        begin
            -- "New size specified for column "Y" must be at least 21 characters."
            execute statement 'alter table ltt alter column y type varchar(21) character set win1252 collate win_ptbr'
            with autonomous transaction;
        end
        ;

        execute block as
        begin
            -- New size specified for column "Z" must be at least 11 characters.
            execute statement 'alter table ltt alter column z type varchar(11) character set win1257 collate win1257_ee'
            with autonomous transaction;

        end
        ;

        commit;

        -- Column unknown / "X"
        -- select count(*) from ltt where x = 12345689.01;

        select count(*) from ltt where y || '' = '12345690.01';

        select count(*) from ltt where y = '12345690.01';

        select count(*) from ltt where z || '' = '3';

        select count(*) from ltt where z = '3';

        set plan off;
        select 'point #1' as msg, v.* from v_mon_info v;

    """

    act.isql(switches = ['-q'], combine_output = True, input = test_sql, io_enc = locale.getpreferredencoding())

    act.expected_stdout = """
        MSG                             point #0
        MON$FIELD_NAME                  X
        MON$FIELD_TYPE                  19
        MON$FIELD_PRECISION             10
        MON$FIELD_SCALE                 -2
        MON$FIELD_LENGTH                8
        MON$FIELD_SUB_TYPE              1
        MON$CHAR_LENGTH                 0
        MON$CHARACTER_SET_ID            <null>
        MON$COLLATION_ID                <null>
        CHARSET_NAME                    <null>
        COLLATION_NAME                  <null>
        MSG                             point #0
        MON$FIELD_NAME                  Y
        MON$FIELD_TYPE                  19
        MON$FIELD_PRECISION             10
        MON$FIELD_SCALE                 -2
        MON$FIELD_LENGTH                8
        MON$FIELD_SUB_TYPE              1
        MON$CHAR_LENGTH                 0
        MON$CHARACTER_SET_ID            <null>
        MON$COLLATION_ID                <null>
        CHARSET_NAME                    <null>
        COLLATION_NAME                  <null>
        MSG                             point #0
        MON$FIELD_NAME                  Z
        MON$FIELD_TYPE                  9
        MON$FIELD_PRECISION             0
        MON$FIELD_SCALE                 0
        MON$FIELD_LENGTH                4
        MON$FIELD_SUB_TYPE              0
        MON$CHAR_LENGTH                 0
        MON$CHARACTER_SET_ID            <null>
        MON$COLLATION_ID                <null>
        CHARSET_NAME                    <null>
        COLLATION_NAME                  <null>

        PLAN ("PUBLIC"."LTT" INDEX ("PUBLIC"."LTT_X"))
        COUNT                           1

        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -ALTER TABLE "PUBLIC"."LTT" failed
        -object TABLE "PUBLIC"."LTT" is in use

        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -ALTER TABLE "PUBLIC"."LTT" failed
        -object TABLE "PUBLIC"."LTT" is in use

        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -ALTER TABLE "PUBLIC"."LTT" failed
        -object TABLE "PUBLIC"."LTT" is in use

        PLAN ("PUBLIC"."LTT" NATURAL)
        COUNT                           1

        PLAN ("PUBLIC"."LTT" INDEX ("PUBLIC"."LTT_Y"))
        COUNT                           1

        PLAN ("PUBLIC"."LTT" NATURAL)
        COUNT                           1

        PLAN ("PUBLIC"."LTT" INDEX ("PUBLIC"."LTT_Z"))
        COUNT                           1

        MSG                             point #1
        MON$FIELD_NAME                  X
        MON$FIELD_TYPE                  19
        MON$FIELD_PRECISION             10
        MON$FIELD_SCALE                 -2
        MON$FIELD_LENGTH                8
        MON$FIELD_SUB_TYPE              1
        MON$CHAR_LENGTH                 0
        MON$CHARACTER_SET_ID            <null>
        MON$COLLATION_ID                <null>
        CHARSET_NAME                    <null>
        COLLATION_NAME                  <null>
        MSG                             point #1
        MON$FIELD_NAME                  Y
        MON$FIELD_TYPE                  19
        MON$FIELD_PRECISION             10
        MON$FIELD_SCALE                 -2
        MON$FIELD_LENGTH                8
        MON$FIELD_SUB_TYPE              1
        MON$CHAR_LENGTH                 0
        MON$CHARACTER_SET_ID            <null>
        MON$COLLATION_ID                <null>
        CHARSET_NAME                    <null>
        COLLATION_NAME                  <null>
        MSG                             point #1
        MON$FIELD_NAME                  Z
        MON$FIELD_TYPE                  9
        MON$FIELD_PRECISION             0
        MON$FIELD_SCALE                 0
        MON$FIELD_LENGTH                4
        MON$FIELD_SUB_TYPE              0
        MON$CHAR_LENGTH                 0
        MON$CHARACTER_SET_ID            <null>
        MON$COLLATION_ID                <null>
        CHARSET_NAME                    <null>
        COLLATION_NAME                  <null>
    """
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()
