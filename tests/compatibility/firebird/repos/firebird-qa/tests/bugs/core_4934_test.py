#coding:utf-8

"""
ID:          issue-5225
ISSUE:       5225
TITLE:       Different collation ID for altered computed column
DESCRIPTION:
JIRA:        CORE-4934
FBTEST:      bugs.core_4934
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table t_country(id int);
    commit;

    create or alter view v_test as
    select
         rf.rdb$relation_name rel_name
        ,rf.rdb$field_name fld_name
        --,rf.rdb$field_source fld_source
        ,f.rdb$character_set_id cset_id
        ,f.rdb$collation_id coll_id
    from rdb$relation_fields rf
    left join rdb$fields f on rf.rdb$field_source = f.rdb$field_name
    where rf.rdb$relation_name in( upper('t_country'), upper('t_translation_meta'))
    order by 1,2;

    commit;

    recreate table t_translation_meta (
        f_trm_id bigint not null,
        f_trm_code varchar(512) character set utf8 not null collate
        unicode_ci
    );
    commit;

    recreate table t_country (
        f_cnr_id bigint not null,
        f_trm_name_id bigint default -1 not null,
        cf_cnr_name computed by (
            (
                (
                    select f_trm_code
                    from t_translation_meta
                    where f_trm_id = f_trm_name_id
                )
            )
        )
    );
    commit;

    set width rel_name 20;
    set width fld_name 20;
    set width fld_source 20;

    set list on;

    select 'before' msg, v.* from v_test v;


    alter table t_country alter cf_cnr_name computed by
    (
        (
            (
                select
                f_trm_code
                from t_translation_meta
                where f_trm_id = f_trm_name_id
            )
        )
    );
    commit;

    select 'after' msg, v.* from v_test v;
"""

act = isql_act('db', test_script)

expected_stdout = """
    MSG                             before
    REL_NAME                        T_COUNTRY
    FLD_NAME                        CF_CNR_NAME
    CSET_ID                         4
    COLL_ID                         3

    MSG                             before
    REL_NAME                        T_COUNTRY
    FLD_NAME                        F_CNR_ID
    CSET_ID                         <null>
    COLL_ID                         <null>

    MSG                             before
    REL_NAME                        T_COUNTRY
    FLD_NAME                        F_TRM_NAME_ID
    CSET_ID                         <null>
    COLL_ID                         <null>

    MSG                             before
    REL_NAME                        T_TRANSLATION_META
    FLD_NAME                        F_TRM_CODE
    CSET_ID                         4
    COLL_ID                         3

    MSG                             before
    REL_NAME                        T_TRANSLATION_META
    FLD_NAME                        F_TRM_ID
    CSET_ID                         <null>
    COLL_ID                         <null>



    MSG                             after
    REL_NAME                        T_COUNTRY
    FLD_NAME                        CF_CNR_NAME
    CSET_ID                         4
    COLL_ID                         3

    MSG                             after
    REL_NAME                        T_COUNTRY
    FLD_NAME                        F_CNR_ID
    CSET_ID                         <null>
    COLL_ID                         <null>

    MSG                             after
    REL_NAME                        T_COUNTRY
    FLD_NAME                        F_TRM_NAME_ID
    CSET_ID                         <null>
    COLL_ID                         <null>

    MSG                             after
    REL_NAME                        T_TRANSLATION_META
    FLD_NAME                        F_TRM_CODE
    CSET_ID                         4
    COLL_ID                         3

    MSG                             after
    REL_NAME                        T_TRANSLATION_META
    FLD_NAME                        F_TRM_ID
    CSET_ID                         <null>
    COLL_ID                         <null>
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

