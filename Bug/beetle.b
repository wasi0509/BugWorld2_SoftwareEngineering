//
// $Id: beetle.b,v 1.2 2004/08/17 09:25:39 lindig Exp $
// 
// How we use markers:
// 
// Markers 0,1,2 encodes the absolute direction (0..5) to follow home.
//
//  0           0 _ _
//  1           _ 1 _
//  2           0 1
//  3           _ _ 2
//  4           0 _ 2
//  5           _ 1 2
// 
// Marker 4 is written after a vector was written -- it signifies
// consistency. Marker 3 and 5 are unused.

define main {
    loop {
        flip 6 then exit; else turn right;
    }
    goto find_food;
}

-- follow vectors home

define follow_trail_home {
    loop {
        sense here home then {drop; goto find_home; }
        sense here marker 0 then {
            sense here marker 1 then {
                sense here marker 2 then {
                    -- illegal marker 012
                    goto find_home;
                } else {
                    loop { direction 2 then exit; turn left; }
                }
            } else {
                sense here marker 2 then {
                    loop { direction 4 then exit; turn left; }
                } else {
                    loop { direction 0 then exit; turn left; }
                }

            }
        } else {
            sense here marker 1 then {
                sense here marker 2 then {
                    loop { direction 5 then exit; turn left; }
                } else {
                    loop { direction 1 then exit; turn left; }
                }
            } else {
                sense here marker 2 then {
                    loop { direction 3 then exit; turn left; }
                } else {
                    goto find_home; -- should not happen
                }
            }
        }
        -- we are now facing into the direction of the vector
        move else choose {
            case: { turn left;  move; goto find_home; }
            case: { turn right; move; goto find_home; }
        }    
    }        
}

-- go home by finding a vector leading home

define find_home {
    -- random walk to find home
    loop {
        sense here home then {drop; goto find_food; }
        sense here marker 4 then goto follow_trail_home;
        sense left ahead marker 4 then {turn left; move;}
        sense right ahead marker 4 then {turn right; move;}
        
        flip 4 then turn right;
        flip 4 then turn left;
        move else loop {turn right; flip 2 then exit; }
    }
}

-- search for food, leave a vector pointing home if there is none
-- already

define find_food {
    loop {
        sense here marker 4 else { 
            direction 0 then { mark 2; unmark 1; unmark 0; mark 4; }
            direction 1 then { mark 2; mark 0  ; unmark 1; mark 4; }
            direction 2 then { mark 1; mark 2  ; unmark 0; mark 4; }
            direction 3 then { mark 0; unmark 1; unmark 2; mark 4; }
            direction 4 then { mark 1; unmark 0; unmark 2; mark 4; }
            direction 5 then { mark 1; mark 0  ; unmark 2; mark 4; }
        }
        
        sense here food then sense here home else pickup then goto find_home;
        
        loop { 
            sense ahead food then exit;
            sense left  ahead food then { turn left; exit; }
            sense right ahead food then { turn right; exit; }
            
            exit;
        } 
        
        flip 16 then turn right;
        flip 16 then turn left;
        move else loop {turn right; flip 2 then exit; }
    }
}
