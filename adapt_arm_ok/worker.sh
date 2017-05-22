#!/bin/sh
ps -fe | grep /zqfs/bin/worker | grep -v grep
if [ $? -ne 0 ] 
then
    /zqfs/bin/worker &
fi
