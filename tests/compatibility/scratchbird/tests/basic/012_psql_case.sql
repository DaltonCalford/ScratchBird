EXECUTE BLOCK AS
    DECLARE VARIABLE status_code INT;
BEGIN
    status_code := 2;

    CASE status_code
        WHEN 1 THEN
            status_code := 10;
        WHEN 2 THEN
            status_code := 20;
        ELSE
            status_code := 30;
    END CASE;

    CASE
        WHEN status_code = 20 THEN
            status_code := 21;
        WHEN status_code = 30 THEN
            status_code := 31;
        ELSE
            status_code := 22;
    END CASE;
END;
