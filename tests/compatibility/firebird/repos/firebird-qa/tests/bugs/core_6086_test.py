#coding:utf-8

"""
ID:          issue-6336
ISSUE:       6336
TITLE:       Creating of the large procedure crashes the server
DESCRIPTION:
JIRA:        CORE-6086
FBTEST:      bugs.core_6086
"""

import pytest
from firebird.qa import *

db = db_factory(charset='WIN1251')

test_script = """
    set bail on;
	CREATE GLOBAL TEMPORARY TABLE SESSION_EDIT (
		ID          INTEGER NOT NULL,
		ID_SESSION  INTEGER NOT NULL,
		I1          INTEGER,
		I2          INTEGER,
		I3          INTEGER,
		I4          INTEGER,
		I5          INTEGER,
		N1          NUMERIC(18,4),
		N2          NUMERIC(18,4),
		N3          NUMERIC(18,4),
		N4          NUMERIC(18,4),
		SYSDAY      TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
		D1          DATE,
		D2          DATE,
		I6          INTEGER,
		I7          INTEGER,
		I8          INTEGER,
		N5          NUMERIC(18,4),
		N6          NUMERIC(18,4),
		N7          NUMERIC(18,4),
		N8          NUMERIC(18,4),
		N9          NUMERIC(18,4),
		N10         NUMERIC(18,4),
		N11         NUMERIC(18,4),
		N12         NUMERIC(18,4),
		VID         INTEGER,
		ID_ITEM     INTEGER,
		N13         NUMERIC(18,4),
		N14         NUMERIC(18,4),
		N15         NUMERIC(18,4),
		N16         NUMERIC(18,4),
		N17         NUMERIC(18,4),
		N18         NUMERIC(18,4),
		N19         NUMERIC(18,4),
		N20         NUMERIC(18,4),
		N21         NUMERIC(18,4),
		N22         NUMERIC(18,4),
		V1          VARCHAR(32),
		V2          VARCHAR(252),
		V3          VARCHAR(32),
		V4          VARCHAR(252),
		V5          VARCHAR(20),
		V6          VARCHAR(20),
		V7          VARCHAR(40),
		V8          VARCHAR(40),
		V9          VARCHAR(40),
		V10         VARCHAR(1024),
		V11         VARCHAR(1024),
		B1          BIGINT
	) ON COMMIT PRESERVE ROWS;

	ALTER TABLE SESSION_EDIT ADD CONSTRAINT PK_SESSION_EDIT PRIMARY KEY (ID);

	set term ^;

	create or alter procedure get_rows(a int) returns(num int) as
	begin
		suspend;
	end
	^

	create or alter procedure CALC_ITEM_SUMMA (
		ID_FIRMA integer,
		D1 date,
		D2 date,
		MAND_CAT integer,
		BL_CAT integer,
		ID_NPRICE integer = null,
		OST integer = 1,
		MAND_CAT2 integer = null)
	returns (
		ID_SESSION integer)
	AS
		declare vid1 int = 1; -- Выпуск
		declare vid2 int = 2; -- Расход сырья по изделиям (рецепт)
		declare vid3 int = 3; -- Закупка ГИ
		declare vid4 int = 4; -- Переработка в сырье
		declare vid5 int = 5; -- Расход по документам
		declare vid6 int = 6; -- Переработка приход
		declare vid7 int = 7; -- Переработка расход
		declare vid8 int = 8; -- Остаток на начало
		declare vid9 int = 9; -- Свернутые Выпуск + остатки + расход по изделию
		declare vid10 int = 10; -- Реализация
		declare vid11 int = 11; -- Н/р по реализации
		declare vid12 int = 12; -- Остаток на конец
		declare vid99 int = 99; -- Накладные расходы
		declare vid20 int = 20; -- Готовые расчеты
		declare vid22 int = 22; -- Прочие движения по ГИ (сведенный)
		declare vid23 int = 23; -- Промежуточные расчеты (расчет расхода сырья)
		declare vid24 int = 24; -- Промежуточные расчеты (расчет переработки)
		declare vid25 int = 25; -- Промежуточные расчеты (перераспределение переработки)
		declare id_item int;
		declare items numeric(18,3);
		declare summa numeric(18,2);
		declare items0 numeric(18,3);
		declare summa0 numeric(18,2);
		declare summa1 numeric(18,2);
		declare items1 numeric(18,3);
		declare items2 numeric(18,3);
		declare summa2 numeric(18,2);
		declare items3 numeric(18,3);
		declare items4 numeric(18,3);
		declare items5 numeric(18,3);
		declare id0 int;
		declare id1 int;
		declare k int;
		declare n int;
		declare type_v int;
	begin
	  -- Получаем данные из документов
	--select id_session from calc_item_summa_save(:id_firma, :d1, :d2, :mand_cat, :bl_cat, :id_nprice, :ost, :mand_cat2)
	--into id_session;

	  -- распределение прочего расхода сырья
	  for select id_item, n1, n2, id from session_edit
		  where id_session = :id_session and vid = :vid5
	  into id_item, items0, summa0, id0 do
	  begin
		items1 = 0; summa1 = 0;
		select sum(n1+n4), sum(n2) from session_edit
		where id_session = :id_session and vid = :vid2 and id_item = :id_item and i5 = 0
		into items1, summa1;

		if (items1 <> 0) then
		begin
		  for select n1+n4, n2, id from session_edit
			  where id_session = :id_session and vid = :vid2 and id_item = :id_item and i5 = 0
			  order by 2, 1
		  into items, summa, id1 do
		  begin
			items2 = 1e0 * items0 * items / items1;
			if (items0 <> 0) then
			  summa2 = 1e0 * items2 * summa0 / items0;
			else
			  summa2 = coalesce(1e0 * summa0 * summa / nullif(summa1, 0), 0);

			update session_edit set
			  n5 = :items2,
			  n6 = :summa2
			where id = :id1;
		  end

		  items2 = 0; summa2 = 0;
		  select sum(n5), sum(n6) from session_edit
		  where id_session = :id_session and vid = :vid2 and id_item = :id_item and i5 = 0
		  into items2, summa2;

		  if (items2 <> items0 or summa2 <> summa0) then
		  update session_edit set
			n5 = n5 + :items0 - :items2,
			n6 = n6 + :summa0 - :summa2
		  where id = :id1;

		  update session_edit set
			i4 = 1
		  where id = :id0;
		end
	  end

	  -- распределение стоимости расхода сырья вне рецептов
	  for select sum(n2) from session_edit
		  where id_session = :id_session and vid = :vid5 and i4 is null
		  having sum(n2) <> 0
	  into summa0 do
	  begin
		summa1 = 0;
		select sum(n2+n6) from session_edit
		where id_session = :id_session and vid = :vid2
		into summa1;

		if (summa1 <> 0) then
		update session_edit set
		  n7 = cast(1e0*(n2+n6)*:summa0/:summa1 as numeric(12,2))
		where id_session = :id_session and vid = :vid2;

		summa2 = 0;
		select sum(n7) from session_edit where id_session = :id_session and vid = :vid2
		into summa2;

		if (summa2 <> summa0) then
		update session_edit set
		  n7 = n7 + :summa0 - :summa2
		where id_session = :id_session and vid = :vid2
		order by n2 desc
		rows 1;

		update session_edit set
		  i4 = 1
		where id_session = :id_session and i2 = :vid5;
	  end

	  -- Списываем остаток из расхода изделий
	  for select id, id_item, n1, n2 from session_edit s
		  where id_session = :id_session and vid = :vid8 and n1 > 0
	  into id0, id_item, items0, summa0 do
	  begin
		items2 = 0;
		select sum(n1) from session_edit
		where id_session = :id_session and vid = :vid2 and id_item = :id_item
		into items2;

		-- Убираем из выпуска
		items1 = minvalue(items0, items2);
		if (items1 > 0) then
		begin
		  summa1 = iif(items1 = items0, summa0, 1e0*items1*summa0/items0);

		  for select id, n1 from session_edit
			  where id_session = :id_session and vid = :vid2 and id_item = :id_item
			  order by 2
		  into id1, items do
		  begin
			items = 1e0*items1*items/items2;
			summa = 1e0*items*summa0/items0;

			update session_edit set
			  n11 = :items,
			  n12 = :summa,
			  n1 = n1 - :items
			where id = :id1;
		  end

		  select sum(n11), sum(n12) from session_edit
		  where id_session = :id_session and vid = :vid2 and id_item = :id_item
		  into items, summa;

		  if (items <> items1 or summa <> summa1) then
		  update session_edit set
			n11 = n11 + :items1 - :items,
			n12 = n12 + :summa1 - :summa,
			n1 = n1 - :items1 + :items
		  where id = :id1;

		  update session_edit set
			n3 = :items1,
			n4 = :summa1
		  where id = :id0;

		  items0 = items0 - items1;
		  summa0 = summa0 - summa1;
		end

		if (items0 <> 0) then
		begin
		  items2 = 0;
		  select sum(n1) from session_edit
		  where id_session = :id_session and vid = :vid7 and id_item = :id_item
		  into items2;

		  -- Убираем из переработки
		  items1 = minvalue(items0, items2);
		  if (items1 > 0) then
		  begin
			summa1 = iif(items1 = items0, summa0, 1e0*items1*summa0/items0);

			for select id, n1 from session_edit
				where id_session = :id_session and vid = :vid7 and id_item = :id_item
				order by 2
			into id1, items do
			begin
			  items = 1e0*items1*items/items2;
			  summa = 1e0*items*summa0/items0;

			  update session_edit set
				n11 = :items,
				n12 = :summa,
				n1 = n1 - :items
			  where id = :id1;
			end

			select sum(n11), sum(n12) from session_edit
			where id_session = :id_session and vid = :vid7 and id_item = :id_item
			into items, summa;

			if (items <> items1 or summa <> summa1) then
			update session_edit set
			  n11 = n11 + :items1 - :items,
			  n12 = n12 + :summa1 - :summa,
			  n1 = n1 - :items1 + :items
			where id = :id1;

			update session_edit set
			  n5 = :items1,
			  n6 = :summa1
			where id = :id0;

			items0 = items0 - items1;
			summa0 = summa0 - summa1;
		  end
		end
	  end

	  -- Списываем закупку из расхода изделий
	  for select id, id_item, n1, n2 from session_edit s
		  where id_session = :id_session and vid = :vid3 and n1 > 0
	  into id0, id_item, items0, summa0 do
	  begin
		items2 = 0;
		select sum(n1) from session_edit
		where id_session = :id_session and vid = :vid2 and id_item = :id_item
		into items2;

		-- Убираем из выпуска
		items1 = minvalue(items0, items2);
		if (items1 > 0) then
		begin
		  summa1 = iif(items1 = items0, summa0, 1e0*items1*summa0/items0);

		  for select id, n1 from session_edit
			  where id_session = :id_session and vid = :vid2 and id_item = :id_item
			  order by 2
		  into id1, items do
		  begin
			items = 1e0*items1*items/items2;
			summa = 1e0*items*summa0/items0;

			update session_edit set
			  n13 = :items,
			  n14 = :summa,
			  n1 = n1 - :items
			where id = :id1;
		  end

		  select sum(n13), sum(n14) from session_edit
		  where id_session = :id_session and vid = :vid2 and id_item = :id_item
		  into items, summa;

		  if (items <> items1 or summa <> summa1) then
		  update session_edit set
			n13 = n13 + :items1 - :items,
			n14 = n14 + :summa1 - :summa,
			n1 = n1 - :items1 + :items
		  where id = :id1;

		  update session_edit set
			n3 = :items1,
			n4 = :summa1
		  where id = :id0;

		  items0 = items0 - items1;
		  summa0 = summa0 - summa1;
		end

		if (items0 <> 0) then
		begin
		  items2 = 0;
		  select sum(n1) from session_edit
		  where id_session = :id_session and vid = :vid7 and id_item = :id_item
		  into items2;

		  -- Убираем из переработки
		  items1 = minvalue(items0, items2);
		  if (items1 > 0) then
		  begin
			summa1 = iif(items1 = items0, summa0, 1e0*items1*summa0/items0);

			for select id, n1 from session_edit
				where id_session = :id_session and vid = :vid7 and id_item = :id_item
				order by 2
			into id1, items do
			begin
			  items = 1e0*items1*items/items2;
			  summa = 1e0*items*summa0/items0;

			  update session_edit set
				n13 = :items,
				n14 = :summa,
				n1 = n1 - :items
			  where id = :id1;
			end

			select sum(n13), sum(n14) from session_edit
			where id_session = :id_session and vid = :vid7 and id_item = :id_item
			into items, summa;

			if (items <> items1 or summa <> summa1) then
			update session_edit set
			  n13 = n13 + :items1 - :items,
			  n14 = n14 + :summa1 - :summa,
			  n1 = n1 - :items1 + :items
			where id = :id1;

			update session_edit set
			  n5 = :items1,
			  n6 = :summa1
			where id = :id0;

			items0 = items0 - items1;
			summa0 = summa0 - summa1;
		  end
		end
	  end

	  insert into session_edit(id_session, vid, id_item, n1, n2, n3, n4, n5, n6, n7, n8, n9, n10, n16, n17, n18, n19)
	  select :id_session, :vid9, id_item, sum(items), sum(summa), sum(summa_inv), sum(summa_in), sum(items_in), sum(items_b),
			 sum(summa_b), sum(items_prih), sum(summa_prih), sum(items_p_in), sum(items_t), sum(summa_t), max(price_b), sum(items_end)
	  from (select id_item,
				   sum(n1+n4+n5) items,
				   sum(n2+n6+n7) summa,
				   sum(n6+n7) summa_inv,
				   null ITEMS_in,
				   null summa_in,
				   0 items_b,
				   0 summa_b,
				   0 items_prih,
				   0 summa_prih,
				   0 items_p_in,
				   0 items_t,
				   0 summa_t,
				   0 price_b,
				   0 items_end
			from session_edit
			where id_session = :id_session and vid = :vid2
			group by 1
			  union all
			select id_item, 0, 0, 0, n1, n2, 0, 0, 0, 0, 0, 0, 0, 0, 0 from session_edit where id_session = :id_session and vid = :vid1  -- приход ГИ
			  union all
			select id_item, 0, 0, 0, 0, 0, 0, 0, n1, n2, 0, 0, 0, 0, 0 from session_edit where id_session = :id_session and vid = :vid3 -- закупка ГИ
			  union all
			select id_item, 0, 0, 0, 0, 0, n1, n2, 0, 0, 0, 0, 0, n3, 0 from session_edit where id_session = :id_session and vid = :vid8 -- остаток ГИ
			  union all
			select id_item, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, n1 from session_edit where id_session = :id_session and vid = :vid12 -- остаток ГИ (на конец)
			  union all
			select id_item, 0, 0, 0, 0, 0, 0, 0, 0, 0, n1, 0, 0, 0, 0 from session_edit where id_session = :id_session and vid = :vid6  -- переработка приход ГИ
			  union all
			select id_item, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, n1, n2, 0, 0 from session_edit where id_session = :id_session and vid = :vid4
			  union all
			select distinct id_item, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 from session_edit where id_session = :id_session and vid in (:vid7, :vid10, :vid22))
	  group by id_item;

	  insert into session_edit(id_session, vid, id_item, n1, n2, n3, n5)
	  with recursive
	  -- Расход по поличеству сырья на изделие
	  rashod as (
		select id_item,
			   i1,
			   sum(n1+n4+n5) items
		from session_edit
		where id_session = :id_session and vid = :vid2
		group by 1, 2),
	  -- Расход сырья по изделиям + остатки
	  prices0 as (
		select id_item,
			   n1 items,
			   n2 summa,
			   n3 summa_inv,
			   n5 items_in
		from session_edit
		where id_session = :id_session and vid = :vid9),
	  prices as (
		select id_item, items, summa, summa_inv, items_in from prices0
		  union all
		select p0.id_item,
			   p0.items_in,
			   1e0 * r.items * p.summa / nullif(p.items, 0),
			   1e0 * r.items * p.summa_inv / nullif(p.items, 0),
			   p0.items_in
		from prices0 p0
		join rashod r on r.i1 = p0.id_item
		join prices p on p.id_item = r.id_item),
	  total as (
		select id_item,
			   min(items_in) items_in,
			   sum(summa) summa,
			   sum(summa_inv) summa_inv,
			   min(items) items
		from prices
		group by id_item
		having min(items_in) is not null)
	  select :id_session, :vid23,
			 id_item,
			 items_in,
			 summa,
			 summa_inv,
			 items
	  from total;

	  insert into session_edit(id_session, vid, id_item, n1, n2, n3, n5, i1, i2)
	  with recursive
	  -- Расход по поличеству сырья на изделие
	  rashod as (
		select id_item,
			   i1,
			   sum(n1+n4+n5) items,
			   sum(n11) items_b,
			   sum(n13) items_prih
		from session_edit
		where id_session = :id_session and vid = :vid2
		group by 1, 2),
	  -- Расход сырья по изделиям + остатки
	  prices0 as (
		select s.id_item,
			   s.n1 items_in,
			   s.n2 summa,
			   s.n3 summa_inv,
			   s.n5 items,
			   s9.n6 items_b,
			   s9.n7 summa_b,
			   s9.n8 items_prih,
			   s9.n9 summa_prih,
			   s9.n10 items_p_in,
			   s9.n18 price_b
		from session_edit s
		join session_edit s9 on s9.id_session = s.id_session and s.id_item = s9.id_item and s9.vid = :vid9
	--  join item i on i.id = s.id_item and i.group_id = 2
		where s.id_session = :id_session and s.vid = :vid23),
	  prices as (
		select id_item, cast(null as int) i1, 0.0000 summa, 0.0000 summa_inv, items_b, summa_b, items_in, summa summa_in, summa_inv summa_inv_in, items_prih, summa_prih, 0.0000 items_r, 0.0000 summa_r, price_b from prices0
			union all
		select p0.id_item,
			   p.id_item,
			   1e0*r.items*p.summa_in/nullif(p.items_in, 0),
			   1e0*r.items*p.summa_inv_in/nullif(p.items_in, 0),
			   p0.items_b,
			   p0.summa_b,
			   0,
			   0,
			   0,
			   p0.items_prih,
			   p0.summa_prih,
			   r.items_b + r.items_prih + r.items,
			   coalesce(1e0*r.items_b * p.summa_b / nullif(p.items_b, 0), 0)+
			   coalesce(1e0*r.items_prih * p.summa_prih / nullif(p.items_prih, 0), 0)+
			   coalesce(1e0*r.items * p.summa_r / nullif(p.items_in, 0), 0),
			   p0.price_b
		from prices0 p0
		join rashod r on r.i1 = p0.id_item
		join prices p on p.id_item = r.id_item),
	  prices2 as (
		select id_item, items_in items, summa_in summa, summa_inv_in summa_inv, cast(summa_r as numeric(12,2)) summa_r, items_prih, summa_prih, items_b, summa_b, price_b, 1 vid from prices
		  union all
		select i1, -items_r, -summa, -summa_inv, -cast(summa_r as numeric(12,2)), null, null, null, null, null, -1 from prices where i1 is not null),
	  vipusk as (
		select id_item,
			   sum(items) items,
			   sum(summa) summa,
			   sum(summa_inv) summa_inv,
			   sum(summa_r) summa_r,
			   coalesce(nullif(coalesce(sum(summa+summa_r)/nullif(sum(items), 0), 0), 0), max(price_b)) price_ceh,
			   min(items_b) items_b,
			   min(summa_b) summa_b,
			   min(items_prih) items_prih,
			   min(summa_prih) summa_prih
		from prices2 p
		group by 1),
	  -- Расход по переработке
	  rashod2 as (
		select id_item,
			   i1,
			   n1 items,
			   n2 koef,
			   n11 items_b,
			   n13 items_prih
		from session_edit
		where id_session = :id_session and vid = :vid7),
	  -- Увеличение себестоимости по переработке
	  pererabotka as (
		select r.i1 id_item,
			   r.id_item id_item2,
			   min(p0.items_p_in) items,
			   sum(1e0 * r.items * r.koef * v.summa /nullif(v.items, 0)) summa,
			   sum(1e0 * r.items * r.koef * v.summa_inv / nullif(v.items, 0)) summa_inv,
			   sum(r.items +r.items_b +  r.items_prih) items_r,
			   cast(coalesce(sum(1e0 * r.items * r.koef * v.summa_r /nullif(v.items, 0)), 0)+
			   coalesce(sum(1e0 * r.items_b * r.koef * v.summa_b /nullif(v.items_b, 0)), 0)+
			   coalesce(sum(1e0 * r.items_prih * r.koef * v.summa_prih /nullif(v.items_prih, 0)), 0) +
			   coalesce(sum(iif(v.items = 0, 1e0 * r.items * r.koef * v.price_ceh, 0)), 0) as numeric(12,2)) summa_r
		from rashod2 r
		join vipusk v on r.id_item = v.id_item
		left join prices0 p0 on p0.id_item = r.i1
		group by 1, 2),
	  total as (
		select id_item, items, summa, summa_inv, summa_r, cast(null as int) id_item2, 0 vid
		from vipusk
		  union all
		select id_item, items, summa, summa_inv, summa_r, id_item2, 1 vid
		from pererabotka
		  union all
		select id_item2, -items_r, -summa, -summa_inv, -summa_r, id_item, 2 vid
		from pererabotka)
	  select :id_session, :vid24,
			 id_item,
			 items n1,
			 summa n2,
			 summa_inv n3,
			 summa_r n5,
			 id_item2 i1,
			 vid i2
	  from total;

	  insert into session_edit(id_session, vid, id_item, n1, n2, n3, n5, i1, i2)
	  with
		total as (
		  select id_item,
				 max(iif(i2 = 1, n1, 0)) + sum(iif(i2 = 1, 0, n1)) items,
				 sum(n2) summa,
				 sum(n3) summa_inv,
				 sum(n5) summa_r,
				 '~'||cast(substring(list(distinct i1, '~') from 1 for 20000) as varchar(20000)) sid_item2
		  from session_edit
		  where id_session = :id_session and vid = :vid24
		  group by 1),
		total1 as (
		  select id_item, summa, summa_inv, summa_r,
				 (select first 1 t.id_item from total t where t1.sid_item2 containing '~'||t.id_item||'~' and t.items <> 0 order by t.summa + t.summa_r desc) id_item2
		  from total t1
		  where items = 0 and (summa <> 0 or summa_inv <> 0 or summa_r <> 0)),
		total2 as (
		  select id_item2 id_item, summa, summa_inv, summa_r from total1
		  where id_item2 is not null
		  union all
		  select id_item, -summa, -summa_inv, -summa_r from total1
		  where id_item2 is not null)
	  select :id_session, :vid25,
			 id_item,
			 cast(null as numeric(12, 3)) items,
			 summa,
			 summa_inv n3,
			 summa_r n5,
			 cast(null as int) id_item2,
			 3 i2
	  from total2;

	  insert into session_edit(id_session, vid, id_item, n1, n2, n3, n6, n7, n8, n9, n10, n11, n12, n13, n14)
	  with
	  total as (
		select s.id_item,
			   max(iif(s.i2 = 1, s.n1, 0)) + sum(iif(s.i2 = 1, 0, s.n1)) items,
			   sum(s.n2) summa,
			   sum(s.n3) summa_inv,
			   sum(s.n5) summa_r,
			   min(s9.n6) items_b,
			   min(s9.n7) summa_b,
			   min(s9.n8) items_prih,
			   min(s9.n9) summa_prih,
			   min(s9.n10) items_p_in,
			   min(s9.n16) items_t,
			   min(s9.n17) summa_t,
			   min(s9.n18) price_b,
			   min(s9.n19) items_end
		from session_edit s
		join session_edit s9 on s9.id_session = s.id_session and s.id_item = s9.id_item and s9.vid = :vid9
		where s.id_session = :id_session and s.vid in (:vid24, :vid25)
		group by 1)
	  select :id_session, :vid20,
			 id_item,
			 cast(items as numeric(12,3)) n1,
			 cast(summa as numeric(12,2)) n2,
			 cast(summa_inv as numeric(12,2)) n3,
			 cast(summa_prih as numeric(12,2)) n6,
			 cast(items_prih as numeric(12,3)) n7,
			 cast(summa_b as numeric(12,2)) n8,
			 cast(items_b as numeric(12,3)) n9,
			 cast(summa_r as numeric(12,2)) n10,
			 cast(items_t as numeric(12,3)) n11,
			 cast(summa_t as numeric(12,2)) n12,
			 cast(price_b as numeric(12,4)) n13,
			 cast(items_end as numeric(12,3)) n14
	  from total;
	/*
	  merge into session_edit s
	  using (select s.id, i.group_id i2 from session_edit s
			 left join item i on i.id = s.id_item
			 where s.id_session = :id_session and s.vid in (:vid20, :vid23)) c on c.id = s.id
	  when matched then
		update set i2 = c.i2, n4 = 0;
	*/
	  -- Выравниваем сырье
	  for select sum(n2+n6+n7) summa,
				 sum(n6+n7) summa_inv
		  from session_edit
		  where id_session = :id_session and vid = :vid2
	  into summa, summa1 do
		for select sum(n2) summa, sum(n3) summa_inv
			from session_edit
			where id_session = :id_session and vid = :vid20
		into items, items1 do
		  update session_edit set
			n2 = n2 + :summa - :items,
			n3 = n3 + :summa1 - :items1
		  where id_session = :id_session and vid = :vid20
		  order by n2 desc
		  rows 1;

	  -- распределение переменных Н/Р (пропорционально сумме сырья)
	  for select sum(n1) from session_edit
		  where id_session = :id_session and vid = :vid99 and i1 = 2
		  having sum(n1) <> 0
	  into summa0 do
	  begin
		summa1 = 0;
		select sum(n2) from session_edit where id_session = :id_session and vid = :vid20 and i2 = 2
		into summa1;
		summa1 = nullif(summa1, 0);

		update session_edit set
		  n4 = cast(coalesce(1e0 * :summa0 * n2 / :summa1, 0) as numeric(12,2))
		where id_session = :id_session and vid in (:vid20, :vid23) and i2 = 2;

		summa2 = 0;
		select sum(n4) from session_edit where id_session = :id_session and vid = :vid20 and i2 = 2
		into summa2;

		if (summa2 <> summa0) then
		update session_edit set
		  n4 = n4 + :summa0 - :summa2
		where id_session = :id_session and vid = :vid20 and i2 = 2
		order by n4 desc
		rows 1;
	  end

	  update session_edit set
		n4 = 0
	  where id_session = :id_session and vid in (:vid20, :vid23) and i2 = 5;

	  -- распределение Н/Р по изделиям (пропорционально выбранному виду)
	  for select iif(i1 = 3, 2, 1) vid, sum(n1) from session_edit
		  where id_session = :id_session and vid = :vid99 and n1 <> 0 and i1 in (0,1,3)
		  group by 1
		  having sum(n1) <> 0
	  into k, summa0 do
	  for select num from get_rows(3)
	  into n do
	  begin
		summa1 = 0;
		select sum(decode(:n, 1, n2, 2, n5, 3, n6)) from session_edit
		where id_session = :id_session and vid = :vid10
		into summa1;

		type_v = (n-1)*2+k;

		if (summa1 <> 0) then
		insert into session_edit(id_session, vid, id_item, i1, n1)
		select id_session, :vid11, id_item, :type_v, cast(1e0*decode(:n, 1, n2, 2, n5, 3, n6)*:summa0/:summa1 as numeric(12,2))
		from session_edit
		where id_session = :id_session and vid = :vid10;

		summa2 = 0;
		select sum(n1) from session_edit where id_session = :id_session and vid = :vid11 and i1 = :type_v
		into summa2;

		if (summa2 <> summa0) then
		update session_edit set
		  n1 = n1 + :summa0 - :summa2
		where id_session = :id_session and vid = :vid11 and i1 = :type_v
		order by n1 desc
		rows 1;
	  end

	  -- Распределение Н/р по ГИ
	  merge into session_edit s
	  using (select id_item,
					sum(iif(i1 = 1, n1, 0)) n7,
					sum(iif(i1 = 2, n1, 0)) n8,
					sum(iif(i1 = 3, n1, 0)) n9,
					sum(iif(i1 = 4, n1, 0)) n10,
					sum(iif(i1 = 5, n1, 0)) n11,
					sum(iif(i1 = 6, n1, 0)) n12
			 from session_edit
			 where id_session = :id_session and vid = :vid11
			 group by 1) c on s.id_session = :id_session and s.id_item = c.id_item and s.vid = :vid10
	  when matched then
	  update set n7  = c.n7,
				 n8  = c.n8,
				 n9  = c.n9,
				 n10 = c.n10,
				 n11 = c.n11,
				 n12 = c.n12;

	  -- Расчет остатка на конец
	  if (ost = 1) then
	  begin
		for select s.id_item,
				   s23.n1 items,
				   s.n1-s23.n1 items_r,
				   s.n1,
				   s.n7 items_prih,
				   s.n9 items_b,
				   s.n14 items_end,
				   s12.n2 summa_end,
				   s.id
			from session_edit s
			left join session_edit s23 on s23.id_session = s.id_session and s23.id_item = s.id_item and s23.vid = :vid23
			left join session_edit s12 on s12.id_session = s.id_session and s12.id_item = s.id_item and s12.vid = :vid12
			where s.id_session = :id_session and s.vid = :vid20
		into id_item, items, items4, items5, items1, items0, items2, summa, id0 do
		begin
		  items3 = maxvalue(items2, 0);

		  if (items3 = 0) then
		  begin
			items = 0;
			items4 = 0;
		  end else
		  begin
			items5  = minvalue(items3, items5);
			items3 = items3 - items5;

			if (items5 <> items + items4) then
			begin
			  items = maxvalue(minvalue(items5, items), 0);
			  items4 = items5 - items;
			end
		  end

		  items1 = maxvalue(minvalue(items3, items1), 0);
		  items3 = items3 - items1;

		  items0 = maxvalue(minvalue(items3, items0), 0);
		  items3 = items3 - items0;

		  merge into session_edit s
		  using (select s.id, :items2 items_end,
						cast(coalesce(:summa,
							 coalesce(1e0 * :items * (s23.n2 + s23.n4) / nullif(s23.n1, 0), 0) +
							 coalesce(1e0 * :items4 * (s23.n2 + s23.n4 - s.n2 - s.n10 - s.n4) / nullif(s23.n1 - s.n1, 0), 0) +
							 coalesce(1e0 * :items1 * s.n6 / nullif(s.n7, 0), 0) +
							 coalesce(1e0 * :items0 * s.n8 / nullif(s.n9, 0), 0) +
							 coalesce(1e0 * :items3 * s.n13, 0)) as numeric(12,2)) summa_end
				 from session_edit s
				 left join session_edit s23 on s23.id_session = s.id_session and s23.id_item = s.id_item and s23.vid = :vid23
				 where s.id = :id0) c on c.id = s.id
		  when matched then
		  update set
			n14 = items_end,
			n15 = summa_end;
		end
	  end else
		update session_edit set
		  n14 = 0,
		  n15 = 0
		where id_session = :id_session and vid = :vid20;

	  suspend;
	end
	^
	set term ;^
	commit;

	set heading off;
	select 'Completed successfully.' from rdb$database;

"""

act = isql_act('db', test_script)

expected_stdout = """
    Completed successfully.
"""

@pytest.mark.version('>=3.0.5')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(charset='utf8')
    assert act.clean_stdout == act.clean_expected_stdout
