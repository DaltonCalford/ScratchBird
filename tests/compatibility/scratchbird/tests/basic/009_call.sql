CREATE PROCEDURE call_basic_proc(IN a INT, IN b INT)
LANGUAGE plpgsql
AS $$
BEGIN
    -- no-op
END;
$$;

CALL call_basic_proc(1, 2);
