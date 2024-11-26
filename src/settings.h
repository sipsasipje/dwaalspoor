// UTC offset.
#define TIMEOFFSET 1

// Run between start and end time if AUTOPAUSE is true.
#define AUTOPAUSE true
int start[2] = {14, 00};
int end[2] = {7, 00};

// Hold the lights.
unsigned int hold = 2000;
// Fade duration.
unsigned int fade = 500;
// randomMode, seriesMode.
LEDMode ledMode = seriesMode;

// Color setting for seriesMode
uint8_t series[][3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}};
uint8_t seriesLength = sizeof(series[0]) / sizeof(series[0][0]);

// Sequences of steps we want to execute. Options: setColor, fadeIn, fadeOut, xFade, holdColor, uvOn, uvOff.
State sequence1[] = {setColor, fadeIn, holdColor, fadeOut};
State sequence2[] = {uvOn, uvOff};

// In what order we want to execute the sequences. Params: the sequence, length, repeat, removeWhenDone
Sequence sequences[] = {
    {sequence1, 4, 3, false},
    {sequence2, 2, 1, false}
}
;