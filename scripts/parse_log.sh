#!/bin/bash
# Usage parse_log.sh yolo.log

if [ "$#" -lt 1 ]
then
echo "Usage: run \"parse_log.sh /path/to/your.log\""
exit
fi

grep '[0-9][0-9]*:.*, ' $1 | sed -n 's/.*, \([0-9][0-9]*.[0-9][0-9]*\) avg.*, \([0-9][0-9]*\) images/\1 \2/p' > aux.txt