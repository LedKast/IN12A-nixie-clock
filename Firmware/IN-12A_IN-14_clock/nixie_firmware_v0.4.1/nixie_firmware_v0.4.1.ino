/*
 * Nixie Clock software, by LedKast.
 * v0.4.1, 2018
*/

/* CONFLICTS
 * 1) Использование оптопар даёт серьезную проблему -
 * физические задержки оптопары не дают сделать вывод без маленьких задержек
 * Решение - использовать другие оптопары или заменить на транзисторный каскад
*/

/* OPTIMIZATION
 * v0.4.1 - среднее время цикла для часов - 13-14 мс
 *
*/

/* Revision configs
 * Rev v1.0, 
		Part Number: 0001 
			decoder {8,4,12,7}
			anodes {11, 10, 9, 6, 5, A0};
			Отставание в сутки: ~600-800ms
 *
*/

/// TODO перегрузить display, просто передавать параметры а не массив, сильно уменьшит код местами
/// TODO пересмотреть логику диммирования, возможно как-то выражать через коэфф яркости по ф-ле
/// TODO ввести новую версию в отдельном файле
/// TODO разбить проект на кучу файлов для удобства

#include <Wire.h>
#include <RTClib.h>
#include <Encoder.h>
#include "Decoder.h"

/* ============ CONFIGS ============ */

#define LED 13 // пин подсветки
#define BTN 16 	// пин кнопки
#define ENC1 2	// пин_1 энкодера
#define ENC2 3	// пин_2 энкодера

#define ENABLEINTRO false 	// показ демки в начале

#define EVETOGGLE 23 	// вечерний час изменения яркости
#define MORTOGGLE 8 	// утренний час

/// Эффект плавной смены цифр. Задержки должны быть кратны 4!!!
// стандартные задержки ON OFF состояний (микросек)
#define HIGHDELAY 1536; 	// def 1536
#define LOWDELAY 512;		// def 512
 
// скорость эффектов (меньше - быстрее), 
#define INDIMMSPD 28		// стандартная скорость разжигания
#define OUTDIMMSPD 56 		// стандартная скорость затухания

#define DIMMSTART 700 	// условно от 0 до 1000 в мсек. Когда стартует затухание (с момента смены цифры)

// Верхний и нижний предел для задержки переключения
// текущие задержки ON OFF состояний
uint16_t HI = HIGHDELAY;
uint16_t LO = LOWDELAY;

// переключатели эффектов
bool inDimm[] = { 1, 1, 1, 1, 1, 1 }; 	// разжигание
bool outDimm[] = { 0, 0, 0, 0, 0, 0 }; 	// затухание
	
// доп штуки для эффекта обновления времени
long secInterval; // время с последней секунды (мсек)
int prevTime[6] = { 0, 0, 0, 0, 0, 0 }; // предыдущее время

// функции разжигания и затухания
void dimmOut(uint16_t speed = OUTDIMMSPD);
void dimmIn(uint16_t speed = INDIMMSPD);

/// TODO change pins to 12, 11, 10, A2, A1, A0
struct Anodes
{
	// physical pin num (ex 10 = D10))
	//L1, L2 (hours)
	//L3, L4 (min's)
	//L5, L6 (secs)
	uint8_t pin[6] = {
		11, 10,
		9, 6,
		5, A0
	};
	uint16_t valueON[6] = {LO, LO, LO, LO, LO, LO};
	uint16_t valueOFF[6] = {HI, HI, HI, HI, HI, HI};
};

/* ============ VARs DEFINITIONS ============ */

Anodes anodes; 	// Anodes duty settings, per level

Decoder DC;
RTC_DS1307 RTC;
DateTime now;

bool isHiBright = true; // если максимальная яркость

/// Кнопка
unsigned long controlTimer = 0; // время с момента действия управления
bool BTNDOWN = false; 			// если кнопка была нажата
bool longCtrlEnter = false; 	// нужно ли входить в ветку долгого нажатия

/// Энкодер. вправо - число увеличивается, и наоборот
Encoder enc(ENC1, ENC2);
long oldPos = -999;				// старое значение позиции ручки энкодера
uint8_t encoderAcc = 0; 		// отсчёт 4 для энкодера

/// Состояния часов
enum estate{CLOCK, ADJUST, PC}; // часы, настройка, связь с ПК (можно допилить для отображения инфы)
estate state = CLOCK;

/// Настройка времени
uint8_t currentPair = 0; 		// указатель на текущую пару цифр (0, 1, 2)
int adjustTime[6]; 				// массив для настройки времени

/// Мигание цифр
bool isBlink = true; // мигание
unsigned long blinkMillis = 0;

bool toggleDimm = true;
bool nextBrightnessState = false;
bool nextDay = false;

uint16_t anyTestNumber = 0; /// TODO DELETE example

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

void setup() 
{
	Wire.begin();
	RTC.begin();
	
	//Serial.begin(9600); /// закомментить
	
	for (uint8_t i = 0; i < 6; i++) //initialize Anodes pins
		pinMode(anodes.pin[i], OUTPUT);
	pinMode(BTN, INPUT); // пин кнопки
		
	if (!RTC.isrunning())
		RTC.adjust(DateTime(__DATE__, __TIME__)); // time from PC
	//RTC.adjust(DateTime(2012, 12, 12, 23, 59, 54)); // test
	
	secInterval = millis();
	if (ENABLEINTRO)
		intro();
}

/* ========================== MAIN ========================== */

void loop()
{
	//uint16_t startExec = millis(); /// DEBUG
	
	control(); // обработчик нажатия кнопки и поворота ручки
		
	// для версии с транзисторами можно отказаться от такой модели
	// и перейти на эффекты при любой яркости
	if (isHiBright) // когда максимальная яркость - включаем эффекты
	{
		dimmIn(); // разжигание
		dimmOut();
	}
		
	switch(state)
	{
	case CLOCK:
	{
		now = RTC.now(); 	//get all time data
		int currTime[] = { 	// parse time
			now.hour()/10,
			now.hour()%10,
			now.minute()/10,
			now.minute()%10,
			now.second()/10,
			now.second()%10
		};
		
		for (uint8_t i = 0; i < 6; i++)
			if (prevTime[i] != currTime[i]) // если цифра изменилась
			{
				if (i == 5)					// если изменилась секунда
					secInterval = millis(); // запоминаем время последней секунды
				
				inDimm[i] = 1;				// включаем разжигание
			}
		
		// если прошло необходимое время с последней секунды, начинаем затухание
		/// WARNING возможно переполнение таймера! millis()
		unsigned long currMillis = millis();
		if (currMillis - secInterval >= DIMMSTART) 
		{
			// Учёт времени для затухания
			outDimm[5] = 1; 		// вкл XX XX X*
			if (currTime[5] == 9) 		// если XX XX X9
			{
				outDimm[4] = 1; 		// вкл XX XX *X
				if (currTime[4] == 5) 		// если XX XX 59
				{
					outDimm[3] = 1; 		// вкл XX X* XX
					if (currTime[3] == 9) 		// если XX X9 59
					{
						outDimm[2] = 1; 		// вкл XX *X XX
						if (currTime[2] == 5) 		// если XX 59 59
						{
							// смена яркости в заданное время
							if (	(currTime[0] == EVETOGGLE/10 && currTime[1] == EVETOGGLE%10 - 1)
								|| 	(currTime[0] == MORTOGGLE/10 && currTime[1] == MORTOGGLE%10 - 1))
								nextBrightnessState = true;

							outDimm[1] = 1; 		// вкл X* XX XX
							if (currTime[1] == 9 || (currTime[0] == 2 && currTime[1] == 3)) // если X9 59 59 или 23 59 59
							{
								outDimm[0] = 1; 		// вкл *X XX XX
								
								if (currTime[0] == 2) // если 23 59 59
									nextDay = true;
							}
						}
					}
				}
			}
		}
		else
		{
			if (nextDay) // смена дня
				if (currTime[5] != prevTime[5])
				{
					/// TODO эффект смены с 23 на 00 (как в игровом автомате, слева направо) и корректировка на deltaDay
					/// TODO препроцессором сделать настройку, выбирать код для установки -delta или +delta
					intro();
					nextDay = false;
				}
			if (nextBrightnessState) // смена яркости
				if (currTime[5] != prevTime[5])
				{
					// повышение яркости баг - выполняется outDimm
					if (isHiBright)
						setLowBrightness();
					else
					{
						for (uint8_t i = 0; i < 6; i++)
						{
							outDimm[i] = 0;
							inDimm[i] = 1;
						}
						HI = HIGHDELAY;
						LO = LOWDELAY;
						isHiBright = true;
					}
					nextBrightnessState = false;
				}
		}
		
		display(currTime); 	// display time
		
		for (int i = 0; i < 6; i++)
			prevTime[i] = currTime[i];
			
		break;
	}
	case ADJUST:
	{
		adjustClock();
		break;
	}
	case PC:
	{
		int example[6];
		uint16_t crr = anyTestNumber;
		for (int i = 5; i >= 0; i--)
		{
			example[i] = crr%10;
			crr /= 10;
		}
		display(example);
		
		break;
	}
	}
	
	//Serial.println(millis() - startExec);  /// DEBUG
}

/* ========================== CONTROL ========================== */

inline void control()
{
	static bool lastLow; // если последний режим яркости был low
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
					/// TODO изменение яркости подсветки
					
					toggleBrightness();
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
				case PC:
				{
					(right) ? anyTestNumber++ : anyTestNumber--;
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
			/// TODO настройка времени корректировки (либо +delta либо -delta, это определяется при компиляции)
			switch(state)
			{
			case CLOCK:
				state = PC;
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
				break;
			case PC:
				state = CLOCK;
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
				
				if (!isHiBright) // если яркость была на минимуме
				{
					lastLow = true;
					setHighBrightness();
				}
				else
					lastLow = false;
				
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

inline void display(int *nums) // отображение 
{
	for (uint8_t i = 0; i < 6; i++)
	{
		/// TODO упростить setState, поработать над оптимизацией декодера (удалить класс)
		DC.setState(nums[i]);
		
		if (nums[i] != 10)
			analogWrite(anodes.pin[i], 255); 		// enable digit
		delayMicroseconds(anodes.valueON[i]);	// ON time
		
		analogWrite(anodes.pin[i], 0); 			// disable digit
		delayMicroseconds(anodes.valueOFF[i]); 	// OFF time
	}
}

inline void dimmIn(uint16_t speed = INDIMMSPD) // плавное разжигание
{
	for (uint8_t i = 0; i < 6; i++)
		if (inDimm[i])
			{
				if (anodes.valueON[i] < HI && anodes.valueOFF[i] > LO) 
				{
					anodes.valueON[i] += speed;
					anodes.valueOFF[i] -= speed;
				}
				else // если дошли до конца
				{
					anodes.valueON[i] = HI;
					anodes.valueOFF[i] = LO;
					inDimm[i] = 0;
				}
			}
}

inline void dimmOut(uint16_t speed = OUTDIMMSPD) // плавное затухание
{
	for (uint8_t i = 0; i < 6; i++)
		if (outDimm[i])
			{
				if (anodes.valueON[i] > LO && anodes.valueOFF[i] < HI) 
				{
					anodes.valueON[i] -= speed;
					anodes.valueOFF[i] += speed;
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

inline void adjustClock()
{	
	int freezeTime[6];
	for (int i = 0; i < 6; i++)
		freezeTime[i] = adjustTime[i];
	
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

	display(freezeTime); 	// display time
}

inline void toggleBrightness() // не особо нужная функция так то
{
  (isHiBright) ? setLowBrightness() : setHighBrightness();
}

/// TODO переключение яркости и включение диммирования
// inline void setLowAndDimm()
// {
// }
// inline void setHiAndDimm()
// {
// }

inline void setLowBrightness()	// минимальная яркость
{
	HI = LOWDELAY;
	LO = HIGHDELAY;
	for (int i = 0; i < 6; i++)
	{
		anodes.valueON[i] = HI;
		anodes.valueOFF[i] = LO;
	}
	isHiBright = false;
}

inline void setHighBrightness() // максимальная яркость
{
	HI = HIGHDELAY;
	LO = LOWDELAY;
	for (int i = 0; i < 6; i++)
	{
		anodes.valueON[i] = HI;
		anodes.valueOFF[i] = LO;
	}
	isHiBright = true;
}