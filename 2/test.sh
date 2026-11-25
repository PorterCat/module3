#!/bin/sh

./bin/news -b &
broker_pid=$!

sleep 3

./bin/news -s news &
subscriber_pid=$!

./bin/news -s news sports &
second_subscriber_pid=$!

sleep 8

printf 'Hello from publisher!' | ./bin/news -p news
printf 'Hello from second publisher!' | ./bin/news -p sports

kill -INT "$broker_pid"
wait "$broker_pid"
wait "$subscriber_pid"
wait "$second_subscriber_pid"