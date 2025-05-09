# combXAgg

- combiner: becuase threds delegate operations to one thread to access a contention hotspot.
- exchanger: because implements ability to eliminate operations.
- aggregate: because n  operations are aggregated to effect change at contention hotspot lesss than n times.
   
##TODO
- add space holder for counter, stack and queue data structures
- change main.cpp to spawn above ds based ops and then aggregate results.
- add a folder for different .h files related to different combinening techniques in literature parallel to recordmgr
