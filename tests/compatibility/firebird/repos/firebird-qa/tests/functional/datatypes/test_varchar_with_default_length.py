#coding:utf-8

"""
ID:          n/a
ISSUE:       https://github.com/FirebirdSQL/firebird/commit/d32276b82b694dcc84a7103fa91fd57afbe88372
TITLE:       Check ability to specify 'varchar' data type WITHOUT length and create DB objects with referencing to such types.
DESCRIPTION:
NOTES:
    [18.02.2026] pzotov
    Test checks only basic functionality: ability to compile domains, tables (persistent, GTT and LTT), views, PSQL-units which has 'varchar' w/o length in DDL.
    For tables we check ability to insert data of appropriate length (see setting 'VC_DEFAULT_LEN').
    For persistent tables we check ability to insert data using views.
    More checks will be added later.

    Checked on 6.0.0.1456-789d467.
"""

import pytest
from firebird.qa import *

db = db_factory()

VC_DEFAULT_LEN = 255

test_script = f"""
    set bail on;
    set list on;
    set autoterm on;
    -- NB! ALL of 'varchar', 'char vaying' and 'character varying' must be supported.
    create domain dm_01 varchar;
    create domain dm_02 char varying;
    create domain dm_03 character varying;
    create domain dm_04 varchar character set win1250;
    create domain dm_05 varchar character set utf8;

    create table test_direct_decl_01(s varchar);
    create table test_direct_decl_02(s char varying);
    create table test_direct_decl_03(s character varying);
    create table test_direct_decl_04(s varchar character set win1250);
    create table test_direct_decl_05(s varchar character set utf8);

    create table test_via_domains_01(s dm_01);
    create table test_via_domains_02(s dm_02);
    create table test_via_domains_03(s dm_03);
    create table test_via_domains_04(s dm_04);
    create table test_via_domains_05(s dm_05);


    create global temporary table gtt_direct_decl_01(s varchar);
    create global temporary table gtt_direct_decl_02(s char varying);
    create global temporary table gtt_direct_decl_03(s character varying);
    create global temporary table gtt_direct_decl_04(s varchar character set win1250);
    create global temporary table gtt_direct_decl_05(s varchar character set utf8);

    create global temporary table gtt_via_domains_01(s dm_01);
    create global temporary table gtt_via_domains_02(s dm_02);
    create global temporary table gtt_via_domains_03(s dm_03);
    create global temporary table gtt_via_domains_04(s dm_04);
    create global temporary table gtt_via_domains_05(s dm_05);


    create local temporary table ltt_direct_decl_01(s varchar);
    create local temporary table ltt_direct_decl_02(s char varying);
    create local temporary table ltt_direct_decl_03(s character varying);
    create local temporary table ltt_direct_decl_04(s varchar character set win1250);
    create local temporary table ltt_direct_decl_05(s varchar character set utf8);

    create local temporary table ltt_via_domains_01(s dm_01);
    create local temporary table ltt_via_domains_02(s dm_02);
    create local temporary table ltt_via_domains_03(s dm_03);
    create local temporary table ltt_via_domains_04(s dm_04);
    create local temporary table ltt_via_domains_05(s dm_05);


    create view v_test_direct_decl_01 as select * from test_direct_decl_01;
    create view v_test_direct_decl_02 as select * from test_direct_decl_02;
    create view v_test_direct_decl_03 as select * from test_direct_decl_03;
    create view v_test_direct_decl_04 as select * from test_direct_decl_04;
    create view v_test_direct_decl_05 as select * from test_direct_decl_05;

    create view v_test_via_domains_01 as select * from test_via_domains_01;
    create view v_test_via_domains_02 as select * from test_via_domains_02;
    create view v_test_via_domains_03 as select * from test_via_domains_03;
    create view v_test_via_domains_04 as select * from test_via_domains_04;
    create view v_test_via_domains_05 as select * from test_via_domains_05;

    create procedure sp_test (
        a_01 varchar
       ,a_02 char varying
       ,a_03 character varying
       ,a_04 varchar character set win1250
       ,a_05 varchar character set utf8
       ,d_01 dm_01
       ,d_02 dm_02
       ,d_03 dm_03
       ,d_04 dm_04
       ,d_05 dm_05
       ,t_01 type of dm_01
       ,t_02 type of dm_02
       ,t_03 type of dm_03
       ,t_04 type of dm_04
       ,t_05 type of dm_05
       ,f_01 type of column test_direct_decl_01.s
       ,f_02 type of column test_direct_decl_02.s
       ,f_03 type of column test_direct_decl_03.s
       ,f_04 type of column test_direct_decl_04.s
       ,f_05 type of column test_direct_decl_05.s
       ,g_01 type of column test_via_domains_01.s
       ,g_02 type of column test_via_domains_02.s
       ,g_03 type of column test_via_domains_03.s
       ,g_04 type of column test_via_domains_04.s
       ,g_05 type of column test_via_domains_05.s
    ) returns (
        o_01 varchar
       ,o_02 char varying
       ,o_03 character varying
       ,o_04 varchar character set win1250
       ,o_05 varchar character set utf8
       ,u_01 dm_01
       ,u_02 dm_02
       ,u_03 dm_03
       ,u_04 dm_04
       ,u_05 dm_05
       ,w_01 type of dm_01
       ,w_02 type of dm_02
       ,w_03 type of dm_03
       ,w_04 type of dm_04
       ,w_05 type of dm_05
       ,x_01 type of column test_direct_decl_01.s
       ,x_02 type of column test_direct_decl_02.s
       ,x_03 type of column test_direct_decl_03.s
       ,x_04 type of column test_direct_decl_04.s
       ,x_05 type of column test_direct_decl_05.s
       ,y_01 type of column test_via_domains_01.s
       ,y_02 type of column test_via_domains_02.s
       ,y_03 type of column test_via_domains_03.s
       ,y_04 type of column test_via_domains_04.s
       ,y_05 type of column test_via_domains_05.s
    ) as
        declare v_01 varchar;
        declare v_02 char varying;
        declare v_03 character varying;
        declare v_04 varchar character set win1250;
        declare v_05 varchar character set utf8;
        declare v_d_01 dm_01;
        declare v_d_02 dm_02;
        declare v_d_03 dm_03;
        declare v_d_04 dm_04;
        declare v_d_05 dm_05;
        declare v_t_01 type of dm_01;
        declare v_t_02 type of dm_02;
        declare v_t_03 type of dm_03;
        declare v_t_04 type of dm_04;
        declare v_t_05 type of dm_05;

        declare v_f_01 type of column test_direct_decl_01.s;
        declare v_f_02 type of column test_direct_decl_02.s;
        declare v_f_03 type of column test_direct_decl_03.s;
        declare v_f_04 type of column test_direct_decl_04.s;
        declare v_f_05 type of column test_direct_decl_05.s;
        declare v_g_01 type of column test_via_domains_01.s;
        declare v_g_02 type of column test_via_domains_02.s;
        declare v_g_03 type of column test_via_domains_03.s;
        declare v_g_04 type of column test_via_domains_04.s;
        declare v_g_05 type of column test_via_domains_05.s;
    begin
        o_01 = lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid()));
        o_02 = lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid()));
        o_03 = lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid()));
        o_04 = lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid()));
        o_05 = lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid()));
        suspend;
    end
    ;

    create function fn_test (
        a_01 varchar
       ,a_02 char varying
       ,a_03 character varying
       ,a_04 varchar character set win1250
       ,a_05 varchar character set utf8
       ,d_01 dm_01
       ,d_02 dm_02
       ,d_03 dm_03
       ,d_04 dm_04
       ,d_05 dm_05
       ,t_01 type of dm_01
       ,t_02 type of dm_02
       ,t_03 type of dm_03
       ,t_04 type of dm_04
       ,t_05 type of dm_05
       ,f_01 type of column test_direct_decl_01.s
       ,f_02 type of column test_direct_decl_02.s
       ,f_03 type of column test_direct_decl_03.s
       ,f_04 type of column test_direct_decl_04.s
       ,f_05 type of column test_direct_decl_05.s
       ,g_01 type of column test_via_domains_01.s
       ,g_02 type of column test_via_domains_02.s
       ,g_03 type of column test_via_domains_03.s
       ,g_04 type of column test_via_domains_04.s
       ,g_05 type of column test_via_domains_05.s
    ) returns varchar as
        declare v_01 varchar;
        declare v_02 char varying;
        declare v_03 character varying;
        declare v_04 varchar character set win1250;
        declare v_05 varchar character set utf8;
        declare v_d_01 dm_01;
        declare v_d_02 dm_02;
        declare v_d_03 dm_03;
        declare v_d_04 dm_04;
        declare v_d_05 dm_05;
        declare v_t_01 type of dm_01;
        declare v_t_02 type of dm_02;
        declare v_t_03 type of dm_03;
        declare v_t_04 type of dm_04;
        declare v_t_05 type of dm_05;
        declare v_f_01 type of column test_direct_decl_01.s;
        declare v_f_02 type of column test_direct_decl_02.s;
        declare v_f_03 type of column test_direct_decl_03.s;
        declare v_f_04 type of column test_direct_decl_04.s;
        declare v_f_05 type of column test_direct_decl_05.s;
        declare v_g_01 type of column test_via_domains_01.s;
        declare v_g_02 type of column test_via_domains_02.s;
        declare v_g_03 type of column test_via_domains_03.s;
        declare v_g_04 type of column test_via_domains_04.s;
        declare v_g_05 type of column test_via_domains_05.s;
    begin
        return lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid()));
    end
    ;

    create package pg_test as
    begin
        procedure pg_sp_test (
            a_01 varchar
           ,a_02 char varying
           ,a_03 character varying
           ,a_04 varchar character set win1250
           ,a_05 varchar character set utf8
           ,d_01 dm_01
           ,d_02 dm_02
           ,d_03 dm_03
           ,d_04 dm_04
           ,d_05 dm_05
           ,t_01 type of dm_01
           ,t_02 type of dm_02
           ,t_03 type of dm_03
           ,t_04 type of dm_04
           ,t_05 type of dm_05
           ,f_01 type of column test_direct_decl_01.s
           ,f_02 type of column test_direct_decl_02.s
           ,f_03 type of column test_direct_decl_03.s
           ,f_04 type of column test_direct_decl_04.s
           ,f_05 type of column test_direct_decl_05.s
           ,g_01 type of column test_via_domains_01.s
           ,g_02 type of column test_via_domains_02.s
           ,g_03 type of column test_via_domains_03.s
           ,g_04 type of column test_via_domains_04.s
           ,g_05 type of column test_via_domains_05.s
        ) returns (
            o_01 varchar
           ,o_02 char varying
           ,o_03 character varying
           ,o_04 varchar character set win1250
           ,o_05 varchar character set utf8
           ,x_01 type of column test_direct_decl_01.s
           ,x_02 type of column test_direct_decl_02.s
           ,x_03 type of column test_direct_decl_03.s
           ,x_04 type of column test_direct_decl_04.s
           ,x_05 type of column test_direct_decl_05.s
           ,y_01 type of column test_via_domains_01.s
           ,y_02 type of column test_via_domains_02.s
           ,y_03 type of column test_via_domains_03.s
           ,y_04 type of column test_via_domains_04.s
           ,y_05 type of column test_via_domains_05.s
        );

        function pg_fn_test (
            a_01 varchar
           ,a_02 char varying
           ,a_03 character varying
           ,a_04 varchar character set win1250
           ,a_05 varchar character set utf8
           ,d_01 dm_01
           ,d_02 dm_02
           ,d_03 dm_03
           ,d_04 dm_04
           ,d_05 dm_05
           ,t_01 type of dm_01
           ,t_02 type of dm_02
           ,t_03 type of dm_03
           ,t_04 type of dm_04
           ,t_05 type of dm_05
           ,f_01 type of column test_direct_decl_01.s
           ,f_02 type of column test_direct_decl_02.s
           ,f_03 type of column test_direct_decl_03.s
           ,f_04 type of column test_direct_decl_04.s
           ,f_05 type of column test_direct_decl_05.s
           ,g_01 type of column test_via_domains_01.s
           ,g_02 type of column test_via_domains_02.s
           ,g_03 type of column test_via_domains_03.s
           ,g_04 type of column test_via_domains_04.s
           ,g_05 type of column test_via_domains_05.s
        ) returns varchar
        ;
    end
    ;

    create package body pg_test as
    begin
        procedure pg_sp_test (
            a_01 varchar
           ,a_02 char varying
           ,a_03 character varying
           ,a_04 varchar character set win1250
           ,a_05 varchar character set utf8
           ,d_01 dm_01
           ,d_02 dm_02
           ,d_03 dm_03
           ,d_04 dm_04
           ,d_05 dm_05
           ,t_01 type of dm_01
           ,t_02 type of dm_02
           ,t_03 type of dm_03
           ,t_04 type of dm_04
           ,t_05 type of dm_05
           ,f_01 type of column test_direct_decl_01.s
           ,f_02 type of column test_direct_decl_02.s
           ,f_03 type of column test_direct_decl_03.s
           ,f_04 type of column test_direct_decl_04.s
           ,f_05 type of column test_direct_decl_05.s
           ,g_01 type of column test_via_domains_01.s
           ,g_02 type of column test_via_domains_02.s
           ,g_03 type of column test_via_domains_03.s
           ,g_04 type of column test_via_domains_04.s
           ,g_05 type of column test_via_domains_05.s
        ) returns (
            o_01 varchar
           ,o_02 char varying
           ,o_03 character varying
           ,o_04 varchar character set win1250
           ,o_05 varchar character set utf8
           ,x_01 type of column test_direct_decl_01.s
           ,x_02 type of column test_direct_decl_02.s
           ,x_03 type of column test_direct_decl_03.s
           ,x_04 type of column test_direct_decl_04.s
           ,x_05 type of column test_direct_decl_05.s
           ,y_01 type of column test_via_domains_01.s
           ,y_02 type of column test_via_domains_02.s
           ,y_03 type of column test_via_domains_03.s
           ,y_04 type of column test_via_domains_04.s
           ,y_05 type of column test_via_domains_05.s
        ) as
            declare v_01 varchar;
            declare v_02 char varying;
            declare v_03 character varying;
            declare v_04 varchar character set win1250;
            declare v_05 varchar character set utf8;
            declare v_d_01 dm_01;
            declare v_d_02 dm_02;
            declare v_d_03 dm_03;
            declare v_d_04 dm_04;
            declare v_d_05 dm_05;
            declare v_t_01 type of dm_01;
            declare v_t_02 type of dm_02;
            declare v_t_03 type of dm_03;
            declare v_t_04 type of dm_04;
            declare v_t_05 type of dm_05;
            declare v_f_01 type of column test_direct_decl_01.s;
            declare v_f_02 type of column test_direct_decl_02.s;
            declare v_f_03 type of column test_direct_decl_03.s;
            declare v_f_04 type of column test_direct_decl_04.s;
            declare v_f_05 type of column test_direct_decl_05.s;
            declare v_g_01 type of column test_via_domains_01.s;
            declare v_g_02 type of column test_via_domains_02.s;
            declare v_g_03 type of column test_via_domains_03.s;
            declare v_g_04 type of column test_via_domains_04.s;
            declare v_g_05 type of column test_via_domains_05.s;
        begin
            o_01 = lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid()));
            o_02 = lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid()));
            o_03 = lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid()));
            o_04 = lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid()));
            o_05 = lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid()));
            suspend;
        end

        function pg_fn_test (
            a_01 varchar
           ,a_02 char varying
           ,a_03 character varying
           ,a_04 varchar character set win1250
           ,a_05 varchar character set utf8
           ,d_01 dm_01
           ,d_02 dm_02
           ,d_03 dm_03
           ,d_04 dm_04
           ,d_05 dm_05
           ,t_01 type of dm_01
           ,t_02 type of dm_02
           ,t_03 type of dm_03
           ,t_04 type of dm_04
           ,t_05 type of dm_05
           ,f_01 type of column test_direct_decl_01.s
           ,f_02 type of column test_direct_decl_02.s
           ,f_03 type of column test_direct_decl_03.s
           ,f_04 type of column test_direct_decl_04.s
           ,f_05 type of column test_direct_decl_05.s
           ,g_01 type of column test_via_domains_01.s
           ,g_02 type of column test_via_domains_02.s
           ,g_03 type of column test_via_domains_03.s
           ,g_04 type of column test_via_domains_04.s
           ,g_05 type of column test_via_domains_05.s
        ) returns varchar as
            declare v_01 varchar;
            declare v_02 char varying;
            declare v_03 character varying;
            declare v_04 varchar character set win1250;
            declare v_05 varchar character set utf8;
            declare v_d_01 dm_01;
            declare v_d_02 dm_02;
            declare v_d_03 dm_03;
            declare v_d_04 dm_04;
            declare v_d_05 dm_05;
            declare v_t_01 type of dm_01;
            declare v_t_02 type of dm_02;
            declare v_t_03 type of dm_03;
            declare v_t_04 type of dm_04;
            declare v_t_05 type of dm_05;
            declare v_f_01 type of column test_direct_decl_01.s;
            declare v_f_02 type of column test_direct_decl_02.s;
            declare v_f_03 type of column test_direct_decl_03.s;
            declare v_f_04 type of column test_direct_decl_04.s;
            declare v_f_05 type of column test_direct_decl_05.s;
            declare v_g_01 type of column test_via_domains_01.s;
            declare v_g_02 type of column test_via_domains_02.s;
            declare v_g_03 type of column test_via_domains_03.s;
            declare v_g_04 type of column test_via_domains_04.s;
            declare v_g_05 type of column test_via_domains_05.s;
        begin
            return lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid()));
        end

    end
    ;
    commit;

    insert into v_test_direct_decl_01(s) values( lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid())) ) returning octet_length(s) as test_direct_decl_01_octets;
    insert into v_test_direct_decl_02(s) values( lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid())) ) returning octet_length(s) as test_direct_decl_02_octets;
    insert into v_test_direct_decl_03(s) values( lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid())) ) returning octet_length(s) as test_direct_decl_03_octets;
    insert into v_test_direct_decl_04(s) values( lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid())) ) returning octet_length(s) as test_direct_decl_04_octets;
    insert into v_test_direct_decl_05(s) values( lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid())) ) returning octet_length(s) as test_direct_decl_05_octets;

    insert into v_test_via_domains_01(s) values( lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid())) ) returning octet_length(s) as test_via_domains_01_octets;
    insert into v_test_via_domains_02(s) values( lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid())) ) returning octet_length(s) as test_via_domains_02_octets;
    insert into v_test_via_domains_03(s) values( lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid())) ) returning octet_length(s) as test_via_domains_03_octets;
    insert into v_test_via_domains_04(s) values( lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid())) ) returning octet_length(s) as test_via_domains_04_octets;
    insert into v_test_via_domains_05(s) values( lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid())) ) returning octet_length(s) as test_via_domains_05_octets;


    insert into gtt_direct_decl_01(s) values( lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid())) ) returning octet_length(s) as gtt_direct_decl_01_octets;
    insert into gtt_direct_decl_02(s) values( lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid())) ) returning octet_length(s) as gtt_direct_decl_02_octets;
    insert into gtt_direct_decl_03(s) values( lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid())) ) returning octet_length(s) as gtt_direct_decl_03_octets;
    insert into gtt_direct_decl_04(s) values( lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid())) ) returning octet_length(s) as gtt_direct_decl_04_octets;
    insert into gtt_direct_decl_05(s) values( lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid())) ) returning octet_length(s) as gtt_direct_decl_05_octets;

    insert into gtt_via_domains_01(s) values( lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid())) ) returning octet_length(s) as gtt_via_domains_01_octets;
    insert into gtt_via_domains_02(s) values( lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid())) ) returning octet_length(s) as gtt_via_domains_02_octets;
    insert into gtt_via_domains_03(s) values( lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid())) ) returning octet_length(s) as gtt_via_domains_03_octets;
    insert into gtt_via_domains_04(s) values( lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid())) ) returning octet_length(s) as gtt_via_domains_04_octets;
    insert into gtt_via_domains_05(s) values( lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid())) ) returning octet_length(s) as gtt_via_domains_05_octets;


    insert into ltt_direct_decl_01(s) values( lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid())) ) returning octet_length(s) as ltt_direct_decl_01_octets;
    insert into ltt_direct_decl_02(s) values( lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid())) ) returning octet_length(s) as ltt_direct_decl_02_octets;
    insert into ltt_direct_decl_03(s) values( lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid())) ) returning octet_length(s) as ltt_direct_decl_03_octets;
    insert into ltt_direct_decl_04(s) values( lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid())) ) returning octet_length(s) as ltt_direct_decl_04_octets;
    insert into ltt_direct_decl_05(s) values( lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid())) ) returning octet_length(s) as ltt_direct_decl_05_octets;

    insert into ltt_via_domains_01(s) values( lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid())) ) returning octet_length(s) as ltt_via_domains_01_octets;
    insert into ltt_via_domains_02(s) values( lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid())) ) returning octet_length(s) as ltt_via_domains_02_octets;
    insert into ltt_via_domains_03(s) values( lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid())) ) returning octet_length(s) as ltt_via_domains_03_octets;
    insert into ltt_via_domains_04(s) values( lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid())) ) returning octet_length(s) as ltt_via_domains_04_octets;
    insert into ltt_via_domains_05(s) values( lpad('', {VC_DEFAULT_LEN}, uuid_to_char(gen_uuid())) ) returning octet_length(s) as ltt_via_domains_05_octets;

"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = f"""
    TEST_DIRECT_DECL_01_OCTETS      {VC_DEFAULT_LEN}
    TEST_DIRECT_DECL_02_OCTETS      {VC_DEFAULT_LEN}
    TEST_DIRECT_DECL_03_OCTETS      {VC_DEFAULT_LEN}
    TEST_DIRECT_DECL_04_OCTETS      {VC_DEFAULT_LEN}
    TEST_DIRECT_DECL_05_OCTETS      {VC_DEFAULT_LEN}
    TEST_VIA_DOMAINS_01_OCTETS      {VC_DEFAULT_LEN}
    TEST_VIA_DOMAINS_02_OCTETS      {VC_DEFAULT_LEN}
    TEST_VIA_DOMAINS_03_OCTETS      {VC_DEFAULT_LEN}
    TEST_VIA_DOMAINS_04_OCTETS      {VC_DEFAULT_LEN}
    TEST_VIA_DOMAINS_05_OCTETS      {VC_DEFAULT_LEN}
    GTT_DIRECT_DECL_01_OCTETS       {VC_DEFAULT_LEN}
    GTT_DIRECT_DECL_02_OCTETS       {VC_DEFAULT_LEN}
    GTT_DIRECT_DECL_03_OCTETS       {VC_DEFAULT_LEN}
    GTT_DIRECT_DECL_04_OCTETS       {VC_DEFAULT_LEN}
    GTT_DIRECT_DECL_05_OCTETS       {VC_DEFAULT_LEN}
    GTT_VIA_DOMAINS_01_OCTETS       {VC_DEFAULT_LEN}
    GTT_VIA_DOMAINS_02_OCTETS       {VC_DEFAULT_LEN}
    GTT_VIA_DOMAINS_03_OCTETS       {VC_DEFAULT_LEN}
    GTT_VIA_DOMAINS_04_OCTETS       {VC_DEFAULT_LEN}
    GTT_VIA_DOMAINS_05_OCTETS       {VC_DEFAULT_LEN}
    LTT_DIRECT_DECL_01_OCTETS       {VC_DEFAULT_LEN}
    LTT_DIRECT_DECL_02_OCTETS       {VC_DEFAULT_LEN}
    LTT_DIRECT_DECL_03_OCTETS       {VC_DEFAULT_LEN}
    LTT_DIRECT_DECL_04_OCTETS       {VC_DEFAULT_LEN}
    LTT_DIRECT_DECL_05_OCTETS       {VC_DEFAULT_LEN}
    LTT_VIA_DOMAINS_01_OCTETS       {VC_DEFAULT_LEN}
    LTT_VIA_DOMAINS_02_OCTETS       {VC_DEFAULT_LEN}
    LTT_VIA_DOMAINS_03_OCTETS       {VC_DEFAULT_LEN}
    LTT_VIA_DOMAINS_04_OCTETS       {VC_DEFAULT_LEN}
    LTT_VIA_DOMAINS_05_OCTETS       {VC_DEFAULT_LEN}
"""

@pytest.mark.version('>=6.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
