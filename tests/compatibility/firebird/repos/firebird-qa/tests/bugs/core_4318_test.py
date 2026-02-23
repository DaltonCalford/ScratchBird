#coding:utf-8

"""
ID:          issue-4641
ISSUE:       4641
TITLE:       Regression: Predicates involving PSQL variables/parameters are not pushed inside the aggregation
DESCRIPTION:
JIRA:        CORE-4318
FBTEST:      bugs.core_4318
NOTES:
    [29.06.2025] pzotov
    1. In 3.0.0.30837 plan was:
        Select Expression
           -> Singularity Check
               -> Filter
                   -> Aggregate
                       -> Table "T T2" Access By ID
                           -> Index "FK_T2_REF_T1" Scan
        (i.e. there was NO "Filter" between "Aggregate" and "Table "T T2" Access By ID")
    2. Separated expected output for FB major versions prior/since 6.x.
       No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.

"""
from firebird.driver import DatabaseError

import pytest
from firebird.qa import *

init_script = """
    recreate table t2 (
      id integer not null,
      t1_id integer
    );
    commit;

    recreate table t1 (
      id integer not null
    );
    commit;

    set term ^;

    execute block
    as
    declare variable i integer = 0;
    begin
      while (i < 1000) do begin
        i = i + 1;

        insert into t2(id, t1_id) values(:i, mod(:i, 10));

        merge into t1 using (
          select mod(:i, 10) as f from rdb$database
        ) src on t1.id = src.f
        when not matched then
           insert (id) values(src.f);

      end -- while (i < 1000) do begin

    end^
    set term ;^
    commit;

    alter table t1 add constraint pk_t1 primary key (id);
    alter table t2 add constraint pk_t2 primary key (id);
    alter table t2 add constraint fk_t2_ref_t1 foreign key (t1_id) references t1(id);
    commit;
"""

db = db_factory(init=init_script)

# line 5, column 15 ==> line N, column N
substitutions = [ ( r'line(:)?\s+\d+', 'line N' ), ( r'col(umn)?(:)?\s+\d+', 'column N' ) ]
act = python_act('db', substitutions = substitutions)

#-----------------------------------------------------------

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#-----------------------------------------------------------

@pytest.mark.version('>=3.0')
def test_1(act: Action, capsys):

    qry_map = {
        1000 :
        """
            execute block returns (s integer) as
                declare v integer = 1;
            begin
              with t as (
                  select t1_id as t1_id, sum(id) as s -- FB 5.x: "Select Expression (line NNN, column MMM)"
                  from t2
                  group by 1
              )
              select s
              from t
              where t1_id = :v
              into :s;

              suspend;
            end

        """
    }

    with act.db.connect() as con:
        cur = con.cursor()

        for k, v in qry_map.items():
            ps, rs = None, None
            try:
                ps = cur.prepare(v)

                print(v)
                # Print explained plan with padding eash line by dots in order to see indentations:
                print( '\n'.join([replace_leading(s) for s in ps.detailed_plan.split('\n')]) )
                print('')
            except DatabaseError as e:
                print(e.__str__())
                print(e.gds_codes)
            finally:
                if rs:
                    rs.close() # <<< EXPLICITLY CLOSING CURSOR RESULTS
                if ps:
                    ps.free() # <<< 29.06.2025 NEED EVEN IF CURSOR WAS NOT SELECT ANY DATA! OTHERWISE PYTEST CAN HANG ON EXIT!

    expected_stdout_4x = f"""
        {qry_map[1000]}
        Select Expression
        ....-> Singularity Check
        ........-> Filter
        ............-> Aggregate
        ................-> Filter
        ....................-> Table "T2" as "T T2" Access By ID
        ........................-> Index "FK_T2_REF_T1" Range Scan (full match)
    """

    expected_stdout_5x = f"""
        {qry_map[1000]}
        Select Expression (line N, column N)
        ....-> Singularity Check
        ........-> Filter
        ............-> Aggregate
        ................-> Filter
        ....................-> Table "T2" as "T T2" Access By ID
        ........................-> Index "FK_T2_REF_T1" Range Scan (full match)
    """

    expected_stdout_6x = f"""
        {qry_map[1000]}
        Select Expression (line N, column N)
        ....-> Singularity Check
        ........-> Filter
        ............-> Aggregate
        ................-> Filter
        ....................-> Table "PUBLIC"."T2" as "T" "PUBLIC"."T2" Access By ID
        ........................-> Index "PUBLIC"."FK_T2_REF_T1" Range Scan (full match)
    """

    act.expected_stdout = expected_stdout_4x if act.is_version('<5') else expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
