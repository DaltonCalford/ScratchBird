-- Script for drop all DB objects: triggers, procedures, functions, packages, tables, domains, exceptions, sequences etc.
-- ::: NB ::: This script must be launched with '-nodbtriggers' switch because DB can have DB-level triggers!

set bail on;
set list on;

select mon$database_name as "Start removing objects in: " from mon$database;

-- 1. Removing dependencies for each view, with preserving column names.
-- One need to do that in separate transaction.
-- (otherwise we can get ISC error 336397288. invalid request BLR at offset 2. context not defined (BLR error).)
-- see letter to dimitr, 29.03.2014 22:43
set term ^;
execute block as
  declare stt varchar(8190);
  declare ref_name varchar(31);
  declare tab_name varchar(31);
  declare view_ddl varchar(8190);
  declare c_make_views_dummy cursor for (
      with
      a as(
        select rf.rdb$relation_name view_name, rf.rdb$field_position fld_pos
              ,iif(  trim(rf.rdb$field_name) = upper(trim(rf.rdb$field_name))
                    ,trim(rf.rdb$field_name)
                    ,'"' || trim(rf.rdb$field_name) || '"'
                  ) as fld_name
        from rdb$relation_fields rf
        join rdb$relations rr on rf.rdb$relation_name=rr.rdb$relation_name
        where
            coalesce(rf.rdb$system_flag,0)=0
            and coalesce(rr.rdb$system_flag,0)=0
            -- XXX DO NOT XXX -- and rr.rdb$relation_type=1 -- WRONG!! Views can simetime have rel_type = 0!
            and rr.RDB$VIEW_BLR is not null
      )
      select view_name,
             cast( 'alter view '||trim(view_name)||' as select '
                   ||list( fld_pos||' '|| fld_name )
                   ||' from rdb$database' as varchar(8190)
                 ) view_ddl
      from a
      group by view_name
  );

begin
    open c_make_views_dummy;
    while (1=1) do
    begin
        fetch c_make_views_dummy into tab_name, stt;
        if (row_count = 0) then leave;
        execute statement (:stt);
    end
    close c_make_views_dummy;
    -- result: no more views depending on any other objects
    -- because all of them has form like 'select 1 <field1>, 2 <field2> from rdb$database'
end
^
commit
^

-------------------------------------------------------------------------------

-- 2. Removing all objects from database is they exists:
execute block as
    declare total_objects_removed int;
    declare stt varchar(4096) character set utf8;
    declare def_coll varchar(64) character set utf8;
    declare ref_name varchar(64) character set utf8;
    declare tab_name varchar(64) character set utf8;
    declare usr_name varchar(64) character set utf8;
    declare sec_plugin varchar(64) character set utf8;

    declare c_trig cursor for                            -- TRIGGERS
      (select '"' || trim(rt.rdb$trigger_name) || '"' as rdb$trigger_name
         from rdb$triggers rt
         where coalesce(rt.rdb$system_flag,0)=0
      );

    declare c_view cursor for                            -- VIEWS
      (select '"' || trim(rr.rdb$relation_name) || '"' as rdb$relation_name
         from rdb$relations rr
        where
            -- XXX DO NOT XXX -- rr.rdb$relation_type=1 -- WRONG!! Views can simetime have rel_type = 0!
            rr.RDB$VIEW_BLR is not null
            and coalesce(rr.rdb$system_flag,0)=0
      );
    declare c_func cursor for                            -- FUNCTIONS
      (select '"' || trim(rf.rdb$function_name) || '"' rdb$function_name
         from rdb$functions rf
        where coalesce(rf.rdb$system_flag,0)=0
      );
    declare c_proc cursor for                            -- PROCEDURES
      (select '"' || trim(rp.rdb$procedure_name) || '"' as rdb$procedure_name
         from rdb$procedures rp
         where coalesce(rp.rdb$system_flag,0)=0
      );

    declare c_pkg cursor for                             -- PACKAGES
      (select '"' || trim(rp.rdb$package_name) || '"' as rdb$package_name
         from rdb$packages rp
         where coalesce(rp.rdb$system_flag,0)=0
      );

    declare c_excp cursor for                            -- EXCEPTIONS
      (select '"' || trim(re.rdb$exception_name) || '"' as rdb$exception_name
         from rdb$exceptions re
         where coalesce(re.rdb$system_flag,0)=0
      );

    declare c_fk cursor for                              -- FK CONSTRAINTS
      (select '"' || trim(rc.rdb$constraint_name) || '"' as rdb$constraint_name
             ,rc.rdb$relation_name
         from rdb$relation_constraints rc
        where rc.rdb$constraint_type ='FOREIGN KEY'
      );

    declare c_tabs cursor for                            -- TABLES: permanent, GTT, external
      (
        select '"' || trim(rr.rdb$relation_name) || '"' as rdb$relation_name
          from rdb$relations rr
         where
             -- 0 = permanent usual table
             -- 2 = external table
             -- 4 = GTT session-level
             -- 5 =  GTT transaction-level
             rr.rdb$relation_type in(0,2,4,5)
             and coalesce(rr.rdb$system_flag,0)=0
             -- MANDATORY! Views can simetime have rel_type = 0!
             and rr.RDB$VIEW_BLR is null
      );

    declare c_doms cursor for                            -- DOMAINS
      (select '"' || trim(rf.rdb$field_name) || '"' as rdb$field_name
        from rdb$fields rf
       where coalesce(rf.rdb$system_flag,0)=0
             and rf.rdb$field_name not starting with 'RDB$'
      );

    declare c_coll cursor for                            -- COLLATIONS
      (select '"' || trim(rc.rdb$collation_name) || '"' as rdb$collation_name
         from rdb$collations rc
        where coalesce(rc.rdb$system_flag,0)=0
      );

    -- cursor for reset charset default collation to initial value
    -- which name is always equals to rdb$character_set_name:
    declare c_cset cursor for                           -- CHAR. SETS
      (select
              cs.rdb$character_set_name as cset_name
             ,cs.rdb$default_collate_name as def_coll
         from rdb$character_sets cs
         where
             cs.rdb$character_set_name is distinct from cs.rdb$default_collate_name
      );

    declare c_gens cursor for                            -- SEQUENCES
      (select '"' || trim(rg.rdb$generator_name) || '"' as rdb$generator_name
        from rdb$generators rg
       where coalesce(rg.rdb$system_flag,0)=0
      );
    declare c_role cursor for                            -- ROLES
      (select '"' || trim(rr.rdb$role_name) || '"' as rdb$role_name
        from rdb$roles rr
       where coalesce(rr.rdb$system_flag,0)=0
      );
    declare c_local_mapping cursor for
      (
       select '"' || trim(rm.rdb$map_name) || '"' as rdb$map_name
       from rdb$auth_mapping rm
       where coalesce(rm.rdb$system_flag,0)=0
      );

    declare c_users cursor for
      (
       select '"' || trim(s.sec$user_name) || '"' as sec$user_name, s.sec$plugin
       from sec$users s
       where upper(s.sec$user_name) <> 'SYSDBA'
      );

begin
    total_objects_removed = 0;

    open c_trig; ----------   d r o p    t r i g g e r s  ----------------------
    while (1=1) do
    begin
        fetch c_trig into stt;
        if (row_count = 0) then leave;
        stt = 'drop trigger '||stt;
        execute statement (:stt);
        total_objects_removed = total_objects_removed + 1;
    end
    close c_trig;

    open c_doms; ------   d r o p    d o m a i n    C O N S T R A I N T S  -----
    while (1=1) do
    begin
        fetch c_doms into stt;
        if (row_count = 0) then leave;
        stt = 'alter domain ' || stt || ' drop constraint';
        execute statement (:stt);
        total_objects_removed = total_objects_removed + 1;
    end
    close c_doms;


    open c_func; ----------   m a k e     f u n c t i o n s    e m p t y  -------
    while (1=1) do
    begin
        fetch c_func into stt;
        if (row_count = 0) then leave;
        stt = 'create or alter function '||stt||' returns int as begin return 1; end';
        execute statement (:stt);
        -- total_objects_removed = total_objects_removed + 1;
    end
    close c_func;

    open c_proc; ----------   m a k e     p r o c e d u r e s    e m p t y  -----
    while (1=1) do
    begin
        fetch c_proc into stt;
        if (row_count = 0) then leave;
        stt = 'create or alter procedure '||stt||' as begin end';
        execute statement (:stt);
        -- total_objects_removed = total_objects_removed + 1;
    end
    close c_proc;

    open c_pkg; ---------------------  d r o p   p a c k a g e   b o d i e s ----
    while (1=1) do
    begin
        fetch c_pkg into stt;
        if (row_count = 0) then leave;
        stt = 'drop package body '||stt;
        execute statement (:stt);
        total_objects_removed = total_objects_removed + 1;
    end
    close c_pkg;


    open c_view; ---------------------  d r o p   v i e w s  ---------------------
    while (1=1) do
    begin
        fetch c_view into stt;
        if (row_count = 0) then leave;
        stt = 'drop view '||stt;
        execute statement (:stt);
        total_objects_removed = total_objects_removed + 1;
    end
    close c_view;

    open c_func; --------------------  d r o p   f u c t i o n s  ----------------
    while (1=1) do
    begin
        fetch c_func into stt;
        if (row_count = 0) then leave;
        stt = 'drop function '||stt;
        execute statement (:stt);
        total_objects_removed = total_objects_removed + 1;
    end
    close c_func;

    open c_proc; -----------------  d r o p   p r o c e d u r e s  ---------------
    while (1=1) do
    begin
        fetch c_proc into stt;
        if (row_count = 0) then leave;
        stt = 'drop procedure '||stt;
        execute statement (:stt);
        total_objects_removed = total_objects_removed + 1;
    end
    close c_proc;


    open c_pkg; ------------------  d r o p     p a c k a g e s  ---------------
    while (1=1) do
    begin
        fetch c_pkg into stt;
        if (row_count = 0) then leave;
        stt = 'drop package '||stt;
        execute statement (:stt);
        total_objects_removed = total_objects_removed + 1;
    end
    close c_pkg;


    open c_excp; -----------------  d r o p   e x c e p t i o n s  ---------------
    while (1=1) do
    begin
        fetch c_excp into stt;
        if (row_count = 0) then leave;
        stt = 'drop exception '||stt;
        execute statement (:stt);
        total_objects_removed = total_objects_removed + 1;
    end
    close c_excp;

    open c_fk; -----------  d r o p    r e f.   c o n s t r a i n t s ------------
    while (1=1) do
    begin
        fetch c_fk into ref_name, tab_name;
        if (row_count = 0) then leave;
        stt = 'alter table '||tab_name||' drop constraint '||ref_name;
        execute statement (:stt);
        total_objects_removed = total_objects_removed + 1;
    end
    close c_fk;

    open c_tabs; -----------  d r o p    t a b l e s  ------------
    while (1=1) do
    begin
        fetch c_tabs into stt;
        if (row_count = 0) then leave;
        stt = 'drop table '||stt;
        execute statement (:stt);
        total_objects_removed = total_objects_removed + 1;
    end
    close c_tabs;

    open c_doms; -------------------  d r o p    d o m a i n s -------------------
    while (1=1) do
    begin
        fetch c_doms into stt;
        if (row_count = 0) then leave;
        stt = 'drop domain '||stt;
        execute statement (:stt);
        total_objects_removed = total_objects_removed + 1;
    end
    close c_doms;

    open c_coll; ---------------  d r o p    c o l l a t i o n s -----------------
    while (1=1) do
    begin
        fetch c_coll into stt;
        if (row_count = 0) then leave;
        stt = 'drop collation '||stt;
        execute statement (:stt);
        total_objects_removed = total_objects_removed + 1;
    end
    close c_coll;

    open c_cset;
    while (1=1) do
    begin
        fetch c_cset into stt, def_coll;
        if (row_count = 0) then leave;
        stt = 'alter character set ' || trim(stt) || ' set default collation ' || trim(stt);
        execute statement (:stt);
        total_objects_removed = total_objects_removed + 1;
    end
    close c_cset;

    open c_gens; -----------------  d r o p    s e q u e n c e s -----------------
    while (1=1) do
    begin
        fetch c_gens into stt;
        if (row_count = 0) then leave;
        stt = 'drop sequence '||stt;
        execute statement (:stt);
        total_objects_removed = total_objects_removed + 1;
    end
    close c_gens;

    open c_role; --------------------  d r o p    r o l e s ----------------------
    while (1=1) do
    begin
        fetch c_role into stt;
        if (row_count = 0) then leave;
        stt = 'drop role '||stt;
        execute statement (:stt);
        total_objects_removed = total_objects_removed + 1;
    end
    close c_role;

    open c_local_mapping; ----------  d r o p   l o c a l   m a p p i n g s  ----
    while (1=1) do
    begin
        fetch c_local_mapping into stt;
        if (row_count = 0) then leave;
        stt = 'drop mapping '|| stt;
        execute statement (:stt);
        total_objects_removed = total_objects_removed + 1;
    end
    close c_local_mapping;


    /*******************************

        ###################################################
        ### TEMPORARY DISABLED OTHERWISE FB HANGS! ###
        WAITING FOR FIX:
        https://github.com/FirebirdSQL/firebird/issues/6861
        ###################################################
        open c_users; ----------  d r o p   u s e r s   e x c e p t   S Y S D B A  ----
        while (1=1) do
        begin
            fetch c_users into usr_name, sec_plugin;
            if (row_count = 0) then leave;

            stt = 'alter user '|| usr_name || ' revoke admin role using plugin ' || sec_plugin;
            execute statement (:stt); -- ?! with autonomous transaction;

            begin
                -- Privileges for GRANT / DROP database remain even when user is droppped.
                -- We have to use REVOKE ALL ON ALL in order to cleanup them:
                stt = 'revoke all on all from '|| usr_name;
                execute statement (:stt); -- ?! with autonomous transaction;
                when any do
                begin
                   --- suppress warning ---
                end
            end

            stt = 'drop user '|| usr_name || ' using plugin ' || sec_plugin;
            execute statement (:stt);
            total_objects_removed = total_objects_removed + 1;
        end
        close c_users;
    
    ***********************************/

    rdb$set_context('USER_SESSION', 'total_objects_removed', total_objects_removed);

end
^
set term ;^
commit;

select rdb$get_context('USER_SESSION', 'total_objects_removed') as "Finish. Total objects removed:" from rdb$database;
