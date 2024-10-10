#!/usr/bin/env bash
#Command to install the testers:
# npm install -g loadtest

TEST_IP="192.168.2.131"
TEST_TIME=60
LOG_FILE=psychic-websocket-loadtest.log
PROTOCOL=ws
#PROTOCOL=wss

if test -f "$LOG_FILE"; then
  rm $LOG_FILE
fi

for CONCURRENCY in 1 2 3 4 5 6 7
do
  printf "\n\nCLIENTS: *** $CONCURRENCY ***\n\n" >> $LOG_FILE
  echo "Testing $CONCURRENCY clients on $PROTOCOL://$TEST_IP/ws"
  loadtest -c $CONCURRENCY --cores 1 -t $TEST_TIME --insecure $PROTOCOL://$TEST_IP/ws --quiet  2> /dev/null >> $LOG_FILE
  sleep 1
done

for CONNECTIONS in 8 10 16 20
#for CONNECTIONS in 20
do
  CONCURRENCY=$((CONNECTIONS / 2))
  printf "\n\nCLIENTS: *** $CONNECTIONS ***\n\n" >> $LOG_FILE
  echo "Testing $CONNECTIONS clients on $PROTOCOL://$TEST_IP/ws"
  loadtest -c $CONCURRENCY --cores 2 -t $TEST_TIME --insecure $PROTOCOL://$TEST_IP/ws --quiet 2> /dev/null >> $LOG_FILE
  sleep 1
done