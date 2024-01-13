
inline void toggleBrightness() // не особо нужная функция так то
{ (isHiBright) ? setLowBrightness() : setHighBrightness(); }

inline void setLowBrightness()	// минимальная яркость
{
	swapDelays();
	setValues();
	isHiBright = false;
}

inline void setHighBrightness() // максимальная яркость
{
	swapDelays();
	setValues();
	isHiBright = true;
}

inline void setValues() // низкая яркость
{
	for (int i = 0; i < 6; i++)
	{
		anodes.valueON[i] = HI;
		anodes.valueOFF[i] = LO;
	}
}

inline void brightRefresh() // восстанавливает макс яркость
{
	if (isHiBright)
		for (int i = 0; i < 6; i++)
		{
			inDimm[i] = 1;
			outDimm[i] = 0;
		}
}

inline void setMaxSaveLast() // установка макс яркости + сохранение ласт режима
{
	if (!isHiBright) // если яркость была на минимуме
	{
		lastLow = true;
		setHighBrightness();
	}
	else
		lastLow = false;
}

inline void setLastBright() // вернуть последнее состояние яркости
{
	if (!lastLow)
		for (int i = 0; i < 6; i++) // уменьшаем яркость, и включаем Dimm In
		{
			inDimm[i] = 1;
			anodes.valueON[i] = LO;
			anodes.valueOFF[i] = HI;
		}
	else
		setLowBrightness(); // если яркость должна быть low
}