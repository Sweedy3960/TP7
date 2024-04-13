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
//valVref_mV
 
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// ----------------------------------------------------------------
// Conversion tension de LSB en mV. Utilise la variable globale "valVref_mV"
// 0 -> 0 mV
// 4095 -> 3300 mV
uint16_t ConvAdcMilliVolt(uint16_t nLsb)
{
	// *** A COMPLETER ! ***
	// conversion du nombre de pas en mV
	return ((nLsb*8)/10);
}

// ----------------------------------------------------------------
// Conversion de mV en chaine de caract�res (ou structure) exprim�e 
// en V sur nDigits digits. Pr�vu pour tension entre 0 et 3,3V (=> u_mV entre 0 et 3300).
// La sortie contiendra donc unit� + �vt point d�cimal, dixi�mes, centi�mes, milli�mes,
//  selon le nb de digits demand�s.
void ConvMilliVoltVolt(uint16_t u_mV, uint8_t nDigits, char* str_V /* *** OU STRUCTURE *** */)
{	
	int tableau[4] = {0,0,0,0};
	int index = nDigits - 1;

	// Remplir le tableau avec les chiffres du nombre, en partant de la fin
	for (int i = 3; i >= 0; i--)
	{
		tableau[i] = u_mV % 10;
		u_mV /= 10;	
	}

	// G�rer l'arrondi
	if (nDigits < 4 && tableau[nDigits] >= 5)
	{
		// Arrondir
		tableau[index]++;
		while (index > 0 && tableau[index] == 10) 
		{
			// Report si n�cessaire
			tableau[index] = 0;
			tableau[--index]++;
		}

		// G�rer le cas o� le premier chiffre devient 10 apr�s l'arrondi
		if (index == 0 && tableau[index] == 10) 
		{
			tableau[index] = 1;
			for (int i = 1; i < 4; i++) 
			{
				tableau[i] = 0;
			}
			// Cela signifie que nous avons maintenant un chiffre suppl�mentaire � afficher
			if (nDigits > 1) 
			{
				nDigits++;
			}
		}
	}

	// Le premier chiffre avant la virgule d�cimale
	str_V[0] = '0' + tableau[0];

	if (nDigits > 1)
	{
		// Ins�rer le point d�cimal apr�s le premier chiffre
		str_V[1] = '.';
		// Remplir les chiffres apr�s la virgule d�cimale -1 pour le premiere chiffre
		for (int i = 1; i <= (nDigits - 1); ++i)
		{
			str_V[i + 1] = '0' + tableau[i]; // i+1 pour prendre en compte le point d�cimal
		}
	}

	// Ajouter le caract�re nul pour terminer la cha�ne de caractere 
	//+2 pour la virgule et le dernier caractere
	str_V[nDigits + 2] = '\0';
}

// ----------------------------------------------------------------
// *** ADC ***

// Lecture d'un canal AD par polling
// Il faut auparavant avoir configur� la/les pin(s) concern�e(s) en entr�e analogique
uint16_t Adc_read(uint8_t chNr)
{
	// *** A COMPLETER ! ***
	uint16_t a= 0;
	HAL_ADC_PollForConversion(&hadc,1);//timeout 1ms
	
	return HAL_ADC_GetValue(&hadc);
	//a= (&hadc)->Instance->DR;
	//HAL_ADC_Stop(&hadc);
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
	char str_V[20];
	// variable pour tension
	int mV;
	// variable pour nbr de digits
	int8_t digit;
	//variable pour etat switch
	int8_t sw1 = 0;
	int8_t sw2 = 0;
	//variable permetant de faire la detection de flan 
	int8_t previousState1 = 0; 
	int8_t previousState2 = 0;
	static uint16_t ValueReadAdc = 0;
	e_States  state;
	state = INIT;
	
	
	// *** A COMPLETER ! ***
	
	
	//init	
	// *** A COMPLETER ! ***
	
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
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
		// permet d'initialiser le tableau a 0
		for (int i = 0; i < sizeof(str_V); i++) 
		{
			str_V[i] = '\0'; // Initialise chaque caract�re � '\0' (fin de cha�ne)
		}
		// enregistrement de la valeur de switch dans un cas precedent
		previousState1 = sw1;
		previousState2 = sw2;
		switch(state)
		{
			case INIT:
				HAL_ADC_Start(&hadc);
				HAL_ADCEx_Calibration_Start(&hadc);
				printf_lcd("TP AdLcd <2024>");
				lcd_gotoxy(1,2);
				printf_lcd("ACL NBN");
				lcd_bl_on();
				
				/*if (*pt_cntTime> _3SEC)
				{
					state = EXEC;
				}
				*/
				break;
			case EXEC:
				//Adc_read(chNr);
				if((sw1 < previousState1) && (digit > 0))
				{
					digit --;
				}
				if((sw2 < previousState2) && (digit < 4))
				{
					digit ++;
				}
				ConvMilliVoltVolt(mV, digit, str_V); // Exemple d'utilisation
				
				break;
			case IDLE:
				
				break;
		}
		Adc_read(0);
		ValueReadAdc = HAL_ADC_GetValue(&hadc);
	//	ValueReadAdc= DR;
		
		// *** A COMPLETER ! ***
		
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
