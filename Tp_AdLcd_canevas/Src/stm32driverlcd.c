/*--------------------------------------------------------*/
//	stm32driverlcd.c
/*--------------------------------------------------------*/
//	Description :		Driver pour LCD sur carte ARM (STM32) 17400
//   Code bas� sur driver LCD pour carte PIC32
//
//  Cible :
//   Mod�les LCD NHD-C0220AA-FSW-FTW et NHD-C0220BA-FSW-FTW-3V3
//   Carte 17400 (versions A-B-C-D) KitArmM0
//
//  Versions :
//	 no version/versions outils/auteur/date/description
//   -v1 / Keil 5.24 / SCA / 12.2020 
//	  Version d'origine (kits v A-B-C avec NHD-C0220AA-FSW-FTW
//   -v2 / Keil 5.33, compilateur C 5.06 / SCA / 10.2022
//    Adaptation pour kit version D avec LCD NHD-C0220BA-FSW-FTW-3V3
//    (l'ancien LCD est devenu obsol�te)
//    La version du kit (et donc le mod�le de LCD) est reconnue
//		� l'init du LCD.
//   -v2.01 / Keil 5.33, compilateur C 5.06 / SCA / 02.2023
//    Ajout activation clk GPIO C et D dans l'init du lcd
//    Sinon, si on ne configure pas graphiquement les GPIO
//		dans STM32CubeMX, les GPIO ne sont pas utilisables
//
/*--------------------------------------------------------*/

#include "stm32f0xx.h"
#include <stdio.h>
#include <stdbool.h>

#include "stm32driverlcd.h"
#include "stm32delays.h"


//pins de comm avec le LCD
#define LCD_DB4_Pin GPIO_PIN_8
#define LCD_DB5_Pin GPIO_PIN_9
#define LCD_DB6_Pin GPIO_PIN_10
#define LCD_DB7_Pin GPIO_PIN_11
#define LCD_E_Pin   GPIO_PIN_12
#define LCD_RW_Pin  GPIO_PIN_13
#define LCD_RS_Pin  GPIO_PIN_14
#define LCD_BL_Pin  GPIO_PIN_15
#define LCD_RST_Pin	GPIO_PIN_2	//port D ! Reset uniquement pr�sent sur mod�le LCD NHD-C0220BA-FSW-FTW-3V3

/*--------------------------------------------------------*/
// 2 diff�rents types de LCD suivant la version du kit
typedef enum
{
	LCD_MODEL_NHD_C0220AA_FSW_FTW=0,		//kit 17400 version A, B, C
  LCD_MODEL_NHD_C0220BA_FSW_FTW_3V3,	//kit 17400 � partir de version D
} LCD_MODELS;

/*--------------------------------------------------------*/
// D�finition du tableau pour l'adresse des lignes
// Pour les 2 affichages : 
//  ligne 0 pas utilis�, ligne 1 commence au caract�re 0, ligne 2 commence au caract�re 64 (0x40)
const uint8_t taddrLines[3] = {0, 0, 0x40};
																
/*--------------------------------------------------------*/
LCD_MODELS lcdModel;																
																
/*--------------------------------------------------------*/
// Fonctions
/*--------------------------------------------------------*/
//uint8_t lcd_read_byte( void )
//{
//	uint8_t readVal;
//	//UINT8_BITS lcd_read_byte;
////  LCD_DB4_T = 1; // 1=input
////  LCD_DB5_T = 1;
////  LCD_DB6_T = 1;
////  LCD_DB7_T = 1;	
//	GPIOC->MODER = GPIOC->MODER & 0xFF00FFFF;	//mettre 4 signaux DB7..DB4 en entr�e
//	GPIOC->BSRR = LCD_RW_Pin; //LCD_RW_W = 1;
//	delay500ns(); //ds0066 demande 0.5us
//	GPIOC->BSRR = LCD_E_Pin; //LCD_E_W = 1;
//	delay500ns(); //ds0066 demande 0.5us
////      lcd_read_byte.bits.b7 = LCD_DB7_R;
////      lcd_read_byte.bits.b6 = LCD_DB6_R;
////      lcd_read_byte.bits.b5 = LCD_DB5_R;
////      lcd_read_byte.bits.b4 = LCD_DB4_R;
//	readVal = (GPIOC->IDR & 0x0F00) >> 4;
//	GPIOC->BRR = LCD_E_Pin; //LCD_E_W = 0; // attention e pulse min = 500ns � 1 et autant � 0
//	delay500ns();
//	GPIOC->BSRR = LCD_E_Pin; //LCD_E_W = 1;
//	delay500ns();
////	lcd_read_byte.bits.b3 = LCD_DB7_R;
////	lcd_read_byte.bits.b2 = LCD_DB6_R;
////	lcd_read_byte.bits.b1 = LCD_DB5_R;
////	lcd_read_byte.bits.b0 = LCD_DB4_R;
//  readVal |= (GPIOC->IDR & 0x0F00) >> 8;
//	GPIOC->BRR = LCD_E_Pin; //LCD_E_W = 0;
//	delay500ns();
////  LCD_DB4_T = 0; // 0=output
////  LCD_DB5_T = 0;
////  LCD_DB6_T = 0;
////  LCD_DB7_T = 0;
//	GPIOC->MODER = GPIOC->MODER | 0x00550000; //remettre 4 signaux DB7..DB4 en sortie
//		
//	return(readVal); //return(lcd_read_byte.Val);
//}

/*--------------------------------------------------------*/
// Lecture du busy flag de l'afficheur
// Retour : 1 si occup�, 0 sinon
//
// ATTENTION :
// -sur l'afficheur NHD-C0220AA-FSW-FTW,
//  en mode de comm. 4 bits, on lit d'abord les 4 LSB, puis 
//  les 4 MSB (donc le busy flag sur D7) arrive a la 2eme 
//  validation de l'enable
//  voir datasheet du contoleur LCD Novatek NT7605 
//  version >= 2.2 (la 2.1 avait une erreur pr�cis�ment au 
//  check du BF en interface 4 bits).
//  Il est possible que ce contr�leur NT7605 soit non 
//  compatible avec le standard Hitachi 44780 au niveau
//  de la lecture du BF.
// -sur l'afficheur NHD_C0220BA_FSW_FTW_3V3 (version carte 17400 >= D)
//  le contr�le du BF est avec les MSB en premier
bool lcd_checkbf( void )
{
	uint8_t readVal;

	GPIOC->MODER = GPIOC->MODER & 0xFF00FFFF;	//mettre 4 signaux datas DB7..DB4 en entr�e
	
	GPIOC->BRR = LCD_RS_Pin; 	//signal RS � 0
	GPIOC->BSRR = LCD_RW_Pin; //signal RW � 1 (lecture)
	
	//1ere pulse validation sur signal E (les LSB ne sont pas lus)
	delay500ns(); //pulse enable � 1 min 300ns, p�riode min 500ns (LCD old NHD-C0220AA-FSW-FTW)
	GPIOC->BSRR = LCD_E_Pin; //signal Enable � 1
	delay500ns(); //pulse enable � 1 min 300ns, p�riode min 500ns	(LCD old NHD-C0220AA-FSW-FTW)
	if (lcdModel == LCD_MODEL_NHD_C0220BA_FSW_FTW_3V3)
		readVal = (GPIOC->IDR >> 11) & 0x0001;	//check BF	
	GPIOC->BRR = LCD_E_Pin; //signal Enable � 0
	
	//2eme pulse validation sur signal E (les MSB contenant le BF sont lus)	
	delay500ns();	//pulse enable � 1 min 300ns, p�riode min 500ns	(LCD old NHD-C0220AA-FSW-FTW)
	GPIOC->BSRR = LCD_E_Pin;	//signal Enable � 1
	delay500ns();	//pulse enable � 1 min 300ns, p�riode min 500ns	(LCD old NHD-C0220AA-FSW-FTW)
	//readVal = (GPIOC->IDR >> 11) & 0x0001;	//check BF
	if (lcdModel == LCD_MODEL_NHD_C0220AA_FSW_FTW)
		readVal = (GPIOC->IDR >> 11) & 0x0001;	//check BF
	GPIOC->BRR = LCD_E_Pin; 	//signal Enable � 0
	//delay500ns();
	
	GPIOC->MODER = GPIOC->MODER | 0x00550000; //remettre 4 signaux DB7..DB4 en sortie
		
	return(readVal == 0x0001);
}

/*--------------------------------------------------------*/
// Envoi d'un nibble (4 bits) et pulse validation enable
void lcd_send_nibble( uint8_t n )   
{
	uint16_t writeVal;
	
	//met en sortie les 4 bits � �crire
	writeVal = GPIOC->ODR;	//lecture port
	writeVal &= 0xF0FF;			//masquage bits � changer
	writeVal |= (uint16_t)n << 8;			//met nouveaux bits
	GPIOC->ODR = writeVal;	//r�crit nouvelle valeur
	
	//toggle signal enable pour validation des datas � �crire
	//delay500ns();	//pulse enable � 1 min 300ns, p�riode min 500ns (LCD old NHD-C0220AA-FSW-FTW)
	//pulse enable � 1 (Attention datasheet LCD new NHD-C0220BA-FSW-FTW-3V3 et controlleur int�grt ST7036
	// erron�s ou lacunaire : une impulsion de 500 ns ne suffit pas !)
	delay_us(2);		
	GPIOC->BSRR = LCD_E_Pin;	//signal Enable � 1
	//delay500ns(); //pulse enable � 1 min 300ns, p�riode min 500ns (LCD old NHD-C0220AA-FSW-FTW)
	//pulse enable � 1 (Attention datasheet LCD new NHD-C0220BA-FSW-FTW-3V3 et controlleur int�grt ST7036
	// erron�s ou lacunaire : une impulsion de 500 ns ne suffit pas !)
	delay_us(2);	
	GPIOC->BRR = LCD_E_Pin;	//signal Enable � 0  
}

/*--------------------------------------------------------*/
// Envoi d'un octet en 2 nibbles
// La ligne RS est pilot�e selon param�tre "address"
// (1 pour �criture donn�e, 0 pour �criture registre config
void lcd_send_byte( uint8_t data, uint8_t n )
{
	while (lcd_checkbf());	//attente lcd plus busy
	
	//pilotage ligne de ctrl RS
	if (data)
		GPIOC->BSRR = LCD_RS_Pin;	//signal RS � 1 (data)
	else
		GPIOC->BRR  = LCD_RS_Pin;	//signal RS � 0 (commande)
	
	GPIOC->BRR = LCD_RW_Pin;	//signal RW � 0 (�criture)
	//LCD_E d�j� � 0
	lcd_send_nibble(n >> 4);	//�criture MSB
	lcd_send_nibble(n & 0xf);	//�criture LSB
}

/*--------------------------------------------------------*/
// Init LCD en interface 4 bits
// Fonction pr�vue pour affichages LCD
//  NHD_C0220AA_FSW_FTW (cartes 17400 versions A-B-C), contr�leur NT6705
//  et NHD_C0220BA_FSW_FTW_3V3 (cartes 17400 versions >= D), contr�leur ST7036
// Ce 2� mod�le a un signal reset en plus. Sinon tous les autres
//  c�bl�s sur les memes pins du MCU (voir sch�mas pour d�tails).
// Le mod�le de carte est automatiquement d�tect� � l'init et les
//  quelques diff�rences dans la comm. avec tel ou tel LCD prises
//  en compte.
// Les 2 LCD ont un contr�leur diff�rent. Pour les d�tails de comm.,
//  les datasheet New Haven Display sont lacunaires. Il faut aller
//  voir directement les datasheet des contr�leurs LCD.
void lcd_init(void)
{
	//active clk des GPIO C et D
	//RCC->AHBENR |= (RCC_AHBENR_GPIOCEN | RCC_AHBENR_GPIODEN); 
	// ou
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();
	
	//d�termination de la r�vision du kit
	GPIOD->MODER = (GPIOD->MODER & 0xFFFFFFCF);	//r�glage pin /RST (PD2) en entr�e (MODER � 00)	
	GPIOD->PUPDR = (GPIOD->PUPDR & 0xFFFFFFCF) | 0x00000020;//activation weak pull-down interne (PUDR � 10)
	delay_us(10);	//Pour �tablissement niveaux
	//lecture signal logique /RST du LCD en entr�e
	// par le jeu de la pull-down interne et de la pull-up externe (uniquement pr�sente
	// � partir de carte 17400 D pour LCD_MODEL_NHD_C0220BA_FSW_FTW_3V3), on obtient 
	// un niveau '0' sur PD2 sur les cartes 17400 A-B-C et '1' pour version >=D
	if (GPIOD->IDR & 0x04)	
		lcdModel = LCD_MODEL_NHD_C0220BA_FSW_FTW_3V3; //kit 17400 version >=D
	else
		lcdModel = LCD_MODEL_NHD_C0220AA_FSW_FTW;	//kit 17400 version A, B, C
	
	//r�glage GPIOs
	GPIOD->PUPDR = (GPIOD->PUPDR & 0xFFFFFFCF);	//d�sactivation pull-down PD2
	GPIOD->MODER = (GPIOD->MODER & 0xFFFFFFCF) | 0x00000010;	//signal LCDRST (PD2) en sortie
	GPIOC->MODER = (GPIOC->MODER & 0x0000FFFF) | 0x55550000; //mettre 8 signaux LCD en sortie
	
	//mise au repos des signaux ctrl
	GPIOC->BRR = LCD_E_Pin;	//signal Enable � 0	
	GPIOC->BRR = LCD_RW_Pin;	//signal RW � 0 (�criture)		
	GPIOC->BRR = LCD_RS_Pin;	//signal RS � 0, demand� pour une commande	
	
	//pulse signal reset (n'existe pas sur tous les mod�les de LCD)
	if (lcdModel == LCD_MODEL_NHD_C0220BA_FSW_FTW_3V3) //kit 17400 version >=D
	{
		GPIOD->BRR = LCD_RST_Pin;	//signal RST actif � 0
		delay_us(200);	//selon datasheet controleur 100 us min
		GPIOD->BSRR = LCD_RST_Pin;	//RST � 1	
  }
	//Attente apr�s reset : selon datasheet 40 ms min pour LCD_MODEL_NHD_C0220BA_FSW_FTW_3V3
	// 30 ms min pour LCD_MODEL_NHD_C0220AA_FSW_FTW
	delay_ms(50);		
	
	//s�quence d'init
	switch(lcdModel)
	{
		case LCD_MODEL_NHD_C0220AA_FSW_FTW:
			//function set : interface 4 bits, 2 lignes, police 5x8 px
			lcd_send_nibble(0x02); // correspond � 0x20 en interface 8 bits
			lcd_send_nibble(0x08); //
			lcd_send_nibble(0x02); // correspond � 0x20 en interface 8 bits	
			lcd_send_nibble(0x08); //
					
			delay_us(40); //wait > 40us

			//Display on/off control
			lcd_send_nibble(0x00); // 
			lcd_send_nibble(0x0C); // display on, cursor off, blink off 	
			//lcd_send_nibble(0x0F); // display on, cursor on, blink on 

			delay_us(40); //wait > 40us

			// clear display
			lcd_send_nibble(0x00); 
			lcd_send_nibble(0x01);  

			delay_ms(2); //wait > 1.64 ms

			//entry mode set
			lcd_send_nibble(0x00);  
			//lcd_send_nibble(0x02); //increment mode, entire shift off
			lcd_send_nibble(0x06); //increment mode, entire shift off*/
			break;
		
		case LCD_MODEL_NHD_C0220BA_FSW_FTW_3V3:
                             
			//command 0x30 = Wake up	
			lcd_send_nibble(0x03);
			delay_ms(2);
																	
			//command 0x30 = Wake up #2
			lcd_send_nibble(0x03);		
			delay_us(30);
				                             
			//command 0x30 = Wake up #3
			lcd_send_nibble(0x03);
			delay_us(30);			
			
			//Function set: 4-bit interface
			lcd_send_nibble(0x02);	
			delay_us(30);
			
			//Function set: 4-bit/2-line
			lcd_send_nibble(0x02);
			lcd_send_nibble(0x08);
			delay_us(30);
			
			//Set cursor
			lcd_send_nibble(0x10);
			lcd_send_nibble(0x00);
			delay_us(30);
			
			//Display ON; Blinking cursor
			lcd_send_nibble(0x00);
			lcd_send_nibble(0x0C);
			delay_us(30);
			
			//Entry Mode set
			lcd_send_nibble(0x00);
			lcd_send_nibble(0x06);
			delay_us(30);
			break;
		
		default:
			// on ne devrait pas arriver ici !
			break;
	}			
}

/*--------------------------------------------------------*/
// Positionnement du curseur ligne y, colonne x
// La num�rotation commence � 1
void lcd_gotoxy( uint8_t x, uint8_t y)
{
	uint8_t address;
	address = taddrLines[y];
	address+=x-1;
	lcd_send_byte(0,0x80|address);
}

/*--------------------------------------------------------*/
//Ecriture un caract�re
void lcd_putc( uint8_t c)
{
	switch (c) 
	{
    	case '\f'   : 	lcd_gotoxy(1,2);	break;  	// formfeed (nouvelle page) => d�but ligne 2
     	case '\n'   : 	lcd_gotoxy(1,2);    break;	//retour � la ligne => d�but ligne 2
     	//case '\b'   : 	lcd_send_byte(0,0x10);	break; //backspace
     	default     : 	lcd_send_byte(1,c);	break;
   }
}

/*--------------------------------------------------------*/
void lcd_put_string_ram( char *ptr_char )
{
	while (*ptr_char != 0) 
	{
    	lcd_putc(*ptr_char);
    	ptr_char++;
  	}
}

/*--------------------------------------------------------*/
// Allumage backlight
void lcd_bl_on( void )
{
	GPIOC->BSRR = LCD_BL_Pin;	//LCD_BL_W = 1;
}

/*--------------------------------------------------------*/
// Extinction backlight
void lcd_bl_off( void )
{
	GPIOC->BRR = LCD_BL_Pin;	//LCD_BL_W = 0;
} 

/*--------------------------------------------------------*/
// Fonction compatible printf
// Auteur C. Huber 15.05.2014
void printf_lcd( const char *format,  ...)
{
    char Buffer[21];
    va_list args;
    va_start(args, format);

    vsprintf(Buffer, format, args);
    lcd_put_string_ram(Buffer);

    va_end(args);
}

/*--------------------------------------------------------*/
// Efface une ligne (par �criture 20 espace)
void lcd_clearLine(uint8_t lineNr)
{
    uint8_t  i;
    if (lineNr >= 1 && lineNr <= 2)  {

        lcd_gotoxy( 1, lineNr) ;
        for (i = 0 ; i < 20 ; i++)
        {
            lcd_send_byte(1,0x20);
        }
     }
}

/*--------------------------------------------------------*/
// Efface le LCD (par �criture instruction)
void lcd_clearScreen(void)	// clear display
{    
	lcd_send_byte(0, 0x01);
	delay_ms(2);
}

