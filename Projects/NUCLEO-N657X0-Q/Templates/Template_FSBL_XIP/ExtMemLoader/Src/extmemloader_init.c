/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    extmemloader_init.c
  * @author  MCD Application Team
  * @brief   This file defines the system initialisation of an external loader.
  *
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
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
#include "extmemloader_init.h"
#include <string.h>
#include <stdio.h>
#include "extmem_manager.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

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
XSPI_HandleTypeDef hxspi2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/

static void MX_XSPI2_Init(void);
#if !defined(STM32_EXTMEMLOADER_STM32CUBEOPENBLTARGET)
static void SystemClock_Config(void);
#endif

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* Write Enable (0x06F9) + Write CR2 a 'addr' = 'val', en octal (DTR si dtr!=0, sinon STR) */
static void Recovery_WrCR2(uint8_t dtr, uint32_t addr, uint8_t val)
{
  XSPI_RegularCmdTypeDef c;
  uint8_t data[2];
  data[0] = val; data[1] = 0x00U;

  /* Write Enable octal */
  (void)memset(&c, 0, sizeof(c));
  c.OperationType      = HAL_XSPI_OPTYPE_COMMON_CFG;
  c.IOSelect           = HAL_XSPI_SELECT_IO_7_0;
  c.InstructionMode    = HAL_XSPI_INSTRUCTION_8_LINES;
  c.InstructionWidth   = HAL_XSPI_INSTRUCTION_16_BITS;
  c.InstructionDTRMode = dtr ? HAL_XSPI_INSTRUCTION_DTR_ENABLE : HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  c.Instruction        = 0x06F9U;
  c.AddressMode        = HAL_XSPI_ADDRESS_NONE;
  c.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  c.DataMode           = HAL_XSPI_DATA_NONE;
  c.DQSMode            = HAL_XSPI_DQS_DISABLE;
  (void)HAL_XSPI_Command(&hxspi2, &c, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);

  /* Write Configuration Register 2 (0x728D) */
  (void)memset(&c, 0, sizeof(c));
  c.OperationType      = HAL_XSPI_OPTYPE_COMMON_CFG;
  c.IOSelect           = HAL_XSPI_SELECT_IO_7_0;
  c.InstructionMode    = HAL_XSPI_INSTRUCTION_8_LINES;
  c.InstructionWidth   = HAL_XSPI_INSTRUCTION_16_BITS;
  c.InstructionDTRMode = dtr ? HAL_XSPI_INSTRUCTION_DTR_ENABLE : HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  c.Instruction        = 0x728DU;
  c.AddressMode        = HAL_XSPI_ADDRESS_8_LINES;
  c.AddressDTRMode     = dtr ? HAL_XSPI_ADDRESS_DTR_ENABLE : HAL_XSPI_ADDRESS_DTR_DISABLE;
  c.AddressWidth       = HAL_XSPI_ADDRESS_32_BITS;
  c.Address            = addr;
  c.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  c.DataMode           = HAL_XSPI_DATA_8_LINES;
  c.DataDTRMode        = dtr ? HAL_XSPI_DATA_DTR_ENABLE : HAL_XSPI_DATA_DTR_DISABLE;
  c.DataLength         = dtr ? 2U : 1U;
  c.DQSMode            = HAL_XSPI_DQS_DISABLE;
  if (HAL_XSPI_Command(&hxspi2, &c, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) == HAL_OK)
  {
    (void)HAL_XSPI_Transmit(&hxspi2, data, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
  }
  HAL_Delay(10);
}

/* --- Mini sortie UART brute sur USART1 (PE5/PE6, AF7) = VCP COM13 a 115200 --- */
static void Recovery_UartInit(void)
{
  GPIO_InitTypeDef g = {0};
  uint32_t clk;
  __HAL_RCC_GPIOE_CLK_ENABLE();
  g.Pin = GPIO_PIN_5 | GPIO_PIN_6;
  g.Mode = GPIO_MODE_AF_PP; g.Pull = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_VERY_HIGH; g.Alternate = GPIO_AF7_USART1;
  HAL_GPIO_Init(GPIOE, &g);
  __HAL_RCC_USART1_CLK_ENABLE();
#if defined(RCC_PERIPHCLK_USART1)
  clk = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_USART1);
#else
  clk = 0U;
#endif
  if (clk == 0U) { clk = HAL_RCC_GetPCLK2Freq(); }
  USART1->CR1 = 0U;
  USART1->BRR = (clk + 57600U) / 115200U;
  USART1->CR1 = USART_CR1_TE | USART_CR1_UE;
}
static void Recovery_UartStr(const char *s)
{
  while (*s != '\0')
  {
    while ((USART1->ISR & USART_ISR_TXE_TXFNF) == 0U) {}
    USART1->TDR = (uint8_t)(*s++);
  }
}
static void Recovery_UartHex(uint8_t b)
{
  static const char h[] = "0123456789ABCDEF";
  while ((USART1->ISR & USART_ISR_TXE_TXFNF) == 0U) {}
  USART1->TDR = (uint8_t)h[(b >> 4) & 0xFU];
  while ((USART1->ISR & USART_ISR_TXE_TXFNF) == 0U) {}
  USART1->TDR = (uint8_t)h[b & 0xFU];
  while ((USART1->ISR & USART_ISR_TXE_TXFNF) == 0U) {}
  USART1->TDR = (uint8_t)' ';
}

/* Telemetrie LED : lit l'ID JEDEC en OPI/DTR puis en SPI, et signale via les LED
 * user (PG0=vert, PG10=rouge) pendant ~8s :
 *   - VERT seul clignote      -> la puce repond en OPI (ID=0xC2) : vivante en Octal
 *   - VERT+ROUGE clignotent   -> la puce repond en SPI
 *   - ROUGE seul clignote     -> aucune reponse (ni OPI ni SPI) */
static void Recovery_Telemetry(void)
{
  XSPI_RegularCmdTypeDef c;
  uint8_t idopi[4] = {0};
  uint8_t idspi[4] = {0};
  int opi_ok = 0, spi_ok = 0, k;
  GPIO_InitTypeDef g = {0};

  __HAL_RCC_GPIOG_CLK_ENABLE();
  g.Pin = GPIO_PIN_0 | GPIO_PIN_10;
  g.Mode = GPIO_MODE_OUTPUT_PP; g.Pull = GPIO_NOPULL; g.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOG, &g);
  HAL_GPIO_WritePin(GPIOG, GPIO_PIN_0 | GPIO_PIN_10, GPIO_PIN_RESET);

  Recovery_UartInit();
  Recovery_UartStr("\r\n=== REC TELEMETRY ===\r\n");

  /* Read ID en OPI/DTR (0x9F60) : on essaie plusieurs nb de cycles dummy et
   * DQS on/off, car la valeur exacte depend de la config de la puce. */
  {
    const uint32_t dummies[5] = {20U, 5U, 8U, 16U, 4U};
    int di, qi;
    for (qi = 0; (qi < 2) && (opi_ok == 0); qi++)
    {
      for (di = 0; (di < 5) && (opi_ok == 0); di++)
      {
        idopi[0] = 0U;
        (void)memset(&c, 0, sizeof(c));
        c.OperationType = HAL_XSPI_OPTYPE_COMMON_CFG; c.IOSelect = HAL_XSPI_SELECT_IO_7_0;
        c.InstructionMode = HAL_XSPI_INSTRUCTION_8_LINES; c.InstructionWidth = HAL_XSPI_INSTRUCTION_16_BITS;
        c.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_ENABLE; c.Instruction = 0x9F60U;
        c.AddressMode = HAL_XSPI_ADDRESS_8_LINES; c.AddressDTRMode = HAL_XSPI_ADDRESS_DTR_ENABLE;
        c.AddressWidth = HAL_XSPI_ADDRESS_32_BITS; c.Address = 0U;
        c.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
        c.DataMode = HAL_XSPI_DATA_8_LINES; c.DataDTRMode = HAL_XSPI_DATA_DTR_ENABLE; c.DataLength = 4U;
        c.DummyCycles = dummies[di];
        c.DQSMode = (qi == 0) ? HAL_XSPI_DQS_ENABLE : HAL_XSPI_DQS_DISABLE;
        if (HAL_XSPI_Command(&hxspi2, &c, 100U) == HAL_OK)
        {
          if (HAL_XSPI_Receive(&hxspi2, idopi, 100U) == HAL_OK)
          {
            if (idopi[0] == 0xC2U) { opi_ok = 1; }
          }
        }
      }
    }
  }

  /* Read ID en SPI (0x9F, 3 octets) */
  (void)memset(&c, 0, sizeof(c));
  c.OperationType = HAL_XSPI_OPTYPE_COMMON_CFG; c.IOSelect = HAL_XSPI_SELECT_IO_7_0;
  c.InstructionMode = HAL_XSPI_INSTRUCTION_1_LINE; c.InstructionWidth = HAL_XSPI_INSTRUCTION_8_BITS;
  c.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE; c.Instruction = 0x9FU;
  c.AddressMode = HAL_XSPI_ADDRESS_NONE; c.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  c.DataMode = HAL_XSPI_DATA_1_LINE; c.DataLength = 3U; c.DummyCycles = 0U; c.DQSMode = HAL_XSPI_DQS_DISABLE;
  if (HAL_XSPI_Command(&hxspi2, &c, 100U) == HAL_OK)
  {
    if (HAL_XSPI_Receive(&hxspi2, idspi, 100U) == HAL_OK)
    {
      if (idspi[0] == 0xC2U) { spi_ok = 1; }
    }
  }

  /* Trace UART (repetee pour capture facile sur COM13) */
  for (k = 0; k < 6; k++)
  {
    Recovery_UartStr("OPI ");
    Recovery_UartHex(idopi[0]); Recovery_UartHex(idopi[1]); Recovery_UartHex(idopi[2]);
    Recovery_UartStr(opi_ok ? "(OK) " : "(--) ");
    Recovery_UartStr("SPI ");
    Recovery_UartHex(idspi[0]); Recovery_UartHex(idspi[1]); Recovery_UartHex(idspi[2]);
    Recovery_UartStr(spi_ok ? "(OK)\r\n" : "(--)\r\n");
    HAL_Delay(300);
  }

  /* Signalisation LED ~8s */
  for (k = 0; k < 16; k++)
  {
    if (opi_ok)
    {
      HAL_GPIO_TogglePin(GPIOG, GPIO_PIN_0);
      HAL_GPIO_WritePin(GPIOG, GPIO_PIN_10, GPIO_PIN_RESET);
    }
    else if (spi_ok)
    {
      HAL_GPIO_TogglePin(GPIOG, GPIO_PIN_0);
      HAL_GPIO_TogglePin(GPIOG, GPIO_PIN_10);
    }
    else
    {
      HAL_GPIO_TogglePin(GPIOG, GPIO_PIN_10);
      HAL_GPIO_WritePin(GPIOG, GPIO_PIN_0, GPIO_PIN_RESET);
    }
    HAL_Delay(400);
  }
}

/* Recovery : sort la flash du mode Octal (OPI), volatile ET non-volatile, en
 * essayant DTR-OPI puis STR-OPI. CR2@0x40000000=0 efface le boot-Octal
 * (DEFSOPI/DEFDOPI non-volatile), CR2@0x00000000=0 sort de l'OPI maintenant. */
static void Recovery_ExitOPI(void)
{
  Recovery_WrCR2(1U, 0x40000000U, 0x00U);  /* DTR : annule boot-Octal non-volatile */
  Recovery_WrCR2(1U, 0x00000000U, 0x00U);  /* DTR : sort de l'OPI -> SPI */
  Recovery_WrCR2(0U, 0x40000000U, 0x00U);  /* STR : idem si la puce etait en SOPI */
  Recovery_WrCR2(0U, 0x00000000U, 0x00U);
  HAL_Delay(10);
}

/* USER CODE END 0 */

/**
  * @brief  main entry of the external flash loader initialization.
  * @param  None
  * @retval  0 : operation is ok else operation is failed

  */
uint32_t extmemloader_Init()
{
  uint32_t retr = 0;
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

#if !defined(STM32_EXTMEMLOADER_STM32CUBEOPENBLTARGET)
  /* Init system */
  SystemInit();

  /* disable all the IRQ */
  __disable_irq();

  /* MCU Configuration--------------------------------------------------------*/
  /* Enable the CPU Cache */

  /* Enable I-Cache---------------------------------------------------------*/
  SCB_EnableICache();

  /* Enable D-Cache---------------------------------------------------------*/
  SCB_EnableDCache();

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock  */
  SystemClock_Config();
#endif

 /* MPU Configuration--------------------------------------------------------*/
  HAL_MPU_Disable();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */

  MX_XSPI2_Init();

  MX_EXTMEM_MANAGER_Init();

  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  return retr;
}

/**
  * @brief  System Clock Configuration
  *         The system Clock is configured as follows :
  *            CPU Clock source               = IC1_CK
  *            System bus Clock source        = IC2_IC6_IC11_CK
  *            CPUCLK (sysa_ck) (Hz)          = 600000000
  *            SYSCLK AXI (sysb_ck) (Hz)      = 400000000
  *            SYSCLK NPU (sysc_ck) (Hz)      = 600000000
  *            SYSCLK AXISRAM3/4/5/6 (sysd_ck) (Hz) = 600000000
  *            HCLKx(Hz)                      = 200000000
  *            PCLKx(Hz)                      = 200000000
  *            AHB Prescaler                  = 2
  *            APB1 Prescaler                 = 1
  *            APB2 Prescaler                 = 1
  *            APB4 Prescaler                 = 1
  *            APB5 Prescaler                 = 1
  *            PLL1 State                     = ON
  *            PLL1 clock source              = HSI
  *            PLL1 M                         = 4
  *            PLL1 N                         = 75
  *            PLL1 P1                        = 1
  *            PLL1 P2                        = 1
  *            PLL1 FRACN                     = 0
  *            PLL2 State                     = BYPASS
  *            PLL2 clock source              = HSI
  *            PLL3 State                     = BYPASS
  *            PLL3 clock source              = HSI
  *            PLL4 State                     = BYPASS
  *            PLL4 clock source              = HSI
  * @retval None
  */
void SystemClock_Config(void)
{
   RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the System Power Supply
  */
  if (HAL_PWREx_ConfigSupply(PWR_EXTERNAL_SOURCE_SUPPLY) != HAL_OK)
  {
    Error_Handler();
  }

  /** Get current CPU/System buses clocks configuration and if necessary switch
  * to intermediate HSI clock to ensure target clock can be set
  */
  HAL_RCC_GetClockConfig(&RCC_ClkInitStruct);
  if ((RCC_ClkInitStruct.CPUCLKSource == RCC_CPUCLKSOURCE_IC1) ||
      (RCC_ClkInitStruct.SYSCLKSource == RCC_SYSCLKSOURCE_IC2_IC6_IC11))
  {
    RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_CPUCLK | RCC_CLOCKTYPE_SYSCLK);
    RCC_ClkInitStruct.CPUCLKSource = RCC_CPUCLKSOURCE_HSI;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct) != HAL_OK)
    {
      /* Initialization Error */
      Error_Handler();
    }
  }


  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL1.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL1.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL1.PLLM = 4;
  RCC_OscInitStruct.PLL1.PLLN = 75;
  RCC_OscInitStruct.PLL1.PLLFractional = 0;
  RCC_OscInitStruct.PLL1.PLLP1 = 1;
  RCC_OscInitStruct.PLL1.PLLP2 = 1;
  RCC_OscInitStruct.PLL2.PLLState = RCC_PLL_NONE;
  RCC_OscInitStruct.PLL3.PLLState = RCC_PLL_NONE;
  RCC_OscInitStruct.PLL4.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_CPUCLK|RCC_CLOCKTYPE_HCLK
                              |RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1
                              |RCC_CLOCKTYPE_PCLK2|RCC_CLOCKTYPE_PCLK5
                              |RCC_CLOCKTYPE_PCLK4;
  RCC_ClkInitStruct.CPUCLKSource = RCC_CPUCLKSOURCE_IC1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_IC2_IC6_IC11;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV1;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV1;
  RCC_ClkInitStruct.APB5CLKDivider = RCC_APB5_DIV1;
  RCC_ClkInitStruct.IC1Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
  RCC_ClkInitStruct.IC1Selection.ClockDivider = 2;
  RCC_ClkInitStruct.IC2Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
  RCC_ClkInitStruct.IC2Selection.ClockDivider = 3;
  RCC_ClkInitStruct.IC6Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
  RCC_ClkInitStruct.IC6Selection.ClockDivider = 4;
  RCC_ClkInitStruct.IC11Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
  RCC_ClkInitStruct.IC11Selection.ClockDivider = 3;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief XSPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_XSPI2_Init(void)
{

  /* USER CODE BEGIN XSPI2_Init 0 */

  /* USER CODE END XSPI2_Init 0 */

   hxspi2.Instance = XSPI2;

  /* XSPI initialization */
  hxspi2.Init.FifoThresholdByte       = 4U;
  hxspi2.Init.MemoryMode              = HAL_XSPI_SINGLE_MEM;
  hxspi2.Init.MemoryType              = HAL_XSPI_MEMTYPE_MACRONIX;
  hxspi2.Init.MemorySize              = HAL_XSPI_SIZE_32GB;
  hxspi2.Init.ChipSelectHighTimeCycle = 2U;
  hxspi2.Init.FreeRunningClock        = HAL_XSPI_FREERUNCLK_DISABLE;
  hxspi2.Init.ClockMode               = HAL_XSPI_CLOCK_MODE_0;
  hxspi2.Init.WrapSize                = HAL_XSPI_WRAP_NOT_SUPPORTED;
  hxspi2.Init.ClockPrescaler          = 0U;
  hxspi2.Init.SampleShifting          = HAL_XSPI_SAMPLE_SHIFT_NONE;
  hxspi2.Init.DelayHoldQuarterCycle   = HAL_XSPI_DHQC_ENABLE;
  hxspi2.Init.ChipSelectBoundary      = HAL_XSPI_BONDARYOF_NONE;
  hxspi2.Init.MaxTran                 = 0U;
  hxspi2.Init.Refresh                 = 0U;
  hxspi2.Init.MemorySelect            = HAL_XSPI_CSSEL_NCS1;

  if (HAL_XSPI_Init(&hxspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN XSPI2_Init 2 */

  /* RECOVERY : sortir la flash du mode Octal (CR2) tant que le peripherique a
   * la config MACRONIX/DHQC, AVANT toute lecture SFDP.
   * On ralentit fortement l'horloge XSPI : le DTR octal a pleine vitesse n'est
   * pas fiable sans calibration, et la puce doit recevoir nos commandes. */
  (void)HAL_XSPI_SetClockPrescaler(&hxspi2, 19U);
  Recovery_Telemetry();   /* DIAGNOSTIC LED : la puce repond-elle en OPI / SPI ? */
  Recovery_ExitOPI();
  (void)HAL_XSPI_SetClockPrescaler(&hxspi2, 0U);

  /* USER CODE END XSPI2_Init 2 */

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* User may add here some code to deal with this error */
  while(1)
  {
  }
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
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}
#endif /* USE_FULL_ASSERT */

