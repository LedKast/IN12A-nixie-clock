inline void adjustClock()
{	
	int freezeTime[6];
	for (int i = 0; i < 6; i++)
		freezeTime[i] = adjustTime[i];
	
	smoothPairBlink();

	display(freezeTime); 	// display time
}

void deltaCorrect() // корректировка времени на deltaDelay
{
	int zeros[6] = {0,0,0,0,0,0};
	unsigned long lastMillis = millis();
	
	if (deltaDelay < 0) // ждем заданное время
	{
		while(millis() - lastMillis < -deltaDelay)
			display(zeros);
		RTC.adjust(DateTime(2012, 12, 12, 0, 0, 0));
	}
	if (deltaDelay > 0) // прибавляем время
	{
		int addsec = deltaDelay/1000;
		
		while(millis() - lastMillis < (1000 - (deltaDelay - addsec*1000))) // ждем 
			display(zeros);
		RTC.adjust(DateTime(2012, 12, 12, 0, 0, addsec + 1));
	}
}