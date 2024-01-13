
inline void dimmIn() // плавное разжигание
{
	for (uint8_t i = 0; i < 6; i++)
		if (inDimm[i])
		{
			if (anodes.valueON[i] < HI && anodes.valueOFF[i] > LO) 
			{
				anodes.valueON[i] += INDIMMSPD;
				anodes.valueOFF[i] -= INDIMMSPD;
			}
			else // если дошли до конца
			{
				anodes.valueON[i] = HI;
				anodes.valueOFF[i] = LO;
				inDimm[i] = 0;
			}
		}
}

inline void dimmOut() // плавное затухание
{
	for (uint8_t i = 0; i < 6; i++)
		if (outDimm[i])
		{
			if (anodes.valueON[i] > LO && anodes.valueOFF[i] < HI) 
			{
				anodes.valueON[i] -= OUTDIMMSPD;
				anodes.valueOFF[i] += OUTDIMMSPD;
			}
			else // если дошли до конца
			{
				anodes.valueON[i] = LO;
				anodes.valueOFF[i] = HI;
				outDimm[i] = 0;
			}
		}
}

bool blink() // вернет true если надо выкл лампу
{
	int32_t delta = millis() - blinkMillis;
	if (isBlink && delta > 80) // не горит
	{
		blinkMillis = millis();
		isBlink = !isBlink;
	}
	else
		if (!isBlink && delta > 650) // горит
		{
			blinkMillis = millis();
			isBlink = !isBlink;
		}
	return isBlink;
}

void smoothPairBlink()
{
	if (toggleDimm)
	{
		if (!inDimm[currentPair*2])
		{
			outDimm[currentPair*2] = outDimm[currentPair*2 + 1] = 1;
			toggleDimm = !toggleDimm;
		}
	}
	else
		if (!outDimm[currentPair*2])
		{
			inDimm[currentPair*2] = inDimm[currentPair*2 + 1] = 1;
			toggleDimm = !toggleDimm;
		}
}