-- Test ID: functional.gtcs.dsql-domain-01
-- Title: GTCS/tests/DSQL_DOMAIN_01. Test the level 0 syntax for SQL create domain defining only the datatype.
-- Description: 
	Original test see in:
        https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/DSQL_DOMAIN_01.script 

    NB: avoid usage of ISQL command 'SHOW DOMAIN' because of unstable output.
    We display info about domains using common VIEW based on RDB$FIELDS table.

    Checked on 4.0.0.1896; 3.0.6.33288; 2.5.9.27149
 
-- Firebird Version: 2.5
-- Platform: All
-- Test Type: ISQL
-- Converted from: dsql-domain-01.fbt

-- === TEST SCRIPT ===
create view v_test as
    select
        ff.rdb$field_name as dm_name
        ,ff.rdb$field_type as dm_type
        ,ff.rdb$field_sub_type as dm_subtype
        ,ff.rdb$field_length as dm_flen
        ,ff.rdb$field_scale as dm_fscale
        ,ff.rdb$field_precision as dm_fprec
        ,ff.rdb$character_set_id as dm_fcset
        ,ff.rdb$collation_id as dm_fcoll
        ,ff.rdb$character_length dm_fchrlen
        ,ff.rdb$null_flag as dm_fnull
        ,ff.rdb$validation_source as dm_fvalid
        ,ff.rdb$default_source as dm_fdefault
    from rdb$fields ff
    where
        ff.rdb$system_flag is distinct from 1
        and ff.rdb$field_name starting with upper( 'dom0' )
    ;
    commit;

    set bail on;
    create domain dom01a_1 as smallint;
    create domain dom01a_2 as numeric(3,1);
    create domain dom01b_1 as integer;
    create domain dom01b_2 as int;
    create domain dom01b_3 as numeric;
    create domain dom01b_4 as numeric(6,2);
    create domain dom01c as date;
    create domain dom01d_1 as char(20);
    create domain dom01d_2 as character(99);
    create domain dom01e_1 as varchar(25);
    create domain dom01e_2 as character varying(100);
    create domain dom01e_3 as char varying(2);
    create domain dom01f_1 as decimal(6,2);
    create domain dom01g_1 as float;
    create domain dom01g_2 as long float;
    create domain dom01g_3 as real;
    create domain dom01h as double precision;
    create domain dom01i_1 as blob;
    create domain dom01i_2 as blob(60,1);
    commit;
    set bail off;

    set list on;
    set count on;
    select * from v_test order by dm_name;
