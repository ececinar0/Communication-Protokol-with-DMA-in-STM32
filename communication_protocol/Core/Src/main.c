/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>         //for printf
#include <stdbool.h>       //for bool
#include <string.h>        //for memse

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define MAX_DATA_LEN 12            // Maksimum veri boyutu (başlık + veri boyu + tür + adres + veri)
uint8_t uartHeader[1];             // Başlık (2) + Veri boyu (1) + Tür(1) + Adres + Veri
uint8_t uartData[MAX_DATA_LEN];    // Başlık (2) + Veri boyu (1) + Tür(1) + Adres + Veri
uint8_t dataLen = 0;               // Alınan veri boyutu
//volatile bool dataCheck = false;   // veri kontrolü için değişken
volatile bool headerCheck = false; // Başlık kontrolü için değişken
volatile bool header0 = false;     // başlık 1. bayte kontrolü için değişken
volatile bool header1 = false;     // başlık 2. bayte kontrolü için değişken
volatile bool header2 = false;     // başlık 3. bayte kontrolü için değişken
// Timeout için
volatile uint32_t dmaStartTick = 0;  // DMA başladığında zamanı kaydedeceksin
volatile uint8_t waitingForData = 0; // Şu anda veri bekliyor musun?
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart4;
DMA_HandleTypeDef hdma_uart4_rx;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_UART4_Init(void);
/* USER CODE BEGIN PFP */
void ProcessIncomingData(uint8_t *data, uint8_t len);//main.h dosyasına da yazılabilir.
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) // Mesajın DMA ile alınması
{
	/*
	 * DMA komutu yazılırken veri boyutu belirtilir.
	 * Eğer gelen mesajın veri boyutu, belirtilen değerden küçük olursa(eksik mesaj gelirse) bu fonksiyon çalışmaz ve muhtemelen donma gerçekleşir.
	 * Eğer gelen mesajın veri boyutu, belirtilen değerden büyük olursa(fazla mesaj gelirse) bu veri kaybına sebep olur.
	 * Yani burada belirtilecek olan dataLen, mesajdan az ya da çok olmamalı ---> HAL_UART_Receive_DMA(&huart4, (uint8_t *)uartData, dataLen);
	 * dataLen başlık kısmının boyutunu içermez, kendinden sonra gelecek olan verinin boyutunu belirtir.
	 */
  if (huart->Instance == UART4) // Eğer UART4 kullanılıyorsa
  {
    if (!header0)
    {
      if (uartHeader[0] == 0x45)                               // Başlık kontrolü
        header0 = true;                                        // Başlık kontrolü başarılı
      HAL_UART_Receive_DMA(&huart4, (uint8_t *)uartHeader, 1); // Başlık kısmını dinlemeye devam et
    }
    else if (header0 && !header1)
    {
      if (uartHeader[0] == 0x43) // Başlık kontrolü
        header1 = true;
      else
        header0 = false;                                       // Başlık kontrolü başarısız, başlık kısmını sıfırla
      HAL_UART_Receive_DMA(&huart4, (uint8_t *)uartHeader, 1); // Başlık kısmını dinlemeye devam et
    }
    else if (header0 && header1 && !header2)
    {
      if (uartHeader[0] == 0x45) // Başlık kontrolü
        header2 = true;
      else
      {
    	  header0 = false;
    	  header1 = false;
      }// Başlık kontrolü başarısız, başlık kısmını sıfırla
      HAL_UART_Receive_DMA(&huart4, (uint8_t *)uartHeader, 1); // Başlık kısmını dinlemeye devam et
    }
    else if (header0 && header1 && header2 && !headerCheck) // Başlık kontrolü
    {

      // DATALEN değeri önemli. Eğer gelen veri ondan az ya da çok olursa haberleşme hatası olur ve haberleşme devam etmesi için kartı resetlemek gerekir.

      dataLen = uartHeader[0];    // Veri boyutunu al
      if (dataLen > MAX_DATA_LEN) // Eğer veri boyutu MAX_DATA_LEN'den büyükse
      {
        header0 = false;                                         // Başlık kısmını sıfırla
        header1 = false;                                         // Başlık kısmını sıfırla
        header2 = false;                                         // Başlık kısmını sıfırla
        HAL_UART_Receive_DMA(&huart4, (uint8_t *)uartHeader, 1); // Başlık kısmını dinlemeye devam et
      }
      else
      {
        headerCheck = true;                                          // Başlık kontrolü başarılı
        dmaStartTick = HAL_GetTick();                                // *** Timeout timer'ı başlat ***
        waitingForData = 1;                                          // *** Timeout bekleme modunu aktif et ***
        HAL_UART_Receive_DMA(&huart4, (uint8_t *)uartData, dataLen); // Gelen veriyi al
      }
    }
    else if (headerCheck)
    {
      headerCheck = false; // Başlık kontrolünü sıfırla
      header0 = false;     // Başlık kısmını sıfırla
      header1 = false;     // Başlık kısmını sıfırla
      header2 = false;     // Başlık kısmını sıfırla

      waitingForData = 0; // *** Timeout bekleme modunu kapat ***

      ProcessIncomingData(&uartData[0], dataLen);   // Gelen veriyi işleme fonksiyonunu çağırıyoruz
      HAL_UART_Receive_DMA(&huart4, uartHeader, 1); // Başlık kısmını dinlemeye devam et
    }
  }
}


void ProcessIncomingData(uint8_t *data, uint8_t len) // VERİLERİN İŞLENME FONKSİYONU
{
	  switch (data[0]) // Gelen verinin türüne göre işlem yapılıyor
	  {
	  case 0x53 : //String işlemi
	  {
	   switch (data[1])
				   {
	   case 0x00: // Mesaj yok, default mesajı yazdır.
	   {
		   printf("Mesaj Yok\n");
	   }
	   break;

	   case 0x01: // Haberleşme ile gelen mesajı yazdır.
	   {
		   for (int i = 2; i< len; i++)
			   {
			   printf("%c", data[i]); //ASCII karşılığını karakter olarak yazdırır.
			   /*
			    *printf("%u", data[i]); //Byte değerini yazdırır
			    * printf("%02X ", data[i]); // HEX değerini yazdırır
			    */
			   }
		   printf("\n");
	   }break;

	   default: break;
	   }

	  }break;

	  case 0x0D : //Dijital işlemler
	  {
		  switch(data[1])
		  {
		  case 0x0B : //BLINK LED
		  {
			  if (data[2])
				  HAL_GPIO_WritePin(GPIOB, BLINK_Pin , GPIO_PIN_SET);
			  else
				  HAL_GPIO_WritePin(GPIOB, BLINK_Pin , GPIO_PIN_RESET);
		  }break;

		  default: break;
		  }
	  }break;


	   default: break;
	  }

}

int __io_putchar(int ch) // printf işlevini UART4e yönlendiren fonksiyon
{
  HAL_UART_Transmit(&huart4, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
  return ch;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

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
  MX_DMA_Init();
  MX_UART4_Init();
  /* USER CODE BEGIN 2 */

  HAL_UART_Init(&huart4);
  HAL_UART_Receive_DMA(&huart4, (uint8_t *)uartHeader, 1); // DMA başlatılıyor, uartHeader 1 byte alınacak
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {      
	  if (waitingForData)
      {
        if (HAL_GetTick() - dmaStartTick > 100) // 100 ms örnek, ihtiyaca göre ayarlayabilirsin
        {
          // Timeout oldu, sistem başa dönsün
          header0 = false;
          header1 = false;
          headerCheck = false;
          waitingForData = 0;
          HAL_UART_Receive_DMA(&huart4, uartHeader, 1); // Baştan başla
        }
      }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief UART4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_UART4_Init(void)
{

  /* USER CODE BEGIN UART4_Init 0 */

  /* USER CODE END UART4_Init 0 */

  /* USER CODE BEGIN UART4_Init 1 */

  /* USER CODE END UART4_Init 1 */
  huart4.Instance = UART4;
  huart4.Init.BaudRate = 115200;
  huart4.Init.WordLength = UART_WORDLENGTH_8B;
  huart4.Init.StopBits = UART_STOPBITS_1;
  huart4.Init.Parity = UART_PARITY_NONE;
  huart4.Init.Mode = UART_MODE_TX_RX;
  huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart4.Init.OverSampling = UART_OVERSAMPLING_16;
  huart4.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart4.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart4.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart4) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart4, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart4, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN UART4_Init 2 */

  /* USER CODE END UART4_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMAMUX1_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(BLINK_GPIO_Port, BLINK_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : BLINK_Pin */
  GPIO_InitStruct.Pin = BLINK_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BLINK_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
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
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
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
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
