

define search {
    loop {
        sense ahead food then move then pickup then exit;
        choose {
            case:   turn left;
            case:   turn right;
            case:   move;
        }
    }
    goto walk;
}

define walk {
    loop {
        sense ahead home then move then { drop; exit;}
        choose {
            case:   turn left;
            case:   turn right;
            case:   move;
        }
    }
    goto search;
}    
