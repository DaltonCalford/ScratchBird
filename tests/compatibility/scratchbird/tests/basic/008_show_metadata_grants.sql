CREATE TABLE show_meta_parent (
    id INT PRIMARY KEY,
    name VARCHAR(50)
);

CREATE TABLE show_meta_child (
    id INT PRIMARY KEY,
    parent_id INT,
    CONSTRAINT fk_show_meta_parent FOREIGN KEY (parent_id) REFERENCES show_meta_parent(id)
);

CREATE INDEX idx_show_meta_parent_name ON show_meta_parent (name);

SHOW TABLES;
SHOW TABLES LIKE 'show_meta_%';
SHOW COLUMNS FROM show_meta_parent;
SHOW COLUMNS FROM show_meta_parent LIKE 'na%';
SHOW INDEXES FROM show_meta_parent;

GRANT SELECT ON show_meta_parent TO PUBLIC;
GRANT INSERT, UPDATE ON show_meta_child TO PUBLIC;
SHOW GRANTS;
SHOW GRANTS FOR show_meta_parent;

REVOKE SELECT ON show_meta_parent FROM PUBLIC;
REVOKE INSERT, UPDATE ON show_meta_child FROM PUBLIC;

DROP TABLE show_meta_child;
DROP TABLE show_meta_parent;
