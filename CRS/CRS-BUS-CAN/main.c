#include "main.h"

//###################################################################
#define VL6180X_PRESS_HUM_TEMP	1
#define MPU9250	0
#define DYN_ANEMO 0
//###################################################################

//====================================================================
//			CAN ACCEPTANCE FILTER
//====================================================================
#define USE_FILTER	1
// Can accept until 4 Standard IDs
#define ID_1	0x01
#define ID_2	0x02
#define ID_3	0x03
#define ID_4	0x04
//====================================================================
extern void systemClock_Config(void);

void (*rxCompleteCallback) (void);
void can_callback(void);

CAN_Message      rxMsg;
CAN_Message      txMsg;
long int        counter = 0;

uint8_t* aTxBuffer[2];

extern float magCalibration[3];

void VL6180x_Init(void);
void VL6180x_Step(void);

int status;
int new_switch_state = 0; // Start in distance mode
int switch_state = -1;

//====================================================================
//			CAN VARIABLES
//====================================================================

//====================================================================

#define CAN_SEND_MESSAGE_ID 0x21
#define CAN_RECEIVE_MESSAGE_ID 0x02

// --------- Function prototypes ---------

void LPS22HB_Init(void);
void read_LPS22HB(void);

void HTS221_Init(void);
void read_HTS221(void);

void Build_CAN_TRAM(void);

//====================================================================

/* CAN TRAM DESCRIPTION
 * Byte 0 -> State (light or range)
 * Byte 1 and 2-> Data (light or range -- 16 bits)
 * Bytes 3 and 4 -> Pressure (16 bits)
 * Bytes 5 and 6 -> Temperature (16 bits)
 * Byte 7 -> Humidity (8 bits -- 0% to 100%)
*/

// ---- Variables to store values ----

uint16_t pressure_hPa;
uint16_t temperature_C;
uint8_t humidity_pct;

// ---- I2C ----

extern I2C_HandleTypeDef hi2c1;

// ---- LPS22HB ----
#define LPS22HB_DEV_ADDR 0xBA // 1011101 slave address with left shift
#define LPS22HB_CTRL_REG1 0x10
#define LPS22HB_PRESS_OUT_XL 0x28

#define PRESSURE_SENS 4096
#define TEMP_SENS 100

// ---- HTS221 ----

#define HTS221_DEV_ADDR 0xBE // 1011111 slave address with left shift
#define HTS221_CTRL_REG1 0x20

#define HTS221_H0_1 0x28

#define HTS221_H0_RH 0x30
#define HTS221_H1_RH 0x31

#define HTS221_H0_T0_OUT_1 0x36

#define HTS221_H1_T0_OUT_1 0x3A

// Calibration variables
int16_t H0_T0_out, H1_T0_out;
int16_t H0_rh, H1_rh;

//====================================================================
// >>>>>>>>>>>>>>>>>>>>>>>>>> MAIN <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
//====================================================================
int main(void)
{
	HAL_Init();
	systemClock_Config();
    SysTick_Config(HAL_RCC_GetHCLKFreq() / 1000); //SysTick end of count event each 1ms
	uart2_Init();
	uart1_Init();
	i2c1_Init();

#if DYN_ANEMO
    anemo_Timer1Init();
#endif

	HAL_Delay(1000); // Wait

#if VL6180X_PRESS_HUM_TEMP
    VL6180x_Init();
    LPS22HB_Init();
    HTS221_Init();
#endif

#if MPU9250
    mpu9250_InitMPU9250();
    mpu9250_CalibrateMPU9250();
#if USE_MAGNETOMETER
    mpu9250_InitAK8963(magCalibration);
#endif
    uint8_t response=0;
	response =  mpu9250_WhoAmI();
	term_printf("%d",response);
#endif


    can_Init();
    can_SetFreq(CAN_BAUDRATE); // CAN BAUDRATE : 500 MHz -- cf Inc/config.h
#if USE_FILTER
    can_Filter_list((ID_1<<21)|(ID_2<<5) , (ID_3<<21)|(ID_4<<5) , CANStandard, 0); // Accept until 4 Standard IDs
#else
    can_Filter_disable(); // Accept everybody
#endif
    can_IrqInit();
    can_IrqSet(&can_callback);

    txMsg.id=0x55;
    txMsg.data[0]=1;
    txMsg.data[1]=2;
    txMsg.len=2;
    txMsg.format=CANStandard;
    txMsg.type=CANData;

    can_Write(txMsg);

    // Décommenter pour utiliser ce Timer ; permet de déclencher une interruption toutes les N ms
    // tickTimer_Init(200); // period in ms

#if DYN_ANEMO
   // TEST MOTEUR
    dxl_LED(1, LED_ON);
    HAL_Delay(500);
    dxl_LED(1, LED_OFF);
    HAL_Delay(500);

    dxl_torque(1, TORQUE_OFF);
    dxl_setOperatingMode(1, VELOCITY_MODE);
    dxl_torque(1, TORQUE_ON);
    dxl_setGoalVelocity(1, 140);
    HAL_Delay(5000);
    dxl_setGoalVelocity(1, 0);
#endif


    while (1) {



#if DYN_ANEMO

#endif

#if VL6180X_PRESS_HUM_TEMP
    VL6180x_Step();

    read_LPS22HB();

    read_HTS221();

    Build_CAN_TRAM();

    can_Write(txMsg);

    //HAL_Delay(500);
#endif



#if MPU9250

#endif

    }
	return 0;
}


//====================================================================
//			CAN CALLBACK RECEPT
//====================================================================

void can_callback(void)
{
	CAN_Message msg_rcv;

	can_Read(&msg_rcv);

	if(msg_rcv.id == CAN_RECEIVE_MESSAGE_ID)
	{
		uint8_t state = msg_rcv.data[0];
		// Range == 0 and Als == 1
		if(state == RunRangePoll|| state == RunAlsPoll) new_switch_state = state;
	}
}
//====================================================================
//			TIMER CALLBACK PERIOD
//====================================================================

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	//term_printf("from timer interrupt\n\r");
}
//====================================================================

#if VL6180X_PRESS_HUM_TEMP
//====================================================================
void VL6180x_Init(void)
{
	uint8_t id;
	State.mode = 1;

    XNUCLEO6180XA1_Init();
    HAL_Delay(500); // Wait
    // RESET
    XNUCLEO6180XA1_Reset(0);
    HAL_Delay(10);
    XNUCLEO6180XA1_Reset(1);
    HAL_Delay(1);

    HAL_Delay(10);
    VL6180x_WaitDeviceBooted(theVL6180xDev);
    id=VL6180x_Identification(theVL6180xDev);
    term_printf("id=%d, should be 180 (0xB4) \n\r", id);
    VL6180x_InitData(theVL6180xDev);

    State.InitScale=VL6180x_UpscaleGetScaling(theVL6180xDev);
    State.FilterEn=VL6180x_FilterGetState(theVL6180xDev);

     // Enable Dmax calculation only if value is displayed (to save computation power)
    VL6180x_DMaxSetState(theVL6180xDev, DMaxDispTime>0);

    switch_state=-1 ; // force what read from switch to set new working mode
    State.mode = AlrmStart;
}

void VL6180x_Step(void)
{
    DISP_ExecLoopBody();

    new_switch_state = XNUCLEO6180XA1_GetSwitch();
    if (new_switch_state != switch_state) {
        switch_state=new_switch_state;
        status = VL6180x_Prepare(theVL6180xDev);
        // Increase convergence time to the max (this is because proximity config of API is used)
        VL6180x_RangeSetMaxConvergenceTime(theVL6180xDev, 63);
        if (status) {
            AbortErr("ErIn");
        }
        else{
            if (switch_state == RunRangePoll) {
                VL6180x_SetupGPIO1(theVL6180xDev, GPIOx_SELECT_GPIO_INTERRUPT_OUTPUT, INTR_POL_HIGH);
                VL6180x_ClearAllInterrupt(theVL6180xDev);
                State.ScaleSwapCnt=0;
                DoScalingSwap( State.InitScale);
            } else {
                 State.mode = RunAlsPoll;
                 InitAlsMode();
            }
        }
    }

    switch (State.mode) {
    case RunRangePoll:
        RangeState();
        break;

    case RunAlsPoll:
        AlsState();
        break;

    case InitErr:
        TimeStarted = g_TickCnt;
        State.mode = WaitForReset;
        break;

    case AlrmStart:
       GoToAlaramState();
       break;

    case AlrmRun:
        AlarmState();
        break;

    case FromSwitch:
        // force reading swicth as re-init selected mode
        switch_state=!XNUCLEO6180XA1_GetSwitch();
        break;

    case ScaleSwap:

        if (g_TickCnt - TimeStarted >= ScaleDispTime) {
            State.mode = RunRangePoll;
            TimeStarted=g_TickCnt; /* reset as used for --- to er display */
        }
        else
        {
        	DISP_ExecLoopBody();
        }
        break;

    default: {
    	 DISP_ExecLoopBody();
          if (g_TickCnt - TimeStarted >= 5000) {
              NVIC_SystemReset();
          }
    }
    }
}

void LPS22HB_Init(void)
{
	// Power ON and 1 Hz Output Data Rate
	uint8_t config = 0x10;

	if (HAL_I2C_Mem_Write(&hi2c1, LPS22HB_DEV_ADDR, LPS22HB_CTRL_REG1, 1, &config, 1, 100) == HAL_OK)
	{
		term_printf("\r\nLPS22HB Woke up!");
	}
	else
	{
		term_printf("\r\nError while waking up LPS22HB!");
	}
}

void read_LPS22HB(void)
{
	uint8_t buffer[5];

	// Size = 5 (read up to 0x2C)
	if (HAL_I2C_Mem_Read(&hi2c1, LPS22HB_DEV_ADDR, LPS22HB_PRESS_OUT_XL, 1, buffer, 5, 100) == HAL_OK)
	{

		// --------- PRESSURE -------------

		int32_t raw_pressure = (buffer[2] << 16) | (buffer[1] << 8) | buffer[0];

		pressure_hPa = (uint16_t) (raw_pressure / PRESSURE_SENS);

		// --------- TEMPERATURE -------------

		int16_t raw_temp = (buffer[4] << 8 ) | buffer[3];

		temperature_C = raw_temp / TEMP_SENS;

		// Testing via UART
		term_printf("\r\nPressure %d hPa | Temperature: %d C", pressure_hPa, temperature_C);
	}
}

void HTS221_Init(void)
{
	// PD (Power down control) = 1, BDU (Block Data Update) = 1, ODR1 = 0, ODR0 = 1 (1 Hz frequency)
	uint8_t config = 0x85;

	if (HAL_I2C_Mem_Write(&hi2c1, HTS221_DEV_ADDR, HTS221_CTRL_REG1, 1, &config, 1, 100) == HAL_OK) {
	        term_printf("\r\nHTS221 Woke up!");
	}

	// Read calibration data
	uint8_t buffer[2];

	HAL_I2C_Mem_Read(&hi2c1, HTS221_DEV_ADDR, HTS221_H0_RH, 1, buffer, 1, 100);
	H0_rh = buffer[0] / 2;

	HAL_I2C_Mem_Read(&hi2c1, HTS221_DEV_ADDR, HTS221_H1_RH, 1, buffer, 1, 100);
	H1_rh = buffer[0] / 2;

	// 0x80 to enable auto increment
	HAL_I2C_Mem_Read(&hi2c1, HTS221_DEV_ADDR, HTS221_H0_T0_OUT_1 | 0x80, 1, buffer, 2, 100);
	H0_T0_out = (buffer[1] << 8) | buffer[0];

	HAL_I2C_Mem_Read(&hi2c1, HTS221_DEV_ADDR, HTS221_H1_T0_OUT_1 | 0x80, 1, buffer, 2, 100);
	H1_T0_out = (buffer[1] << 8) | buffer[0];
}

void read_HTS221(void)
{
	uint8_t buffer[2];

	// 0x80 to enable auto increment
	if (HAL_I2C_Mem_Read(&hi2c1, HTS221_DEV_ADDR, HTS221_H0_1 | 0x80, 1, buffer, 2, 100) == HAL_OK)
	    {
	        int16_t H_out = (buffer[1] << 8) | buffer[0];

	        // Linear Interpolation
	        int32_t tmp = (H1_rh - H0_rh) * (H_out - H0_T0_out);

	        humidity_pct = (tmp / (H1_T0_out - H0_T0_out)) + H0_rh;

	        term_printf(" | Hum: %d %%\r\n", humidity_pct);
	    }
}

void Build_CAN_TRAM(void)
{
	// CAN TRAM config
	txMsg.id = CAN_SEND_MESSAGE_ID;
	txMsg.len=8; // Nombre d'octets à envoyer
	txMsg.format=CANStandard;
	txMsg.type=CANData;

	// ========================================================
	    // OPTICAL SENSOR - BYTES 0,1,2
	// ========================================================

	// Since we can only measure either the light or the range once at a time, we must choose

	// If we're measuring the range
	if (State.mode == RunRangePoll)
	{
		// Tells IHM that we're sending the range (the enum value is 0)
		txMsg.data[0] = RunRangePoll;

		// Divide range 16 bit range variable in two pieces -- RANGE IS MEASURED IN MM
		txMsg.data[1] = (range >> 8) & 0xFF;
		txMsg.data[2] = (range) & 0XFF;
	}
	// If we're measuring the light
	else if (State.mode == RunAlsPoll)
	{
		// Tells IHM that we're sending the light (the enum value is 1)
		txMsg.data[0] = RunAlsPoll;

		uint16_t light;

		// We'll use only 16 bits to save bytes on the message
		if (Als.lux > 65535) { // if the light value is greater than our limit, we cap it
		        light = 65535;
		    } else {
		        light = (uint16_t)Als.lux;
		    }

		txMsg.data[1] = (light >> 8) & 0xFF;
		txMsg.data[2] = (light) & 0xFF;
	}

	// ========================================================
		    // AMBIENTAL SENSORS - BYTES 3,4,5,6,7
	// ========================================================

	// Pressure -> 16 bits sliced in two bytes
	txMsg.data[3] = (pressure_hPa >> 8) & 0xFF;
	txMsg.data[4] = (pressure_hPa) & 0xFF;

	// Temperature -> 16 bits sliced in two bytes
	txMsg.data[5] = (temperature_C >> 8) & 0xFF;
	txMsg.data[6] = (temperature_C) & 0xFF;

	// Humidity -> 8 bits
	txMsg.data[7] = humidity_pct;
}
#endif
//====================================================================

