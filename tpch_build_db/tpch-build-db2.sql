
BEGIN;

	CREATE TABLE REGION (
		R_REGIONKEY	SERIAL PRIMARY KEY,
		R_NAME		CHAR(25),
		R_COMMENT	VARCHAR(152)
	);

	COPY region FROM '/home/postgres/pgstrom/tpch_db_test/data1G/region.csv' WITH (FORMAT csv, DELIMITER '|');

COMMIT;


