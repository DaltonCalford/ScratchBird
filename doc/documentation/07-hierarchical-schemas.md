# Hierarchical Schema System 🟡

ScratchBird's hierarchical schema system is a revolutionary feature that extends traditional flat database schemas into nested, tree-like structures. This enables PostgreSQL-style organization with syntax like `company.finance.accounting.reports`, providing unprecedented database organization capabilities.

## 🌳 Schema Hierarchy Concept

### **Traditional vs. Hierarchical Schemas**

**Traditional Flat Schemas (Firebird/PostgreSQL):**
```sql
-- Limited to single-level organization
CREATE SCHEMA finance;
CREATE SCHEMA accounting;
CREATE SCHEMA reports;
-- No relationship between schemas
```

**ScratchBird Hierarchical Schemas:**
```sql
-- Nested, logical organization
CREATE SCHEMA company;
CREATE SCHEMA company.finance;
CREATE SCHEMA company.finance.accounting;
CREATE SCHEMA company.finance.accounting.reports;
-- Clear parent-child relationships
```

### **Benefits of Hierarchical Organization**
- **Logical Structure**: Mirror your organizational hierarchy
- **Namespace Management**: Avoid naming conflicts with deep nesting
- **Access Control**: Inherit permissions down the hierarchy
- **Easy Navigation**: Intuitive path-based object location
- **Scalability**: Support up to 8 levels deep with 511-character paths

---

## 🏗️ Creating Hierarchical Schemas

### **Basic Schema Creation**
```sql
-- Create root schema
CREATE SCHEMA company;

-- Create child schema
CREATE SCHEMA company.finance;

-- Create grandchild schema
CREATE SCHEMA company.finance.accounting;

-- Create great-grandchild schema
CREATE SCHEMA company.finance.accounting.reports;
```

### **Complex Hierarchy Example**
```sql
-- Enterprise organizational structure
CREATE SCHEMA enterprise;
CREATE SCHEMA enterprise.operations;
CREATE SCHEMA enterprise.operations.manufacturing;
CREATE SCHEMA enterprise.operations.manufacturing.plant_a;
CREATE SCHEMA enterprise.operations.manufacturing.plant_a.quality_control;

-- Financial hierarchy
CREATE SCHEMA enterprise.finance;
CREATE SCHEMA enterprise.finance.accounting;
CREATE SCHEMA enterprise.finance.accounting.receivables;
CREATE SCHEMA enterprise.finance.accounting.payables;
CREATE SCHEMA enterprise.finance.budgeting;
CREATE SCHEMA enterprise.finance.budgeting.quarterly;
CREATE SCHEMA enterprise.finance.budgeting.annual;

-- Human resources hierarchy
CREATE SCHEMA enterprise.hr;
CREATE SCHEMA enterprise.hr.recruiting;
CREATE SCHEMA enterprise.hr.payroll;
CREATE SCHEMA enterprise.hr.benefits;
CREATE SCHEMA enterprise.hr.training;
```

### **Schema Validation and Constraints**
```sql
-- ScratchBird automatically validates:
-- 1. Parent schema must exist before creating child
-- 2. Maximum depth of 8 levels
-- 3. Maximum path length of 511 characters
-- 4. Circular references are prevented

-- This will fail - parent doesn't exist:
CREATE SCHEMA nonexistent.child.schema;  -- ERROR

-- This works - proper hierarchy:
CREATE SCHEMA parent;
CREATE SCHEMA parent.child;
CREATE SCHEMA parent.child.grandchild;
```

---

## 📋 Working with Schema Hierarchy

### **Schema Navigation Commands**
```sql
-- Show all schemas in hierarchy
SHOW SCHEMAS;

-- Show schema hierarchy tree
SHOW SCHEMA HIERARCHY;

-- Show schemas at specific level
SHOW SCHEMAS LEVEL 2;

-- Show child schemas of specific parent
SHOW SCHEMAS UNDER 'company.finance';
```

### **Setting Working Schema**
```sql
-- Set current working schema
SET SCHEMA 'company.finance.accounting';

-- Now you can reference objects without full path
SELECT * FROM transactions;  -- References company.finance.accounting.transactions

-- Set home schema (default for new connections)
SET HOME SCHEMA 'company.finance';

-- Show current schema settings
SHOW SCHEMA;
SHOW HOME SCHEMA;
```

### **Schema Path Resolution**
ScratchBird provides intelligent path resolution:

```sql
-- Current schema: company.finance.accounting

-- Absolute path (starts from root)
SELECT * FROM company.hr.employees;

-- Relative path (relative to current schema)
SELECT * FROM reports.monthly_summary;  -- Resolves to company.finance.accounting.reports.monthly_summary

-- Parent reference
SELECT * FROM ../budgets.annual_budget;  -- Resolves to company.finance.budgets.annual_budget

-- Sibling reference
SELECT * FROM ../payroll.salary_data;  -- Resolves to company.finance.payroll.salary_data
```

---

## 🗂️ Objects in Hierarchical Schemas

### **Creating Tables in Schemas**
```sql
-- Set working schema
SET SCHEMA 'company.finance.accounting';

-- Create table in current schema
CREATE TABLE transactions (
    transaction_id INTEGER PRIMARY KEY,
    amount DECIMAL(15,2),
    transaction_date DATE,
    description VARCHAR(200)
);

-- Create table with explicit schema path
CREATE TABLE company.finance.accounting.reports.monthly_summary (
    month_year VARCHAR(7) PRIMARY KEY,
    total_revenue DECIMAL(15,2),
    total_expenses DECIMAL(15,2),
    net_profit DECIMAL(15,2)
);

-- Create table in sibling schema
CREATE TABLE ../budgets.annual_budget (
    budget_year INTEGER PRIMARY KEY,
    allocated_amount DECIMAL(15,2),
    spent_amount DECIMAL(15,2) DEFAULT 0
);
```

### **Cross-Schema References**
```sql
-- Foreign key across schema hierarchy
CREATE TABLE company.finance.accounting.invoice_lines (
    line_id INTEGER PRIMARY KEY,
    invoice_id INTEGER,
    product_id INTEGER,
    quantity INTEGER,
    unit_price DECIMAL(10,2),
    
    -- Reference to different schema
    FOREIGN KEY (product_id) REFERENCES company.operations.manufacturing.products(product_id)
);

-- Views spanning multiple schemas
CREATE VIEW company.finance.reports.comprehensive_revenue AS
SELECT 
    t.transaction_date,
    t.amount,
    p.product_name,
    c.customer_name
FROM company.finance.accounting.transactions t
JOIN company.operations.manufacturing.products p ON t.product_id = p.product_id
JOIN company.sales.customers c ON t.customer_id = c.customer_id;
```

### **Procedures and Functions**
```sql
-- Stored procedure in specific schema
SET SCHEMA 'company.finance.accounting';

CREATE PROCEDURE calculate_monthly_totals(
    input_month VARCHAR(7)
)
RETURNS (
    total_revenue DECIMAL(15,2),
    total_expenses DECIMAL(15,2),
    net_profit DECIMAL(15,2)
)
AS
BEGIN
    -- Access tables in current schema
    SELECT SUM(amount) FROM transactions 
    WHERE transaction_type = 'REVENUE' 
    AND EXTRACT(YEAR FROM transaction_date) || '-' || LPAD(EXTRACT(MONTH FROM transaction_date), 2, '0') = :input_month
    INTO :total_revenue;
    
    -- Access tables in related schema
    SELECT SUM(amount) FROM ../expenses.expense_records
    WHERE EXTRACT(YEAR FROM expense_date) || '-' || LPAD(EXTRACT(MONTH FROM expense_date), 2, '0') = :input_month
    INTO :total_expenses;
    
    net_profit = total_revenue - total_expenses;
    
    SUSPEND;
END;
```

---

## 🔐 Security and Permissions

### **Hierarchical Permission Inheritance**
```sql
-- Grant permissions on parent schema
GRANT USAGE ON SCHEMA company.finance TO finance_team;

-- Automatically inherits to child schemas:
-- - company.finance.accounting
-- - company.finance.budgeting
-- - company.finance.reports

-- Grant specific permissions on child schema
GRANT SELECT, INSERT, UPDATE ON ALL TABLES IN SCHEMA company.finance.accounting TO accountants;

-- Override parent permissions
GRANT ALL ON SCHEMA company.finance.accounting.confidential TO senior_accountants;
REVOKE ALL ON SCHEMA company.finance.accounting.confidential FROM finance_team;
```

### **Role-Based Schema Access**
```sql
-- Create roles for different organizational levels
CREATE ROLE company_viewer;
CREATE ROLE finance_manager;
CREATE ROLE accounting_clerk;
CREATE ROLE senior_accountant;

-- Grant hierarchical access
GRANT USAGE ON SCHEMA company TO company_viewer;
GRANT ALL ON SCHEMA company.finance TO finance_manager;
GRANT SELECT, INSERT, UPDATE ON SCHEMA company.finance.accounting TO accounting_clerk;
GRANT ALL ON SCHEMA company.finance.accounting TO senior_accountant;

-- Assign roles to users
GRANT company_viewer TO all_employees;
GRANT finance_manager TO 'john.doe';
GRANT accounting_clerk TO 'jane.smith';
GRANT senior_accountant TO 'bob.wilson';
```

### **Schema-Level Auditing**
```sql
-- Enable auditing for specific schema hierarchy
CREATE AUDIT TRAIL FOR SCHEMA company.finance.accounting;

-- Audit specific operations
CREATE AUDIT TRAIL FOR SCHEMA company.finance 
ON INSERT, UPDATE, DELETE 
TO audit_log_table;

-- View audit information
SELECT * FROM RDB$AUDIT_TRAIL 
WHERE RDB$SCHEMA_PATH LIKE 'company.finance%'
ORDER BY RDB$AUDIT_TIMESTAMP DESC;
```

---

## 🔧 Advanced Schema Operations

### **Schema Migration and Reorganization**
```sql
-- Move objects between schemas
ALTER TABLE company.temp.old_data 
SET SCHEMA company.finance.accounting.historical;

-- Rename schema (maintains hierarchy)
ALTER SCHEMA company.finance.accounting.old_reports 
RENAME TO company.finance.accounting.archived_reports;

-- Move entire schema branch
ALTER SCHEMA company.operations.old_manufacturing
MOVE TO company.archive.operations.manufacturing;
```

### **Schema Templates and Automation**
```sql
-- Create schema template
CREATE SCHEMA TEMPLATE department_template AS (
    CREATE TABLE employees (
        emp_id INTEGER PRIMARY KEY,
        name VARCHAR(100),
        hire_date DATE
    ),
    
    CREATE TABLE budget (
        budget_year INTEGER PRIMARY KEY,
        allocated_amount DECIMAL(15,2)
    ),
    
    CREATE VIEW current_employees AS
    SELECT * FROM employees WHERE status = 'ACTIVE'
);

-- Apply template to new schema
CREATE SCHEMA company.sales FROM TEMPLATE department_template;
CREATE SCHEMA company.marketing FROM TEMPLATE department_template;
```

### **Schema Monitoring and Statistics**
```sql
-- Get schema hierarchy statistics
SELECT 
    schema_path,
    schema_level,
    table_count,
    object_count,
    total_size_mb
FROM RDB$SCHEMA_HIERARCHY_STATS
ORDER BY schema_level, schema_path;

-- Monitor schema usage
SELECT 
    schema_path,
    access_count,
    last_access_time,
    active_connections
FROM RDB$SCHEMA_USAGE_STATS
WHERE access_count > 0
ORDER BY access_count DESC;
```

---

## 💼 Real-World Examples

### **E-commerce Platform Schema**
```sql
-- Complete e-commerce hierarchy
CREATE SCHEMA ecommerce;

-- Customer management
CREATE SCHEMA ecommerce.customers;
CREATE SCHEMA ecommerce.customers.profiles;
CREATE SCHEMA ecommerce.customers.preferences;
CREATE SCHEMA ecommerce.customers.loyalty;

-- Product management
CREATE SCHEMA ecommerce.products;
CREATE SCHEMA ecommerce.products.catalog;
CREATE SCHEMA ecommerce.products.inventory;
CREATE SCHEMA ecommerce.products.pricing;
CREATE SCHEMA ecommerce.products.reviews;

-- Order processing
CREATE SCHEMA ecommerce.orders;
CREATE SCHEMA ecommerce.orders.cart;
CREATE SCHEMA ecommerce.orders.checkout;
CREATE SCHEMA ecommerce.orders.fulfillment;
CREATE SCHEMA ecommerce.orders.returns;

-- Analytics and reporting
CREATE SCHEMA ecommerce.analytics;
CREATE SCHEMA ecommerce.analytics.sales;
CREATE SCHEMA ecommerce.analytics.customer_behavior;
CREATE SCHEMA ecommerce.analytics.inventory_turnover;

-- Set up working environment
SET SCHEMA 'ecommerce.orders.checkout';

-- Create checkout process tables
CREATE TABLE sessions (
    session_id UUID PRIMARY KEY,
    customer_id INTEGER,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    expires_at TIMESTAMP,
    
    -- Reference customer profile
    FOREIGN KEY (customer_id) REFERENCES ../../customers.profiles.customer_data(customer_id)
);

CREATE TABLE payment_methods (
    payment_id INTEGER PRIMARY KEY,
    session_id UUID,
    method_type VARCHAR(20),
    encrypted_details BLOB,
    
    FOREIGN KEY (session_id) REFERENCES sessions(session_id)
);

-- Create cross-schema view for order analytics
CREATE VIEW ../../../analytics.sales.checkout_conversion AS
SELECT 
    DATE_TRUNC('day', s.created_at) as checkout_date,
    COUNT(*) as sessions_started,
    COUNT(p.payment_id) as payments_attempted,
    COUNT(CASE WHEN o.order_status = 'COMPLETED' THEN 1 END) as orders_completed,
    
    -- Calculate conversion rates
    (COUNT(p.payment_id) * 100.0 / COUNT(*)) as payment_conversion_rate,
    (COUNT(CASE WHEN o.order_status = 'COMPLETED' THEN 1 END) * 100.0 / COUNT(*)) as order_conversion_rate
    
FROM sessions s
LEFT JOIN payment_methods p ON s.session_id = p.session_id
LEFT JOIN ../fulfillment.orders o ON s.session_id = o.checkout_session_id
GROUP BY DATE_TRUNC('day', s.created_at)
ORDER BY checkout_date DESC;
```

### **Multi-Tenant SaaS Application**
```sql
-- SaaS platform with customer isolation
CREATE SCHEMA saas_platform;

-- Tenant management
CREATE SCHEMA saas_platform.tenants;
CREATE SCHEMA saas_platform.tenants.configurations;
CREATE SCHEMA saas_platform.tenants.billing;
CREATE SCHEMA saas_platform.tenants.usage_metrics;

-- Create tenant-specific schemas dynamically
-- For tenant 'acme_corp'
CREATE SCHEMA saas_platform.tenant_data;
CREATE SCHEMA saas_platform.tenant_data.acme_corp;
CREATE SCHEMA saas_platform.tenant_data.acme_corp.application_data;
CREATE SCHEMA saas_platform.tenant_data.acme_corp.user_data;
CREATE SCHEMA saas_platform.tenant_data.acme_corp.settings;

-- For tenant 'tech_startup'
CREATE SCHEMA saas_platform.tenant_data.tech_startup;
CREATE SCHEMA saas_platform.tenant_data.tech_startup.application_data;
CREATE SCHEMA saas_platform.tenant_data.tech_startup.user_data;
CREATE SCHEMA saas_platform.tenant_data.tech_startup.settings;

-- Shared application schema
CREATE SCHEMA saas_platform.shared;
CREATE SCHEMA saas_platform.shared.authentication;
CREATE SCHEMA saas_platform.shared.notifications;
CREATE SCHEMA saas_platform.shared.analytics;

-- Security: Each tenant can only access their own data
CREATE ROLE tenant_acme_corp;
GRANT ALL ON SCHEMA saas_platform.tenant_data.acme_corp TO tenant_acme_corp;
GRANT USAGE ON SCHEMA saas_platform.shared TO tenant_acme_corp;

CREATE ROLE tenant_tech_startup;
GRANT ALL ON SCHEMA saas_platform.tenant_data.tech_startup TO tenant_tech_startup;
GRANT USAGE ON SCHEMA saas_platform.shared TO tenant_tech_startup;
```

### **Healthcare Information System**
```sql
-- Healthcare organization hierarchy
CREATE SCHEMA healthcare_system;

-- Hospital management
CREATE SCHEMA healthcare_system.facilities;
CREATE SCHEMA healthcare_system.facilities.general_hospital;
CREATE SCHEMA healthcare_system.facilities.general_hospital.emergency;
CREATE SCHEMA healthcare_system.facilities.general_hospital.surgery;
CREATE SCHEMA healthcare_system.facilities.general_hospital.pediatrics;
CREATE SCHEMA healthcare_system.facilities.general_hospital.cardiology;

CREATE SCHEMA healthcare_system.facilities.specialty_clinic;
CREATE SCHEMA healthcare_system.facilities.specialty_clinic.orthopedics;
CREATE SCHEMA healthcare_system.facilities.specialty_clinic.dermatology;

-- Patient management
CREATE SCHEMA healthcare_system.patients;
CREATE SCHEMA healthcare_system.patients.demographics;
CREATE SCHEMA healthcare_system.patients.medical_history;
CREATE SCHEMA healthcare_system.patients.insurance;
CREATE SCHEMA healthcare_system.patients.emergency_contacts;

-- Clinical data
CREATE SCHEMA healthcare_system.clinical;
CREATE SCHEMA healthcare_system.clinical.appointments;
CREATE SCHEMA healthcare_system.clinical.diagnoses;
CREATE SCHEMA healthcare_system.clinical.treatments;
CREATE SCHEMA healthcare_system.clinical.prescriptions;
CREATE SCHEMA healthcare_system.clinical.lab_results;

-- Compliance and reporting
CREATE SCHEMA healthcare_system.compliance;
CREATE SCHEMA healthcare_system.compliance.hipaa;
CREATE SCHEMA healthcare_system.compliance.quality_measures;
CREATE SCHEMA healthcare_system.compliance.audit_trails;

-- Example: Patient data access with proper security
SET SCHEMA 'healthcare_system.clinical.appointments';

CREATE TABLE appointment_schedule (
    appointment_id INTEGER PRIMARY KEY,
    patient_id INTEGER,
    provider_id INTEGER,
    facility_id INTEGER,
    appointment_datetime TIMESTAMP,
    duration_minutes INTEGER,
    status VARCHAR(20),
    
    -- Secure references to patient data
    FOREIGN KEY (patient_id) REFERENCES ../../patients.demographics.patient_info(patient_id)
);

-- Create procedure with cross-schema access
CREATE PROCEDURE schedule_appointment(
    patient_id INTEGER,
    provider_id INTEGER,
    requested_datetime TIMESTAMP
)
AS
DECLARE facility_id INTEGER;
DECLARE patient_insurance VARCHAR(100);
BEGIN
    -- Check patient insurance (different schema)
    SELECT primary_insurance INTO patient_insurance
    FROM ../../patients.insurance.coverage
    WHERE patient_id = :patient_id AND status = 'ACTIVE';
    
    -- Find appropriate facility
    SELECT facility_id INTO facility_id
    FROM ../../../facilities.general_hospital.availability
    WHERE provider_id = :provider_id 
    AND datetime_slot = :requested_datetime
    AND accepts_insurance = :patient_insurance;
    
    -- Create appointment
    INSERT INTO appointment_schedule (
        patient_id, provider_id, facility_id, 
        appointment_datetime, status
    ) VALUES (
        :patient_id, :provider_id, :facility_id,
        :requested_datetime, 'SCHEDULED'
    );
    
    -- Log for compliance
    INSERT INTO ../../../compliance.audit_trails.appointment_logs (
        action_type, patient_id, timestamp, user_id
    ) VALUES (
        'APPOINTMENT_SCHEDULED', :patient_id, CURRENT_TIMESTAMP, USER
    );
END;
```

---

## 🔍 Schema Administration

### **Monitoring Schema Usage**
```sql
-- View schema hierarchy
SELECT 
    schema_name,
    parent_schema_name,
    schema_path,
    schema_level,
    object_count,
    creation_date
FROM RDB$SCHEMAS
ORDER BY schema_level, schema_path;

-- Monitor schema access patterns
SELECT 
    schema_path,
    access_frequency,
    last_access_time,
    active_sessions,
    data_size_mb
FROM RDB$SCHEMA_STATISTICS
WHERE access_frequency > 0
ORDER BY access_frequency DESC;
```

### **Schema Maintenance Operations**
```sql
-- Update schema statistics
UPDATE STATISTICS FOR SCHEMA company.finance.accounting;

-- Analyze schema performance
ANALYZE SCHEMA company.finance FOR OPTIMIZATION;

-- Clean up unused schemas
SELECT schema_path 
FROM RDB$SCHEMA_STATISTICS 
WHERE last_access_time < CURRENT_DATE - 90
AND object_count = 0;

-- Backup specific schema hierarchy
BACKUP SCHEMA company.finance TO '/backups/finance_schema.fbk';
```

### **Schema Migration Tools**
```sql
-- Export schema definition
EXTRACT SCHEMA company.finance.accounting TO '/exports/accounting_schema.sql';

-- Import schema to different hierarchy
IMPORT SCHEMA FROM '/exports/accounting_schema.sql' 
TO 'company_backup.finance.accounting_restored';

-- Clone schema structure
CLONE SCHEMA company.finance.accounting 
TO company.finance.accounting_template
WITHOUT DATA;
```

---

## 🎯 Best Practices

### **Schema Design Principles**
1. **Logical Organization**: Mirror your business structure
2. **Reasonable Depth**: Avoid unnecessary nesting (3-5 levels typically sufficient)
3. **Clear Naming**: Use descriptive, consistent names
4. **Security Boundaries**: Align schemas with security requirements
5. **Performance Consideration**: Consider query patterns when organizing

### **Naming Conventions**
```sql
-- Good: Clear, hierarchical naming
company.finance.accounting.receivables
company.operations.manufacturing.quality_control

-- Avoid: Overly deep nesting
company.division.department.team.project.module.component.detail  -- Too deep

-- Avoid: Unclear abbreviations
comp.fin.acc.rec  -- Unclear
```

### **Performance Optimization**
```sql
-- Use schema-aware queries
SET SCHEMA 'company.finance.accounting';
SELECT * FROM transactions WHERE amount > 1000;  -- Fast

-- Avoid unnecessary cross-schema joins
-- Good: Related data in same schema
SELECT t.*, d.description 
FROM transactions t
JOIN transaction_details d ON t.id = d.transaction_id;

-- Less optimal: Frequent cross-schema access
SELECT t.*, p.name
FROM company.finance.accounting.transactions t
JOIN company.operations.manufacturing.products p ON t.product_id = p.id;
```

---

## 🆘 Troubleshooting

### **Common Schema Issues**

**Issue**: "Schema not found"
```sql
-- Check schema existence
SHOW SCHEMAS LIKE 'company.finance%';

-- Check current schema setting
SHOW SCHEMA;

-- Reset to known schema
SET SCHEMA 'company';
```

**Issue**: "Permission denied on schema"
```sql
-- Check schema permissions
SELECT * FROM RDB$USER_PRIVILEGES 
WHERE RDB$RELATION_NAME LIKE 'company.finance%';

-- Grant necessary permissions
GRANT USAGE ON SCHEMA company.finance TO username;
```

**Issue**: "Circular schema reference"
```sql
-- ScratchBird prevents this automatically, but if you encounter it:
-- Check schema hierarchy
SELECT schema_name, parent_schema_name, schema_path
FROM RDB$SCHEMAS
WHERE schema_path LIKE '%problematic%'
ORDER BY schema_level;
```

---

## 🎯 Next Steps

- **[SQL Language Guide](06-sql-language.md)** - Learn ScratchBird SQL with schema features
- **[sb_isql Guide](10-sb_isql.md)** - Use interactive SQL with schemas
- **[Security Guide](08-security.md)** - Implement schema-based security
- **[Best Practices](28-best-practices.md)** - Schema design best practices

## 📚 Related Documentation

- **[Database Engine](05-database-engine.md)** - Understanding ScratchBird architecture
- **[API Reference](17-api-reference.md)** - Programming with hierarchical schemas
- **[Performance Tuning](20-performance.md)** - Optimizing schema-aware queries

---

## 💡 Pro Tips

> **Start Simple**: Begin with 2-3 levels and expand as needed
> ```sql
> CREATE SCHEMA company;
> CREATE SCHEMA company.finance;
> CREATE SCHEMA company.finance.accounting;
> ```

> **Use SET SCHEMA**: Simplify object references by setting working schema
> ```sql
> SET SCHEMA 'company.finance.accounting';
> SELECT * FROM transactions;  -- No full path needed
> ```

> **Plan for Growth**: Design hierarchy to accommodate future organizational changes
> ```sql
> -- Flexible design
> CREATE SCHEMA company.regions.north_america.sales;
> CREATE SCHEMA company.regions.europe.sales;
> ```

**🌳 Ready to revolutionize your database organization?** ScratchBird's hierarchical schemas provide unprecedented organization capabilities that scale with your business!