#!/bin/bash
for ((i=0;i<11;++i))
do
  src/rcssdeadlyclient &
done

sleep 1

src/rcssdeadlyclient -port 6002 &

exit 0
