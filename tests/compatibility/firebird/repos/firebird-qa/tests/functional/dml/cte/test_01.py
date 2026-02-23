#coding:utf-8

"""
ID:          dml.cte-01
TITLE:       Non-Recursive CTEs
FBTEST:      functional.dml.cte.01
DESCRIPTION:
    Rules for Non-Recursive CTEs :
    * Multiple table expressions can be defined in one query
    * Any clause legal in a SELECT specification is legal in table expressions
    * Table expressions can reference one another
    * References between expressions should not have loops
    * Table expressions can be used within any part of the main query or another table expression
    * The same table expression can be used more than once in the main query
    * Table expressions (as subqueries) can be used in INSERT, UPDATE and DELETE statements
    * Table expressions are legal in PSQL code
    * WITH statements can not be nested
"""

import pytest
from firebird.qa import *

init_script = """
    create table employee( id_employee integer , prenom varchar(20) ,id_department integer,age integer ,  primary key(id_employee));
    create table department(id_department integer, name varchar(20));

    insert into department(id_department, name) values(1,'service compta');
    insert into department(id_department, name) values(2,'production');
    insert into employee(id_employee, prenom,id_department,age) values (1,'benoit',1 , 30 );
    insert into employee(id_employee, prenom,id_department,age) values (2,'ludivine',1 , 30 );
    insert into employee(id_employee, prenom,id_department,age) values (3,'michel',1 , 27 );
    insert into employee(id_employee, prenom,id_department,age) values (4,'gilbert',1 , 42 );
    insert into employee(id_employee, prenom,id_department,age) values (5,'tom',2 ,23);
    insert into employee(id_employee, prenom,id_department,age) values (6,'jacque',2,44 );
    insert into employee(id_employee, prenom,id_department,age) values (7,'justine',2,30 );
    insert into employee(id_employee, prenom,id_department,age) values (8,'martine',2,31 );
    insert into employee(id_employee, prenom,id_department,age) values (9,'noemie',2,39 );
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    set count on;
    with
    repartition_by_age as (
        select
            age/10 as trancheage
           ,id_department
           ,count(1) as nombre
        from employee
        group by age/10, id_department
    )
    select
        d.name
       ,jeune.nombre as jeune
       ,trentenaire.nombre as trentenaire
       ,quarentenaire.nombre as quantenaire
    from department d
    left join repartition_by_age jeune
        on d.id_department = jeune.id_department and jeune.trancheage = 2
    left join repartition_by_age trentenaire
        on d.id_department = trentenaire.id_department and trentenaire.trancheage = 3
    left join repartition_by_age quarentenaire
        on d.id_department = quarentenaire.id_department and quarentenaire.trancheage = 4 ;

"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    NAME service compta
    JEUNE 1
    TRENTENAIRE 2
    QUANTENAIRE 1
    
    NAME production
    JEUNE 1
    TRENTENAIRE 3
    QUANTENAIRE 1

    Records affected: 2
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
