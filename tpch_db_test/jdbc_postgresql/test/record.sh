mvn compile
mvn exec:java -Dexec.args="load"
mvn exec:java -Dexec.args="run 1"
mvn exec:java -Dexec.args="run"

create extension pg_strom;
