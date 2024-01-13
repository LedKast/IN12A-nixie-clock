#include <Wire.h>
#include <RTClib.h>
#include <Encoder.h>
#include <EEPROM.h>
#include "Decoder.h"

/*
 * Nixie Clock software, by LedKast.
 * v0.5, 2018

 * CONFLICTS
 * 1) Использование оптопар даёт серьезную проблему -
 * физические задержки оптопары не дают сделать вывод без маленьких задержек
 * Решение - использовать другие оптопары или заменить на транзисторный каскад

 * Revision configs
 * Rev v1.0, 
		Part Number: 0001
			decoder {8,4,12,7}
			anodes {11, 10, 9, 6, 5, A0};
			Отставание в сутки: ~600-800ms
 *
*/

/// TODO перегрузить display, просто передавать параметры а не массив, сильно уменьшит код местами
/// TODO пересмотреть логику диммирования, возможно как-то выражать через коэфф яркости по ф-ле

/* ============ CONFIGS ============ */

#define LED 13 // пин подсветки
#define BTN 16 	// пин кнопки
#define ENC1 2	// пин_1 энкодера
#define ENC2 3	// пин_2 энкодера

#define ENABLEINTRO false 	// показ демки в начале

#define EVETOGGLE 23 	// вечерний час изменения яркости
#define MORTOGGLE 8 	// утренний час

// Эффект плавной смены цифр. Задержки должны быть кратны 4!!!
// максимальные задержки ON OFF состояний (микросек)
#define MAXHIGHDELAY 1536; 	// def 1536
#define MAXLOWDELAY 512;		// def 512
 
// скорость эффектов (меньше - быстрее), 
#define INDIMMSPD 28		// стандартная скорость разжигания
#define OUTDIMMSPD 56 		// стандартная скорость затухания

#define DIMMSTART 700 	// условно от 0 до 1000 в мсек. Когда стартует затухание (с момента смены цифры)

// сдвиг в день, в мс. при положительном значении прибавляется, при отрицательном отнимается
#define DELTA 650

/* ============ DEFINITIONS ============ */

/// Определения функций
void dimmOut();
void dimmIn();
void control();
void intro(uint16_t d = 1200);
void display(int *nums);
void dimmIn();
inline void dimmOut();
bool blink();
void toggleBrightness();
void setLowBrightness();
void setHighBrightness();
void adjustClock();
void EEPROMWriteInt(int p_address, int p_value);
unsigned int EEPROMReadInt(int p_address);
void brightRefresh();
void setMaxSaveLast();
void setLastBright();
void deltaCorrect();
void smoothBlink();

// Верхний и нижний предел для задержки переключения
// текущие задержки ON OFF состояний
/// TODO рефактор в offDelay и onDelay
uint16_t HI = MAXHIGHDELAY;
uint16_t LO = MAXLOWDELAY;
	
/// доп штуки для эффекта обновления времени
long secInterval; // время с последней секунды (мсек)
int prevTime[6] = { 0, 0, 0, 0, 0, 0 }; // предыдущее время

/// TODO change pins to 12, 11, 8, A2, A1, A0
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

Anodes anodes; 	// Anodes duty settings, per level

Decoder DC;
RTC_DS1307 RTC;
DateTime now;

/// Состояния часов
enum estate{CLOCK, ADJUST, SETUP}; // часы, настройка, связь с ПК (можно допилить для отображения инфы)
estate state = CLOCK;

/// Кнопка
unsigned long controlTimer = 0; // время с момента действия управления
bool BTNDOWN = false; 			// если кнопка была нажата
bool longCtrlEnter = false; 	// нужно ли входить в ветку долгого нажатия

/// Энкодер. вправо - число увеличивается, и наоборот
Encoder enc(ENC1, ENC2);
long oldPos = -999;				// старое значение позиции ручки энкодера
uint8_t encoderAcc = 0; 		// отсчёт 4 для энкодера

/// переключатели эффектов
bool inDimm[] = { 1, 1, 1, 1, 1, 1 }; 	// разжигание
bool outDimm[] = { 0, 0, 0, 0, 0, 0 }; 	// затухание

/// Мигание цифр
bool isBlink = true; // мигание
unsigned long blinkMillis = 0;

/// Яркость
bool isHiBright = true; // если максимальная яркость
bool nextBrightnessState = false;
bool lastLow; // если последний режим яркости был low
bool autobright = true; // автосмена яркости вкл/выкл

/// Настройка времени
uint8_t currentPair = 0; 		// указатель на текущую пару цифр (0, 1, 2)
int adjustTime[6]; 				// массив для настройки времени
bool toggleDimm = true;

/// Other
bool nextDay = false;
uint16_t currSetup = 1;

// байты в EEPROM - [0,1]
int16_t deltaDelay = DELTA; // сдвиг в день, в мс. 0 -delta, 1 +delta

bool startOut = true; 		// флаг того, что надо начать outDimm. Введен для оптимизации

int8_t evetog = EVETOGGLE; 	// вечерний час изменения яркости
int8_t mortog = MORTOGGLE; 	// утренний час

/* ========================== SETUP ========================== */

void setup() 
{
	Wire.begin();
	RTC.begin();
	
	//Serial.begin(9600); /// закомментить
	
	for (uint8_t i = 0; i < 6; i++) //initialize Anodes pins
		pinMode(anodes.pin[i], OUTPUT);
	pinMode(BTN, INPUT); // пин кнопки
		
	if (!RTC.isrunning())
		RTC.adjust(DateTime(2012, 12, 12, 11, 11, 11)); // стартовое время
		//RTC.adjust(DateTime(__DATE__, __TIME__)); // time from PC
	
	/// чтение из EEPROM
	// delta - [0,1][2]; autobright - [3]; eve - [4], mor - [5]
	deltaDelay = EEPROMReadInt(0); // чтение настройки deltaDelay
	if (!EEPROM.read(2)) // если отрицательно
		deltaDelay = -deltaDelay;
	autobright = EEPROM.read(3);
	evetog = EEPROM.read(4);
	mortog = EEPROM.read(5);
	
	secInterval = millis();
	if (ENABLEINTRO)
		intro();
}

/* ========================== MAIN ========================== */

void loop()
{
fastloop:
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
				{
					startOut = true;
					secInterval = millis(); // запоминаем время последней секунды
				}
				inDimm[i] = 1;				// включаем разжигание
			}
		
		// если прошло необходимое время с последней секунды, начинаем затухание
		if (startOut)
		{
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
								if (autobright)
									if (	(currTime[0] == evetog/10 && currTime[1] == evetog%10 - 1) || 
											(currTime[0] == mortog/10 && currTime[1] == mortog%10 - 1))
										nextBrightnessState = true;

								outDimm[1] = 1; 		// вкл X* XX XX
								if (currTime[1] == 9 || (currTime[0] == 2 && currTime[1] == 3)) // если X9 59 59 или 23 59 59
								{
									outDimm[0] = 1; 		// вкл *X XX XX
									
									if (currTime[0] == 2) // если 23 59 59
									{
										nextDay = true;
										if (autobright && evetog == 0)
											nextBrightnessState = true;
									}
								}
							}
						}
					}
				}
				startOut = false;
			}
		}
		if (nextDay) // если смена дня
			if (currTime[5] != prevTime[5])
			{
				/// TODO эффект смены с 23 на 00 (как в игровом автомате, слева направо) и корректировка на deltaDelay
				deltaCorrect();
				//intro();
				nextDay = false;
			}
		if (nextBrightnessState) // если смена яркости
			if (currTime[5] != prevTime[5])
			{
				/// к сожаление с высокой на низкую нельзя сделать переход
				/// из-за отключения эффектов при низкой яркости
				if (isHiBright)
				{
					isHiBright = false;
					setLowBrightness();
				}
				else
				{
					for (uint8_t i = 0; i < 6; i++)
					{
						outDimm[i] = 0;
						inDimm[i] = 1;
					}
					swapDelays();
					isHiBright = true;
				}
				nextBrightnessState = false;
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
	case SETUP:
	{
		// use SETUPCOUNT, deltaDelay
		int example[6];
		uint16_t curr = abs(deltaDelay);
		example[0] = currSetup;
		example[1] = 10;
		
		//Serial.println(deltaDelay);
		switch(currSetup)
		{
			case 1: // корректировка deltaDelay
				example[5] = curr%10;
				example[4] = (curr/10)%10;
				example[3] = (curr/100)%10;
				example[2] = (curr/1000)%10;
				break;
			case 2: // ON/OFF autobright
				example[2] = example[3] = example[4] = 10;
				example[5] = autobright;
				break;
			case 3: // время для автосмены яркости
				smoothPairBlink();
				example[5] = mortog%10;
				example[4] = mortog/10;
				example[3] = evetog%10;
				example[2] = evetog/10;
				break;
			default:
				break;
		}
		
		display(example);
		break;
	}
	}
	
	//Serial.println(millis() - startExec);  /// DEBUG
	goto fastloop;
}

// обмен задержек для ON OFF состояний
inline void swapDelays()
{
	uint16_t temp = HI;
	HI = LO;
	LO = temp;
}