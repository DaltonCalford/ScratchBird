#coding:utf-8

"""
ID:          tabloid.comment-in-object-names
TITLE:       All DB objects types must allow name COMMENT. Also, COMMENT ON ... must allow occurence of "comment" in it.
DESCRIPTION: 
  Original issue: https://granicus.if.org/pgbugs/15555
FBTEST:      functional.tabloid.comment_in_object_names
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create or alter user comment password 'comment';

    create collation comment for utf8 from unicode case insensitive 'NUMERIC-SORT=1';
    create exception comment 'comment';
    create domain comment varchar(100);
    create table comment(comment comment);
    create index comment on comment(comment);
    create role comment;
    create sequence comment;
    recreate trigger comment for comment active before insert as begin end;

    comment on database is '"comment": this is database with strange name: "comment"';
    comment on user comment is '"comment": this is user with strange name: "comment"';
    comment on collation comment is '"comment": this is collation with strange name: "comment"';
    comment on exception comment is '"comment": this is exception with strange name: "comment"';
    comment on domain comment is '"comment": this is domain with strange name: "comment"';
    comment on table comment is '"comment": this is table with strange name: "comment"';
    comment on column comment.comment is '"comment": this is a column with strange name: "comment"';
    comment on index comment is '"comment": this is index for table with strange name: "comment"';
    comment on role comment is '"comment": this is role with strange name: "comment"';
    comment on sequence comment is '"comment": this is generator with strange name: "comment"';
    comment on trigger comment is '"comment": this is trigger for table with strange name: "comment"';
    commit;

    drop user comment;
    commit;


    drop table comment;
    commit;

    create view comment as select 1 x from rdb$database;
    comment on view comment is '"comment": this is view with strange name: "comment"';
    commit;
    drop view comment;
    commit;


    create procedure comment as begin end;
    comment on procedure comment is '"comment": this is procedure with strange name: "comment"';
    commit;

    set term ^;
    create function comment returns comment as
    begin
        return 'comment';
    end^
    set term ;^
    commit;

    comment on function comment is '"comment": this is function with strange name: "comment"';
    commit;

    set term ^;
    create or alter package comment as
    begin
        function comment returns comment;
        procedure comment returns (comment comment);
    end
    ^
    recreate package body comment as
    begin
        function comment returns comment as
        begin
            return 'comment';
        end
        
        procedure comment returns (comment comment) as
        begin
            comment='comment';
            suspend;
        end

    end
    ^
    execute block as
        declare comment comment;
    begin
        comment = 'comment';
    end
    ^
    set term ;^
    commit;

    comment on package comment is '"comment": this is procedure with strange name: "comment"';
    commit;

"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.execute(combine_output = True)
