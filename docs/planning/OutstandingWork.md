**HIGH PRIORITY - Cross-Dialect Core Features**                                                                                                                                  

1. Table Partitioning (All dialects)                                                                                                                                           
- Native parser records PARTITION BY, but semantic/executor + catalog routing missing                                                                                          
- PostgreSQL/MySQL DDL support absent (PG parser has no PARTITION DDL; MySQL rejects partition options)                                                                        
- Partition management (ADD/DROP PARTITION) not implemented                                                                                                                    
- Files: native/02, postgresql/02, mysql/02, firebirdsql/02                                                                                                                    
2. Table Inheritance (Native/PostgreSQL)                                                                                                                                       
- INHERITS clause parsed but inheritance behavior not implemented in executor                                                                                                  
- Files: native/02, postgresql/02                                                                                                                                              
3. CREATE TABLE AS SELECT (CTAS) (Native)                                                                                                                                      
- Not parsed in V2 parser                                                                                                                                                      
- Syntax: CREATE TABLE <name> AS SELECT ... [WITH [NO] DATA]                                                                                                                   
- File: native/02                                                                                                                                                              

---                                                                                                                                                                            

**MEDIUM PRIORITY - MySQL Parser Gaps**                                                                                                                                          

Security DCL (All missing)                                                                                                                                                     

- CREATE USER / ALTER USER / DROP USER                                                                                                                                         
- CREATE ROLE / DROP ROLE                                                                                                                                                      
- GRANT (privileges and roles)                                                                                                                                                 
- REVOKE                                                                                                                                                                       
- SET ROLE                                                                                                                                                                     
- SHOW GRANTS                                                                                                                                                                  
- File: mysql/09                                                                                                                                                               

DDL Operations                                                                                                                                                                 

- DROP PROCEDURE / DROP FUNCTION / DROP TRIGGER                                                                                                                                
- ALTER PROCEDURE / ALTER FUNCTION                                                                                                                                             
- ALTER VIEW                                                                                                                                                                   
- ALTER TABLE ... ADD/DROP CONSTRAINT                                                                                                                                   
- Files: mysql/05, mysql/03, mysql/02                                                                                                                                          

Utility Commands                                                                                                                                                                

- SHOW TABLE STATUS, SHOW PROCESSLIST, SHOW VARIABLES, SHOW STATUS                                                                                                             
- SHOW WARNINGS / SHOW ERRORS                                                                                                                                                  
- EXPLAIN / EXPLAIN ANALYZE                                                                                                                                                    
- KILL, FLUSH, LOCK/UNLOCK TABLES                                                                                                                                              
- ANALYZE TABLE, OPTIMIZE TABLE, CHECK TABLE, REPAIR TABLE                                                                                                                     
- File: mysql/11                                                                                                                                                               

Dynamic SQL                                                                                                                                                                    

- PREPARE / EXECUTE / DEALLOCATE PREPARE                                                                                                                                       
- File: mysql/05                                                                                                                                                               

DML Features                                                                                                                                                                   

- Window functions (CTEs work in PostgreSQL but not MySQL)                                                                                                                     
- Multi-table DELETE                                                                                                                                                           
- Full-text search (MATCH ... AGAINST)                                                                                                                                         
- File: mysql/06, mysql/07                                                                                                                                                     

---                                                                                                                                                                            

MEDIUM PRIORITY - PostgreSQL Parser Gaps                                                                                                                                       

Security Features                                                                                                                                                              

- ALTER DEFAULT PRIVILEGES                                                                                                                                                     
- Column-level GRANT/REVOKE                                                                                                                                                    
- File: postgresql/09                                                                                                                                                          

Index Features                                                                                                                                                                 

- Expression indexes (explicitly errors: "not supported in current bytecode")                                                                                                  
- TABLESPACE clauses (explicitly rejected)                                                                                                                                     
- File: postgresql/03                                                                                                                                                          

Other                                                                                                                                                                          

- DISTINCT ON (parsed but ON list discarded)                                                                                                                                   
- Range types (CREATE TYPE RANGE rejected)                                                                                                                               
- Files: postgresql/06, postgresql/04                                                                                                                                          

---                                                                                                                                                                            

MEDIUM PRIORITY - Firebird Parser Gaps                                                                                                                                         

Index/Sequence Operations                                                                                                                                                      

- ALTER SEQUENCE (RESTART WITH, INCREMENT BY)                                                                                                                                  
- File: firebirdsql/03                                                                                                                                                         

DML                                                                                                                                                                            

- UPDATE OR INSERT - UPDATE path compiled as ON CONFLICT UPDATE, verify parity semantics                                                                                                     
- File: firebirdsql/07                                                                                                                                                         

ALTER DATABASE                                                                                                                                                                 

- Only ALIAS ADD/DROP supported                                                                                                                                                
- Missing: OWNER, RENAME, SET DEFAULT CHARACTER SET                                                                                                                            
- File: firebirdsql/01                                                                                                                                                         

Functions                                                                                                                                                                      

- LOCALTIME, LOCALTIMESTAMP, TODAY, YESTERDAY, TOMORROW                                                                                                                        
- RDB$GET_CONTEXT, DATEADD                                                                                                                                                     
- File: firebirdsql/14                                                                                                                                                         

---                                                                                                                                                                            

LOWER PRIORITY - Native V2 Parser Gaps                                                                                                                                         

Operators                                                                                                                                                                      

- Unary +                                                                                                                                                                      
- Power ^ (use POWER() function instead)                                                                                                                                       
- JSON operators: ?, ?|, ?&                                                                                                                                                    
- Array operators: @>, <@, &&, [] subscript                                                                                                                                    
- Bitwise operators: &, |, ^, ~, <<, >>                                                                                                                                        
- File: native/12                                                                                                                                                              

ALTER TABLE Subcommands                                                                                                                                                        

- ALTER COLUMN SET STATISTICS                                                                                                                                                  
- ALTER COLUMN SET STORAGE                                                                                                                                                     
- ENABLE/DISABLE TRIGGER                                                                                                                                                       
- INHERIT/NO INHERIT                                                                                                                                                           
- VALIDATE CONSTRAINT                                                                                                                                                          
- File: native/02                                                                                                                                                              

Views                                                                                                                                                                          

- INSTEAD OF triggers for views                                                                                                                                                
- Security barrier views                                                                                                                                                       
- File: native/03                                                                                                                                                              

Sequences                                                                                                                                                                      

- Distributed sequence generation                                                                                                                                              
- File: native/03                                                                                                                                                              

Session Variables                                                                                                                                                              

- CURRENT_USER, CURRENT_ROLE, CURRENT_CONNECTION, CURRENT_TRANSACTION                                                                                                          
- File: native/12                                                                                                                                                              

---                                                                                                                                                                            

TABLESPACE (All Emulated Dialects)                                                                                                                                             

- CREATE/ALTER/DROP TABLESPACE explicitly not supported                                                                                                                        
- PostgreSQL/MySQL parsers reject with error                                                                                                                                   
- Firebird doesn't have TABLESPACE concept                                                                                                                                     
- Files: postgresql/01, mysql/01                                                                                                                                               

---                                                                                                                                                                            

System Catalog Gaps                                                                                                                                                            

MySQL                                                                                                                                                                          

- mysql.db, privilege tables not emulated                                                                                                                                      
- performance_schema.* not collecting metrics                                                                                                                                  
- information_schema.ROUTINES/TRIGGERS limited                                                                                                                                 
- File: mysql/13                                                                                                                                                               

PostgreSQL                                                                                                                                                                     

- pg_stat_* statistics tables not populated beyond pg_stat_activity/pg_stat_user_tables                                                                                                                                   
- File: postgresql/13                                                                                                                                                          

---                                                                                                                                                                            

Quick Wins (Easy to Implement)                                                                                                                                                 

1. MySQL DROP PROCEDURE/FUNCTION/TRIGGER - Just add cases to parseDropStmt()                                                                                                   
2. MySQL SHOW VARIABLES/STATUS - Add dispatch cases                                                                                                                            
3. Unary + operator - Trivial to add to expression parser                                                                                                                      
4. MySQL EXPLAIN - Basic version could emit query plan info   
