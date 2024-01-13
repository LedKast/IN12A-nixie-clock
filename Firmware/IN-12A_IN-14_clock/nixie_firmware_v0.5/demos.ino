/// Any demos for clock

inline void intro(uint16_t d = 1200) // demo intro
{
	randomSeed(analogRead(A1)); // пинание генератора пустым пином
	
	unsigned long currMillis = millis();
	int interv = 0;
	while (millis() - currMillis < d) // время показа
	{
		unsigned long insideMillis = millis();
		int map[] = {
			random(0, 10), random(0, 10), random(0, 10), 
			random(0, 10), random(0, 10), random(0, 10)
		};
		while (millis() - insideMillis < interv)
			display(map);
		interv += 15;
	}
}