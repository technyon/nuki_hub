#!/usr/bin/env bash
#Command to install the testers:
# npm install -g autocannon

TEST_IP="192.168.2.131"
TEST_TIME=60
LOG_FILE=psychic-http-loadtest.log
TIMEOUT=10000
PROTOCOL=http
#PROTOCOL=https

if test -f "$LOG_FILE"; then
  rm $LOG_FILE
fi

for CONCURRENCY in 1 2 3 4 5 6 7 8 9 10 15 20
#for CONCURRENCY in 20
do
  printf "\n\nCLIENTS: *** $CONCURRENCY ***\n\n" >> $LOG_FILE
  echo "Testing $CONCURRENCY clients on $PROTOCOL://$TEST_IP/"
  #loadtest -c $CONCURRENCY --cores 1 -t $TEST_TIME --timeout $TIMEOUT "$PROTOCOL://$TEST_IP/" --quiet >> $LOG_FILE
  autocannon -c $CONCURRENCY -w 1 -d $TEST_TIME --renderStatusCodes "$PROTOCOL://$TEST_IP/" >> $LOG_FILE 2>&1
  printf "\n\n----------------\n\n" >> $LOG_FILE
  sleep 1

  echo "Testing $CONCURRENCY clients on $PROTOCOL://$TEST_IP/api"
  #loadtest -c $CONCURRENCY --cores 1 -t $TEST_TIME --timeout $TIMEOUT "$PROTOCOL://$TEST_IP/api?foo=bar" --quiet >> $LOG_FILE
  autocannon -c $CONCURRENCY -w 1 -d $TEST_TIME --renderStatusCodes "$PROTOCOL://$TEST_IP/api?foo=bar" >> $LOG_FILE 2>&1
  printf "\n\n----------------\n\n" >> $LOG_FILE
  sleep 1
  
  echo "Testing $CONCURRENCY clients on $PROTOCOL://$TEST_IP/alien.png"
  #loadtest -c $CONCURRENCY --cores 1 -t $TEST_TIME --timeout $TIMEOUT "$PROTOCOL://$TEST_IP/alien.png" --quiet >> $LOG_FILE
  autocannon -c $CONCURRENCY -w 1 -d $TEST_TIME --renderStatusCodes "$PROTOCOL://$TEST_IP/alien.png" >> $LOG_FILE 2>&1
  printf "\n\n----------------\n\n" >> $LOG_FILE
  sleep 1
done