/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "17400.h"
#include "stm32delays.h"
#include "stm32driverlcd.h"
#include "time.h"
#include "stm32f0xx_it.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */


/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint16_t valVref_mV=3300;
e_States  state;
e_States *pt_state =  &state;
int8_t digit = 1;
int8_t *pt_digit  = &digit;
bool firstTime = true;
bool *pt_firstTime=&firstTime;
bool flagCalibrage = false;

bool *pt_flagCalibrage=&flagCalibrage;
uint16_t calibrationValue = 3300;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// ----------------------------------------------------------------
// Conversion tension de LSB en mV. Itilise la variable globale "valVref_mV"
// 0 -> 0 mV
// 4095 -> 3300 mV
uint16_t ConvAdcMilliVolt(uint16_t nLsb)
{
		//conversion du nombre de pas en mV
	
		//modif calcul pour utilisation de val ref + pas d'utilisation de float donc * 100 rester avec resultats entier 
		// engendre une erreur (apres la , dans le calcul)
		return ((((((valVref_mV*100)/ADCMAXVAL))*nLsb))/100);
	
	
	
	
		//return ((nLsb*8)/10);
}

// ----------------------------------------------------------------
// Conversion de mV en chaine de caractères (ou structure) exprimée 
// en V sur nDigits digits. Prévu pour tension entre 0 et 3,3V (=> u_mV entre 0 et 3300).
// La sortie contiendra donc unité + évt point décimal, dixièmes, centièmes, millièmes,
//  selon le nb de digits demandés.
char ConvMilliVoltVolt(uint16_t u_mV, uint8_t nDigits, char* str_V /* *** OU STRUCTURE *** */)
{	
	int tableau[4] = {0,0,0,0};
	int index = nDigits - 1;

	// Remplir le tableau avec les chiffres du nombre, en partant de la fin
	for (int i = 3; i >= 0; i--)
	{
		tableau[i] = u_mV % 10;
		u_mV /= 10;	
	}

	// Gérer l'arrondi
	if (nDigits < 4 && tableau[nDigits] >= 5)
	{
		// Arrondir
		tableau[index]++;
		while (index > 0 && tableau[index] == 10) 
		{
			// Report si nécessaire
			tableau[index] = 0;
			index--;
			tableau[index]++;
		}

		// Gérer le cas où le premier chiffre devient 10 après l'arrondi
		if (index == 0 && tableau[index] == 10) 
		{
			tableau[index] = 1;
			for (int i = 1; i < 4; i++) 
			{
				tableau[i] = 0;
			}
			// Cela signifie que nous avons maintenant un chiffre supplémentaire à afficher
			if (nDigits > 1) 
			{
				nDigits++;
			}
		}
	}

	// Le premier chiffre avant la virgule décimale
	str_V[0] = '0' + tableau[0];

	if (nDigits > 1)
	{
		// Insérer le point décimal après le premier chiffre
		str_V[1] = '.';
		// Remplir les chiffres après la virgule décimale -1 pour le premiere chiffre
		for (int i = 1; i <= (nDigits - 1); ++i)
		{
			str_V[i + 1] = '0' + tableau[i]; // i+1 pour prendre en compte le point décimal
		}
	}

	// Ajouter le caractère nul pour terminer la chaîne de caractere 
	//+2 pour la virgule et le dernier caractere
	str_V[nDigits + 2] = '\0';
	return 0;
}

// ----------------------------------------------------------------
// *** ADC ***

// Lecture d'un canal AD par polling
// Il faut auparavant avoir configuré la/les pin(s) concernée(s) en entrée analogique
uint16_t Adc_read(uint8_t chNr)
{	
	//démarage périph
	HAL_ADC_Start(&hadc);
	//calibration ADC 
	HAL_ADCEx_Calibration_Start(&hadc);
	
	if(!HAL_ADC_PollForConversion(&hadc,5))
	{//timeout 5ms
		return HAL_ADC_GetValue(&hadc);
	}
	//a= (&hadc)->Instance->DR;
	HAL_ADC_Stop(&hadc);
	return 0;
}


void SetStatus(void)
//Cette fonction change l'état de la machine lors de son appel
{
	
	switch (*pt_state)
	{
		case INIT:
			*pt_state = EXEC;
			break;
		case EXEC:
			*pt_state = IDLE;
			break;
		case IDLE:
			*pt_state = EXEC;
			break;
	  default:
			break;
	}
}
void GetTimeFlag(char *tb_portEntree)
//cette fonction vérifie si le flag de 5ms est actif pour calculer les timmings demander 
//de plus la lecture des entré est faite lorsque que le flag est actif ce qui permet d'avoir 
//une base de temps pour les echantillons du buffer d'entrée au détriment du temps de réaction
{
	static bool InitialisationHasOccured = false;
	static uint16_t cntTime =0;
	//test du flag
	if(flag5Ms)
	{
		//incrémetn compteur de temps 
		cntTime++;
		//si le flag d'initialisatione a eu lieu  est actif 
		if (InitialisationHasOccured)
		{
			// le temps a compter est 50ms
			if (cntTime>=_50MSEC)
			{
				//appel de la fonction pour changer d'état 
				SetStatus();
				//remise à 0 du cnt de temps
				cntTime=0;
			}
		}
		else
		{	
			//autrement l'initialisation dure 3s
			if (cntTime>=_3SEC)
			{
				//appel de la fonction pour changer d'état 
				SetStatus();
				//remise à 0 du cnt de temps
				cntTime=0;
				//set du flag d'initialisatione a eu lieu : actif
				InitialisationHasOccured=true;
			}
		}
		//appel de la fonction de lecture d'entrée
		readInput(tb_portEntree);
	}
	//remise à 0 du flag te temps
	flag5Ms=false; 
	
}
void initialisation(void)
//Affichage durant l'initialisation partie LCD 
{
	
	printf_lcd("TP AdLcd <2024>");
	lcd_gotoxy(1,2);
	printf_lcd("Clauzel aymeric");
	lcd_bl_on();
}



void InputActions(char *tb_portEntree)
{
	//Annalyse buffer entrée
		char i;
		char cntflanc=0;
		char edgeUp = 0;
		char edgeDown =0;
	  uint32_t a=2;
		
		
		//balayage du tableau 
		for (i=0; i<_500MSEC; i++ )
		{
			//si la case est différente de la suivante 
			if (tb_portEntree[i] != tb_portEntree[(i+1)])
			{
				//test si premier flanc 
				if(cntflanc)
				{
					//save n° case de tableau
					edgeDown =i;
					
				}
				else
				{
					// save n° case de tableau
					edgeUp = i;
					cntflanc++;
				}
			}
		}
		//le temps dappuis se trouve entre les flanc (nbcases*Tempsentrechaquecase) 
		//Test pour pulse temps actif moin 500ms et pas 0 
		if (((edgeDown - edgeUp) != 0)&& edgeDown)
		{

			//Fonction des switch actif incrément ou décrémente le mode ou la base de temps ou les DEU
			// Gestion 2 touche appuyee en mm temps
			switch (~(tb_portEntree[edgeDown])&0x0F)
			{
				case S2 :
					//change l'état du flag pour la deuxieme ligne / premier appuis sur btn0
					*pt_firstTime =!(*pt_firstTime);
					//selon létat du flag allume led 0 ou éteind toutes
					if(*pt_firstTime){
					GPIOC -> ODR |= (LEDS);
					}
					else{
					GPIOC -> ODR &= ~(LED0);
					}
					//pas besoin de tester mais chaque appuis fait sortir de la calibration 
					flagCalibrage = false;
					
					break;
					
				case S3 :
					//si en mode calib
					if(flagCalibrage)
					{
						//décrémente
						calibrationValue -=5;
					
					}
					else
						
					{
						//si ligne 2 active 
						if (!*pt_firstTime)
						{
							//test limites valeurs digits
							if(*pt_digit>1)
							{
								//décrémente 
								*pt_digit -= 1 ;
								//************************************
								//traitement de leds 
								for(int i=1;i<=*pt_digit;i++)
								{
									a *=2;
								}
								GPIOC -> ODR |= (((a)/2)<<4);							
							}
						}
					}
					
					break;
					
				case S4 :
					//si en mode calib
					if(flagCalibrage)
					{
						//incrémente
						calibrationValue +=5;
					
					}
					else
					{
						//si ligne 2 active 
						if(!*pt_firstTime)
						{
							//test limites valeurs digits
							if(*pt_digit < 4)
							{
								//incrémente
								*pt_digit += 1 ;
								//************************************
								//traitement de leds 
								for(int i=1;i<*pt_digit;i++)
								{
									a *=2;
								}
								GPIOC -> ODR &= ~(((a)/2)<<4);
							}			
						}
					}
					break;
				
				case S5 :
					//pas besoin de tester mais chaque appuis fait sortir de la calibration 
					flagCalibrage = false;
					//set la nouvelle valeure de calib 
					valVref_mV = calibrationValue;
					break;

					
				default:
					break;
			}				
		}
		else
		{
			//test si un le premer flanc lors de l'appuis est apparus mais pas le deuxieme 
			//--> plus de 500ms appuyer  
			if(edgeUp && !edgeDown)
			{
				//test pour savoir quelle touche 
			   switch (~(tb_portEntree[edgeDown])&0x0F)
				 {
						case S2 :
							//"active la calibration "
							flagCalibrage = true;
							//reset de la valeur de calibration
							calibrationValue = 3300;
							break;
				 }
			}
		}
	
}
void readInput(char *tb_portEntree)
// cette fonction remplis toute les 5ms une case du tableau avec le port d'entrée
//une fois plein le tableau est annalyser, la taille du tableau est faite pour les limites cdc 500ms
{
	
		static uint16_t i=0;
		//test pour pas déborder 
		if(i<=_500MSEC)
		{
			//remplissage du tableau 
			tb_portEntree[i+1] = tb_portEntree[i];
			tb_portEntree[i] = GPIOC-> IDR & 0x0F;	
			i++;
		}
		else
		{
			i=0;
			//Buffer plein annalyse
			InputActions(tb_portEntree);
		}
}


void exec(char *tb_portEntree,char *str_V)
{
				static uint16_t valueAdc;
				//clean de la ligne 
				lcd_clearLine(2);
				lcd_gotoxy(1,0);
				//lecture de lla valeure du potentiometre
				valueAdc=Adc_read(0);
				//affichage de la valeur
				printf_lcd("AI0 :%d / %d mv ",valueAdc,ConvAdcMilliVolt(valueAdc));
			
				//test si ligne 2 inactive
				if (*pt_firstTime)
				{
					//nettoye la ligne
					lcd_clearLine(2);
				}
				else
				{
					//si le mode calibration est actif
					if(flagCalibrage)
					{
						//-----affichage mode calib*********
						lcd_gotoxy(1,0);
						printf_lcd("*** Calibration *** ");
						lcd_gotoxy(1,2);
						printf_lcd("%d",calibrationValue);
					}
					else
					{
						//-----affichage mode normal*********
					  ConvMilliVoltVolt(ConvAdcMilliVolt(valueAdc), *pt_digit, str_V); // Exemple d'utilisation
						lcd_clearLine(2);
						lcd_gotoxy(1,2);
						printf_lcd("%s V",str_V);
					}
				}
				//appel fct pour repartir en IDLE
				SetStatus();
}


// ----------------------------------------------------------------


/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
	//variables
	char str_V[20]={0};
	state = INIT;
	char tb_portEntree[_500MSEC] = {NULL};
	
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
	
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM6_Init();
  MX_ADC_Init();
  /* USER CODE BEGIN 2 */
	HAL_TIM_Base_Start_IT(&htim6);
	lcd_init();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	
		
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

		switch(state)
		{
			case INIT:
				initialisation();
				break;
			case EXEC:			
				exec(tb_portEntree,str_V);
				break;
			case IDLE:
				break;
			default: 
				break;
		}
		//appel fct pour base de temps et donc lecture des entrée d'ou le tableau en paramètres
		GetTimeFlag(tb_portEntree);	
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI14|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSI14State = RCC_HSI14_ON;
  RCC_OscInitStruct.HSI14CalibrationValue = 16;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL6;
  RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV1;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
  /** Enables the Clock Security System
  */
  HAL_RCC_EnableCSS();
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */

  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
