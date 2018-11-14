
BEGIN;

	CREATE TABLE NATION (
		N_NATIONKEY		SERIAL PRIMARY KEY,
		N_NAME			CHAR(25),
		N_REGIONKEY		BIGINT NOT NULL,  
		N_COMMENT		VARCHAR(152)
	);

	COPY nation FROM '/home/postgres/pgstrom/tpch_db_test/data1G/nation.csv' WITH (FORMAT csv, DELIMITER '|');

COMMIT;
