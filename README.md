# combXAgg

- combiner: becuase threds delegate operations to one thread to access a contention hotspot.
- exchanger: because implements ability to eliminate operations.
- aggregate: because n  operations are aggregated to effect change at contention hotspot lesss than n times.

#Compile Instructions:

From microbench/
make DATA_STRUCTURES='stacks' -j4

generates binary in /bin/


#Execute instructions:

./stacks.debra -nwork 1 -nprefill 1 -i 5 -d 5 -rq 0 -rqsize 1 -k 20000 -nrq 0 -t 3000


NOTE: The code as of now shall allow you to work on your stack code wih multiple threads and debug etc.
   
##TODO
- change main.cpp to spawn above ds based ops and then aggregate results.
- In adapters invoke push/add/enq with insert requests.
