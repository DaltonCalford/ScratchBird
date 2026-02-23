set bail on;
create domain dm_emp_id smallint;
create domain dm_dep_id smallint;
create domain dm_name varchar(20);

create table department (
    dept_no dm_dep_id not null,
    dept_name dm_name not null,
    constraint dept_key primary key (dept_no)
);

create table employee (
    emp_no dm_emp_id not null,
    last_name dm_name not null, 
    dept_no dm_dep_id not null constraint ref_key references department(dept_no),
    constraint emp_key primary key (emp_no)
);
commit;
insert into department( dept_no, dept_name) values (1, 'd1');
insert into department( dept_no, dept_name) values (2, 'd2');
insert into department( dept_no, dept_name) values (3, 'd3');
insert into employee( emp_no, last_name, dept_no) values (1, 'e1', 1);
insert into employee( emp_no, last_name, dept_no) values (2, 'e2', 2);
insert into employee( emp_no, last_name, dept_no) values (3, 'e3', 3);
insert into employee( emp_no, last_name, dept_no) values (4, 'e4', 1);
insert into employee( emp_no, last_name, dept_no) values (5, 'e5', 1);
insert into employee( emp_no, last_name, dept_no) values (6, 'e6', 1);
insert into employee( emp_no, last_name, dept_no) values (7, 'e7', 2);
insert into employee( emp_no, last_name, dept_no) values (8, 'e8', 3);
insert into employee( emp_no, last_name, dept_no) values (9, 'e9', 3);
commit;

-- ### ACHTUNG: IMPORTANT ###
-- On WINDOWS setting 'set bail on' is siletnly ignored when .sql commands
-- are passed via PIPE, i.e. using command like this:
-- type <script_with_bail_on.sql> | %fb_home%\isql host/port:db_file
-- If some statement from <script_with_bail_on.sql> failed then script
-- will be CONTINUED, thus ignoring SET BAIL ON!
-- The reason is that PIPE operator (vertical bar, "|") is considered
-- on Windows as *interactive*, i.e. like user manually types command-by-command.
-- On Linux this is not so, and any failed statement will stop execurion
-- of the whole script immediately. So, we have a
-- ###############################################
-- ### DIFFERENT BEHAVIOUR ON WINDOWS vs LINUS ###
-- ###############################################
-- See also:
-- 1. Discussion with pcisar, alex and dimitr:
--    subj: "[new-qa] ISQL "SET BAIL ON" problem on Windows when commands come from PIPE"
--    date: 12-mar-2022 16:04 
-- 2. doc\README.isql_enhancements.txt ("if the user loads isql interactively and later
--    executes a script with the input command, this is considered an interactive session
--    even though isql knows it's executing a script")

-- BECAUSE OF THIS, IT IS MANDATORY TO RETURN BAIL TO ITS DEFAULT VALUE:
--############
set bail off;
--############
