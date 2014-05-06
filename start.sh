#!/bin/bash
for ((i=0;i<11;++i))
do
  src/rcssclient &
done

sleep 1

src/rcssclient -port 6002 &

exit 0
