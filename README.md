
________________________________________
Local Controller 1
NORMAL sequence
1.	R3 South→North and North→South Straight + Right Green
Vehicles on R3 in both directions (S→N and N→S) may go straight or turn right.
Duration: 20 seconds
2.	R3 South→North and North→South Straight + Right Yellow
Warning before stopping R3 straight and right-turn movements.
Duration: 4 seconds
3.	R3 South→North and North→South Left-Turn Green
Protected left turns on R3 in both directions.
Duration: 12 seconds
4.	R3 South→North and North→South Left-Turn Yellow
Warning before stopping R3 left-turn movements.
Duration: 4 seconds
5.	All directions Red (safety buffer)
Clears the intersection before switching roads.
Duration: 2 seconds
6.	R1 West→East and East→West Straight + Right Green
Vehicles on R1 in both directions (W→E and E→W) may go straight or turn right.
Duration: 20 seconds
7.	R1 West→East and East→West Straight + Right Yellow
Warning before stopping R1 straight and right-turn movements.
Duration: 4 seconds
8.	R1 West→East and East→West Left-Turn Green
Protected left turns on R1 in both directions.
Duration: 12 seconds
9.	R1 West→East and East→West Left-Turn Yellow
Warning before stopping R1 left-turn movements.
Duration: 4 seconds
10.	All directions Red (safety buffer)
Clears the intersection before restarting the cycle.
Duration: 2 seconds
________________________________________
===================================================================
When TRAIN mode is active (Local Controller 1)
•	TRAIN starts only at an ALL-RED state.
•	Traffic is cleared safely away from the railway crossing.
•	Pedestrians are only allowed during ALL-RED checkpoints.
________________________________________
TRAIN sequence (one full loop)
1.	R3 North→South Straight + Left Green
Vehicles on R3 travelling North→South may go straight or turn left.
(R3 South→North remains RED.)
Duration: 8 seconds
2.	R3 North→South Straight + Left Yellow
Warning before stopping this movement.
Duration: 4 seconds
3.	All directions Red (safe checkpoint)
Safety buffer and decision point.
Duration: 2 seconds
4.	R3 South→North Left-Turn Green
Vehicles on R3 travelling South→North may turn left only.
(R3 North→South remains RED.)
Duration: 8 seconds
5.	R3 South→North Left-Turn Yellow
Warning before stopping this movement.
Duration: 4 seconds
6.	All directions Red (safe checkpoint)
Safety buffer and pedestrian decision point.
Duration: 2 seconds
7.	R1 Restricted Green
•	R1 West→East: Straight + Right allowed
•	R1 East→West: Straight + Left allowed
Conflicting turns are restricted while the train is present.
Duration: 15 seconds
8.	R1 Restricted Yellow
Warning before stopping restricted R1 movements.
Duration: 4 seconds
9.	All directions Red (decision checkpoint)
•	If the train is still present, repeat the TRAIN sequence.
•	If the train has cleared, return to NORMAL mode.
Duration: 2 seconds
________________________________________
===================================================================
PEDESTRIAN operation (Local Controller 1)
When pedestrian mode is allowed
•	A pedestrian request is made by pressing the pedestrian button.
•	Pedestrians are only served during an ALL-RED phase.
•	If it is not safe, the request is queued and served later.
•	Pedestrian movements never conflict with vehicle movements.
________________________________________
PEDESTRIAN sequence
1.	Pedestrian WALK
Pedestrians are allowed to cross the road.
Duration: 8 seconds
2.	Pedestrian FLASH
Warning that the crossing time is ending.
Duration: 4 seconds
3.	All directions Red (pedestrian clearance)
Clears pedestrians from the road before traffic resumes.
Duration: 2 seconds
________________________________________
After pedestrian crossing
•	If the system was in NORMAL mode, traffic resumes at the next NORMAL phase.
•	If the system was in TRAIN mode, traffic resumes at the next TRAIN phase.
Pedestrian operation does not break safety or sequencing.
________________________________________
________________________________________
Local Controller 2
NORMAL sequence
1.	R3 South→North and North→South Straight + Right Green
Vehicles on R3 in both directions (S→N and N→S) may go straight or turn right.
Duration: 20 seconds
2.	R3 South→North and North→South Straight + Right Yellow
Warning before stopping R3 straight and right-turn movements.
Duration: 4 seconds
3.	R3 South→North and North→South Left-Turn Green
Protected left turns on R3 in both directions.
Duration: 12 seconds
4.	R3 South→North and North→South Left-Turn Yellow
Warning before stopping R3 left-turn movements.
Duration: 4 seconds
5.	All directions Red (safety buffer)
Clears the intersection before switching roads.
Duration: 2 seconds
6.	R2 West→East and East→West Straight + Right Green
Vehicles on R2 in both directions (W→E and E→W) may go straight or turn right.
Duration: 15 seconds
7.	R2 West→East and East→West Straight + Right Yellow
Warning before stopping R2 straight and right-turn movements.
Duration: 4 seconds
8.	R2 West→East and East→West Left-Turn Green
Protected left turns on R2 in both directions.
Duration: 8 seconds
9.	R2 West→East and East→West Left-Turn Yellow
Warning before stopping R2 left-turn movements.
Duration: 4 seconds
10.	All directions Red (safety buffer)
Clears the intersection before restarting the cycle.
Duration: 2 seconds
________________________________________
===================================================================
When TRAIN mode is active (Local Controller 2)
•	TRAIN starts only at an ALL-RED state.
•	Traffic is cleared safely away from the railway crossing.
•	Train clearing movements apply only to R3 South→North.
•	Pedestrians are only allowed during ALL-RED checkpoints.
________________________________________
TRAIN sequence (one full loop)
1.	R3 South→North Straight + Left Green
Vehicles on R3 travelling South→North may go straight or turn left.
(R3 North→South remains RED.)
Duration: 8 seconds
2.	R3 South→North Straight + Left Yellow
Warning before stopping this movement.
Duration: 4 seconds
3.	All directions Red (safe checkpoint)
Safety buffer and decision point.
Duration: 2 seconds
4.	R3 South→North Left-Turn Green
Vehicles on R3 travelling South→North may turn left only.
(R3 North→South remains RED.)
Duration: 8 seconds
5.	R3 South→North Left-Turn Yellow
Warning before stopping this movement.
Duration: 4 seconds
6.	All directions Red (safe checkpoint)
Safety buffer and pedestrian decision point.
Duration: 2 seconds
7.	R2 Restricted Green
•	R2 West→East: Straight + Left allowed (no right turn)
•	R2 East→West: Straight + Right allowed (no left turn)
Conflicting turns are restricted while the train is present.
Duration: 15 seconds
8.	R2 Restricted Yellow
Warning before stopping restricted R2 movements.
Duration: 4 seconds
9.	All directions Red (decision checkpoint)
•	If the train is still present, repeat the TRAIN sequence.
•	If the train has cleared, return to NORMAL mode.
Duration: 2 seconds
________________________________________
===================================================================
PEDESTRIAN operation (Local Controller 2)
When pedestrian mode is allowed
•	A pedestrian request is made by pressing the pedestrian button.
•	Pedestrians are only served during an ALL-RED phase.
•	If it is not safe, the request is queued and served later.
•	Pedestrian movements never conflict with vehicle movements.
________________________________________
PEDESTRIAN sequence
1.	Pedestrian WALK
Pedestrians are allowed to cross the road.
Duration: 8 seconds
2.	Pedestrian FLASH
Warning that the crossing time is ending.
Duration: 4 seconds
3.	All directions Red (pedestrian clearance)
Clears pedestrians from the road before traffic resumes.
Duration: 2 seconds
________________________________________
After pedestrian crossing
•	If the system was in NORMAL mode, traffic resumes at the next NORMAL phase.
•	If the system was in TRAIN mode, traffic resumes at the next TRAIN phase.
Pedestrian operation does not break safety or sequencing.
________________________________________
Comparison of NORMAL mode timing
Local Controller 1 vs Local Controller 2
1. NORMAL sequence timing breakdown
R3 phases (major road) — same for both
Phase	Duration
R3 Straight + Right Green	20 s
R3 Straight + Right Yellow	4 s
R3 Left Green	12 s
R3 Left Yellow	4 s
All-Red buffer	2 s
R3 subtotal	42 s
 R3 timing is identical in both Local 1 and Local 2 to keep priority and coordination.
________________________________________
Side-road phases
Local Controller 1 (R1 – high traffic)
Phase	Duration
R1 Straight + Right Green	20 s
R1 Straight + Right Yellow	4 s
R1 Left Green	12 s
R1 Left Yellow	4 s
All-Red buffer	2 s
R1 subtotal	42 s
Local Controller 2 (R2 – moderate traffic)
Phase	Duration
R2 Straight + Right Green	15 s
R2 Straight + Right Yellow	4 s
R2 Left Green	8 s
R2 Left Yellow	4 s
All-Red buffer	2 s
R2 subtotal	33 s
________________________________________
2. Total NORMAL cycle time
Controller	R3 time	Side-road time	Total NORMAL cycle
Local 1	42 s	42 s	84 seconds
Local 2	42 s	33 s	75 seconds
________________________________________
3. Difference summary
•	Local 2 NORMAL cycle is 9 seconds shorter than Local 1.
•	The reduction comes only from the side road:
o	Straight + Right: −5 s
o	Left turn: −4 s
•	R3 timing is unchanged in both controllers.

