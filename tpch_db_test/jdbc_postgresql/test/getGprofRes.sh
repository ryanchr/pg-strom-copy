#!/bin/bash

sqlPath=/home/postgresql11/test_ads_pg/pg_tpch_modified_sqls/
exePath=/home/postgresql11/jdbc_postgresql/test/
gprofPath=/home/postgresql11/test_ads_pg/pgdatabasessd/gprof/

for sql in `seq 3 22`; do
    if [ $sql -eq 15 || $sql -eq 18 || $sql -eq 19 || $sql -eq 20 ];
    then
        continue
    fi

    cd ${exePath}
    args=$sqlPath${sql}.sql
    mvn exec:java -Dexec.args=${args}

    cd $gprofPath

    for d in */ ; do
        gmonName=${d::-1}.${sql}.ssd.gmon.out
        cp ./${d}/gmon.out ${exePath}/${gmonName}
    done
    for d in */ ; do
	rm -rf $d
    done
done
