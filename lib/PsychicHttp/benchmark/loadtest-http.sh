#!/usr/bin/env bash
#Command to install the testers:
# npm install

TEST_IP="psychic.local"
TEST_TIME=10
#LOG_FILE=psychic-http-loadtest.log
LOG_FILE=_psychic-http-loadtest.json
RESULTS_FILE=http-loadtest-results.csv
TIMEOUT=10000
WORKERS=1
PROTOCOL=http
#PROTOCOL=https

echo "url,connections,rps,latency,errors" > $RESULTS_FILE

for CONCURRENCY in 1 2 3 4 5 6 7 8 9 10 15 20
do
  printf "\n\nCLIENTS: *** $CONCURRENCY ***\n\n" >> $LOG_FILE
  echo "Testing $CONCURRENCY clients on $PROTOCOL://$TEST_IP/"
  autocannon -c $CONCURRENCY -w $WORKERS -d $TEST_TIME -j "$PROTOCOL://$TEST_IP/" > $LOG_FILE
  node parse-http-test.js $LOG_FILE $RESULTS_FILE
  sleep 5
done

for CONCURRENCY in 1 2 3 4 5 6 7 8 9 10 15 20
do
  echo "Testing $CONCURRENCY clients on $PROTOCOL://$TEST_IP/api"
  autocannon -c $CONCURRENCY -w $WORKERS -d $TEST_TIME -j "$PROTOCOL://$TEST_IP/api?foo=bar" > $LOG_FILE
  node parse-http-test.js $LOG_FILE $RESULTS_FILE
  sleep 5
done

for CONCURRENCY in 1 2 3 4 5 6 7 8 9 10 15 20
do
  echo "Testing $CONCURRENCY clients on $PROTOCOL://$TEST_IP/alien.png"
  autocannon -c $CONCURRENCY -w $WORKERS -d $TEST_TIME -j "$PROTOCOL://$TEST_IP/alien.png" > $LOG_FILE
  node parse-http-test.js $LOG_FILE $RESULTS_FILE
  sleep 5
done

rm $LOG_FILE
