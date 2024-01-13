/// Вывод цифр
inline void display(int *nums) // отображение 
{
	for (uint8_t i = 0; i < 6; i++)
	{
		/// TODO упростить setState, поработать над оптимизацией декодера (удалить класс)
		DC.setState(nums[i]);
		
		if (nums[i] != 10)
			digitalWrite(anodes.pin[i], HIGH); 		// enable digit
		delayMicroseconds(anodes.valueON[i]);	// ON time
    //delayMicroseconds(1000);

		digitalWrite(anodes.pin[i], LOW); 			// disable digit
		delayMicroseconds(anodes.valueOFF[i]); 	// OFF time
    //delayMicroseconds(300);
	}
}
