#coding:utf-8

"""
ID:          dml.cte-02
TITLE:       Recursive CTEs
FBTEST:      functional.dml.cte.02
DESCRIPTION:
    Rules for Recursive CTEs
    * A recursive CTE is self-referencing (has a reference to itself)
    * A recursive CTE is a UNION of recursive and non-recursive members:
    * At least one non-recursive member (anchor) must be present
    * Non-recursive members are placed first in the UNION
    * Recursive members are separated from anchor members and from one another with UNION ALL clauses, i.e.,
         non-recursive member (anchor)
          UNION [ALL | DISTINCT]
          non-recursive member (anchor)
          UNION [ALL | DISTINCT]
          non-recursive member (anchor)
          UNION ALL
          recursive member
          UNION ALL
          recursive member

    References between CTEs should not have loops
    Aggregates (DISTINCT, GROUP BY, HAVING) and aggregate functions (SUM, COUNT, MAX etc) are not allowed in recursive members
    A recursive member can have only one reference to itself and only in a FROM clause
    A recursive reference cannot participate in an outer join
"""

import pytest
from firebird.qa import *

init_script = """
    create table product( id_product integer , name varchar(20) ,id_type_product integer,  primary key(id_product));
    create table type_product(id_type_product integer, name varchar(20),id_sub_type integer);
    insert into type_product(id_type_product,name,id_sub_type) values(1,'dvd',null);
    insert into type_product(id_type_product,name,id_sub_type) values(2,'book',null);
    insert into type_product(id_type_product,name,id_sub_type) values(3,'film sf',1);
    insert into type_product(id_type_product,name,id_sub_type) values(4,'film action',1);
    insert into type_product(id_type_product,name,id_sub_type) values(5,'film romance',1);
    insert into product(id_product, name,id_type_product) values (1,'harry potter 8',3  );
    insert into product(id_product, name,id_type_product) values (2,'total recall',3  );
    insert into product(id_product, name,id_type_product) values (3,'kingdom of heaven',3  );
    insert into product(id_product, name,id_type_product) values (4,'desperate housewives',5  );
    insert into product(id_product, name,id_type_product) values (5,'reign over me',5  );
    insert into product(id_product, name,id_type_product) values (6,'prison break',4  );
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    set count on;
    with recursive
    type_product_recur (id_type_product,name,father) as (
        select id_type_product, '+ ' || name as name, id_type_product as father
        from type_product
        where type_product.id_sub_type is null
        
        UNION ALL
        
        select t.id_type_product, ' - ' || t.name, tr.id_type_product as father
        from type_product t
        join type_product_recur tr on tr.id_type_product = t.id_sub_type
    ),
    count_by_type as (
        select p.id_type_product,count(id_product) as count_p from product p
        group by p.id_type_product
        
        UNION

        select tp.father, count(id_product) as count_p from
        type_product_recur tp, product p
        where tp.id_type_product = p.id_type_product
        group by tp.father
    )
    select  t.id_type_product, t.name, c.count_p
    from type_product_recur t
    left join  count_by_type c
    on c.id_type_product = t.id_type_product
    ;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    ID_TYPE_PRODUCT 1
    NAME + dvd
    COUNT_P 6
    
    ID_TYPE_PRODUCT 3
    NAME - film sf
    COUNT_P 3
    
    ID_TYPE_PRODUCT 4
    NAME - film action
    COUNT_P 1
    
    ID_TYPE_PRODUCT 5
    NAME - film romance
    COUNT_P 2
    
    ID_TYPE_PRODUCT 2
    NAME + book
    COUNT_P <null>

    Records affected: 5
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
