#coding:utf-8

"""
ID:          issue-4998
ISSUE:       4998
TITLE:       DISTINCT vs non-DISTINCT affects the result of IN subquery
DESCRIPTION:
JIRA:        CORE-4690
FBTEST:      bugs.core_4690
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table comi_ingr (id int);
    recreate table comidas(id int);
    recreate table ingredientes(id int);
    recreate table paises(id int);
    commit;

    set term ^;
    execute block as
    begin
      begin execute statement 'drop domain dcad30'; when any do begin end end
      begin execute statement 'drop domain dcodingrediente'; when any do begin end end
      begin execute statement 'drop domain dreal'; when any do begin end end
      begin execute statement 'drop domain dtipoingre'; when any do begin end end
      begin execute statement 'drop domain dcodpais'; when any do begin end end
      begin execute statement 'drop domain dentero'; when any do begin end end
      begin execute statement 'drop domain dcad50'; when any do begin end end
      begin execute statement 'drop domain dcodcomida'; when any do begin end end
      begin execute statement 'drop domain dfecha'; when any do begin end end
      begin execute statement 'drop domain dcodcoming'; when any do begin end end
      begin execute statement 'drop domain dcodreceta'; when any do begin end end
    end
    ^
    set term ;^
    commit;

    create domain dcad30 as varchar(30) default '';
    create domain dcodingrediente as  integer;
    create domain dreal as decimal(7, 2) default 0;
    create domain dtipoingre as   char(1)   default 'v'   check (value in ('v','a','m'));
    create domain dcodpais as   integer;
    create domain dentero as   integer   default 0;
    create domain dcad50 as   varchar(50)   default '';
    create domain dcodcomida as integer;
    create domain dfecha as date;
    create domain dcodcoming as integer;
    create domain dcodreceta as   integer;
    commit;

    recreate table ingredientes (
      cod_ingrediente dcodingrediente not null,
      nombre dcad30 not null,
      tipo dtipoingre not null,
      imp_unidad dreal not null,
      constraint pk_ingredientes primary key (cod_ingrediente)
    );

    recreate table paises (
      cod_pais dcodpais not null,
      nombre dcad30 not null,
      n_habitantes dentero not null,
      constraint pk_paises primary key (cod_pais)
    );

    recreate table comidas (
      cod_comida dcodcomida not null,
      cod_pais dcodpais not null,
      nombre dcad50 not null,
      fecha_creacion dfecha not null,
      precio dreal not null,
      fecha_ult dfecha,
      constraint pk_comidas primary key (cod_comida),
      constraint fk_comidas1 foreign key (cod_pais) references paises(cod_pais) on update cascade
    );


    recreate table comi_ingr (
      cod_comi_ingr dcodcoming not null,
      cod_ingrediente dcodingrediente not null,
      cod_comida dcodcomida not null,
      n_unidades dreal not null,
      constraint pk_comi_ingr primary key (cod_comi_ingr),
      constraint fk_comi_ingr1 foreign key (cod_ingrediente) references ingredientes(cod_ingrediente) on update cascade,
      constraint fk_comi_ingr2 foreign key (cod_comida) references comidas(cod_comida) on update cascade
    );
    commit;

    insert into ingredientes values (1, 'tomate', 'v', 1);
    insert into ingredientes values (2, 'sal', 'm', 0.5);
    insert into ingredientes values (3, 'magro', 'a', 5);
    insert into ingredientes values (4, 'pimiento verde', 'v', 0.8);
    insert into ingredientes values (5, 'lomo', 'a', 6.5);
    commit;


    insert into paises values (1, 'espana', 48000000);
    insert into paises values (2, 'marruecos', 33000000);
    insert into paises values (3, 'eeuu', 300000000);
    insert into paises values (4, 'francia', 66000000);
    commit;


    insert into comidas values (4, 1, 'migas', '2014-12-13', 3, null);
    insert into comidas values (5, 3, 'chuletones a la brasa', '2015-02-01', 10, '2015-02-11');
    insert into comidas values (1, 1, 'estofado de cerdo', '2006-09-24', 7, '2014-11-06');
    insert into comidas values (2, 1, 'pisto', '2014-09-12', 4, '2014-12-09');
    insert into comidas values (3, 2, 'cuscus', '2014-09-12', 4.5, '2014-12-02');
    commit;


    insert into comi_ingr values (8, 3, 5, 0.01);
    insert into comi_ingr values (7, 2, 5, 1);
    insert into comi_ingr values (1, 1, 1, 0.25);
    insert into comi_ingr values (2, 3, 1, 1);
    insert into comi_ingr values (3, 2, 1, 0.05);
    insert into comi_ingr values (4, 1, 2, 0.5);
    insert into comi_ingr values (5, 4, 2, 0.5);
    insert into comi_ingr values (6, 2, 3, 0.01);
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    select a.cod_ingrediente as ca, b.cod_ingrediente as cb
    from (
        select *
        from ingredientes i
        where
            i.cod_ingrediente <
                all
                (
                    select cod_pais -- 2 3 4
                    from paises
                    where nombre != 'espana'
                )
            and
            i.cod_ingrediente
            in
            (
                select ci.cod_ingrediente -- 1 3 2 1 4
                from comi_ingr ci
                join comidas c on (ci.cod_comida=c.cod_comida)
                join paises p on (p.cod_pais=c.cod_pais)
                where p.nombre='espana'
            )
    ) a
    natural full join
    (
        select i.*
        from ingredientes i
        where
            i.cod_ingrediente <
                all
                (
                    select cod_pais
                    from paises
                    where nombre != 'espana'
                )
            and
            i.cod_ingrediente
                in
                (
                    select DISTINCT
                        ci.cod_ingrediente -- 1 2 3 4
                    from comi_ingr ci
                        join comidas c on (ci.cod_comida=c.cod_comida)
                        join paises p on (p.cod_pais=c.cod_pais)
                    where p.nombre='espana'
                )
    ) b
    ;
    set list off;
"""

act = isql_act('db', test_script)

expected_stdout = """
    CA                              1
    CB                              1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

