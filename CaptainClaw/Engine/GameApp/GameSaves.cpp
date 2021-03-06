#include "GameSaves.h"

Point GetSpawnPosition(uint32 levelNumber, uint32 checkpointNumber)
{
    switch (levelNumber)
    {
        case 1:
            if (checkpointNumber == 0) { return Point(689, 4723); }
            else if (checkpointNumber == 1) { return Point(6873, 4970); }
            else if (checkpointNumber == 2) { return Point(14227, 2281); }
            break;
        case 2:
            if (checkpointNumber == 0) { return Point(420, 4789); }
            else if (checkpointNumber == 1) { return Point(8235, 4848); }
            else if (checkpointNumber == 2) { return Point(15657, 4650); }
            break;
        case 3:
            if (checkpointNumber == 0) { return Point(4600, 6270); }
            else if (checkpointNumber == 1) { return Point(15358, 6868); }
            else if (checkpointNumber == 2) { return Point(21712, 3464); }
            break;
        case 4:
            if (checkpointNumber == 0) { return Point(1439, 4760); }
            else if (checkpointNumber == 1) { return Point(9300, 4248); }
            else if (checkpointNumber == 2) { return Point(7081, 5254); }
            break;
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        default:
            assert(false && "Not implemented yet");
    }

    assert(false && "Unknown level / checkpoint number.");
    return Point();
}