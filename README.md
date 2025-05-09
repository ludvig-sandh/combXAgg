# combXAgg

- combiner: becuase threds delegate operations to one thread to access a contention hotspot.
- exchanger: because implements ability to eliminate operations.
- aggregate: because n  operations are aggregated to effect change at contention hotspot lesss than n times.
   
##TODO
- change main.cpp to spawn above ds based ops and then aggregate results.
- In adapters invoke push/add/enq with insert requests.
