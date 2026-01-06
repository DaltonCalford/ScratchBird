# **ScratchBird SQL Language Specification \- Master Document**

Welcome to the complete and authoritative documentation for the ScratchBird SQL dialect. This collection of documents provides a comprehensive guide to the language, from its high-level architecture and syntax down to detailed specifications for every command and feature.

The documentation is the result of analyzing and consolidating 18 source files into a cohesive, non-redundant, and logically structured set of specifications suitable for guiding the development of a context-aware parser and serving as the definitive language reference.

## **Guiding Principles**

The ScratchBird dialect is built on a set of core principles designed to create a powerful, flexible, and developer-friendly database system:

* **Domain-Centric Design**: Types are "smart" objects that carry their own validation, security, and integrity rules.  
* **Hierarchical Security**: A multi-layered model combining access control (Roles/Groups) with data-centric rules (Domains).  
* **Advanced Programmability**: A rich procedural language (PSQL) with unique features like autonomous transactions and universal cursors.  
* **Superior Developer Experience**: A context-aware parser, automatic documentation, and precise error reporting minimize friction.  
* **Performance and Compatibility**: Advanced optimization features and broad compatibility with major SQL dialects.

## **Master Table of Contents**

This master document serves as the primary navigation hub. The specification is divided into two main parts: the Core Language Specification, which defines *what* the language is, and the Appendices, which detail *how* certain internal or compatibility features are implemented.

### **Part I: Core Language Specification**

This section defines the abstract language and its features, independent of the underlying implementation.

* [**00\_GRAMMAR\_BNF.md**](00_GRAMMAR_BNF.md): The single, authoritative Backus-Naur Form (BNF) grammar for the entire language. This is the technical blueprint for the parser.  
* [**01\_SQL\_DIALECT\_OVERVIEW.md**](01_SQL_DIALECT_OVERVIEW.md): A high-level introduction to the dialect, its core principles, statement categories, and compatibility features.  
* [**02\_DDL\_STATEMENTS\_OVERVIEW.md**](02_DDL_STATEMENTS_OVERVIEW.md): An index and overview of all Data Definition Language (DDL) commands used to define and manage database object structures.  
  * **Detailed DDL Lifecycle Documents:**  
    * [DDL\_DATABASES.md](DDL_DATABASES.md)  
    * [DDL\_SCHEMAS.md](DDL_SCHEMAS.md)  
    * [DDL\_TABLES.md](DDL_TABLES.md)  
    * [DDL\_TABLE\_PARTITIONING.md](DDL_TABLE_PARTITIONING.md)  
    * [DDL\_TEMPORAL\_TABLES.md](DDL_TEMPORAL_TABLES.md)  
    * [DDL\_INDEXES.md](DDL_INDEXES.md)  
    * [DDL\_VIEWS.md](DDL_VIEWS.md)  
    * [DDL\_SEQUENCES.md](DDL_SEQUENCES.md)  
    * [DDL\_DOMAINS.md](DDL_DOMAINS.md)  
    * [DDL\_FUNCTIONS.md](DDL_FUNCTIONS.md)  
    * [DDL\_PROCEDURES.md](DDL_PROCEDURES.md)  
    * [DDL\_TRIGGERS.md](DDL_TRIGGERS.md)  
    * [DDL\_ROLES\_AND\_GROUPS.md](DDL_ROLES_AND_GROUPS.md)  
    * [DDL\_EXCEPTIONS.md](DDL_EXCEPTIONS.md)  
    * [DDL\_PACKAGES.md](DDL_PACKAGES.md)  
    * [DDL\_EVENTS.md](DDL_EVENTS.md)  
    * [DDL\_USER\_DEFINED\_RESOURCES.md](DDL_USER_DEFINED_RESOURCES.md)  
    * [DDL\_FOREIGN\_DATA.md](DDL_FOREIGN_DATA.md)  
* [**03\_TYPES\_AND\_DOMAINS.md**](03_TYPES_AND_DOMAINS.md): A cornerstone document detailing the complete type system, from primitive types to the advanced, rule-based DOMAIN object and the polymorphic VARIANT type.  
* [**EXTRACT\_AND\_ALTER\_ELEMENT.md**](EXTRACT_AND_ALTER_ELEMENT.md): Component access and update semantics for EXTRACT and ALTER_ELEMENT across all supported types.  
* [**04\_DML\_STATEMENTS\_OVERVIEW.md**](04_DML_STATEMENTS_OVERVIEW.md): An index and overview of all Data Manipulation Language (DML) commands used to query and modify data.  
  * **Detailed DML Command Documents:**  
    * [DML\_SELECT.md](DML_SELECT.md)  
    * [DML\_INSERT.md](DML_INSERT.md)  
    * [DML\_UPDATE.md](DML_UPDATE.md)  
    * [DML\_DELETE.md](DML_DELETE.md)  
    * [DML\_MERGE.md](DML_MERGE.md)  
    * [DML\_XML\_JSON\_TABLES.md](DML_XML_JSON_TABLES.md)  
* [**05\_PSQL\_PROCEDURAL\_LANGUAGE.md**](05_PSQL_PROCEDURAL_LANGUAGE.md): A comprehensive guide to the database's programmability features, including variables, control flow, cursors, exception handling, EXECUTE BLOCK, and autonomous transactions.  
* [**06\_SECURITY\_MODEL.md**](06_SECURITY_MODEL.md): The authoritative specification for the multi-layered security architecture, covering access control (Roles, Schema Permissions), data-centric security (Domains), and procedural security.  
  * **Supporting Security Documents:**  
    * [DDL\_ROW\_LEVEL\_SECURITY.md](06_SECURITY/DDL_ROW_LEVEL_SECURITY.md)  
* [**07\_TRANSACTION\_AND\_SESSION\_CONTROL.md**](07_TRANSACTION_AND_SESSION_CONTROL.md): A complete reference for Transaction Control Language (TCL), concurrency control (LOCK, FOR UPDATE), and session management (SET, SHOW).  
* [**08\_PARSER\_AND\_DEVELOPER\_EXPERIENCE.md**](08_PARSER_AND_DEVELOPER_EXPERIENCE.md): Defines the "intelligent" features of the parser that create a superior developer experience, including context-aware parsing, automatic documentation via comments, and precise error reporting.

### **Part II: Appendices (Implementation Details)**

This section contains low-level technical specifications that describe the internal workings of the ScratchBird engine.

* [**Appendix\_A\_SBLR\_BYTECODE.md**](Appendix_A_SBLR_BYTECODE.md): The complete, low-level specification for the ScratchBird Bytecode Language Representation (SBLR), the virtual machine language that executes compiled PSQL code.  
* [**Appendix\_B\_POSTGRESQL\_EMULATION.md**](Appendix_B_POSTGRESQL_EMULATION.md): A technical guide to the PostgreSQL compatibility layer, detailing the emulation of the wire protocol, system catalogs, and dialect-specific features.
