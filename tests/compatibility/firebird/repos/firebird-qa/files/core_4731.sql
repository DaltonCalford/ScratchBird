	set wng off;
	set term ^;
	execute block as
	begin
		-- do NOT remote! This can be useful later for debug purposes!
		--rdb$set_context('USER_SESSION','DEBUG_RDB_TABLE', 'RDB$PROCEDURES');
	end
	^
	set term ;^

	create or alter user %(non_privileged_name)s password '123';
	create or alter user %(dba_privileged_name)s password '123';
	commit;

	revoke all on all from %(non_privileged_name)s;
	revoke all on all from %(dba_privileged_name)s;
	commit;

	grant RDB$ADMIN to %(dba_privileged_name)s;
	commit;

	grant create TABLE to %(non_privileged_name)s, %(dba_privileged_name)s;
	grant create VIEW to %(non_privileged_name)s, %(dba_privileged_name)s;
	grant create PROCEDURE to %(non_privileged_name)s, %(dba_privileged_name)s;
	grant create FUNCTION to %(non_privileged_name)s, %(dba_privileged_name)s;
	grant create PACKAGE to %(non_privileged_name)s, %(dba_privileged_name)s;
	grant create GENERATOR to %(non_privileged_name)s, %(dba_privileged_name)s;
	grant create SEQUENCE to %(non_privileged_name)s, %(dba_privileged_name)s;
	grant create DOMAIN to %(non_privileged_name)s, %(dba_privileged_name)s;
	grant create EXCEPTION to %(non_privileged_name)s, %(dba_privileged_name)s;
	grant create ROLE to %(non_privileged_name)s, %(dba_privileged_name)s;
	grant create CHARACTER SET to %(non_privileged_name)s, %(dba_privileged_name)s;
	grant create COLLATION to %(non_privileged_name)s, %(dba_privileged_name)s;
	grant create FILTER to %(non_privileged_name)s, %(dba_privileged_name)s;

	grant alter any TABLE to %(non_privileged_name)s, %(dba_privileged_name)s;
	grant alter any VIEW to %(non_privileged_name)s, %(dba_privileged_name)s;
	grant alter any PROCEDURE to %(non_privileged_name)s, %(dba_privileged_name)s;
	grant alter any FUNCTION to %(non_privileged_name)s, %(dba_privileged_name)s;
	grant alter any PACKAGE to %(non_privileged_name)s, %(dba_privileged_name)s;
	grant alter any GENERATOR to %(non_privileged_name)s, %(dba_privileged_name)s;
	grant alter any SEQUENCE to %(non_privileged_name)s, %(dba_privileged_name)s;
	grant alter any DOMAIN to %(non_privileged_name)s, %(dba_privileged_name)s;
	grant alter any EXCEPTION to %(non_privileged_name)s, %(dba_privileged_name)s;
	grant alter any ROLE to %(non_privileged_name)s, %(dba_privileged_name)s;
	grant alter any CHARACTER SET to %(non_privileged_name)s, %(dba_privileged_name)s;
	grant alter any COLLATION to %(non_privileged_name)s, %(dba_privileged_name)s;
	grant alter any FILTER to %(non_privileged_name)s, %(dba_privileged_name)s;

	grant drop any TABLE to %(non_privileged_name)s, %(dba_privileged_name)s;
	grant drop any VIEW to %(non_privileged_name)s, %(dba_privileged_name)s;
	grant drop any PROCEDURE to %(non_privileged_name)s, %(dba_privileged_name)s;
	grant drop any FUNCTION to %(non_privileged_name)s, %(dba_privileged_name)s;
	grant drop any PACKAGE to %(non_privileged_name)s, %(dba_privileged_name)s;
	grant drop any GENERATOR to %(non_privileged_name)s, %(dba_privileged_name)s;
	grant drop any SEQUENCE to %(non_privileged_name)s, %(dba_privileged_name)s;
	grant drop any DOMAIN to %(non_privileged_name)s, %(dba_privileged_name)s;
	grant drop any EXCEPTION to %(non_privileged_name)s, %(dba_privileged_name)s;
	grant drop any ROLE to %(non_privileged_name)s, %(dba_privileged_name)s;
	grant drop any CHARACTER SET to %(non_privileged_name)s, %(dba_privileged_name)s;
	grant drop any COLLATION to %(non_privileged_name)s, %(dba_privileged_name)s;
	grant drop any FILTER to %(non_privileged_name)s, %(dba_privileged_name)s;

	commit;

	create or alter view v_passed as select 1 id from rdb$database;
	commit;


	recreate table dml_expr(s varchar(1024));
	commit;
	insert into dml_expr(s)
	select 'create sequence g_common' from rdb$database union all
	select 'create exception ex_bad_argument ''argument @1 is invalid''' from rdb$database union all
	select 'create collation nums_coll for utf8 from unicode case insensitive ''NUMERIC-SORT=1''' from rdb$database union all

	-- domain DDL contains 'default' and 'check' clauses in order to have ability to get record from rdb$fields
	-- which has non-empty rdb$default_source and rdb$validation_source:
	select 'create domain dm_int as int default -2147483648 check(value is distinct from 0)' from rdb$database union all
	select 'create domain dm_nums as varchar(20) character set utf8 collate nums_coll' from rdb$database union all
	
	-- ::: NB ::: table 't01_single' must be created BEFORE any other custom tables and must have NO pk/uk/fk
	-- in order to get at least one record from rdb$indices related to index not participated in constraint
	-- (that index we can try to DROP)
	-- 16.10.2025: added array column in order to check rdb$field_dimensions:
	-- 18.10.2025: added t01_partial index in order to check rdb$indices.rdb$condition_source and rdb$indices.rdb$condition_blr
	select 'create table t01_single(x int, y int, arr_int integer[0:3, 0:3])' from rdb$database union all
	select 'create index t01_single_x on t01_single(x)' from rdb$database union all
	select 'create index t01_computed on t01_single computed by(x+y)' from rdb$database union all
	select 'create index t01_partial on t01_single(y) where y is not null'from rdb$database where left(rdb$get_context('SYSTEM','ENGINE_VERSION'),2) NOT in ('3.', '4.') union all
	select 'create table tmain(id dm_int primary key using index tmain_pk, txt dm_nums, z computed by(char_length(txt)), constraint tmain_chk_01 check(txt is not null) )' from rdb$database union all
	select 'create table tdetl(id dm_int primary key using index tdetl_pk, pid dm_int, constraint tdetl_fk foreign key(pid) references tmain(id) on delete cascade)' from rdb$database union all
	-- select 'create index test_master_txt on tmain(txt)' from rdb$database union all
	select 'create view v_test_master as select * from tmain' from rdb$database union all
	select 'create procedure sp_test_master(a_id dm_int default 2147483647) returns(o_txt dm_nums) as begin for select txt from tmain where id=:a_id into o_txt do suspend; end' from rdb$database union all
	select 'create function fn_test_master(a_id dm_int default 2147483647) returns dm_nums as begin return (select txt from tmain where id=:a_id); end' from rdb$database union all
	select 'create trigger test_master_bi for tmain active before insert as begin new.id=gen_id(g_common,1); end' from rdb$database union all
	select 'create package test_pkg as begin procedure sp_pkg_test(a_id dm_int default 2147483647) returns(o_txt dm_nums); end' from rdb$database union all 
	select 'create package body test_pkg as begin procedure sp_pkg_test(a_id dm_int) returns(o_txt dm_nums) as begin for select txt from tmain where id=:a_id into o_txt do suspend; end end' from rdb$database union all
	-- added 16.10.2025 ---
    select 'declare filter aboba input_type 1 output_type -4 entry_point ''desc_filter'' module_name ''filterlib''' from rdb$database union all
	select 'create mapping map_local_existent using plugin Srp from any user to user' from rdb$database union all
	select 'create role manager' from rdb$database union all
	select 'alter database include all to publication' from rdb$database where rdb$get_context('SYSTEM','ENGINE_VERSION') not starting with '3.0'
	-----------------------
	;
	commit;

	set term ^;
	execute block as
		declare v_dbname type of column mon$database.mon$database_name;
		declare v_usr type of column sec$users.sec$user_name = '%(dba_privileged_name)s';
		declare v_pwd varchar(20) = '123';
		declare v_role varchar(20) = 'RDB$ADMIN';
	begin
		v_dbname = 'localhost:' ||  rdb$get_context('SYSTEM','DB_NAME');

		for select s from dml_expr as cursor c
		do
		execute statement (c.s)
				on external (v_dbname)
				as user  upper(v_usr) password (v_pwd) role (v_role);
	end
	^
	set term ;^
	commit;

    -- DML for RDB$TYPES is allowed to SYSDBA or user who has system privilege CREATE_USER_TYPES.
    -- We add custom type in order to check further ability to change it using direct commands.
    insert into rdb$types(rdb$field_name, rdb$type, rdb$type_name, rdb$description, rdb$system_flag)
    values('amount_avaliable', -32767, 'stock_amount','Total number of units that can be sold immediately to any customer', 0);

	recreate table vulnerable_on_sys_tables(
		sys_table rdb$relation_name, 
		ord_pos smallint, -- for proper sorting
		ret_dbkey smallint default 0, -- 1 ==> this expression CONTAINS 'returning [t.]rdb$dbkey, otherwise 0
		sys_dbkey char(8) character set octets, -- value of returned rdb$db_key (to be sure that change really occured)
		vulnerable_type varchar(10),
		vulnerable_gdscode int,
		vulnerable_sqlcode int,
		vulnerable_sqlstate char(5),
		vulnerable_expr varchar(1024),
		dml_where varchar(255),
		actual_role varchar(20),
		id int generated by default as identity constraint pk_vulnerable_on_sys_tables primary key
	);
	commit;

	create or alter view v_passed as
	select vulnerable_expr -- || ' rollback;' 
		   || iif( 
				   v.ret_dbkey = 1,
				   ' -- length of returned rdb$dbkey='|| coalesce(octet_length(sys_dbkey),'0'), 
				   '' 
				 )
		   as vulnerable_expr
	from vulnerable_on_sys_tables v 
	where v.vulnerable_gdscode < 0
	and (
		  v.ret_dbkey is distinct from 1
		  or 
		  coalesce(octet_length(sys_dbkey),0) > 0 
		)
	order by 
	     sys_table
	    ,vulnerable_expr -- before 23.10.2015: "order by  sys_table, id" --> FAILED on comparison with expected stdout because 'id' was not included in output
	;
	commit;

	-- ##################################################################
	-- [ 1 ] attempts to create new tables with FKs ref. to SYSTEM tables 
	-- ##################################################################

	set term ^;

	create or alter procedure sp_gen_expr_for_creating_fkeys as
		declare rel_name varchar(31) character set unicode_fss collate unicode_fss;
		declare vulnerable_expr varchar(1024) character set unicode_fss collate unicode_fss;
		declare v_fk1 varchar(8192) character set unicode_fss collate unicode_fss;
		declare v_fks varchar(8192) character set unicode_fss collate unicode_fss;
		declare v_mf varchar(8192) character set unicode_fss collate unicode_fss;
	begin
		for
			with
			a as (
			select
				lower(rc.rdb$relation_name) tab_name
				--,rc.rdb$constraint_name cnt_name
				,lower(rc.rdb$index_name) idx_name
				--,rc.rdb$constraint_type
				--,ri.rdb$index_id
				,rs.rdb$field_position fld_pos
				,lower(rs.rdb$field_name) fld_name
				--,rf.rdb$field_name
				,rf.rdb$field_source fld_src

                ,case f.rdb$field_type
                    when 7 then
                        -- smallint; numeric 1.x-4.x ('x' see in f.rdb$field_scale)
                        case coalesce(f.rdb$field_sub_type,0)
                            when 0 then 'smallint'
                            when 1 then 'numeric'
                            else 'unknown'
                        end
                    when 8 then
                        -- integer; numeric 5.x-9.x; decimal 1.x-9.x
                        case coalesce(f.rdb$field_sub_type,0)
                            when 0 then 'integer'
                            when 1 then 'numeric'
                            when 2 then 'decimal'
                            else 'unknown'
                        end
                    when 10 then 'float'
                    when 12 then 'date'
                    when 13 then 'time' -- w/o time zone
                    when 14 then
                        case coalesce(f.rdb$field_sub_type,0)
                            when 1 then 'binary'
                            else 'char' -- NB: field_sub_type can be 3 in rdb$fields.rdb$field_name
                            --when 0 then 'char'
                            --else 'unknown'
                        end
                    when 16 then
                        -- bigint; numeric 10.x-18.x; decimal 10.x-18.x
                        case coalesce(f.rdb$field_sub_type,0)
                            when 0 then 'bigint'
                            when 1 then 'numeric'
                            when 2 then 'decimal'
                            else 'unknown'
                        end
                    when 23 then 'boolean'
                    when 24 then 'decfloat(16)'
                    when 25 then 'decfloat(34)'
                    when 26 then
                        -- int128; numeric 19.x-38.x; decimal 19.x-38.x
                        case coalesce(f.rdb$field_sub_type,0)
                            when 0 then 'int128'
                            when 1 then 'numeric'
                            when 2 then 'decimal'
                            else 'unknown'
                        end
                    when 27 then -- dialect 1 only
                      case f.rdb$field_scale
                        when 0 then 'double precision'
                        else 'numeric' -- (15,' || cast(-f.rdb$field_scale as varchar(6)) || ')'
                      end
                    when 28 then 'time with time zone' -- with time zone
                    when 29 then 'timestamp with time zone' -- with time zone
                    when 35 then iif(m.dia=1, 'date', 'timestamp') -- timestamp w/o timezone
                    when 37 then
                        case coalesce(f.rdb$field_sub_type,0)
                            when 1 then 'varbinary'
                            else 'varchar'
                            --when 0 then 'varchar'
                            --else 'unknown'
                        end
                    when 261 then 'blob' -- sub_type ' || f.rdb$field_sub_type || ' segment size ' || f.rdb$segment_length
                    else 'unknown'
                end as fld_base_type

			   ,cast(f.rdb$field_length / iif(ce.rdb$character_set_name in ( upper('utf8'), upper('un~icode_fss') ), 4, 1) as int) fld_len
			   ,lower(ce.rdb$character_set_name) cset
			   ,lower(co.rdb$collation_name) coll
			   ,dense_rank()over(partition by rc.rdb$relation_name order by rc.rdb$index_name, rs.rdb$field_position) dr1
			   ,dense_rank()over(partition by rc.rdb$relation_name, rc.rdb$index_name order by rs.rdb$field_position) dr2
			   ,dense_rank()over(partition by rc.rdb$relation_name, rc.rdb$index_name order by rs.rdb$field_position desc) dr2d
			   ,count(*)over(partition by rc.rdb$relation_name) fld_cnt
			from (select m.mon$sql_dialect as dia from mon$database m) m
			cross join rdb$relation_constraints rc
			join rdb$relations rr on rc.rdb$relation_name = rr.rdb$relation_name
			join rdb$indices ri on rc.rdb$index_name = ri.rdb$index_name
			join rdb$index_segments rs on ri.rdb$index_name = rs.rdb$index_name
			left join rdb$relation_fields rf on rc.rdb$relation_name = rf.rdb$relation_name and rs.rdb$field_name = rf.rdb$field_name
			left join rdb$fields f on rf.rdb$field_source = f.rdb$field_name
			left join rdb$collations co on f.rdb$character_set_id=co.rdb$character_set_id and f.rdb$collation_id=co.rdb$collation_id
			left join rdb$character_sets ce on co.rdb$character_set_id=ce.rdb$character_set_id
			where
				--rc.rdb$relation_name --starting with upper('rdb$')
				coalesce(rr.rdb$system_flag,0) is distinct from 0 -- ###  S Y S T E M    t a b l e s  ###
				and rr.rdb$relation_type = 0 -- exclude: views, GTTs, external tables
				and rr.rdb$relation_name not starting with 'MON$'
				AND (RR.RDB$RELATION_NAME = rdb$get_context('USER_SESSION','DEBUG_RDB_TABLE') or rdb$get_context('USER_SESSION','DEBUG_RDB_TABLE') is null)
			)
		   ,b as (
				select
					tab_name,
					idx_name,
					fld_pos,
					fld_name,
					fld_src,
					fld_base_type,
					fld_len,
					cset,
					coll,
					dr1,
					dr2,
					dr2d,
					fld_cnt,
					trim( replace( fld_name, '$','_') )||' '||trim(fld_base_type)
					||iif( fld_base_type in ('char','varchar', 'numeric'),
						  '(' || fld_len || ')' || iif( fld_base_type='numeric', '', ' '||trim(coalesce(' character set '||cset, ''))||' '||trim(coalesce(' collate '||coll, '')))
						  ,''
						 ) fld_full
				from a
		   )

		   ,c as (
				select
					tab_name,
					idx_name,
					fld_pos,
					fld_name,
					fld_src,
					fld_base_type,
					fld_len,
					cset,
					coll,
					dr1,
					dr2,
					dr2d,
					fld_cnt,
					fld_full,
					(select list(distinct fld_full) from b x where x.tab_name = b.tab_name) fld_list
				from b
		   )
		select *
		from c
		order by tab_name, idx_name, dr1

		as cursor c
		do begin
		   if ( c.dr1 = 1 ) then
		   begin
			   vulnerable_expr = 'recreate table ' || trim(replace(c.tab_name,'$','_')) || '(' || c.fld_list;
			   v_fks = '';
		   end

		   if ( c.dr2 = 1 ) then begin
			   v_fk1 = '';
			   v_mf = '';
		   end

		   if ( c.dr2 = 1 ) then
			   v_fk1 = v_fk1 || '  ,constraint fk_'
							 || lower(trim(replace(c.idx_name,'$','_')))
							 || '   foreign key(';

		   v_fk1 = v_fk1 || iif( c.dr2 = 1, '', ', ') || trim( replace( c.fld_name, '$', '_' ) );
		   v_mf = v_mf || iif( c.dr2 = 1, '', ', ') || trim( c.fld_name );

		   if ( c.dr2d = 1 ) then
		   begin
			   v_fk1 = v_fk1 ||  ')' || '   references ' || trim(lower(c.tab_name)) || '(' || v_mf || ')';
			   v_fks = v_fks || v_fk1;
		   end

		   if ( c.dr1 = c.fld_cnt ) then
		   begin
			   vulnerable_expr = vulnerable_expr || v_fks || ');';

			   rel_name = c.tab_name;

			   insert into vulnerable_on_sys_tables( sys_table, vulnerable_type, vulnerable_expr)
											 values( :rel_name, 'ADD_FKY',            :vulnerable_expr);
		   end
		end
	end
	^ -- sp_gen_expr_for_creating_fkeys

	-- #################################################################
	-- [ 2 ] attempts to change SYSTEM tables with DML or DDL statements 
	-- #################################################################

	create or alter procedure sp_gen_expr_for_direct_change as
	begin

		for
		    ------------------
            with recursive
            dbg as (
                 --select upper('rdb$fields') as debug_rdb_table from rdb$database
                 --select upper('rdb$transactions') as debug_rdb_table from rdb$database
                 --select upper('rdb$character_sets') as debug_rdb_table from rdb$database
                 --select upper('rdb$auth_mapping') as debug_rdb_table from rdb$database
                 --select upper('tbase') as debug_rdb_table from rdb$database
                 --select upper('RDB$BACKUP_HISTORY') as debug_rdb_table from rdb$database
                 --select upper('SEC$USERS') as debug_rdb_table from rdb$database
                 --select upper('RDB$GENERATORS') as debug_rdb_table from rdb$database
                 --select upper('RDB$INDICES') as debug_rdb_table from rdb$database
                 --select upper('RDB$INDEX_SEGMENTS') as debug_rdb_table from rdb$database
                 --select upper('RDB$TYPES') as debug_rdb_table from rdb$database
                 select null as debug_rdb_table from rdb$database
             )
            ,c0 as(
                select
                    lower(r.rdb$relation_name) rel_name
                    ,rf.rdb$field_position fld_pos
                    ,lower(rf.rdb$field_name) fld_name
                    ,f.rdb$field_type as fld_type
                    ,f.rdb$field_sub_type
                    ,f.rdb$field_precision
                    ,f.rdb$field_scale
                    ,case f.rdb$field_type
                        when 7 then
                            -- smallint; numeric 1.x-4.x ('x' see in f.rdb$field_scale)
                            case coalesce(f.rdb$field_sub_type,0)
                                when 0 then 'smallint'
                                when 1 then 'numeric'
                                else 'unknown'
                            end
                        when 8 then
                            -- integer; numeric 5.x-9.x; decimal 1.x-9.x
                            case coalesce(f.rdb$field_sub_type,0)
                                when 0 then 'integer'
                                when 1 then 'numeric'
                                when 2 then 'decimal'
                                else 'unknown'
                            end
                        when 10 then 'float'
                        when 12 then 'date'
                        when 13 then 'time' -- w/o time zone
                        when 14 then
                            case coalesce(f.rdb$field_sub_type,0)
                                when 1 then 'binary'
                                else 'char' -- NB: field_sub_type can be 3 in rdb$fields.rdb$field_name
                                --when 0 then 'char'
                                --else 'unknown'
                            end
                        when 16 then
                            -- bigint; numeric 10.x-18.x; decimal 10.x-18.x
                            case coalesce(f.rdb$field_sub_type,0)
                                when 0 then 'bigint'
                                when 1 then 'numeric'
                                when 2 then 'decimal'
                                else 'unknown'
                            end
                        when 23 then 'boolean'
                        when 24 then 'decfloat(16)'
                        when 25 then 'decfloat(34)'
                        when 26 then
                            -- int128; numeric 19.x-38.x; decimal 19.x-38.x
                            case coalesce(f.rdb$field_sub_type,0)
                                when 0 then 'int128'
                                when 1 then 'numeric'
                                when 2 then 'decimal'
                                else 'unknown'
                            end
                        when 27 then -- dialect 1 only
                          case f.rdb$field_scale
                            when 0 then 'double precision'
                            else 'numeric' -- (15,' || cast(-f.rdb$field_scale as varchar(6)) || ')'
                          end
                        when 28 then 'time with time zone' -- with time zone
                        when 29 then 'timestamp with time zone' -- with time zone
                        when 35 then iif(m.dia=1, 'date', 'timestamp') -- timestamp w/o timezone
                        when 37 then
                            case coalesce(f.rdb$field_sub_type,0)
                                when 1 then 'varbinary'
                                else 'varchar'
                                --when 0 then 'varchar'
                                --else 'unknown'
                            end
                        when 261 then 'blob' -- sub_type ' || f.rdb$field_sub_type || ' segment size ' || f.rdb$segment_length
                        else 'unknown'
                    end as fld_base_type
                    -- value for 'alter table ... alter column type <T>(<size>) where <T> from { [var]char, numeric, decimal }
                    ,case
                        when f.rdb$field_type in (14, 37) then f.rdb$field_length/iif(ce.rdb$character_set_name=upper('utf8'),4,1) -- char; varchar
                        when f.rdb$field_type in(7, 8, 16, 26) and f.rdb$field_sub_type in (1,2) then f.rdb$field_precision -- numeric; decimal
                        when f.rdb$field_type = 27 and f.rdb$field_scale < 0 then f.rdb$field_precision -- numeric (in dialect-1)
                     end fld_new_size
                     -- if field is NOT null then we must try to insert/update it only with some not-null value:
                    ,iif( coalesce(rf.rdb$null_flag, f.rdb$null_flag, 0) = 0, 1, 0) fld_nullable
                    -- if system table has rdb$system_flag then we have to check ability to change some of its fields
                    -- that relate to custom DB objects (e.g. update rdb$triggers set rdb$trigger_source = NULL where rdb$trigger_name = 'MY_TRIGGER'):
                    ,sum(iif( lower(rf.rdb$field_name) = lower('rdb$system_flag'),1,0))over(partition by r.rdb$relation_name) has_sys_flag
                    ,count(*)over(partition by r.rdb$relation_name) fld_count
                    --,ce.rdb$character_set_name cs_name
                    --,ce.rdb$default_collate_name cs_coll
                    --,co.rdb$collation_name co_name
                    --,co.rdb$base_collation_name co_base
                from (select m.mon$sql_dialect as dia from mon$database m) m
                join rdb$relations r on 1=1
                join dbg d on (r.rdb$relation_name = d.debug_rdb_table or d.debug_rdb_table is null)
                join rdb$relation_fields rf on r.rdb$relation_name = rf.rdb$relation_name
                join rdb$fields f on rf.rdb$field_source = f.rdb$field_name
                left join rdb$collations co on f.rdb$character_set_id=co.rdb$character_set_id and f.rdb$collation_id=co.rdb$collation_id
                left join rdb$character_sets ce on co.rdb$character_set_id=ce.rdb$character_set_id
                where
                    (coalesce(r.rdb$system_flag,0) is distinct from 0 or d.debug_rdb_table is not null) --------------- ### S Y S T E M    t a b l e s  ###
                    -- Include: permanent tables, virtual tables: mon$*; sec$*; rdb$config; rdb$keywords, rdb$time_zones
                    -- Exclude: views, GTTs, external tables
                    and r.rdb$relation_type in (0,3)
                    --and r.rdb$relation_type = 0 -- exclude: views, GTTs, external tables
                    -- and lower(r.rdb$relation_name) not starting with lower('MON$') -- prevent from 'delete from mon$attachments' or 'from mon$statements'
                    and f.rdb$computed_source is null ------------------------------ ### exclude COMPUTED BY fields! ###
                order by rel_name,fld_pos
            )
            --select * from c0

            ,c1 as (
                select
                    rel_name
                    ,has_sys_flag
                    ,fld_pos
                    ,fld_name
                    ,fld_base_type
                    ,fld_new_size
                    ,fld_nullable
                    ,fld_count
                    ,fld_type
                    --,trim(iif(has_sys_flag=1, 'where coalesce(rdb$system_flag,0)=0', '')) as dml_where
                     -- fields like 'rdb$view_source', 'rdb$function_source' etc: we have to check ability
                     -- to write NULL there:
                    ,iif( fld_base_type = 'blob' and trim( upper(fld_name) ) similar to upper( ascii_char(37) || '#_SOURCE') escape '#', 1, 0 ) as has_source_code
                    ,lower(lpad('',8,uuid_to_char(gen_uuid()))) as unq_suff
                    -- these are values which we'll try to write in insert or update statements
                    -- if corresponding fields are NOT null:
                    ,decode( fld_base_type,
                             'smallint', iif(upper(fld_name) = upper('rdb$system_flag'), '0', '32767'),
                             'integer', '2147483647',
                             'bigint', '9223372036854775807',
                             'float', '0e0',
                             'numeric', '0.00',
                             'decimal', '0.00',
                             'double precision', '0e0',
                             'date', 'current_date',
                             'time', 'current_time',
                             'timestamp', 'current_timestamp',
                             'char', '''C''',
                             'varchar', '''V''',
                             -- added 16.10.2025 ---
                             'binary', '''A''',
                             'varbinary', '''B''',
                             'blob', '''test_for_blob''',
                             'boolean', 'true',
                             'decfloat(16)', '9.999999999999999E384',
                             'decfloat(34)', '9.999999999999999999999999999999999E6144',
                             'int128', '170141183460469231731687303715884105727',
                             'time with time zone', '''11:11:11.111 Indian/Cocos''',
                             'timestamp with time zone', '''2018-12-31 12:31:42.543 Pacific/Fiji'''
                             ------------------------
                          ) ins_default
                from c0
            )
            --select * from c1

            ,sttm_wo_cols as (
                -- generate DML/DDL that do not require columns:
                -- SELECT * FROM ... WITH LOCK
                -- DELETE FROM ...
                -- ALTER TABLE
                -- DROP TABLE
                select
                    rel_name
                    ,fld_pos
                    ,fld_name
                    ,fld_base_type
                    ,fld_nullable
                    ,fld_count
                    ,ins_default
                    ,has_sys_flag
                    --,dml_where
                    ,cast( 'select * from ' || trim(rel_name) || ' <DML_WHERE> rows 1 with lock' as varchar(1024) ) as lok_stt
                    ,cast( 'delete from ' || trim(rel_name) || ' <DML_WHERE> rows 1 returning rdb$db_key' as varchar(1024) ) del_stt
                    ,cast( 'alter table '|| trim(rel_name) || ' add rnd_suffix_' || c1.unq_suff || ' char(31)'  as varchar(1024)) add_fld
                     -- added 16.10.2025 ---
                    ,cast( 'alter table '|| trim(rel_name) || ' alter sql security definer' as varchar(1024)) alt_ssd -- 4.x+
                    ,cast( 'alter table '|| trim(rel_name) || ' alter sql security invoker' as varchar(1024)) alt_ssi -- 4.x+
                    ,cast( 'alter table '|| trim(rel_name) || ' enable publication' as varchar(1024)) pub_ena -- 4.x+
                    ,cast( 'alter table '|| trim(rel_name) || ' disable publication' as varchar(1024)) pub_dis -- 4.x+
                    ,cast( 'alter table '|| trim(rel_name) || ' alter '||trim(fld_name)|| ' position 1'  as varchar(1024)) alt_pos
                    ,cast( 'alter table '|| trim(rel_name) || ' alter '||trim(fld_name)|| ' to rnd_suffix_' || c1.unq_suff as varchar(1024)) alt_name
                    ,cast( 'alter table '|| trim(rel_name) || ' alter '||trim(fld_name)|| ' type ' || trim(fld_base_type) || iif(fld_new_size > 0, '(' || fld_new_size || ')', '') as varchar(1024)) alt_type
                    ,cast( 'alter table '|| trim(rel_name) || ' alter '||trim(fld_name)|| ' drop default ' as varchar(1024)) drop_def
                     -----------------------
                    ,cast( 'alter table '|| trim(rel_name) || ' alter '||trim(fld_name)|| ' drop not null'  as varchar(1024)) set_nul
                    ,cast( 'alter table '|| trim(rel_name) || ' add constraint '||left(trim(fld_name)||'_dummy',31) ||' check ('||trim(fld_name) ||' is not distinct from '||trim(fld_name)||')'  as varchar(1024)) set_chk
                    ,cast( 'alter table '|| trim(rel_name) || ' alter '||trim(fld_name)|| ' set default '||ins_default as varchar(1024)) set_def
                    ,cast( 'create descending index rnd_suffix_' || c1.unq_suff || ' on '||trim(rel_name)||'('||trim(fld_name)||')' as varchar(1024)) idx_tab
                    ,cast( 'alter table '|| trim(rel_name) || ' drop '||trim(fld_name) as varchar(1024)) kil_fld
                    ,cast( 'drop table '|| trim(rel_name) as varchar(1024)) kil_tab
                from c1
                where fld_pos = 0 -- take only first column (to be subject for 'alter <t> alter <col>' statements)
            )
            --select * from sttm_wo_cols

            ,c_recur as (
                -- RECURSIVE query to generate DML (INSERT, DELETE) and DDL (ALTER / DROP) statements:
                select
                    rel_name
                    ,fld_pos
                    ,fld_name
                    ,fld_base_type
                    ,fld_nullable
                    ,fld_count
                    ,ins_default
                    ,cast( 'insert into '
                           || trim(rel_name)
                           || '('
                           || trim(fld_name)
                           || iif( c1.fld_pos+1 = c1.fld_count, ')', '')  as varchar(1024)
                         )  as ins_stt
                    ,cast( 'values('
                           || trim(iif(fld_nullable=1, 'null', ins_default))
                           || iif( c1.fld_pos+1 = c1.fld_count, ') returning rdb$db_key', '') as varchar(1024)
                         ) as ins_val
                    ,iif( c1.fld_pos+1 = c1.fld_count, 1, 0) cmd_end
                from c1
                where fld_pos = 0

                UNION ALL

                -- RECURSIVE PART:
                select
                    c1.rel_name
                    ,c1.fld_pos
                    ,c1.fld_name
                    ,c1.fld_base_type
                    ,c1.fld_nullable
                    ,c1.fld_count
                    ,c1.ins_default
                    ,trim(c_recur.ins_stt) || ', ' || trim(c1.fld_name) || iif( c1.fld_pos+1 = c1.fld_count, ')', '')
                    ,trim(c_recur.ins_val) || ', ' 
                      || trim(iif(c1.fld_nullable=1, 'null', c1.ins_default)) 
                      || iif( c1.fld_pos+1 = c1.fld_count, ') returning rdb$db_key', '')
                    ,iif( c1.fld_pos+1 = c1.fld_count, 1, 0) cmd_end
                from c_recur join c1 on c_recur.rel_name = c1.rel_name and c1.fld_pos = c_recur.fld_pos + 1
            )
            --
            ,sttm_ins as (
                select * from c_recur
                where cmd_end = 1
            )
            --select * from sttm_ins


            ,dml_vuln as (
                select
                    i.rel_name
                   ,n.op
                   ,iif(w.has_sys_flag = 1 and n.op in ('DML_LOK', 'DML_DEL'), 'where coalesce(rdb$system_flag,0)=' || m.k, '') dml_where
                   ,decode(
                        n.op
                        ,'DML_INS' ,i.ins_stt || ' ' ||i.ins_val
                        ,'DML_LOK' ,replace(w.lok_stt, '<DML_WHERE>', iif(w.has_sys_flag=1, 'where coalesce(rdb$system_flag,0)=' || m.k, ''))
                        ,'DML_DEL' ,replace(w.del_stt, '<DML_WHERE>', iif(w.has_sys_flag=1, 'where coalesce(rdb$system_flag,0)=' || m.k, ''))
                        ,'ALT_ADC' ,w.add_fld
                        -- added 16.10.2025 ---
                        ,'ALT_POS' ,w.alt_pos  -- alter column <name> position <new_pos>
                        ,'ALT_NAM' ,w.alt_name -- alter column <old_name> to <new_name>
                        ,'ALT_TYP', w.alt_type -- alter column <name> type <new_type>
                        ,'KIL_DEF', w.drop_def -- alter column <name> drop default
                        -----------------------
                        ,'SET_NUL' ,w.set_nul
                        ,'ADD_CTR' ,w.set_chk
                        ,'ADD_DEF' ,w.set_def
                        --,'X' ,w.idx_tab
                        ,'SQL_DEF' ,w.alt_ssd
                        ,'SQL_INV' ,w.alt_ssi
                        ,'PUB_ENA' ,w.pub_ena
                        ,'PUB_DIS' ,w.pub_dis
                        ,'KIL_FLD' ,w.kil_fld
                        ,'KIL_TAB' ,w.kil_tab
                    ) vulnerable_expr
                   ,n.ord_pos
                   ,n.ret_dbkey
                from sttm_ins i
                join sttm_wo_cols w on i.rel_name = w.rel_name
                cross join(
                    select           'DML_INS' as op, 10 as ord_pos, 1 as ret_dbkey from rdb$database -- DML, insert (with specifying all columns)
                    union all select 'DML_LOK',       15,            0 from rdb$database -- DML, select WITH LOCK (no column names required)
                    union all select 'DML_DEL',       20,            1 from rdb$database -- DML, delete some record (no column names required)
                    union all select 'ALT_ADC',       30,            0 from rdb$database -- DDL, alter TABLE add column
                    -- added 16.10.2025 ---
                    union all select 'ALT_POS',       40,            0 from rdb$database -- DDL, alter column: change position
                    union all select 'ALT_NAM',       41,            0 from rdb$database -- DDL, alter column: change its name
                    union all select 'ALT_TYP',       42,            0 from rdb$database -- DDL, alter column: change its type
                    union all select 'KIL_DEF',       43,            0 from rdb$database -- DDL, alter column: drop default
                    -----------------------
                    union all select 'SET_NUL',       50,            0 from rdb$database -- DDL, alter column drop not-null flag (set column nullable)
                    union all select 'ADD_CTR',       51,            0 from rdb$database -- DDL, alter column add new constraint on it
                    union all select 'ADD_DEF',       52,            0 from rdb$database -- DDL, alter column set DEFAULT value
                    -- ### commented 11-04-2018: no need anymore because now one may to create index on system tables:
                    -- ### union all select 'X',       53,            0 from rdb$database -- DDL, CREATE INDEX

                    -- added 16.10.2025 ---
                    union all
                    select op, ord_pos, ret_dbkey
                    from (
                                  select 'SQL_DEF' as op, 60 as ord_pos, 0 as ret_dbkey from rdb$database -- DDL, alter TABLE: set sql security definer
                        union all select 'SQL_INV',       61,            0 from rdb$database -- DDL, alter TABLE: set sql security invoker
                        union all select 'PUB_ENA',       62,            0 from rdb$database -- DDL, alter TABLE: enable publication
                        union all select 'PUB_DIS',       63,            0 from rdb$database -- DDL, alter TABLE: disable publication
                    )
                    where rdb$get_context('SYSTEM','ENGINE_VERSION') not starting with '3.0'
                    ---------------------
                    union all select 'KIL_FLD',       80,            0 from rdb$database -- DDL, alter TABLE: drop column
                    union all select 'KIL_TAB',       90,            0 from rdb$database -- DDL, DROP RDB$-table
                ) n
                join (select 0 k from rdb$database union all select 1 from rdb$database) m
                     on m.k=0 or n.op in ('DML_LOK', 'DML_DEL') and w.has_sys_flag=1 and m.k=1

                order by i.rel_name, n.ord_pos
            )
            --select * from dml_vuln

            ,dml_upd as (
                -- query to generate UPDATE statements:
                select
                     t.rel_name
                    ,t.has_sys_flag
                    ,t.fld_pos
                    ,t.fld_name
                    ,t.fld_base_type
                    ,t.fld_nullable
                    ,t.fld_count
                    ,t.ins_default
                    ,t.has_source_code
                    ,t.dml_where
                    -- NB: for BLR-containing blob columns we have to use
                    -- content of THIS field as new value for update rather than
                    -- just '' or 'test_blob' string.
                    -- Example: update rdb$indices set rdb$condition_blr = '' where ...
                    -- Raises: filter not found to convert type 1 to type 2 / gdscodes = (335544454,)
                    -- But: update rdb$indices set rdb$condition_blr = rdb$condition_blr where ...
                    -- raises expected:
                    -- UPDATE operation is not allowed for system table ... / GDSCODE: 335545030
                    ,cast(
                              'update ' || trim(rel_name)
                           || ' set ' || trim(fld_name) || ' = '
                           || trim(iif(t.i=0, iif(t.fld_base_type = 'blob', trim(fld_name),ins_default), 'null')) || ' '
                           || dml_where
                           || ' rows 1'
                           || ' returning rdb$db_key'
                         as varchar(1024)
                     ) upd_stt
                    ,30 as ord_pos
                    ,1 as ret_dbkey
                from (
                    select
                         c1.rel_name
                        ,c1.has_sys_flag
                        ,c1.fld_pos
                        ,c1.fld_name
                        ,c1.fld_base_type
                        ,c1.fld_nullable
                        ,c1.fld_count
                        ,c1.ins_default
                        ,c1.has_source_code
                        ,trim(iif(has_sys_flag=1, 'where coalesce(rdb$system_flag,0)='||m.k || iif( n.i = 1 and c1.has_source_code = 1, ' and ' || trim(fld_name) || ' is not null', '' ), '')) dml_where
                        ,30 as ord_pos
                        ,1 as ret_dbkey
                        ,n.i
                    from c1
                    join (select 0 i from rdb$database union all select 1 from rdb$database) n
                         on n.i=0 or c1.fld_nullable=1 and n.i=1
                    join (select 0 k from rdb$database union all select 1 from rdb$database) m
                         on m.k=0 or c1.has_sys_flag=1 and m.k=1
                ) t
                -- core-4772: fields like rdb$procedure_source have to be updated AFTER all previous (except RDB$SYSTEM_FLAG - this is the LATEST):
                -- select 1 from rdb$database where 'RDB$TRIGGER_SOURCE' similar to upper( ascii_char(37) || '#_SOURCE' ) escape '#'; ==> must return 1
                order by
                     rel_name
                    ,iif(  trim( upper(fld_name) ) = upper('RDB$SYSTEM_FLAG'), 3
                          ,iif( trim( upper(fld_name) ) similar to upper( ascii_char(37) || '#_SOURCE') escape '#', 2 ,1 )
                     )

            )
            --select * from dml_upd -- where has_source_code = 1

            ,ddl_alt as (
                -- query to generate ALTER INDEX INACTIVE / DROP INDEX / ALTER TABLE DROP CONSTRAINT
                select
                     lower(ri.rdb$relation_name) as rel_name
                    ,95 as ord_pos
                    ,0  as ret_dbkey
                    ,'ADROPIC' as op
                    ,'' as dml_where
                     --, ri.rdb$index_name,  rc.rdb$constraint_name, rc.rdb$constraint_type
                    ,iif(  rc.rdb$constraint_name is null -- ==> we can try both: make index inactive and drop it
                          ,iif( i.i = 1
                               ,'alter index ' || lower(trim(ri.rdb$index_name)) || ' inactive'
                               ,'drop index ' || lower(trim(ri.rdb$index_name))
                              )
                           -- not null ==> we can ONLY try to drop constraint
                           , 'alter table ' || lower(trim(ri.rdb$relation_name)) || ' drop constraint ' || lower(trim(rc.rdb$constraint_name))
                        ) as ddl_stt
                from rdb$indices ri
                join dbg d on (ri.rdb$relation_name = d.debug_rdb_table or d.debug_rdb_table is null)
                left join rdb$relation_constraints rc on ri.rdb$relation_name = rc.rdb$relation_name and ri.rdb$index_name = rc.rdb$index_name
                join (select 1 i from rdb$database union all select 2 from rdb$database) i on i.i = 1 or i.i = 2 and rc.rdb$constraint_name is null
                where
                    coalesce(ri.rdb$system_flag,1) = 1 and coalesce(ri.rdb$index_inactive,0) = 0
                order by decode( rc.rdb$constraint_type, 'CHECK', 0, 'FOREIGN KEY', 1, 'UNIQUE', 2, 'PRIMARY KEY', 3), ri.rdb$relation_name, i.i
                nulls first
            )
            ,ddl_gen as (
                -- query to generate statements to change/drop SEQUENCES
                select
                    rel_name,
                    ord_pos,
                    ret_dbkey,
                    op,
                    dml_where,
                    alt_stt
                from (
                    select
                        'rdb$generators'  as rel_name
                        ,90 + n.i as ord_pos
                        ,0 as ret_dbkey
                        ,decode(n.i, 0, 'SET_GEN', 1, 'ALT_GNR', 2, 'ALT_GNI', 3, 'RECR_GN', 4, 'KIL_GEN', 5, 'KIL_GID') as op
                        ,'' as dml_where
                        ,decode(n.i
                                , 0, 'set generator ' || lower(trim(g.rdb$generator_name)) || ' to -9223372036854775808'
                                , 1, 'alter sequence ' || lower(trim(g.rdb$generator_name)) || ' restart with -9223372036854775808'
                                , 2, 'alter sequence ' || lower(trim(g.rdb$generator_name)) || ' increment by 32767'
                                , 3, 'recreate sequence ' || lower(trim(g.rdb$generator_name))
                                , 4, 'drop /* system */ sequence ' || lower(trim(g.rdb$generator_name))
                                , 5, 'drop /* identity */ sequence ' || lower(trim(g.rdb$generator_name))
                               )
                         as alt_stt
                        ,g.rdb$system_flag as sys_flag
                    from rdb$generators g
                    join dbg d on ('RDB$GENERATORS' = d.debug_rdb_table or d.debug_rdb_table is null)
                    cross join (select row_number()over()-1 i from rdb$types rows 6) n
                    -- 1 =  system
                    -- 6 = inner sequence for identity columns
                    where
                    g.rdb$system_flag in (1,6)
                )
                -- ::: NB :::
                -- 1. We must NOT check 'recreate sequence' for system generators on 6.x+.
                --    This statement will create sequence in PUBLIC schema (no errors will raise).
                --    Check for 'recreate sequence' must be done for 3.x ... 5.x which have no schemas.
                -- 2. We must NOT check 'set generator' or 'alter sequence' for sequences created
                --    by engine for IDENTITY columns becase these statements *allowed*.
                --    Such generators must be checked only by 'recreate' and 'drop' statements
                --    with raising -DROP SEQUENCE "PUBLIC"."RDB$1" failed / -Cannot delete system generator "PUBLIC"."RDB$1"
                -- Letter from dimitr, 29.10.2025 11:29
                where
                    ( sys_flag = 1
                      and op <> 'KIL_GID'
                      and (left(rdb$get_context('SYSTEM','ENGINE_VERSION'),2) in ('3.', '4.', '5.') or op <> 'RECR_GN')
                      or
                      sys_flag = 6
                      and op in ('RECR_GN', 'KIL_GID')
                    )
            )
            -- select * from ddl_gen

            ,fin as (
                --        1        2         3                  4               5          6
                select rel_name, ord_pos, ret_dbkey, op as vulnerable_type, dml_where, vulnerable_expr
                from dml_vuln
                
                UNION ALL
                
                select rel_name, ord_pos, ret_dbkey, 'DML_UPD' as vulnerable_type, dml_where, upd_stt as vulnerable_expr
                from dml_upd
                
                UNION ALL
                
                select rel_name, ord_pos, ret_dbkey, op as vulnerable_type, dml_where, ddl_stt as vulnerable_expr
                from ddl_alt
                
                UNION ALL
                
                select rel_name, ord_pos, ret_dbkey, op as vulnerable_type, dml_where, alt_stt as vulnerable_expr
                from ddl_gen
            )
            select * from fin
            ----------------
 		    as cursor c
        do
            insert into vulnerable_on_sys_tables(  sys_table,   ord_pos,   ret_dbkey,   vulnerable_type,   vulnerable_expr,   dml_where)
										  values( c.rel_name, c.ord_pos, c.ret_dbkey, c.vulnerable_type, c.vulnerable_expr, c.dml_where);
	end
	^ -- sp_gen_expr_for_direct_change 

	-- 6.x:
	execute block as
	    declare v_sys_schema varchar(63) character set utf8;
    begin
        if ( rdb$get_context('SYSTEM','ENGINE_VERSION') similar to '([6-9]|[[:DIGIT:]]{2,}).' || ascii_char(37) ) then
             --if ( left(rdb$get_context('SYSTEM','ENGINE_VERSION'),2) NOT in ('3.', '4.', '5.') ) -- 6.x+
        begin
            for
                execute statement 'select lower(trim(rdb$schema_name)) from rdb$schemas where coalesce(rdb$system_flag,0) = 1'
                into v_sys_schema
            do begin
                insert into vulnerable_on_sys_tables(  sys_table,   ord_pos,   ret_dbkey,   vulnerable_type,   vulnerable_expr,   dml_where)
                values('rdb$schemas', 1, 0, 'ALT_SH1', 'alter schema ' || :v_sys_schema || ' set default character set utf8', '');

                insert into vulnerable_on_sys_tables(  sys_table,   ord_pos,   ret_dbkey,   vulnerable_type,   vulnerable_expr,   dml_where)
                values('rdb$schemas', 2, 0, 'ALT_SH2', 'alter schema ' || :v_sys_schema || ' set default sql security invoker', '');

                insert into vulnerable_on_sys_tables(  sys_table,   ord_pos,   ret_dbkey,   vulnerable_type,   vulnerable_expr,   dml_where)
                values('rdb$schemas', 3, 0, 'ALT_SH3', 'alter schema ' || :v_sys_schema || ' drop default character set', '');

                insert into vulnerable_on_sys_tables(  sys_table,   ord_pos,   ret_dbkey,   vulnerable_type,   vulnerable_expr,   dml_where)
                values('rdb$schemas', 4, 0, 'ALT_SH4', 'alter schema ' || :v_sys_schema || ' drop default sql security', '');

                insert into vulnerable_on_sys_tables(  sys_table,   ord_pos,   ret_dbkey,   vulnerable_type,   vulnerable_expr,   dml_where)
                values('rdb$schemas', 5, 0, 'DROP_SH', 'drop schema ' || :v_sys_schema, '');

                insert into vulnerable_on_sys_tables(  sys_table,   ord_pos,   ret_dbkey,   vulnerable_type,   vulnerable_expr,   dml_where)
                values('rdb$schemas', 6, 0, 'RECR_SH', 'recreate schema ' || :v_sys_schema, '');
            end
        end
    end
    ^
    set term ;^
    commit;

    execute procedure sp_gen_expr_for_creating_fkeys;
    execute procedure sp_gen_expr_for_direct_change;
    -- result: table 'vulnerable_on_sys_tables' ready to be used as source for vulnerable statements
    commit;
