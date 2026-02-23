#coding:utf-8

"""
ID:          n/a
TITLE:       Test of CURSOR functionality
DESCRIPTION:
    Original test see in:
    https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/FB_SQL_CUR_FIELD.script
NOTES:
    [21.09.2025] pzotov
    Firebird 3.x is not checked: it issues "attempted update of read-only column" without name of cursor ("CUR_01.ID").
    Checked on 6.0.0.1277 5.0.4.1713 4.0.7.3231
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """

    set list on;
    set term ^;

    create table t1 (n1 integer, n2 integer, n3 integer)^
    create trigger t1_1 before insert on t1 as
    begin
      new.n2 = new.n1;
    end^

    -- Test assignment with ':' in the l-value
    create trigger t1_2 before insert on t1
    as
    begin
      :new.n3 = :new.n1;
    end^

    create table t2 (n1 integer)^

    create view v1 as
      select *
        from (
          select 1 n1
            from rdb$database
        ) a
        full join (
          select 2 n2
            from rdb$database
        ) b
          on 1 = 0
        order by a.n1^

    commit^

    --######################

    -- Test assignment with ':' in the left-value
    execute block returns (o integer) as
      declare v1 integer;
    begin
      v1 = 1;

      o = v1;
      suspend;

      o = :v1;
      suspend;

      :v1 = 2;

      :o = :v1;
      suspend;
    end^
    select 'point-000' as msg from rdb$database^

    --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

    insert into t1 (n1) values (11)^
    select * from t1^
    select 'point-050' as msg from rdb$database^

    --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

    insert into t2 (n1) values (1)^
    insert into t2 (n1) values (2)^
    insert into t2 (n1) values (3)^

    execute block
    as
    begin
      for select *
            from t2
            as cursor cur_01
      do
      begin
        update t2 set n1 = n1 * 10 where current of cur_01;
      end
    end^
    select * from t2 order by n1^
    select 'point-100' as msg from rdb$database^

    execute block as
      declare cur_01 cursor for (
        select *
          from t2
      );
    begin
      open cur_01;
      
      while (1 = 1)
      do
      begin
        fetch cur_01;
        if (row_count = 0) then
            leave;

        update t2 set n1 = n1 * 10 where current of cur_01;
      end

      close cur_01;  
    end^

    select * from t2 order by n1^
    select 'point-150' as msg from rdb$database^

    --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

    execute block returns (out_id integer, out_name varchar(31)) as
    begin
      for select rdb$relation_id as id, rdb$relation_name as name
            from rdb$relations
            where rdb$relation_name in ('RDB$PAGES', 'RDB$DATABASE', 'RDB$FIELDS')
            order by rdb$relation_name
            as cursor cur_01
      do
      begin
        out_id = cur_01.id;       -- without ':'
        out_name = :cur_01.name;  -- with ':'
        suspend;
      end
    end^
    select 'point-200' as msg from rdb$database^

    --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

    execute block returns (out_id integer, out_name varchar(31)) as
    begin
      for select rdb$relation_id as id, rdb$relation_name as name
            from rdb$relations
            where rdb$relation_name in ('RDB$PAGES', 'RDB$DATABASE', 'RDB$FIELDS')
            order by rdb$relation_name
            into out_id, :out_name
            as cursor cur_01
      do
      begin
        suspend;
      end
    end^
    select 'point-250' as msg from rdb$database^

    --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

    execute block returns (out_id integer, out_name varchar(31)) as
    begin
      for select rdb$relation_id as id, rdb$relation_name as name
            from rdb$relations
            where rdb$relation_name in ('RDB$PAGES', 'RDB$DATABASE', 'RDB$FIELDS')
            order by rdb$relation_name
            as cursor cur_01
      do
      begin
        for select rdb$relation_id as id, rdb$relation_name as name
              from rdb$relations
              where rdb$relation_id = :cur_01.id
              into out_id, :out_name
        do
        begin
          suspend;
        end
      end
    end^
    select 'point-300' as msg from rdb$database^

    --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

    execute block returns (out_id integer, out_name varchar(31)) as
      declare cur_01 cursor for (
        select rdb$relation_id as id, rdb$relation_name as name
          from rdb$relations
          where rdb$relation_name in ('RDB$PAGES', 'RDB$DATABASE', 'RDB$FIELDS')
          order by rdb$relation_name
      );
    begin
      open cur_01;
      
      while (1 = 1)
      do
      begin
        fetch cur_01;
        if (row_count = 0) then
            leave;

        out_id = cur_01.id; 
        out_name = :cur_01.name; 
        suspend; 
      end

      close cur_01;  

      open cur_01;
      
      while (1 = 1)
      do
      begin
        fetch cur_01 into out_id, :out_name;
        if (row_count = 0) then
            leave;

        suspend; 
      end

      close cur_01;  
    end^
    select 'point-350' as msg from rdb$database^

    --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

    execute block returns (out_id integer, out_name varchar(31))
    as
      declare cur_01 cursor for (
        select rdb$relation_id as id, rdb$relation_name as name
          from rdb$relations
          where rdb$relation_name in ('RDB$PAGES', 'RDB$DATABASE', 'RDB$FIELDS')
          order by rdb$relation_name
      );

      declare cur_02 cursor for (
        select rdb$relation_id as id, rdb$relation_name as name
          from rdb$relations
          where rdb$relation_name in ('RDB$PAGES', 'RDB$DATABASE', 'RDB$FIELDS')
          and rdb$relation_id = :cur_01.id
          order by rdb$relation_name
      );
    begin
      open cur_01;

      open cur_02;
      fetch cur_02;
    end^
    select 'point-400' as msg from rdb$database^

    --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

    -- Test view

    execute block returns (n1 integer, n2 integer) as
    begin
      for select *
            from v1
            as cursor cur_01
      do
      begin
        n1 = cur_01.n1;
        n2 = cur_01.n2;
        suspend;
      end
    end^
    select 'point-450' as msg from rdb$database^

    --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

    -- Test access to a cursor out of scope
    execute block returns (out_id integer, out_name varchar(31))
    as
    begin
      for select rdb$relation_id as id, rdb$relation_name as name
            from rdb$relations
            where rdb$relation_name in ('RDB$PAGES', 'RDB$DATABASE', 'RDB$FIELDS')
            order by rdb$relation_name
            as cursor cur_01
      do
      begin
      end

      out_id = cur_01.id;
      out_name = :cur_01.name;
    end^
    select 'point-500' as msg from rdb$database^

    --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

    -- Test write to a field cursor - 1
    execute block returns (out_id integer, out_name varchar(31))
    as
      declare cur_01 cursor for (
        select rdb$relation_id as id, rdb$relation_name as name
          from rdb$relations
          where rdb$relation_name in ('RDB$PAGES', 'RDB$DATABASE', 'RDB$FIELDS')
          order by rdb$relation_name
      );
    begin
      cur_01.id = 1;
    end^
    select 'point-550' as msg from rdb$database^

    --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

    -- Test write to a field cursor - 2
    execute block returns (out_id integer, out_name varchar(31))
    as
    begin
      for select rdb$relation_id as id, rdb$relation_name as name
            from rdb$relations
            where rdb$relation_name in ('RDB$PAGES', 'RDB$DATABASE', 'RDB$FIELDS')
            order by rdb$relation_name
            as cursor cur_01
      do
      begin
        cur_01.id = 1;
      end
    end^
    select 'point-600' as msg from rdb$database^

    --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

    -- Test read a closed cursor
    execute block returns (out_id integer, out_name varchar(31))
    as
      declare cur_01 cursor for (
        select rdb$relation_id as id
          from rdb$database
      );
    begin
      out_id = cur_01.id; 
    end^
    select 'point-650' as msg from rdb$database^

    --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

    -- Test read a derived expression of a closed cursor
    execute block returns (out_id integer)
    as
      declare cur_01 cursor for (
        select 1 as id
          from rdb$database
      );
    begin
      out_id = cur_01.id; 
    end^
    select 'point-700' as msg from rdb$database^

    --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

    -- Test read a opened but not fetched cursor
    execute block returns (out_id integer, out_name varchar(31))
    as
      declare cur_01 cursor for (
        select rdb$relation_id as id
          from rdb$database
      );
    begin
      open cur_01;
      out_id = cur_01.id; 
    end^
    select 'point-750' as msg from rdb$database^

    --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

    -- Test read a cursor in the eof state
    execute block returns (out_id integer, out_name varchar(31))
    as
      declare cur_01 cursor for (
        select rdb$relation_id as id
          from rdb$database
      );
    begin
      open cur_01;
      fetch cur_01;
      fetch cur_01;
      out_id = cur_01.id; 
    end^
    select 'point-800' as msg from rdb$database^

    --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

    -- Test duplicate cursor - 1
    execute block returns (out_id integer, out_name varchar(31))
    as
      declare cur_01 cursor for (
        select rdb$relation_id as id
          from rdb$database
      );

      declare cur_01 cursor for (
        select rdb$relation_id as id
          from rdb$database
      );
    begin
    end^
    select 'point-850' as msg from rdb$database^

    --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

    -- Test duplicate cursor - 2
    execute block returns (out_id integer, out_name varchar(31))
    as
      declare cur_01 cursor for (
        select rdb$relation_id as id
          from rdb$database
      );
    begin
      for select rdb$relation_id as id
            from rdb$database
            as cursor cur_01
      do
      begin
      end
    end^
    select 'point-900' as msg from rdb$database^

    --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

    -- Test duplicate cursor - 3
    execute block returns (out_id integer, out_name varchar(31))
    as
    begin
      for select rdb$relation_id as id
            from rdb$database
            as cursor cur_01
      do
      begin
        for select rdb$relation_id as id
              from rdb$database
              as cursor cur_01
        do
        begin
        end
      end
    end^
    select 'point-999' as msg from rdb$database^
    set term ;^
"""

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

substitutions=[('=', ''), ('[ \t]+', ' '), ('(-)?At (block )?line(:)?\\s+\\d+.*', '')]
for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = isql_act('db', test_script, substitutions = substitutions)

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    expected_stdout = """
        O 1
        O 1
        O 2
        MSG point-000
        N1 11
        N2 11
        N3 11
        MSG point-050
        N1 10
        N1 20
        N1 30
        MSG point-100
        N1 100
        N1 200
        N1 300
        MSG point-150
        OUT_ID 1
        OUT_NAME RDB$DATABASE
        OUT_ID 2
        OUT_NAME RDB$FIELDS
        OUT_ID 0
        OUT_NAME RDB$PAGES
        MSG point-200
        OUT_ID 1
        OUT_NAME RDB$DATABASE
        OUT_ID 2
        OUT_NAME RDB$FIELDS
        OUT_ID 0
        OUT_NAME RDB$PAGES
        MSG point-250
        OUT_ID 1
        OUT_NAME RDB$DATABASE
        OUT_ID 2
        OUT_NAME RDB$FIELDS
        OUT_ID 0
        OUT_NAME RDB$PAGES
        MSG point-300
        OUT_ID 1
        OUT_NAME RDB$DATABASE
        OUT_ID 2
        OUT_NAME RDB$FIELDS
        OUT_ID 0
        OUT_NAME RDB$PAGES
        OUT_ID 1
        OUT_NAME RDB$DATABASE
        OUT_ID 2
        OUT_NAME RDB$FIELDS
        OUT_ID 0
        OUT_NAME RDB$PAGES
        MSG point-350
        Statement failed, SQLSTATE HY109
        Cursor CUR_01 is not positioned in a valid record
        -At block line: 22, col: 7
        MSG point-400
        N1 <null>
        N2 2
        N1 1
        N2 <null>
        MSG point-450
        Statement failed, SQLSTATE 42S22
        Dynamic SQL Error
        -SQL error code -206
        -Column unknown
        -CUR_01.ID
        -At line 16, column 16
        MSG point-500
        Statement failed, SQLSTATE 42000
        attempted update of read-only column CUR_01.ID
        MSG point-550
        Statement failed, SQLSTATE 42000
        attempted update of read-only column CUR_01.ID
        MSG point-600
        Statement failed, SQLSTATE 24000
        Cursor is not open
        -At block line: 11, col: 7
        MSG point-650
        Statement failed, SQLSTATE 24000
        Cursor is not open
        -At block line: 11, col: 7
        MSG point-700
        Statement failed, SQLSTATE HY109
        Cursor CUR_01 is not positioned in a valid record
        -At block line: 12, col: 7
        MSG point-750
        Statement failed, SQLSTATE HY109
        Cursor CUR_01 is not positioned in a valid record
        -At block line: 14, col: 7
        MSG point-800
        Statement failed, SQLSTATE 42000
        Dynamic SQL Error
        -SQL error code -637
        -duplicate specification of CUR_01 - not supported
        MSG point-850
        Statement failed, SQLSTATE 34000
        Dynamic SQL Error
        -SQL error code -502
        -Invalid cursor declaration
        -Cursor CUR_01 already exists in the current context
        MSG point-900
        Statement failed, SQLSTATE 34000
        Dynamic SQL Error
        -SQL error code -502
        -Invalid cursor declaration
        -Cursor CUR_01 already exists in the current context
        MSG point-999
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
