CREATE TABLE vncr_bed9e5_employee(
    id_employee INTEGER,
    prenom VARCHAR(20),
    id_department INTEGER,
    PRIMARY KEY(id_employee)
);
CREATE TABLE vncr_bed9e5_department(
    id_department INTEGER,
    name VARCHAR(20)
);
INSERT INTO vncr_bed9e5_department(id_department, name) VALUES (1, 'somme');
INSERT INTO vncr_bed9e5_department(id_department, name) VALUES (2, 'pas de calais');
INSERT INTO vncr_bed9e5_employee(id_employee, prenom, id_department) VALUES (1, 'benoit', 1);
INSERT INTO vncr_bed9e5_employee(id_employee, prenom, id_department) VALUES (2, 'tom', 2);
