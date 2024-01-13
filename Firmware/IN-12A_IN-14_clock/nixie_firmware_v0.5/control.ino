/// Control from button and encoder

/* ========================== CONTROL ========================== */
inline void control()
{
	// считывание данных
	long newPos = enc.read();
	bool btnState = digitalRead(BTN);
	
	/// Обработка поворота ручки (энкодер)
	// Вправо - число увеличивается, и наоборот
	if (newPos != oldPos)
	{
		if (encoderAcc == 4)
		{
			bool right = (newPos > oldPos);
			switch(state)
			{
				case CLOCK:
				{
					nextBrightnessState = true;
					break;
				}
				case ADJUST:
				{
					int newAdjust;
					switch(currentPair) // перечисление чисел 0->23, 0->59 (в adjustTime)
					{
						case 0:
							newAdjust = (adjustTime[0]*10 + adjustTime[1] + ((right) ? 1 : (-1))) % 24;
							if (newAdjust == -1) newAdjust = 23;
							adjustTime[0] = newAdjust / 10;
							adjustTime[1] = newAdjust % 10;
							break;
						case 1:
							newAdjust = (adjustTime[2]*10 + adjustTime[3] + ((right) ? 1 : (-1))) % 60;
							if (newAdjust == -1) newAdjust = 59;
							adjustTime[2] = newAdjust / 10;
							adjustTime[3] = newAdjust % 10;
							break;
					}
					break;
				}
				case SETUP:
				{
					switch(currSetup)
					{
						case 1: // корректировка deltaDelay
							
							if (deltaDelay > -9950 && deltaDelay < 9950)
								deltaDelay += (right) ? 50 : -50;
							else
							{
								if (deltaDelay > 0 && !right)
									deltaDelay -= 50;
								else
									if (deltaDelay < 0 && right)
										deltaDelay += 50;
							}
							break;
						case 2: // ON/OFF autobright
							autobright = !autobright;
							break;
						case 3: // время для автосмены яркости
							/// TODO время смены яркости
							
							switch(currentPair)
							{
								case 1: // вечернее время. evetog
									evetog = evetog + ((right) ? 1 : (-1));
									if (evetog == 17) evetog++; // диапазон с 18 вечера
									if (evetog == 3) evetog--;	// до 2 ночи
									if (evetog == -1) evetog = 23;
									if (evetog == 24) evetog = 0;
									break;
								case 2: // утреннее время. mortog
									mortog = mortog + ((right) ? 1 : (-1));
									if (mortog == 4) mortog++; 	// диапазон с 5 утра
									if (mortog == 11) mortog--;	// до 10 дня
									break;
							}
							break;
					}
					break;
				}
			}
			encoderAcc = 0;
		}
		else
			encoderAcc++;
		
		oldPos = newPos;
	}

	/// обработка нажатий кнопки
	if (BTNDOWN == false) // если кнопка НЕ была нажата
	{
		if (btnState == LOW) // если кнопка нажата в данный момент, запускаем таймер
		{
			BTNDOWN = true;
			longCtrlEnter = true;
			controlTimer = millis();
		}
	}
	else // если кнопка была нажата
	{
		unsigned long delta = millis() - controlTimer; // вычисляем сколько была нажата кнопка
	  
		/// короткое нажатие (нажать и отпустить)
		if (delta < 500 && btnState == HIGH)
		{
			switch(state)
			{
			case CLOCK:
				setMaxSaveLast();
				brightRefresh();
				state = SETUP;
				break;
				
			case ADJUST:
				currentPair++;
				if (currentPair == 2) // переход в режим часов когда прошли настройку
				{
					RTC.adjust(DateTime(2012, 12, 12, // настройка времени
					adjustTime[0]*10 + adjustTime[1], 
					adjustTime[2]*10 + adjustTime[3], 
					0));
					
					currentPair = 0;
					state = CLOCK;
					
					setLastBright();
				}
				break;
				
			case SETUP:
				switch(currSetup) // пишем в EEPROM настройки и управляем 3м режимом
				{
					case 1:
						EEPROMWriteInt(0, abs(deltaDelay));
						EEPROM.write(2, (deltaDelay >= 0));
						currSetup++;
						break;
					case 2:
						EEPROM.write(3, autobright);
						currSetup++;
						currentPair = 1; // настройка для след режима
						break;
					case 3:
						if (currentPair == 2) // если последняя пара
						{
							EEPROM.write(4, evetog);
							EEPROM.write(5, mortog);
							setLastBright();
							currSetup = 1;
							state = CLOCK;
						}
						else
						{
							currentPair++;
						}
						break;
				}
				break;
			}
		}
		/// долгое нажатие (нажать и держать)
		if (longCtrlEnter && delta > 800 && btnState == LOW)
		{
			for (int i = 0; i < 6; i++)
				inDimm[i] = 1;
			switch(state)
			{
			case CLOCK:
				currentPair = 0;
				toggleDimm = true;
				
				setMaxSaveLast();
				brightRefresh();
				
				// начальное время для настройки
				adjustTime[0] = now.hour()/10;
				adjustTime[1] = now.hour()%10;
				adjustTime[2] = now.minute()/10;
				adjustTime[3] = now.minute()%10;
				adjustTime[4] = adjustTime[5] = 0; // секунды-нули
				
				state = ADJUST;
				break;
			}
			longCtrlEnter = false;
		}
		
		if (btnState == HIGH)
			BTNDOWN = false;
	}
}