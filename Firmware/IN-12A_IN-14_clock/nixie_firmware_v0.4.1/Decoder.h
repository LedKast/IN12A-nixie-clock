class Decoder //for К155ИД1
{	
	// набор состояния для дешифратора
	bool state[11][4] = { // 0->9 + 10(off)
		{0, 0, 0, 0}, 	// 0
		{1, 0, 0, 0},	// 1
		{0, 1, 0, 0},
		{1, 1, 0, 0},
		{0, 0, 1, 0},
		{1, 0, 1, 0},
		{0, 1, 1, 0},
		{1, 1, 1, 0},
		{0, 0, 0, 1},
		{1, 0, 0, 1},	// 9
		{0, 1, 1, 1}	// disable. def 1 1 1 1
	};
	
	/// TODO change to 7, 5, 4, 6
	uint8_t pin[4] = {8,4,12,7}; //write pins address 
	
public:
	Decoder()
	{
		for (uint8_t i = 0; i < 4; i++) //init pins to decoder
			pinMode(pin[i], OUTPUT);	
	}
	
	inline void setState(uint8_t digit)
	{
		for (uint8_t i = 0; i < 4; i++)	//set state
			digitalWrite(pin[i], state[digit][i]);
	}
};
