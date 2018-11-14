#!/bin/bash

NEW=/home/postgres/pgstrom/tpch_db_test/data1G/
OLD=/ssdp2/tpch_data/1G/

for file in tpch-build-db*; do
    sed -i "s|$OLD|$NEW|g" "$file";
done;
