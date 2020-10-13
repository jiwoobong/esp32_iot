#ifndef _DEFINE_APP_H
#define _DEFINE_APP_H

//
// dmx

#define PROTOCOL_ID_NONE               0xFF
#define PROTOCOL_ID_SETADDR            0
#define PROTOCOL_ID_SETTARGET_ADDR     1
#define PROTOCOL_ID_SETTEMP_THRES      2
#define PROTOCOL_ID_MAX                3

#define PROTOCOL_KEY_SIZE              35


const uint8_t const_protocols[PROTOCOL_ID_MAX][PROTOCOL_KEY_SIZE] = {
	{
		// global_update
		0xAA,0xBB,0xAA,0xBB,
		0xAA,0xBB,0xAA,0xBB,
		0xAA,0xBB,0xAA,0xBB,
		0xAA,0xBB,0xAA,0xBB,
		0xAA,0xBB,0xBC,0xBB,
		0xAA,0xBB,0xAA,0xBB,
		0xAA,0xBB,0xAA,0xBB,
		0xAA,0xBB,0xAA,0xBB,
		0xAA,0xBB,0xFF

	} , {
		// target_update
		0xAA,0xBB,0xAA,0xDD,
		0xAA,0xBB,0xAA,0xBB,
		0xAA,0xBB,0xCC,0xAA,
		0xAA,0xBB,0xCC,0xBB,
		0xAA,0xCC,0xBC,0xAA,
		0xAA,0xCC,0xAA,0xBB,
		0xAA,0xCC,0xAA,0xAA,
		0xAA,0xBB,0xAA,0xBB,
		0xAA,0xBB,0xFF

	}	, {
		// set temperature threshold
		0xAA,0xBB,0xAA,0xBB,
		0xCC,0xBB,0xAA,0xBB,
		0xAA,0xEE,0xAA,0xBB,
		0xAA,0xBB,0xAB,0xBB,
		0xAA,0xBA,0xBC,0xBB,
		0xDA,0xBB,0xAA,0xBB,
		0xDA,0xAB,0xAA,0xBB,
		0xDE,0xFB,0xAA,0xBB,
		0xAA,0xBB,0xFF

	}

};


//http://www.daycounter.com/Calculators/Sine-Generator-Calculator.phtml
uint8_t  sine_wave[] = {
		128,150,172,193,211,
		227,240,249,254,255,
		252,245,234,220,202,
		183,161,139,116,94,
		72,53,35,21,10,
		3,0,1,6,15,
		28,44,62,83,105
};



#define NOTE_B0  	31
#define NOTE_C1  	33
#define NOTE_CS1 	35
#define NOTE_D1  	37
#define NOTE_DS1 	39
#define NOTE_E1  	41
#define NOTE_F1  	44
#define NOTE_FS1 	46
#define NOTE_G1  	49
#define NOTE_GS1 	52
#define NOTE_A1  	55
#define NOTE_AS1 	58
#define NOTE_B1  	62
#define NOTE_C2  	65
#define NOTE_CS2 	69
#define NOTE_D2  	73
#define NOTE_DS2 	78
#define NOTE_E2  	82
#define NOTE_F2  	87
#define NOTE_FS2 	92
#define NOTE_G2  	98
#define NOTE_GS2 	104
#define NOTE_A2  	110
#define NOTE_AS2 	117
#define NOTE_B2  	123
#define NOTE_C3  	131
#define NOTE_CS3 	139
#define NOTE_D3  	147
#define NOTE_DS3 	156
#define NOTE_E3  	165
#define NOTE_F3  	175
#define NOTE_FS3 	185
#define NOTE_G3  	196
#define NOTE_GS3 	208
#define NOTE_A3  	220
#define NOTE_AS3 	233
#define NOTE_B3  	247
#define NOTE_C4  	262
#define NOTE_CS4 	277
#define NOTE_D4  	294
#define NOTE_DS4 	311
#define NOTE_E4  	330
#define NOTE_F4  	349
#define NOTE_FS4 	370
#define NOTE_G4  	392
#define NOTE_GS4 	415
#define NOTE_A4  	440
#define NOTE_AS4 	466
#define NOTE_B4  	494
#define NOTE_C5  	523
#define NOTE_CS5 	554
#define NOTE_D5  	587
#define NOTE_DS5 	622
#define NOTE_E5  	659
#define NOTE_F5  	698
#define NOTE_FS5 	740
#define NOTE_G5  	784
#define NOTE_GS5 	831
#define NOTE_A5  	880
#define NOTE_AS5 	932
#define NOTE_B5  	988
#define NOTE_C6  	1047
#define NOTE_CS6 	1109
#define NOTE_D6  	1175
#define NOTE_DS6 	1245
#define NOTE_E6  	1319
#define NOTE_F6  	1397
#define NOTE_FS6 	1480
#define NOTE_G6  	1568
#define NOTE_GS6 	1661
#define NOTE_A6  	1760
#define NOTE_AS6 	1865
#define NOTE_B6  	1976
#define NOTE_C7  	2093
#define NOTE_CS7 	2217
#define NOTE_D7  	2349
#define NOTE_DS7 	2489
#define NOTE_E7  	2637
#define NOTE_F7  	2794
#define NOTE_FS7 	2960
#define NOTE_G7  	3136
#define NOTE_GS7 	3322
#define NOTE_A7  	3520
#define NOTE_AS7 	3729
#define NOTE_B7  	3951
#define NOTE_C8  	4186

#if 0
int melody[] = {
  NOTE_E7, NOTE_E7, 0, NOTE_E7,
  0, NOTE_C7, NOTE_E7, 0,
  NOTE_G7, 0, 0,  0,
  NOTE_G6, 0, 0, 0,

  NOTE_C7, 0, 0, NOTE_G6,
  0, 0, NOTE_E6, 0,
  0, NOTE_A6, 0, NOTE_B6,
  0, NOTE_AS6, NOTE_A6, 0,

  NOTE_G6, NOTE_E7, NOTE_G7,
  NOTE_A7, 0, NOTE_F7, NOTE_G7,
  0, NOTE_E7, 0, NOTE_C7,
  NOTE_D7, NOTE_B6, 0, 0,

  NOTE_C7, 0, 0, NOTE_G6,
  0, 0, NOTE_E6, 0,
  0, NOTE_A6, 0, NOTE_B6,
  0, NOTE_AS6, NOTE_A6, 0,

  NOTE_G6, NOTE_E7, NOTE_G7,
  NOTE_A7, 0, NOTE_F7, NOTE_G7,
  0, NOTE_E7, 0, NOTE_C7,
  NOTE_D7, NOTE_B6, 0, 0
};
//Mario main them tempo
int tempo[] = {
  12, 12, 12, 12,
  12, 12, 12, 12,
  12, 12, 12, 12,
  12, 12, 12, 12,

  12, 12, 12, 12,
  12, 12, 12, 12,
  12, 12, 12, 12,
  12, 12, 12, 12,

  9, 9, 9,
  12, 12, 12, 12,
  12, 12, 12, 12,
  12, 12, 12, 12,

  12, 12, 12, 12,
  12, 12, 12, 12,
  12, 12, 12, 12,
  12, 12, 12, 12,

  9, 9, 9,
  12, 12, 12, 12,
  12, 12, 12, 12,
  12, 12, 12, 12,
};
#else

//Underworld melody
int melody[] = {
  NOTE_C4, NOTE_C5, NOTE_A3, NOTE_A4,
  NOTE_AS3, NOTE_AS4, 0,
  0,
  NOTE_C4, NOTE_C5, NOTE_A3, NOTE_A4,
  NOTE_AS3, NOTE_AS4, 0,
  0,
  NOTE_F3, NOTE_F4, NOTE_D3, NOTE_D4,
  NOTE_DS3, NOTE_DS4, 0,
  0,
  NOTE_F3, NOTE_F4, NOTE_D3, NOTE_D4,
  NOTE_DS3, NOTE_DS4, 0,
  0, NOTE_DS4, NOTE_CS4, NOTE_D4,
  NOTE_CS4, NOTE_DS4,
  NOTE_DS4, NOTE_GS3,
  NOTE_G3, NOTE_CS4,
  NOTE_C4, NOTE_FS4, NOTE_F4, NOTE_E3, NOTE_AS4, NOTE_A4,
  NOTE_GS4, NOTE_DS4, NOTE_B3,
  NOTE_AS3, NOTE_A3, NOTE_GS3,
  0, 0, 0
};
//Underwolrd tempo
int tempo[] = {
  12, 12, 12, 12,
  12, 12, 6,
  3,
  12, 12, 12, 12,
  12, 12, 6,
  3,
  12, 12, 12, 12,
  12, 12, 6,
  3,
  12, 12, 12, 12,
  12, 12, 6,
  6, 18, 18, 18,
  6, 6,
  6, 6,
  6, 6,
  18, 18, 18, 18, 18, 18,
  10, 10, 10,
  10, 10, 10,
  3, 3, 3
};
#endif

uint16_t keys[] = {
239, 253, 268, 284, 301, 319, 338, 358, 379, 402, 426, 451, 478, 506,
536, 568, 602, 638, 676, 716, 758, 804, 851, 902, 956, 1012, 1073, 1136,
1204, 1276, 1351, 1432, 1517, 1607, 1703, 1804, 1911, 2025, 2145, 2273,
2408, 2551, 2703, 2863, 3034, 3214, 3405, 3608, 3822, 4050, 4290, 4545,
4816, 5102, 5405, 5727, 6067, 6428, 6811, 7215, 7645, 8099, 8581, 9091,
9631, 10811, 11454, 12135, 12856, 13621, 14431, 15289, 16198, 17161, 18182,
19263, 20408, 21622, 22908, 24270, 25713, 27242, 28862, 30578, 32396, 34323,
36364
};


#endif
