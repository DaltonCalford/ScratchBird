CREATE TABLE trigger_case_table (
    id INT PRIMARY KEY,
    status INT
);

CREATE PROCEDURE proc_case_nested()
LANGUAGE plpgsql
AS $$
DECLARE
    v_status INT;
BEGIN
    v_status := 1;

    CASE v_status
        WHEN 1 THEN
            CASE
                WHEN v_status = 1 THEN
                    v_status := 2;
                ELSE
                    v_status := 3;
            END CASE;
        ELSE
            v_status := 4;
    END CASE;
END;
$$;

CREATE TRIGGER trg_case_nested
BEFORE INSERT ON trigger_case_table
FOR EACH ROW
AS $$
DECLARE
    v_local INT;
BEGIN
    v_local := NEW.status;

    CASE
        WHEN v_local = 10 THEN
            NEW.status := 11;
        WHEN v_local = 20 THEN
            CASE v_local
                WHEN 20 THEN NEW.status := 21;
                ELSE NEW.status := 22;
            END CASE;
        ELSE
            NEW.status := 30;
    END CASE;
END;
$$;

CALL proc_case_nested();

INSERT INTO trigger_case_table (id, status) VALUES (1, 10);
INSERT INTO trigger_case_table (id, status) VALUES (2, 20);
INSERT INTO trigger_case_table (id, status) VALUES (3, 99);

DROP TRIGGER trg_case_nested;
DROP PROCEDURE proc_case_nested;
DROP TABLE trigger_case_table;
