#coding:utf-8

"""
ID:          gtcs.conversion-error-from-string
FBTEST:      functional.gtcs.conversion_error_from_string
TITLE:       Script issues conversion error from string
DESCRIPTION:
  ::: NB :::
  ### Name of original test has no any relation with actual task of this test: ###
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/CF_ISQL_31.script

  Source description of problem with script for reproducing:
  https://sourceforge.net/p/firebird/mailman/message/17016915/

  Issue in original test: bug in devel-list / Reported by lobolo2000 18-May-2004
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create domain dm_id as bigint
     not null;

    create domain dm_amount as numeric(18,4)
     default 0
      not null;

    create domain dm_quantity as numeric(18,3)
     default 0
     not null;

    create table items (
     uid dm_id primary key ,
     description varchar(80));

    create table mitems (
           uid dm_id primary key,
     item_uid dm_id references items(uid) on delete cascade,
     mfield1 varchar(15) default '' not null,
     mfield2 varchar(15) default '' not null,
     mfield3 varchar(15) default '' not null,
     mfield4 varchar(15) default '' not null);

    create table bs (
      uid dm_id primary key,
      mitem_uid dm_id references mitems(uid) on delete cascade,
      be varchar(25));

    create table mitems_expiry (
      uid dm_id primary key,
     mitem_uid dm_id references mitems(uid) on delete cascade,
      expiry_date date);

    create table mitems_expiry_warehouses (
      uid dm_id primary key,
     mitem_expiry_uid dm_id references mitems_expiry(uid) on delete cascade,
     reorder_lvl dm_quantity,
     restock_lvl dm_quantity,
     qoh dm_quantity,
     qoc dm_quantity,
     qty_on_order dm_quantity,
     qty_reserved dm_quantity,
     dtd_qty_purchased dm_quantity,
     mtd_qty_purchased dm_quantity,
     ytd_qty_purchased dm_quantity,
     dtd_qty_sold dm_quantity,
     mtd_qty_sold dm_quantity,
     ytd_qty_sold dm_quantity);

    alter table mitems_expiry
      add reorder_lvl computed by (coalesce((select sum(reorder_lvl) from

    mitems_expiry_warehouses),0)),
      add restock_lvl computed by (coalesce((select sum(restock_lvl) from

    mitems_expiry_warehouses),0)),
      add qoh computed by (coalesce((select sum(qoh) from
    mitems_expiry_warehouses),0)),
      add qoc computed by (coalesce((select sum(qoc) from
    mitems_expiry_warehouses),0)),
      add qty_on_order computed by (coalesce((select sum(qty_on_order) from

    mitems_expiry_warehouses),0)),
      add qty_reserved computed by (coalesce((select sum(qty_reserved) from

    mitems_expiry_warehouses),0)),
      add dtd_qty_purchased computed by (coalesce((select sum(dtd_qty_purchased)
    from

    mitems_expiry_warehouses),0)),
      add mtd_qty_purchased computed by (coalesce((select sum(mtd_qty_purchased)
    from

    mitems_expiry_warehouses),0)),
      add ytd_qty_purchased computed by (coalesce((select sum(ytd_qty_purchased)
    from

    mitems_expiry_warehouses),0)),
      add dtd_qty_sold computed by (coalesce((select sum(dtd_qty_sold) from

    mitems_expiry_warehouses),0)),
      add mtd_qty_sold computed by (coalesce((select sum(mtd_qty_sold) from

    mitems_expiry_warehouses),0)),
      add ytd_qty_sold computed by (coalesce((select sum(ytd_qty_sold) from

    mitems_expiry_warehouses),0));

    alter table mitems
      add reorder_lvl computed by (coalesce((select sum(reorder_lvl) from
    mitems_expiry),0)),
      add restock_lvl computed by (coalesce((select sum(restock_lvl) from
    mitems_expiry),0)),
      add qoh computed by (coalesce((select sum(qoh) from mitems_expiry),0)),
      add qoc computed by (coalesce((select sum(qoc) from mitems_expiry),0)),
      add qty_on_order computed by (coalesce((select sum(qty_on_order) from
    mitems_expiry),0)),
      add qty_reserved computed by (coalesce((select sum(qty_reserved) from
    mitems_expiry),0)),
      add dtd_qty_purchased computed by (coalesce((select sum(dtd_qty_purchased)
    from

    mitems_expiry),0)),
      add mtd_qty_purchased computed by (coalesce((select sum(mtd_qty_purchased)
    from

    mitems_expiry),0)),
      add ytd_qty_purchased computed by (coalesce((select sum(ytd_qty_purchased)
    from

    mitems_expiry),0)),
      add dtd_qty_sold computed by (coalesce((select sum(dtd_qty_sold) from
    mitems_expiry),0)),
      add mtd_qty_sold computed by (coalesce((select sum(mtd_qty_sold) from
    mitems_expiry),0)),
      add ytd_qty_sold computed by (coalesce((select sum(ytd_qty_sold) from
    mitems_expiry),0));

    alter table items
      add reorder_lvl computed by (coalesce((select sum(reorder_lvl) from
    mitems),0)),
      add restock_lvl computed by (coalesce((select sum(restock_lvl) from
    mitems),0)),
      add qoh computed by (coalesce((select sum(qoh) from mitems),0)),
      add qoc computed by (coalesce((select sum(qoc) from mitems),0)),
      add qty_on_order computed by (coalesce((select sum(qty_on_order) from
    mitems),0)),
      add qty_reserved computed by (coalesce((select sum(qty_reserved) from
    mitems),0)),
      add dtd_qty_purchased computed by (coalesce((select sum(dtd_qty_purchased)
    from

    mitems),0)),
      add mtd_qty_purchased computed by (coalesce((select sum(mtd_qty_purchased)
    from

    mitems),0)),
      add ytd_qty_purchased computed by (coalesce((select sum(ytd_qty_purchased)
    from

    mitems),0)),
      add dtd_qty_sold computed by (coalesce((select sum(dtd_qty_sold) from
    mitems),0)),
      add mtd_qty_sold computed by (coalesce((select sum(mtd_qty_sold) from
    mitems),0)),
      add ytd_qty_sold computed by (coalesce((select sum(ytd_qty_sold) from
    mitems),0));

    set term ^;

    create generator items_gen^

    create trigger items_tr0 for items
     active before insert position 0 as
      begin
       if (new.uid is null or new.uid<0) then new.uid=gen_id(items_gen,1);
      end^

    create trigger items_tr2 for items
     active after insert position 2 as
      begin
        insert into mitems(item_uid) values(new.uid);
      end^

    create generator mitems_gen^

    create trigger mitems_tr0 for mitems
     active before insert position 0 as
      begin
       if (new.uid is null or new.uid<0) then new.uid=gen_id(mitems_gen,1);
      end^

    create generator bs_gen^

    create trigger bs_tr0 for bs
     active before insert position 0 as
      begin
       if (new.uid is null or new.uid<0) then new.uid=gen_id(bs_gen,1);
      end^

    create generator mitems_expiry_gen^

    create trigger mitems_expiry_tr0 for mitems_expiry
     active before insert position 0 as
      begin
       if (new.uid is null or new.uid<0) then
    new.uid=gen_id(mitems_expiry_gen,1);
      end^

    create generator mitems_expiry_warehouses_gen^

    create trigger mitems_expiry_warehouses_tr0 for mitems_expiry_warehouses
     active before insert position 0 as
      begin
       if (new.uid is null or new.uid<0) then

    new.uid=gen_id(mitems_expiry_warehouses_gen,1);
      end^

    set term ;^

    commit;

    insert into items(description) values('pa');
    insert into items(description) values('fa');
    insert into items(description) values('sa');
    insert into items(description) values('la');
    insert into items(description) values('ma');
    insert into items(description) values('ka');

    commit;

    set list on;
    set count on;

    select 'point-1' msg, m.* from items m;
    select 'point-2' msg, m.qoh, m.qoh, m.qoh, m.qoh, m.qoh, m.qoh, m.qoh, m.qoh, m.qoh, m.qoh from items m;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    MSG                             point-1
    UID                             1
    DESCRIPTION                     pa
    REORDER_LVL                     0.000
    RESTOCK_LVL                     0.000
    QOH                             0.000
    QOC                             0.000
    QTY_ON_ORDER                    0.000
    QTY_RESERVED                    0.000
    DTD_QTY_PURCHASED               0.000
    MTD_QTY_PURCHASED               0.000
    YTD_QTY_PURCHASED               0.000
    DTD_QTY_SOLD                    0.000
    MTD_QTY_SOLD                    0.000
    YTD_QTY_SOLD                    0.000

    MSG                             point-1
    UID                             2
    DESCRIPTION                     fa
    REORDER_LVL                     0.000
    RESTOCK_LVL                     0.000
    QOH                             0.000
    QOC                             0.000
    QTY_ON_ORDER                    0.000
    QTY_RESERVED                    0.000
    DTD_QTY_PURCHASED               0.000
    MTD_QTY_PURCHASED               0.000
    YTD_QTY_PURCHASED               0.000
    DTD_QTY_SOLD                    0.000
    MTD_QTY_SOLD                    0.000
    YTD_QTY_SOLD                    0.000

    MSG                             point-1
    UID                             3
    DESCRIPTION                     sa
    REORDER_LVL                     0.000
    RESTOCK_LVL                     0.000
    QOH                             0.000
    QOC                             0.000
    QTY_ON_ORDER                    0.000
    QTY_RESERVED                    0.000
    DTD_QTY_PURCHASED               0.000
    MTD_QTY_PURCHASED               0.000
    YTD_QTY_PURCHASED               0.000
    DTD_QTY_SOLD                    0.000
    MTD_QTY_SOLD                    0.000
    YTD_QTY_SOLD                    0.000

    MSG                             point-1
    UID                             4
    DESCRIPTION                     la
    REORDER_LVL                     0.000
    RESTOCK_LVL                     0.000
    QOH                             0.000
    QOC                             0.000
    QTY_ON_ORDER                    0.000
    QTY_RESERVED                    0.000
    DTD_QTY_PURCHASED               0.000
    MTD_QTY_PURCHASED               0.000
    YTD_QTY_PURCHASED               0.000
    DTD_QTY_SOLD                    0.000
    MTD_QTY_SOLD                    0.000
    YTD_QTY_SOLD                    0.000

    MSG                             point-1
    UID                             5
    DESCRIPTION                     ma
    REORDER_LVL                     0.000
    RESTOCK_LVL                     0.000
    QOH                             0.000
    QOC                             0.000
    QTY_ON_ORDER                    0.000
    QTY_RESERVED                    0.000
    DTD_QTY_PURCHASED               0.000
    MTD_QTY_PURCHASED               0.000
    YTD_QTY_PURCHASED               0.000
    DTD_QTY_SOLD                    0.000
    MTD_QTY_SOLD                    0.000
    YTD_QTY_SOLD                    0.000

    MSG                             point-1
    UID                             6
    DESCRIPTION                     ka
    REORDER_LVL                     0.000
    RESTOCK_LVL                     0.000
    QOH                             0.000
    QOC                             0.000
    QTY_ON_ORDER                    0.000
    QTY_RESERVED                    0.000
    DTD_QTY_PURCHASED               0.000
    MTD_QTY_PURCHASED               0.000
    YTD_QTY_PURCHASED               0.000
    DTD_QTY_SOLD                    0.000
    MTD_QTY_SOLD                    0.000
    YTD_QTY_SOLD                    0.000


    Records affected: 6

    MSG                             point-2
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000

    MSG                             point-2
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000

    MSG                             point-2
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000

    MSG                             point-2
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000

    MSG                             point-2
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000

    MSG                             point-2
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000
    QOH                             0.000


    Records affected: 6
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
