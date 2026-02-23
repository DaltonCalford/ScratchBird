#coding:utf-8

"""
ID:          issue-4298
ISSUE:       4298
TITLE:       Creating a procedure containing "case when" expression leads to a server crash
DESCRIPTION:
JIRA:        CORE-3965
FBTEST:      bugs.core_3965
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='employee-ods12.fbk')

test_script = """
    -- Table 'sales' for this SP is taken from standard samples database 'employee.fdb' which comes with every FB build.
    set term ^;
    create or alter procedure p_beteiligung_order (
        gid char(36) character set iso8859_1 collate iso8859_1,
        ordernr integer,
        dir smallint,
        mit_fuehrender char(1) character set iso8859_1 collate iso8859_1)
    as
    declare variable cur_ordernr integer;
    declare variable max_ordernr integer;
    declare variable fk_ref char(36);
    begin

      if (mit_fuehrender is null) then
        mit_fuehrender = 'F';


      select r.qty_ordered, r.item_type
      from sales r
      where r.po_number = :gid
      into :cur_ordernr, :fk_ref;

      if (ordernr is null) then
        ordernr = cur_ordernr + coalesce(dir, 0);

      if (ordernr <= case mit_fuehrender
                       when 'T' then 1
                       else 2
                     end) then
        ordernr = case mit_fuehrender
                    when 'T' then 1
                    else 2
                  end;
      else
      begin
        select max(r.qty_ordered)
        from sales r
        where r.item_type = :fk_ref
        into :max_ordernr;
        if (ordernr > max_ordernr) then
          ordernr = max_ordernr;
      end

      if (ordernr = cur_ordernr) then
        exit;
      else
      if (ordernr < cur_ordernr) then
        update sales r
        set r.qty_ordered = r.qty_ordered + 1
        where r.item_type = :fk_ref and
              r.qty_ordered between :ordernr and :cur_ordernr;
      else
        update sales r
        -- ::: NB ::: in the ticket it was: "set r.qty_ordered = r.ordernr - 1" - but there is NO such field in the table SALES!
        set r.qty_ordered = :ordernr - 1
        where r.qty_ordered = :fk_ref and
              r.qty_ordered between :cur_ordernr and :ordernr;

      update sales r
      set r.qty_ordered = :ordernr
      where r.po_number = :gid;

    end^
    set term ;^
    commit;

    drop procedure p_beteiligung_order;
    commit;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.execute()
