#!/usr/bin/env bash
#Command to install the testers:
# npm install

TEST_IP="psychic.local"
TEST_TIME=60
LOG_FILE=psychic-websocket-loadtest.json
RESULTS_FILE=websocket-loadtest-results.csv
PROTOCOL=ws
#PROTOCOL=wss

if test -f "$LOG_FILE"; then
  rm $LOG_FILE
fi

echo "url,clients,rps,latency,errors" > $RESULTS_FILE

CORES=1
for CONCURRENCY in 1 2 3 4 5
do
  echo "Testing $CONCURRENCY clients on $PROTOCOL://$TEST_IP/ws"
  loadtest -c $CONCURRENCY --cores $CORES -t $TEST_TIME --insecure $PROTOCOL://$TEST_IP/ws --quiet  2> /dev/null >> $LOG_FILE
  node parse-websocket-test.js $LOG_FILE $RESULTS_FILE
  sleep 2
done

CORES=2
for CONNECTIONS in 6 8 10 12 14
do
  CONCURRENCY=$((CONNECTIONS / 2))
  echo "Testing $CONNECTIONS clients on $PROTOCOL://$TEST_IP/ws"
  loadtest -c $CONCURRENCY --cores $CORES -t $TEST_TIME --insecure $PROTOCOL://$TEST_IP/ws --quiet  2> /dev/null >> $LOG_FILE
  node parse-websocket-test.js $LOG_FILE $RESULTS_FILE
  sleep 2
done

CORES=4
for CONNECTIONS in 16 20 24 28 32
do
 CONCURRENCY=$((CONNECTIONS / CORES))
 echo "Testing $CONNECTIONS clients on $PROTOCOL://$TEST_IP/ws"
 loadtest -c $CONCURRENCY --cores $CORES -t $TEST_TIME --insecure $PROTOCOL://$TEST_IP/ws --quiet 2> /dev/null >> $LOG_FILE
 node parse-websocket-test.js $LOG_FILE $RESULTS_FILE
 sleep 2
done
