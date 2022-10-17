#include <EEPROM.h>
#include "avr/pgmspace.h"
//#include <avr/wdt.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <CyberLib.h>
#include <iarduino_RTC.h> //библиотека часов
//Errom adress 0 - n_phone  1-10 phone1 11-20 phone2 21-30 phone3
#define sms_res 2 // вывод перезагрузки смс

//выводы ТН
#define TN_A 3
#define TN_G 4
#define TN_N 6

//Коэф. трансформации ТН
#define Ktr_TN_A 227
#define Ktr_TN_G 224
#define Ktr_TN_N 233

//кнопки
#define BUTTON_NONE 0 //ничего не нажато
#define BUTTON_RIGHT 1 //нажата кнопка вправо
#define BUTTON_UP 2 //нажата кнопка вверх
#define BUTTON_DOWN 3 //нажата кнопка вниз
#define BUTTON_LEFT 4 //нажата кнопка влево
#define BUTTON_SELECT 5 //нажата кнопка выбор

//мигающий светодиод
#define LED_BLINK 7
#define CHARGE 8
//ежемесячная отправка баланса
#define balance_day 01 //день отправки баланса
#define balance_hour 10 //час отправки баланса
#define balance_minutes 01 //минута отправки баланса
#define balance_minutes1 02 //минута отправки баланса +1
#define balance_reset 00 // минута отправки баланса -1

LiquidCrystal_I2C lcd(0x3F, 16, 2); // Устанавливаем дисплей
iarduino_RTC time(RTC_DS3231); //устанавливаем часы

							   //измерение напряжения

int Utek[3]; //напряжения
byte Chrg = 0; //статус зарядки 0- нет, 1- зарядка
unsigned long Chrg_time = 0; //время зарядки

							 //пременные для временных вычислений
int tmp1;
byte tmp2;
byte tmp3;
byte balance = 0; //сотояние отправки баланса в 10-00 1 числа каждого месяца
int alanogReadValue; //временная для чтения analogRead
unsigned long Tin; //временная для время начала
unsigned long Tout; //временная для время окончания
unsigned long Tin1; //временная для время начала
unsigned long Tout1; //временная для время окончания
unsigned long Usr; //временная для время окончания
int Uaprev; //предшевствующие значения напряжения
int Unprev;
byte type_U = 0; //тип возмущения 0 норм. схема, 1 откл все, 2 откл ввод, 3откл нагр.
byte type_U_prev = 0;
unsigned long T_ot; //временная для время начала
byte timer_on = 0;

//Меню
const char MenuName_0[] PROGMEM = "Ua  Ug  Un      ";
const char MenuName_1[] PROGMEM = "  current time  ";
const char MenuName_2[] PROGMEM = "    n_phone     ";
const char MenuName_3[] PROGMEM = "     phone-1    ";
const char MenuName_4[] PROGMEM = "     phone-2    ";
const char MenuName_5[] PROGMEM = "     phone-3    ";
const char MenuName_6[] PROGMEM = "sms-code   cmd+ ";
const char MenuName_7[] PROGMEM = "Set up Time/Date";
const char* const MenuNames[] PROGMEM = { MenuName_0, MenuName_1, MenuName_2, MenuName_3, MenuName_4, MenuName_5, MenuName_6, MenuName_7 };
byte MenuNowPos = 0; //текущая позиция меню
byte MenuEdit = 0; //редактирование меню 0- просмотр 1- редактирование
byte MenuEditPos = 0; //позиция в символа при редактировании
char menubuffer[16]; // буфер для избежания фрагментации памяти
String ph_buf;
String message; //буфер сообщений
String mess;

void setup() {//Настройка выводов
	Serial.begin(2400);
	Serial.setTimeout(4000);
	time.begin();               //инициализация часов
	pinMode(TN_A, INPUT);      //инициализация ТН
	pinMode(TN_G, INPUT);
	pinMode(TN_N, INPUT);
	pinMode(LED_BLINK, OUTPUT);
	pinMode(CHARGE, OUTPUT); //инициализаци реле заряда
	digitalWrite(CHARGE, HIGH);
	Chrg = 0;
	lcd.noBacklight();            // Включаем подсветку
	pinMode(sms_res, OUTPUT);
	pinMode(13, OUTPUT);
	digitalWrite(sms_res, HIGH);
	delay(200);
	Uaprev = 220;
	Unprev = 220;
	timer_on = 0;
	type_U_prev = 0;
	//пинг sms модуля
	Serial.println(F("AT"));
	Serial.println(F("AT+CMGF=1"));
}

byte getPressedButton() { //процедура определения нажатой кнопки для lcd_keyboard_shield
	Tin = millis();
	alanogReadValue = 1000;
	do {
		if (alanogReadValue > analogRead(6)) {
			alanogReadValue = analogRead(6);
		}
		delay(10);
	} while (abs(millis() - Tin) < 250);
	if (alanogReadValue < 100) {  //0
		return BUTTON_UP;
	}
	else if (alanogReadValue < 400) { //   316
		return BUTTON_DOWN;
	}
	else if (alanogReadValue < 650) {// 593
		return BUTTON_RIGHT;
	}
	else if (alanogReadValue < 820) { // 780
		return BUTTON_LEFT;
	}
	else if (alanogReadValue < 900) { //850
		return BUTTON_SELECT;
	}
	return BUTTON_NONE;
}

void DrawMenu0() { // рисование 0 строки
	lcd.setCursor(0, 0);
	strcpy_P(menubuffer, (char*)pgm_read_word(&(MenuNames[MenuNowPos])));
	lcd.print(menubuffer);
}

void DrawMenu1() {  // рисование 1 строки
	lcd.noCursor();
	lcd.setCursor(0, 1);
	lcd.print("                ");
	if (MenuNowPos == 0) {
		for (tmp1 = 0; tmp1 < 3; tmp1++) {
			lcd.setCursor((4 * tmp1), 1);
			lcd.print(Utek[tmp1]);
		};
	}
	if (MenuNowPos == 1 || MenuNowPos == 7) {                    //!!!часы!!!
		lcd.setCursor(0, 1);
		lcd.print(time.gettime("H:i d-m-Y"));
		if (MenuEdit == 1) {
			lcd.setCursor(MenuEditPos, 1);
			lcd.cursor();
		};
	}
	if (MenuNowPos == 2) {
		lcd.setCursor(7, 1);
		lcd.print(EEPROM.read(0));
		if (MenuEdit == 1) {
			lcd.setCursor(7, 1);
			lcd.cursor();
		};
	}
	if (MenuNowPos == 3 || MenuNowPos == 4 || MenuNowPos == 5) {  //отображение номеров телефонов
		lcd.setCursor(0, 1);
		lcd.print("+7              ");
		for (tmp1 = 0; tmp1 < 10; tmp1++) {
			lcd.setCursor((tmp1 + 2), 1);
			if (MenuNowPos == 3) {
				lcd.print(EEPROM.read(tmp1 + 1));
			};
			if (MenuNowPos == 4) {
				lcd.print(EEPROM.read(tmp1 + 11));
			};
			if (MenuNowPos == 5) {
				lcd.print(EEPROM.read(tmp1 + 21));
			};
		};
		if (MenuEdit == 1) {
			lcd.setCursor(MenuEditPos, 1);
			lcd.cursor();
		};
	};
	if (MenuNowPos == 6) {
		lcd.setCursor(0, 1);
		lcd.print("1- state 2-bal.");
	}
}

int Ua() {
	Usr = 0;
	for (tmp2 = 0; tmp2 < 10; tmp2++) {
		Tin = 0;
		Tout = 0;
		tmp1 = 220;
		Tin = millis();
		while (digitalRead(TN_A) == LOW) {};
		while (digitalRead(TN_A) == HIGH && Tout < 11) {
			Tout = millis() - Tin;
		};
		if (Tout < 11) {
			while (digitalRead(TN_A) == HIGH) {};
			Tin = micros();
			while (digitalRead(TN_A) == LOW) {};
			Tout = micros();

			tmp1 = Ktr_TN_A / (1.414 * cos(0.0001571 * (Tout - Tin)));
			if (tmp1 < 0 || tmp1 > 280) {
				Usr += 220;
			}
			else {
				Usr += tmp1;
			}
		}
		else {
			Usr += 0;
		}
	};
	Usr = Usr / 10;
	return Usr;
}

int Ug() {
	Usr = 0;
	for (tmp2 = 0; tmp2 < 10; tmp2++) {
		Tin = 0;
		Tout = 0;
		Tin = millis();
		tmp1 = 220;
		while (digitalRead(TN_G) == LOW) {};
		while (digitalRead(TN_G) == HIGH && Tout < 11) {
			Tout = millis() - Tin;
		};
		if (Tout < 11) {
			while (digitalRead(TN_G) == HIGH) {};
			Tin = micros();
			while (digitalRead(TN_G) == LOW) {};
			Tout = micros();
			tmp1 = Ktr_TN_G / (1.414 * cos(0.0001571 * (Tout - Tin)));
			if (tmp1 < 0 || tmp1 > 280) {
				Usr += 220;
			}
			else {
				Usr += tmp1;
			}
		}
		else {
			Usr += 0;
		}
	};
	Usr = Usr / 10;
	return Usr;
}

int Un() {
	Usr = 0;
	for (tmp2 = 0; tmp2 < 10; tmp2++) {
		Tin = 0;
		Tout = 0;
		Tin = millis();
		while (digitalRead(TN_N) == LOW) {};
		while (digitalRead(TN_N) == HIGH && Tout < 11) {
			Tout = millis() - Tin;
		};
		if (Tout < 11) {
			while (digitalRead(TN_N) == HIGH) {};
			Tin = micros();
			while (digitalRead(TN_N) == LOW) {};
			Tout = micros();
			tmp1 = Ktr_TN_N / (1.414 * cos(0.0001571 * (Tout - Tin)));
			if (tmp1 < 0 || tmp1 > 280) {
				Usr += 220;
			}
			else {
				Usr += tmp1;
			}
		}
		else {
			Usr += 0;
		}
	};
	Usr = Usr / 10;
	return Usr;
}

void sendSms(byte ph_n, String text) { //отправка соощения text по номеру телефона 1, 2,3, из errom
	ph_buf = "";
	for (tmp2 = 0; tmp2 < 10; tmp2++) {
		if (ph_n == 1) {
			ph_buf = ph_buf + EEPROM.read(tmp2 + 1);
		}

		if (ph_n == 2) {
			ph_buf = ph_buf + EEPROM.read(tmp2 + 11);
		}

		if (ph_n == 3) {
			ph_buf = ph_buf + EEPROM.read(tmp2 + 21);
		}
	}
	Serial.println(F("AT+CMGF= 1")); //set sms to text mode
	delay(250);
	Serial.println(F("AT+CSCS= \"GSM\" "));
	delay(250);
	Serial.print(F("AT+CMGS= \"+7"));
	delay(250);
	Serial.print(ph_buf);
	delay(250);
	Serial.println(F("\""));
	delay(250);
	Serial.print(text);
	delay(1000);
	Serial.print((char)26);
	delay(250);
	Serial.print(F("\r"));
	delay(500);
	Serial.flush();
}

void loop() {
	digitalWrite(LED_BLINK, !digitalRead(LED_BLINK));
	Utek[0] = Ua();
	Utek[1] = Ug();
	Utek[2] = Un();
	if (analogRead(6) < 900) { //переход в меню
		Tin = 0;
		Tout = 0;
		Tin = millis();
		while (analogRead(6) < 900 && Tout < 200) {
			Tout = abs(millis() - Tin);
		}
		lcd.init();                 // Инициализация lcd
		lcd.backlight();            // Включаем подсветку
		if (Tout > 190) {
			MenuNowPos = 0;
			MenuEdit = 0;
			MenuEditPos = 0;
			DrawMenu0();
			DrawMenu1();
			Tin = 0;
			Tout1 = 0;
			Tin1 = millis();
			do {
				tmp2 = MenuNowPos;
				tmp3 = 0;
				switch (getPressedButton()) {
				case BUTTON_RIGHT: // при нажатии кнопки выводим следующий текст
					if (MenuEdit == 0) {
						if (MenuNowPos == 7) {
							MenuNowPos = 0;
						}
						else if (MenuNowPos < 6 && MenuNowPos >(1 + EEPROM.read(0))) {
							MenuNowPos = 6;
						}
						else {
							MenuNowPos++;
						}
					}
					if ((MenuNowPos == 3 || MenuNowPos == 4 || MenuNowPos == 5) && MenuEdit == 1) { //проверить
						MenuEditPos++;
						if (MenuEditPos >= 12) {
							MenuEditPos = 2;
						};
						tmp3 = 1;
					};
					if (MenuNowPos == 7 && MenuEdit == 1) { //set up date time
						if (MenuEditPos == 1) {
							MenuEditPos = 4;
						}
						else if (MenuEditPos == 4) {
							MenuEditPos = 7;
						}
						else if (MenuEditPos == 7) {
							MenuEditPos = 10;
						}
						else if (MenuEditPos == 10) {
							MenuEditPos = 15;
						}
						else if (MenuEditPos == 15) {
							MenuEditPos = 1;
						};
						tmp3 = 1;
					};
					Tin1 = millis();
					break;
				case BUTTON_LEFT:
					if (MenuEdit == 0) {
						if (MenuNowPos == 0) {
							MenuNowPos = 7;
						}
						else if (MenuNowPos == 6) {
							MenuNowPos = 2 + EEPROM.read(0);
						}
						else {
							MenuNowPos--;
						}
					}
					if ((MenuNowPos == 3 || MenuNowPos == 4 || MenuNowPos == 5) && MenuEdit == 1) { //проверить
						MenuEditPos--;
						if (MenuEditPos < 2) {
							MenuEditPos = 11;
						};
						tmp3 = 1;
					};
					if (MenuNowPos == 7 && MenuEdit == 1) { //set up date time
						if (MenuEditPos == 1) {
							MenuEditPos = 15;
						}
						else if (MenuEditPos == 15) {
							MenuEditPos = 10;
						}
						else if (MenuEditPos == 10) {
							MenuEditPos = 7;
						}
						else if (MenuEditPos == 7) {
							MenuEditPos = 4;
						}
						else if (MenuEditPos == 4) {
							MenuEditPos = 1;
						};
						tmp3 = 1;
					};
					Tin1 = millis();
					break;
				case BUTTON_UP:
					if (MenuNowPos == 2 && MenuEdit == 1) { //n_phone Errom(0)
						tmp1 = EEPROM.read(0) + 1;
						if (tmp1 > 3) {
							tmp1 = 0;
						};
						EEPROM.write(0, tmp1);
					}
					if (MenuNowPos == 3 && MenuEdit == 1) { //phone 1 errom(1-10)
						tmp1 = EEPROM.read(MenuEditPos - 1) + 1;
						if (tmp1 > 9) {
							tmp1 = 0;
						};
						EEPROM.write((MenuEditPos - 1), tmp1);
					};
					if (MenuNowPos == 4 && MenuEdit == 1) { //phone 2 errom (11-20)
						tmp1 = EEPROM.read(MenuEditPos + 9) + 1;
						if (tmp1 > 9) {
							tmp1 = 0;
						};
						EEPROM.write((MenuEditPos + 9), tmp1);
					};
					if (MenuNowPos == 5 && MenuEdit == 1) { //phone 3 errom (21-30)
						tmp1 = EEPROM.read(MenuEditPos + 19) + 1;
						if (tmp1 > 9) {
							tmp1 = 0;
						};
						EEPROM.write((MenuEditPos + 19), tmp1);
					};
					if (MenuNowPos == 7 && MenuEdit == 1) {
						tmp1 = 0;
						time.gettime();
						if (MenuEditPos == 1) {
							tmp1 = time.Hours + 1;
							if (tmp1 > 23) {
								tmp1 = 0;
							};
							time.settime(-1, -1, tmp1);
						};
						if (MenuEditPos == 4) {
							tmp1 = time.minutes + 1;
							if (tmp1 > 59) {
								tmp1 = 0;
							};
							time.settime(-1, tmp1);
						};
						if (MenuEditPos == 7) {
							tmp1 = time.day + 1;
							if (tmp1 > 28) {
								tmp1 = 1;
							};
							time.settime(-1, -1, -1, tmp1);

						}
						if (MenuEditPos == 10) {
							tmp1 = time.month + 1;
							if (tmp1 > 12) {
								tmp1 = 1;
							};
							time.settime(-1, -1, -1, -1, tmp1);
						}
						if (MenuEditPos == 15) {
							tmp1 = time.year + 1;
							if (tmp1 > 99) {
								tmp1 = 1;
							};
							time.settime(-1, -1, -1, -1, -1, tmp1);
						}
					}
					tmp3 = 1;
					Tin1 = millis();
					break;
				case BUTTON_DOWN:
					if (MenuNowPos == 2 && MenuEdit == 1) {
						tmp1 = EEPROM.read(0);
						if (tmp1 == 0) {
							tmp1 = 3;
						}
						else {
							tmp1--;
						}
						EEPROM.write(0, tmp1);
					}
					if (MenuNowPos == 3 && MenuEdit == 1) {
						tmp1 = EEPROM.read(MenuEditPos - 1);
						if (tmp1 == 0) {
							tmp1 = 9;
						}
						else {
							tmp1--;
						}
						EEPROM.write((MenuEditPos - 1), tmp1);
					};
					if (MenuNowPos == 4 && MenuEdit == 1) {
						tmp1 = EEPROM.read(MenuEditPos + 9);
						if (tmp1 == 0) {
							tmp1 = 9;
						}
						else {
							tmp1--;
						}
						EEPROM.write((MenuEditPos + 9), tmp1);
					};
					if (MenuNowPos == 5 && MenuEdit == 1) {
						tmp1 = EEPROM.read(MenuEditPos + 19);
						if (tmp1 == 0) {
							tmp1 = 9;
						}
						else {
							tmp1--;
						}
						EEPROM.write((MenuEditPos + 19), tmp1);
					};
					if (MenuNowPos == 7 && MenuEdit == 1) {
						tmp1 = 0;
						time.gettime();
						if (MenuEditPos == 1) {

							if (time.Hours == 0) {
								tmp1 = 23;
							}
							else {
								tmp1 = time.Hours - 1;
							};
							time.settime(-1, -1, tmp1);
						};
						if (MenuEditPos == 4) {
							if (time.minutes == 0) {
								tmp1 = 59;
							}
							else {
								tmp1 = time.minutes - 1;
							};
							time.settime(-1, tmp1);
						};
						if (MenuEditPos == 7) {
							if (time.day == 1) {
								tmp1 = 28;
							}
							else {
								tmp1 = time.day - 1;
							}
							time.settime(-1, -1, -1, tmp1);
						}
						if (MenuEditPos == 10) {
							if (time.month == 1) {
								tmp1 = 12;
							}
							else {
								tmp1 = time.month - 1;
							};
							time.settime(-1, -1, -1, -1, tmp1);
						}
						if (MenuEditPos == 15) {
							if (time.year == 1) {
								tmp1 = 99;
							}
							else {
								tmp1 = time.year - 1;
							};
							time.settime(-1, -1, -1, -1, -1, tmp1);
						}
					}
					tmp3 = 1;
					Tin1 = millis();
					break;
				case BUTTON_SELECT:
					if (MenuEdit == 0 && (MenuNowPos == 2 || MenuNowPos == 3 || MenuNowPos == 4 || MenuNowPos == 5 || MenuNowPos == 7)) {
						MenuEdit = 1;
						if (MenuNowPos == 3 || MenuNowPos == 4 || MenuNowPos == 5) {
							MenuEditPos = 2;
						}
						if (MenuNowPos == 7) {
							MenuEditPos = 1;
						}
					}
					else {
						MenuEdit = 0;
					}
					tmp3 = 1;
					Tin1 = millis();
					break;
				case BUTTON_NONE:
					break;
				}
				if (MenuNowPos != tmp2) {
					DrawMenu0();
					DrawMenu1();
				}
				if (tmp3 == 1 || MenuNowPos == 0) {
					delay(100);
					DrawMenu1();
				}
				Tout1 = abs(millis() - Tin1);
			} while (Tout1 < 10000);
			lcd.noBacklight();
		}
	}
	//зарядка
	if (analogRead(7) < 500) {    //762 (7.35 V) откл.зарядки 683 (6.8 V) вкл.зарядку коэф. тр-ци 0.965
		digitalWrite(CHARGE, LOW);
		Chrg = 1;
		Chrg_time = millis();
	}
	if (Chrg == 1) {
		if (analogRead(7) > 570 || abs(millis() - Chrg_time) > 36000000) { //ограничение по времени 10 часов
			digitalWrite(CHARGE, HIGH);
			Chrg = 0;
		}
	}
	//разбор команд смс
	if (EEPROM.read(0) > 0) { //чтение команд смс
		ph_buf = "";
		for (tmp2 = 0; tmp2 < 10; tmp2++) {
			ph_buf = ph_buf + EEPROM.read(tmp2 + 1);
		}
		Serial.flush();
		message = Serial.readString();
		message = "";
		mess = "";
		Tin = 0;
		Tout = 0;
		tmp1 = 0;
		Tin = millis();
		Serial.flush();

		Serial.println("AT+CMGR=1");
		while (Serial.available() == 0 && Tout < 10000) {
			Tout = millis() - Tin;
		};
		if (Serial.available() > 0) {
			Serial.flush();
			message = Serial.readString();
			delay(50);
			tmp1 = message.length();
		}
		else {
			tmp1 = 0;
		};
		if (Tout < 9900 && tmp1 < 30) {
			Serial.println(F("AT"));
		}
		if (Tout > 9900 && tmp1 < 3) {
			digitalWrite(sms_res, LOW);
			delay(2000);
			digitalWrite(sms_res, HIGH);
			Tout = 0;
			Tout = millis() + 5000;
			while (Serial.available() == 0 && Tout > millis()) {
				Serial.println(F("AT"));
				delay(250);
			}
			delay(2000);
			Serial.println(F("AT"));
			Serial.println(F("AT+CMGF=1"));
		};
		if (tmp1 > 75) {
			Serial.println("AT+CMGDA=\"DEL ALL\"");
			Serial.flush();
			message.remove(0, (message.indexOf(',') + 2));
			message.remove(message.indexOf(','), 5);
			message.setCharAt(12, ',');
			message.remove(27, 7);
			message.remove(message.indexOf('\n', 29));
			if (message.substring(2, 12) == ph_buf) { //!!!!!!!!!
				if (message.substring(29, 33) == "cmd2") { //команда2 проверка баланса
					Serial.println("AT+CUSD=1,\"#100#\""); //запрос баланса #100# транслит мтс
					Tout = 0;
					Tout = millis() + 5000;
					while (Serial.available() == 0 && Tout > millis()) {
						delay(50);
					}
					Serial.flush();
					mess = "";
					mess = Serial.readString();
					mess.remove(0, (mess.indexOf(':') + 6));
					mess.remove(mess.indexOf('"'));
				};
				if (message.substring(29, 33) == "cmd1") { //команда1 статус сети 1
					mess = "Ua=";
					mess = mess + Utek[0];
					mess = mess + " : Ug=";
					mess = mess + Utek[1];
					mess = mess + " : Un=";
					mess = mess + Utek[2];
				};
				sendSms(1, mess);
			};
		};
	}
	if (Utek[0] > 160 && Utek[1] < 160) { //определение тек. сх. сети
		type_U = 0;
	}
	else if (Utek[0] < 160 && Utek[1] < 160) {
		type_U = 1;
	}
	else if (Utek[0] < 160 && Utek[1] > 160) {
		type_U = 2;
	}
	else if (Utek[0] > 160 && Utek[1] > 160) {
		type_U = 3;
	}
	if (type_U != type_U_prev && timer_on != 1) {
		timer_on = 1;
		T_ot = millis();
	}
	if (type_U == type_U_prev && timer_on == 1) {
		timer_on = 0;
	}
	//перезагрузка смс модуля перед отправкой смс
	if (abs(millis() - T_ot) > 15000 && abs(millis() - T_ot) < 30000 && timer_on == 1 && EEPROM.read(0) > 0) {
		digitalWrite(sms_res, LOW);
		delay(2000);
		digitalWrite(sms_res, HIGH);
	}
	//отправка смс по наступлению события
	if ((abs(millis() - T_ot) > 60000 && timer_on == 1 && EEPROM.read(0) > 0)) {
		timer_on = 0;
		message = "";
		type_U_prev = type_U;
		if (Utek[0] < 160) {
			mess = "Svet Otkl / ";
		}
		else {
			mess = "Svet Vkl / ";
		}
		if (Utek[2] < 160) {
			mess = mess + "Nagr Otkl / ";
		}
		else {
			mess = mess + "Nagr Vkl / ";
		}
		if (Utek[1] < 160) {
			mess = mess + "Gen Otkl ";
		}
		else {
			mess = mess + "Gen Vkl ";
		}
		mess = mess + time.gettime("H:i d-m-Y");
		tmp1 = 0;
		tmp1 = EEPROM.read(0);
		if (tmp1 == 1 || tmp1 == 2 || tmp1 == 3) {
			for (tmp3 = 1; tmp3 <= tmp1; tmp3++) {
				sendSms(tmp3, mess);
			}
		}
	};
	//отправка баланса каждый месяц
	time.gettime();
	if (EEPROM.read(0) > 0 && time.day == balance_day && time.Hours == balance_hour && time.minutes == balance_reset) {
		digitalWrite(sms_res, LOW);
		delay(2000);
		digitalWrite(sms_res, HIGH);
	}
	if (EEPROM.read(0) > 0 && time.day == balance_day && time.Hours == balance_hour && time.minutes == balance_minutes && balance == 0) {
		Serial.flush();
		message = Serial.readString();
		message = "";
		balance = 1;
		Serial.println("AT+CUSD=1,\"#100#\""); //запрос баланса #100# транслит мтс
		Tout = 0;
		Tout = millis() + 5000;
		while (Serial.available() == 0 && Tout > millis()) {
			delay(50);
		}
		Serial.flush();
		mess = "";
		mess = Serial.readString();
		mess.remove(0, (mess.indexOf(':') + 6));
		mess.remove(mess.indexOf('"'));
		sendSms(1, mess);
	}
	if (time.day == balance_day && time.Hours == balance_hour && time.minutes == balance_minutes1 && balance == 1) {
		balance = 0;
	}
	Uaprev = Utek[0];
	Unprev = Utek[3];
}