#!/usr/bin/env bash

rm ./*.log
rm ./MyBot

set -e

cmake .
make MyBot
./halite "./MyBot" "./MyBot" -i ../replays
