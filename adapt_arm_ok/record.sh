#!/bin/bash
echo "RecordCmd=StartRecord?1" | nc 192.168.13.174 1230
echo "RecordCmd=StartRecord?1" | nc 192.168.13.173 1230
sleep 10
echo "RecordCmd=StartRecord?0" | nc 192.168.13.174 1230
echo "RecordCmd=StartRecord?0" | nc 192.168.13.173 1230
