#coding:utf-8

"""
ID:          n/a
ISSUE:       https://github.com/FirebirdSQL/firebird/commit/8f728e632e49ed0fb470cf1f0c6749572502d2a2
TITLE:       Fix cardinality estimations for semi/anti joins
DESCRIPTION:
    As explained by dimitr (letter 26.01.2026 18:02), improvement relates to cardinality evaluation
    for a query which can be transformed from IN/EXISTS to semi-join (i.e. return columns only from
    left table for which at least one matching row exists in the right table; only one row from left
    table must be returned unlike an INNER JOIN).
    Before fix, semi-join cardinality was estimated as card(outer) * card(inner) * <join_type_mult>
    where <join_type_mult> is 1 for NL and 0.001 for HJ.
    But for card(inner) > 1000 and join_type_mult = 0.001 (i.e. HJ), the semi-join cardinality become
    than greater card(outer) adn this must not occur at all (by definition of semi-join).

    After this fix, semi-join cardinality is evaluated as card(outer) / 2.
    
    Test creates master-detail tables (both are indexed) and two views:
        * V_TEST_NL - for semi-join query that will use Nested Loops
        * V_TEST_HJ - for semi-join query that will use Hash Join

    We obtain explained plans for these views and also run rdb$sql.explain() in order to get cardinality
    for each line of explained plan. Parsing of rdb$sql.explain() output is performed in order to find
    there lines that match to paterns that point to semi-join and first line with 'Table ...' in it.
    First line contain cardinality to semi-join node, second - for OUTER data sources.
    The ratio semi_join_card / outer_src_card (rounded to 1 digit after decimal point) must be ~0.5.
NOTES:
    [27.01.2026] pzotov
    Confirmed problem on 6.0.0.1397-051c625 (25.01.2026 20:22).
    Checked on 6.0.0.1397-653b619 (intermediate snapshot, 26.01.2026 21:24).
"""
import re
from firebird.driver import DatabaseError

import pytest
from firebird.qa import *

init_sql = f"""
    recreate sequence g;
    recreate table tplan(rn int, o_name varchar(63), o_alias varchar(63), card double precision, access_txt varchar(32760));
    recreate table tmain(id int primary key using index tmain_pk);
    recreate table tdetl(id int primary key using index tdetl_pk, pid int);

    insert into tmain(id) select row_number()over() from rdb$types rows 100;
    set term ^;
    execute block as
        declare n int = 100000;
        declare c int;
    begin
        select count(*) from tmain into c;
        while (n>0) do
        begin
            if (mod(:n, 67) > 0) then
            begin
                insert into tdetl(id, pid) values( gen_id(g,1), mod(:n, 67) );
            end
            n =  n - 1;
        end
    end
    ^
    set term ;^
    commit;

    set echo on;
    alter table tdetl add constraint tdetl_fk foreign key(pid) references tmain(id);

    set statistics index tmain_pk;
    set statistics index tdetl_pk;
    set statistics index tdetl_fk;
    commit;

    recreate view v_test_nl as
    select count(*) as cnt
    from tmain m
    where
        exists(select * from tdetl d where d.pid = m.id)
        and m.id between 35 and 75
    ;
    recreate view v_test_hj as
    select count(*) as cnt
    from tmain m
    where
        exists(select * from tdetl d where d.pid + 0 = m.id)
        and m.id between 35 and 75
    ;
    commit;
"""

db = db_factory(init = init_sql)

substitutions = []

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = python_act('db', substitutions = substitutions)

#---------------------------------------------------------

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#---------------------------------------------------------

query_map = {
    1000 : (
              """
                select * from v_test_nl
              """
             ,'Check that plan uses NESTED LOOPS'
           )
   ,2000 : (
              """
                select * from v_test_hj
              """
             ,'Check that plan uses HASH JOIN'
           )
}

@pytest.mark.version('>=6.0')
def test_1(act: Action, capsys):

    p_line_with_semi_join = re.compile( '(Nested Loop Join \\(?semi\\)?)|(Hash Join \\(?semi\\)?)', re.IGNORECASE )
    p_line_with_table_name = re.compile('Table\\s+\\S+\\s+as\\s+')
    ratio_prefix = 'Ratio semi_join_card / outer_src_card:'

    with act.db.connect() as con:
        cur = con.cursor()
        for q_idx, q_tuple in query_map.items():
            test_sql, qry_comment = q_tuple[:2]
            ps = None
            try:
                ps = cur.prepare(test_sql)
                print(q_idx)
                print(test_sql)
                print(qry_comment)
                print( '\n'.join([replace_leading(s) for s in ps.detailed_plan.split('\n')]) )
            except DatabaseError as e:
                print(e.__str__())
                print(e.gds_codes)
            finally:
                if ps:
                    ps.free()
            #---------------------------------------------------------------
            rdb_explain_sql = f"""
                select
                     plan_line as rn
                    ,trim(object_name) as o_name
                    ,alias as o_alias
                    ,cardinality
                    ,cast(access_path as varchar(32760)) as access_txt
                from rdb$sql.explain(q'#{test_sql}#')
            """
            cur.execute(rdb_explain_sql)
            ccol=cur.description

            cardinality_column = [i for i,x in enumerate(ccol) if x[0] == 'CARDINALITY']
            assert cardinality_column, "Query to rdb$sql.explain does not contain column 'CARDINALITY'"
            cardinality_col_idx = cardinality_column[0]

            access_column = [i for i,x in enumerate(ccol) if x[0] == 'ACCESS_TXT']
            assert access_column, "Query to rdb$sql.explain does not contain column 'ACCESS_TXT'"
            access_col_idx = access_column[0]

            semi_join_card = outer_src_card = -1
            for r in cur:
                for i in range(0,len(ccol)):
                    # Example of lines with acces path related to semi-join:
                    #     Nested Loop Join (semi)
                    #     Hash Join (semi) (keys: 1, total key length: 8) 
                    if semi_join_card < 0 and p_line_with_semi_join.search( r[ access_col_idx ] ):
                        semi_join_card= r[ cardinality_col_idx ] 

                    # FIRST line in explained plan that matches to pattern 'Table <some_name> ...' descibes
                    # OUTER source. We have to store its cardinality to be compared further with semi_join_card.
                    # Example of access path for table 'TMAIN' (multi-lined):
                    #     Table "PUBLIC"."TMAIN" as "PUBLIC"."V_TEST" "M" Access By ID
                    #         -> Bitmap
                    #             -> Index "PUBLIC"."TMAIN_PK" Range Scan (lower bound: 1/1, upper bound: 1/1)
                    #
                    if outer_src_card < 0 and p_line_with_table_name.search(r[ access_col_idx ]):
                        outer_src_card = r[ cardinality_col_idx ] 

                    if semi_join_card > 0 and outer_src_card > 0:
                        break
            if semi_join_card > 0 and outer_src_card > 0 and round(semi_join_card / outer_src_card, 1) == 0.5:
                print(ratio_prefix + ' EXPECTED')
            else:
                print(ratio_prefix + ' ### UNEXPECTED ###')
                print(f'{semi_join_card=} ; {outer_src_card=} ; {semi_join_card / outer_src_card=}')
            

    expected_stdout = f"""
        1000
        {query_map[1000][0]}
        {query_map[1000][1]}
        Select Expression
        ....-> Aggregate
        ........-> Nested Loop Join (semi)
        ............-> Filter
        ................-> Table TMAIN as V_TEST_NL M Access By ID
        ....................-> Bitmap
        ........................-> Index TMAIN_PK Range Scan (lower bound: 1/1, upper bound: 1/1)
        ............-> Filter
        ................-> Table TDETL as V_TEST_NL D Access By ID
        ....................-> Bitmap
        ........................-> Index TDETL_FK Range Scan (full match)
        {ratio_prefix} EXPECTED

        2000
        {query_map[2000][0]}
        {query_map[2000][1]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Hash Join (semi) (keys: 1, total key length: 8)
        ................-> Filter
        ....................-> Table TMAIN as V_TEST_HJ M Access By ID
        ........................-> Bitmap
        ............................-> Index TMAIN_PK Range Scan (lower bound: 1/1, upper bound: 1/1)
        ................-> Record Buffer (record length: 25)
        ....................-> Table TDETL as V_TEST_HJ D Full Scan
        {ratio_prefix} EXPECTED
    """
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
