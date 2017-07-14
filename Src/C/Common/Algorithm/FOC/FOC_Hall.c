#include "Globe.h"
#include "FOC_Globle.h"
#include "FOC_Motor.h"
#include "FOC_Hall.h"

#define 	EANGLE_0		0
#define 	EANGLE_360		((s32)0xfffff)		//2^20
#define 	EANGLE_30		(s32)(EANGLE_360*30/360)
#define 	EANGLE_45		(s32)(EANGLE_360*45/360)
#define 	EANGLE_60		(s32)(EANGLE_360*60/360)
#define 	EANGLE_90		(s32)(EANGLE_360*90/360)
#define 	EANGLE_120		(s32)(EANGLE_360*120/360)
#define 	EANGLE_150		(s32)(EANGLE_360*150/360)
#define 	EANGLE_180		(s32)(EANGLE_360*180/360)
#define 	EANGLE_210		(s32)(EANGLE_360*210/360)
#define 	EANGLE_240		(s32)(EANGLE_360*240/360)
#define 	EANGLE_270		(s32)(EANGLE_360*270/360)
#define 	EANGLE_300		(s32)(EANGLE_360*300/360)
#define 	EANGLE_330		(s32)(EANGLE_360*330/360)


#define HALL_MAX_RATIO			((u16)200)								//��ʱ������Ƶֵ
#define HALL_MAX_OVFCNT			((u16)U16_MAX / HALL_MAX_RATIO - 1)
#define HALL_MAX_OVFTIME		((u32)(HALL_MAX_RATIO+1)*U16_MAX) 		//������ۼ�ʱ�䳬����ֵʱ����Ƶϵ���ĵ����ֵ
#define PSEUDO_FREQ_CONV        ((u32)EANGLE_60*(CKTIM/FOC_FREQ))		//m_Hall_dpp = EANGLE_60/(PeriodMeasAux.wPeriod/CKTIM)/FOC_FREQ;  1118481066
#define HALL_ZERO_OVFTIME		PSEUDO_FREQ_CONV						//��ʱ��Ϊ��ֵʱ��������ٶ�Ϊ0
#define ICx_FILTER 					(u8) 0x0B // 11 <-> 1333 nsec 

#define HALL_PERIOD_FIFO_SIZE 	((u8)4)				//����ʱ����ģ�˲�������
#define LOW_RES_THRESHOLD   ((u32)0x3333u)		//����ֵ���ڸ�ֵ���Ƶϵ����С��U16_MAX/5
#define BEST_CAP_VALUE 		((u32)0x7FFFu)			//��Ѳ���ֵ��U16_MAX/2
#define HIGH_RES_THRESHOLD 	((u32)0xCCCCu)		//����ֵ���ڸ�ֵ���Ƶϵ�����ӣ�U16_MAX*4/5

//
#define NEGATIVE          (s8)-1
#define POSITIVE          (s8)1
#define NEGATIVE_SWAP     (s8)-2
#define POSITIVE_SWAP     (s8)2
#define HALL_ERROR             (s8)127

/* Private macro -------------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/
typedef struct {
	u32 wPeriod;
	s8 bDirection;
} PeriodMeas_S;
/* Private variables ---------------------------------------------------------*/
//volatile SpeedMeas_s SensorPeriod[HALL_PERIOD_FIFO_SIZE];// Holding the last captures
volatile PeriodMeas_S g_PeriodMeas[HALL_PERIOD_FIFO_SIZE];
volatile vu8 m_PeriodFIFO_Index;   // Index of above array
volatile u16 bGP1_OVF_Counter;   // Count overflows if prescaler is too low
volatile u16 hCaptCounter;      // Counts the number of captures interrupts

volatile u8 g_TimerPSCChanged;
volatile u8 m_bDoRollingAverage;
volatile u8 m_bHallTimeOut;
volatile s32 m_Hall_dpp;
volatile s32 m_Hall_dpp_WithoutAvr = 0;

s32 m_Hall_dpp_forsee = 0;
s32 m_forsee_cnt = 0;
s32 m_forsee_time = 0;

volatile s32 m_angle_error = 0;

void FOC_Hall1_Int(void);
s32 FOC_Hall1_FristCheck(void);
s32 SVP_Calculate_Electric_Angle_By_Hall(u32 HallValue);
u8 SVP_ReadHallState(void);
void HALL_InitHallMeasure(void);

volatile u8 m_bCaptProcess = 0; 			//���ڲ����жϸ���
volatile s32 m_Electrical_Angle = 0; 
volatile s32 m_Electrical_Angle_Wall = 0;
volatile s32 m_Elec_Angle_Wall_H = 0;
volatile s32 m_Elec_Angle_Wall_L = 0;
volatile u8 m_bHalfWall = 0;
//static s32 m_RotorFreq_dpp = 0;
static u8 m_prevHallState = 0;
volatile s32 m_HallDir = 0;
volatile u32 m_OvfPeriod = 0;			//ÿ�����ʱ�ۼ����ʱ��
volatile u16 m_OldPRCReg = 0;	   	 	//��Ƶϵ���ı�ʱ������һ�η�Ƶֵ
volatile s32 m_HallSpeed = 0;	//����ʱ���ٶȣ�rpm
volatile s32 m_HallSpeed_Avr = 0;	//����ʱ���ٶȣ�rpm

#ifdef ENABLE_ERROR_REC
volatile u16 m_HallErrorCnt = 0;
#endif


void (*OnHallEdgeCallback)(int);

void OnHallEdgeCallbackRegistery(void (*func)(int))
{
    OnHallEdgeCallback = func;
}

u8 Hall_GethalfWall(void)
{
	return m_bHalfWall;
}

//Hall��س�ʼ��
u32 FOC_Hall_Int(void)
{
	u32 result = 0;
	
	FOC_Hall1_Int();
	if(FOC_Hall1_FristCheck())
		result = 1;
	HALL_InitHallMeasure();
	return result;
}

void FOC_Hall1_Int(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	TIM_TimeBaseInitTypeDef TIM_HALLTimeBaseInitStructure;
	TIM_ICInitTypeDef       TIM_HALLICInitStructure;
	
	//ʹ�ܶ˿�ʱ��
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, ENABLE);
	
	//GPIO����
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;		//��������
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;			//50Mʱ���ٶ�
	//PA0,1,2 ����
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	// Timer configuration in Clear on capture mode
    TIM_DeInit(HALL_TIMER);
    
    TIM_TimeBaseStructInit(&TIM_HALLTimeBaseInitStructure);
    // Set full 16-bit working range
	TIM_HALLTimeBaseInitStructure.TIM_Prescaler = HALL_MAX_RATIO;    		//Ԥ��Ƶֵ����ʼ����Ϊ���
    TIM_HALLTimeBaseInitStructure.TIM_Period = 0xffff;	  					//��������65535
    TIM_HALLTimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_HALLTimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up; 	//���ϼ���
    TIM_TimeBaseInit(HALL_TIMER,&TIM_HALLTimeBaseInitStructure);
    
    TIM_ICStructInit(&TIM_HALLICInitStructure);
    TIM_HALLICInitStructure.TIM_Channel = TIM_Channel_1;
    TIM_HALLICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Falling;
	TIM_HALLICInitStructure.TIM_ICSelection = TIM_ICSelection_TRC;  //����TRC��ѡ����������˵Ҫһ����6�жϵĻ���ӣ�
    TIM_HALLICInitStructure.TIM_ICFilter = ICx_FILTER;
    
    TIM_ICInit(HALL_TIMER,&TIM_HALLICInitStructure);
    
    // Force the HALL_TIMER prescaler with immediate access (no need of an update event) 
    TIM_PrescalerConfig(HALL_TIMER, (u16) HALL_MAX_RATIO, 
		TIM_PSCReloadMode_Immediate);						//��ʼԤ��Ƶֵ��Ϊ���ֵ��800��
    TIM_InternalClockConfig(HALL_TIMER);					//ʹ���ڲ�ʱ�ӣ���������оƬʱ��������
    
    //Enables the XOR of channel 1, channel2 and channel3
    TIM_SelectHallSensor(HALL_TIMER, ENABLE);				//TIMx_CH1��TIMx_CH2��TIMx_CH3�ܽž���������TI1���롣
    
	//    TIM_SelectInputTrigger(HALL_TIMER, TIM_TS_TI1FP1);		//ѡ���˲���Ķ�ʱ������1��Ϊ����Դ
	TIM_SelectInputTrigger(HALL_TIMER, TIM_TS_TI1F_ED);     //���봥����ʽ��Ϊ����źţ���������˵Ҫһ����6�жϵĻ���ӣ�
    TIM_SelectSlaveMode(HALL_TIMER,TIM_SlaveMode_Reset);	//��ģʽѡ��λģʽ �C ѡ�еĴ�������(TRGI)�����������³�ʼ����������
	//���Ҳ���һ�����¼Ĵ������źš�����hallֵ�仯��ʱ�����¼�����
	
    // Source of Update event is only counter overflow/underflow�������ģʽ�����ĸ���Ҳ�ᴥ���жϣ�
    TIM_UpdateRequestConfig(HALL_TIMER, TIM_UpdateSource_Regular);	//���ø�������Դ
	
	//�����жϱ�־
    TIM_ClearFlag(HALL_TIMER, TIM_FLAG_Update + TIM_FLAG_CC1 + TIM_FLAG_CC2 + \
		TIM_FLAG_CC3 + TIM_FLAG_CC4 + TIM_FLAG_Trigger + TIM_FLAG_CC1OF + \
		TIM_FLAG_CC2OF + TIM_FLAG_CC3OF + TIM_FLAG_CC4OF);
	
    // Selected input capture and Update (overflow) events generate interrupt
    TIM_ITConfig(HALL_TIMER, TIM_IT_CC1, ENABLE);		//ÿ��hallֵ�仯�������ж�
    TIM_ITConfig(HALL_TIMER, TIM_IT_Update, ENABLE);	//��ʱ����������ж�
	
    TIM_SetCounter(HALL_TIMER, 0);
	TIM_Cmd(HALL_TIMER, ENABLE);
}

s32 HALL_CalWallByHall(u32 HallValue)
{
	u32 result = 0;
	switch(HallValue)
	{
	case HALL_30:			
		m_Elec_Angle_Wall_H = EANGLE_60;
		m_Elec_Angle_Wall_L = EANGLE_0;
		break;
	case HALL_90:
		m_Elec_Angle_Wall_H = EANGLE_120;
		m_Elec_Angle_Wall_L = EANGLE_60;
		break;
	case HALL_150:
		m_Elec_Angle_Wall_H = EANGLE_180;
		m_Elec_Angle_Wall_L = EANGLE_120;
		break;
	case HALL_210:
		m_Elec_Angle_Wall_H = EANGLE_240;
		m_Elec_Angle_Wall_L = EANGLE_180;
		break;
	case HALL_270:
		m_Elec_Angle_Wall_H = EANGLE_300;
		m_Elec_Angle_Wall_L = EANGLE_240;
		break;
	case HALL_330:
		m_Elec_Angle_Wall_H = EANGLE_360;
		m_Elec_Angle_Wall_L = EANGLE_300;
		break;
	default:
		break;
	}	   // */
	return 	result; 
}

s32 FOC_Hall1_FristCheck(void)
{
	s32 result = 0;
	u8 Hall_State;
	Hall_State = SVP_ReadHallState();
	if(Hall_State == 0 || Hall_State == 7)
	{
		result = 0;
	}
	else
	{
		m_prevHallState = Hall_State;
		result = 1;
	}
	//���ݻ���״̬��ʼ����Ƕ�
	if(result)
	{
		m_Electrical_Angle = SVP_Calculate_Electric_Angle_By_Hall(Hall_State);
		HALL_CalWallByHall(Hall_State);
	}
	return result;
}		 // */

s32 SVP_Calculate_Electric_Angle_By_Hall(u32 HallValue)
{
	u32 result = 0;
	switch(HallValue)
	{
	case HALL_30:			
		result = EANGLE_30;
		break;
	case HALL_90:
		result = EANGLE_90;
		break;
	case HALL_150:
		result = EANGLE_150;
		break;
	case HALL_210:
		result = EANGLE_210;
		break;
	case HALL_270:
		result = EANGLE_270;
		break;
	case HALL_330:
		result = EANGLE_330;
		break;
	default:
		break;
	}	   // */
	return 	result; 
}

void HALL_InitHallMeasure( void )
{
   // Mask interrupts to insure a clean intialization
	s32 i = 0;
   TIM_ITConfig(HALL_TIMER, TIM_IT_CC1, DISABLE);
    
 //  RatioDec = 0;
  // RatioInc = 0;
   g_TimerPSCChanged = 0;
   m_bDoRollingAverage = 0;
   m_bHallTimeOut = 0;

   hCaptCounter = 0;
   bGP1_OVF_Counter = 0;
   m_OvfPeriod = HALL_ZERO_OVFTIME;
   m_OldPRCReg = HALL_MAX_RATIO;
   m_bCaptProcess = 0;
	 m_Hall_dpp = 0;
	m_Hall_dpp_WithoutAvr = 0;
	m_bHalfWall = 1;

   for (i=0; i<HALL_PERIOD_FIFO_SIZE; i++)
   {
		g_PeriodMeas[m_PeriodFIFO_Index].wPeriod = HALL_MAX_OVFTIME;
		g_PeriodMeas[m_PeriodFIFO_Index].bDirection = POSITIVE;
   }	 

   // First measurement will be stored in the 1st array location
   m_PeriodFIFO_Index = 0;

   // Re-initialize partly the timer
   HALL_TIMER->PSC = HALL_MAX_RATIO;
   
   hCaptCounter = 0;
     
   TIM_SetCounter(HALL_TIMER, 0);
   
   TIM_Cmd(HALL_TIMER, ENABLE);

   TIM_ITConfig(HALL_TIMER, TIM_IT_CC1, ENABLE);
}

u8 SVP_ReadHallState(void)
{
	return (u8)(GPIOA->IDR & (u32)0x00000007);
}

PeriodMeas_S GetAvrgHallPeriod(void)
{
  u32 wAvrgBuffer = 0;
	u32 i = 0;
  PeriodMeas_S PeriodMeasAux;
	
	for ( i = 0; i < HALL_PERIOD_FIFO_SIZE; i++ )
	{
		wAvrgBuffer += g_PeriodMeas[i].wPeriod;
	}
	// Round to upper value
	wAvrgBuffer = (u32)(wAvrgBuffer + (HALL_PERIOD_FIFO_SIZE/2)-1);  
	wAvrgBuffer /= HALL_PERIOD_FIFO_SIZE;        // Average value	
	
	PeriodMeasAux.wPeriod = wAvrgBuffer;
	PeriodMeasAux.bDirection = g_PeriodMeas[m_PeriodFIFO_Index].bDirection;
	
	return (PeriodMeasAux);
}

s16 HALL_GetRotorFreq ( void )
{
	PeriodMeas_S PeriodMeasAux;
	PeriodMeas_S PeriodMeasLast;
	
	s32 temp32 = 0;
	
	PeriodMeasAux = GetAvrgHallPeriod();
	PeriodMeasLast = g_PeriodMeas[m_PeriodFIFO_Index];
	
	if (m_bHallTimeOut == 1)
	{
		m_HallSpeed = 0;
		m_HallSpeed_Avr = 0;
		m_Hall_dpp = 0;
		m_Hall_dpp_WithoutAvr = 0;
		m_Hall_dpp_forsee = 0;
	}
	else
	{
		if(PeriodMeasLast.wPeriod != 0)
		{
			m_Hall_dpp_WithoutAvr = PSEUDO_FREQ_CONV/PeriodMeasLast.wPeriod;			//EANGLE_60/(PeriodMeasAux.wPeriod/CKTIM)/FOC_FREQ;
			m_Hall_dpp_WithoutAvr *= PeriodMeasLast.bDirection; 
			if(!m_bDoRollingAverage)
			{
				m_Hall_dpp = m_Hall_dpp_WithoutAvr;
				m_Hall_dpp_forsee = m_Hall_dpp;			//Ԥ���ٶ���ʵ���ٶ�ͬ��
				if(m_Hall_dpp != 0)									//Ԥ����һ�β����ʱ��
					m_forsee_cnt = EANGLE_60/ABS(m_Hall_dpp);
				else
					m_forsee_cnt = S32_MAX;
				m_forsee_time = 0;									//���ڼ���Ԥ���ٶȵ�ʱ������
			}
		}
		if(PeriodMeasAux.wPeriod != 0)
		{
			//����ǶȻ���ֵ
			if(m_bDoRollingAverage)
			{
				m_Hall_dpp = PSEUDO_FREQ_CONV/PeriodMeasAux.wPeriod;			//EANGLE_60/(PeriodMeasAux.wPeriod/CKTIM)/FOC_FREQ;
				m_Hall_dpp *= PeriodMeasAux.bDirection;  
				m_Hall_dpp_forsee = m_Hall_dpp;			//Ԥ���ٶ���ʵ���ٶ�ͬ��
				if(m_Hall_dpp != 0)									//Ԥ����һ�β����ʱ��
					m_forsee_cnt = EANGLE_60/ABS(m_Hall_dpp);
				else
					m_forsee_cnt = S32_MAX;
				m_forsee_time = 0;									//���ڼ���Ԥ���ٶȵ�ʱ������
			}
			//������ƽ��ת��
			temp32 = 10*CKTIM/(PeriodMeasAux.wPeriod);		//60/(6*PeriodMeasAux.wPeriod/CKTIM);	//ת����rpm
			temp32 *= PeriodMeasAux.bDirection;	
			m_HallSpeed_Avr = temp32;
		}
	}
	
	return (m_Hall_dpp);
}

volatile s16 VoltOutfinaly;
void Hall_SynElectricAngleAtHallEdge(void)
{
	static u8 newHallState = 0;
	static s32 s_old_dpp_1 = 0;
	static s32 s_old_dpp_2 = 0;
	static s32 s_bDirChangedLastTime = 1;
	s32 old_HallDir = 0;
	s32 temp32 = 0;
	
	m_prevHallState = newHallState;
	newHallState = SVP_ReadHallState();
	//�ж��Ƿ���ٹ��̣��������Ƿ��ð�ǽ
	temp32 = ABS(m_Hall_dpp_forsee);		//������һ�ε�dpp
	if( ((s_old_dpp_2 > temp32 || s_old_dpp_1 > temp32) && temp32 < 1500)	//���ٹ����в���dppֵ��1500���ڣ�Լ0.51r/s��
			|| m_bHallTimeOut
			|| temp32 < 200														//�����ٶȺܵͣ����ð�ǽ
	//		|| temp32 < 1000
			|| s_bDirChangedLastTime == 1						//������һ���ٶȷ������˸ı�
			|| (m_HallSpeed_Avr > 0 && VoltOutfinaly < 0 || m_HallSpeed_Avr < 0 && VoltOutfinaly > 0) )		//ռ�ձȷ�����ת�ٷ����෴
	{
		m_bHalfWall = 1;
	}
	//else if(temp32 > 1000 && s_old_dpp_1 > 1000 && s_old_dpp_2 > 1000)
	else
	{
		m_bHalfWall = 0;
	}// */
//	m_bHalfWall = 1;
	
	if(temp32 > 3000 && s_old_dpp_2 > 3000)						//�ٶȴ���1r/s�����û�ģ�˲���
		m_bDoRollingAverage = 1;
	else if(temp32 < 2300)
		m_bDoRollingAverage = 0;	//*/
	
	s_old_dpp_1 = s_old_dpp_2;
	s_old_dpp_2 = temp32;
	
	//������hallֵͬ���Ƕȼ��Ƕ�ǽ
	old_HallDir = m_HallDir;
	switch(newHallState)
	{
	case HALL_30:
		if(m_prevHallState == HALL_330)				//����0��
			m_HallDir = POSITIVE;
		else if(m_prevHallState == HALL_90)	 		//����60��
			m_HallDir = NEGATIVE;
		else if(m_prevHallState == HALL_30)  		//������ 	
		{
			if(m_HallDir < 0)
				m_HallDir = POSITIVE_SWAP;
			else
				m_HallDir = NEGATIVE_SWAP;
		}
#ifdef ENABLE_ERROR_REC
		else
		{
			m_HallErrorCnt++;
			ErrRec_Push_Error(ONE_BB_ERR_HALL, m_HallErrorCnt);
		}
#endif
//		old_HallDir = m_HallDir;
		if(m_HallDir < 0)
		{
			m_Electrical_Angle = EANGLE_60;
			m_Elec_Angle_Wall_H = EANGLE_60;
			if(m_bHalfWall || old_HallDir != m_HallDir)
				m_Elec_Angle_Wall_L = EANGLE_30;
			else
				m_Elec_Angle_Wall_L = EANGLE_0;
		}
		else if(m_HallDir != HALL_ERROR)
		{
			m_Electrical_Angle = EANGLE_0; 
			m_Elec_Angle_Wall_L = EANGLE_0;
			if(m_bHalfWall || old_HallDir != m_HallDir)
				m_Elec_Angle_Wall_H = EANGLE_30;
			else
				m_Elec_Angle_Wall_H = EANGLE_60;
		}
		break;
	case HALL_90:
		if(m_prevHallState == HALL_30)				//����60��
			m_HallDir = POSITIVE;
		else if(m_prevHallState == HALL_150)	 	//����120��
			m_HallDir = NEGATIVE;
		else if(m_prevHallState == HALL_90)  		//������ 	
		{
			if(m_HallDir < 0)
				m_HallDir = POSITIVE_SWAP;
			else
				m_HallDir = NEGATIVE_SWAP;
		}
#ifdef ENABLE_ERROR_REC
		else
		{
			m_HallErrorCnt++;
			ErrRec_Push_Error(ONE_BB_ERR_HALL, m_HallErrorCnt);
		}
#endif
//		old_HallDir = m_HallDir;
		if(m_HallDir < 0)
		{
			m_Electrical_Angle = EANGLE_120;
			m_Elec_Angle_Wall_H = EANGLE_120;
			if(m_bHalfWall || old_HallDir != m_HallDir)
				m_Elec_Angle_Wall_L = EANGLE_90;
			else
				m_Elec_Angle_Wall_L = EANGLE_60;
		}
		else if(m_HallDir != HALL_ERROR)
		{
			m_Electrical_Angle = EANGLE_60; 
			m_Elec_Angle_Wall_L = EANGLE_60;
			if(m_bHalfWall || old_HallDir != m_HallDir)
				m_Elec_Angle_Wall_H = EANGLE_90;
			else
				m_Elec_Angle_Wall_H = EANGLE_120;
		}
		break;
	case HALL_150:
		if(m_prevHallState == HALL_90)				//����120��
			m_HallDir = POSITIVE;
		else if(m_prevHallState == HALL_210)	 	//����180��
			m_HallDir = NEGATIVE;
		else if(m_prevHallState == HALL_150)  		//������ 	
		{
			if(m_HallDir < 0)
				m_HallDir = POSITIVE_SWAP;
			else
				m_HallDir = NEGATIVE_SWAP;
		}
#ifdef ENABLE_ERROR_REC
		else
		{
			m_HallErrorCnt++;
			ErrRec_Push_Error(ONE_BB_ERR_HALL, m_HallErrorCnt);
		}
#endif
//		old_HallDir = m_HallDir;
		if(m_HallDir < 0)
		{
			m_Electrical_Angle = EANGLE_180;
			m_Elec_Angle_Wall_H = EANGLE_180;
			if(m_bHalfWall || old_HallDir != m_HallDir)
				m_Elec_Angle_Wall_L = EANGLE_150;
			else
				m_Elec_Angle_Wall_L = EANGLE_120;
		}
		else if(m_HallDir != HALL_ERROR)
		{
			m_Electrical_Angle = EANGLE_120; 
			m_Elec_Angle_Wall_L = EANGLE_120;
			if(m_bHalfWall || old_HallDir != m_HallDir)
				m_Elec_Angle_Wall_H = EANGLE_150;
			else
				m_Elec_Angle_Wall_H = EANGLE_180;
		} 
		break; 
	case HALL_210:
		if(m_prevHallState == HALL_150)				//����180��
			m_HallDir = POSITIVE;
		else if(m_prevHallState == HALL_270)	 	//����240��
			m_HallDir = NEGATIVE;
		else if(m_prevHallState == HALL_210)  		//������ 	
		{
			if(m_HallDir < 0)
				m_HallDir = POSITIVE_SWAP;
			else
				m_HallDir = NEGATIVE_SWAP;
		}
#ifdef ENABLE_ERROR_REC
		else
		{
			m_HallErrorCnt++;
			ErrRec_Push_Error(ONE_BB_ERR_HALL, m_HallErrorCnt);
		}
#endif
	//	old_HallDir = m_HallDir;
		if(m_HallDir < 0)
		{
			m_Electrical_Angle = EANGLE_240;
			m_Elec_Angle_Wall_H = EANGLE_240;
			if(m_bHalfWall || old_HallDir != m_HallDir)
				m_Elec_Angle_Wall_L = EANGLE_210;
			else
				m_Elec_Angle_Wall_L = EANGLE_180;
		}
		else if(m_HallDir != HALL_ERROR)
		{
			m_Electrical_Angle = EANGLE_180; 
			m_Elec_Angle_Wall_L = EANGLE_180;
			if(m_bHalfWall || old_HallDir != m_HallDir)
				m_Elec_Angle_Wall_H = EANGLE_210;
			else
				m_Elec_Angle_Wall_H = EANGLE_240;
		}
		break;
	case HALL_270:
		if(m_prevHallState == HALL_210)				//����240��
			m_HallDir = POSITIVE;
		else if(m_prevHallState == HALL_330)	 	//����300��
			m_HallDir = NEGATIVE;
		else if(m_prevHallState == HALL_270)  		//������ 	
		{
			if(m_HallDir < 0)
				m_HallDir = POSITIVE_SWAP;
			else
				m_HallDir = NEGATIVE_SWAP;
		}
#ifdef ENABLE_ERROR_REC
		else
		{
			m_HallErrorCnt++;
			ErrRec_Push_Error(ONE_BB_ERR_HALL, m_HallErrorCnt);
		}
#endif
//		old_HallDir = m_HallDir;
		if(m_HallDir < 0)
		{
			m_Electrical_Angle = EANGLE_300;
			m_Elec_Angle_Wall_H = EANGLE_300;
			if(m_bHalfWall || old_HallDir != m_HallDir)
				m_Elec_Angle_Wall_L = EANGLE_270;
			else
				m_Elec_Angle_Wall_L = EANGLE_240;
		}
		else if(m_HallDir != HALL_ERROR)
		{
			m_Electrical_Angle = EANGLE_240; 
			m_Elec_Angle_Wall_L = EANGLE_240;
			if(m_bHalfWall || old_HallDir != m_HallDir)
				m_Elec_Angle_Wall_H = EANGLE_270;
			else
				m_Elec_Angle_Wall_H = EANGLE_300;
		}
		break;
	case HALL_330:
		if(m_prevHallState == HALL_270)				//����300��
			m_HallDir = POSITIVE;
		else if(m_prevHallState == HALL_30)	 		//����0��
			m_HallDir = NEGATIVE;
		else if(m_prevHallState == HALL_330)  		//������ 	
		{
			if(m_HallDir < 0)
				m_HallDir = POSITIVE_SWAP;
			else
				m_HallDir = NEGATIVE_SWAP;
		}
#ifdef ENABLE_ERROR_REC
		else
		{
			m_HallErrorCnt++;
			ErrRec_Push_Error(ONE_BB_ERR_HALL, m_HallErrorCnt);
		}
#endif
	//	old_HallDir = m_HallDir;
		if(m_HallDir < 0)
		{
			m_Electrical_Angle = EANGLE_360;
			m_Elec_Angle_Wall_H = EANGLE_360;
			if(m_bHalfWall || old_HallDir != m_HallDir)
				m_Elec_Angle_Wall_L = EANGLE_330;
			else
				m_Elec_Angle_Wall_L = EANGLE_300;
		}
		else if(m_HallDir != HALL_ERROR)
		{
			m_Electrical_Angle = EANGLE_300;
			m_Elec_Angle_Wall_L = EANGLE_300;
			if(m_bHalfWall || old_HallDir != m_HallDir)
				m_Elec_Angle_Wall_H = EANGLE_330;
			else
				m_Elec_Angle_Wall_H = EANGLE_360;
		}
		break;
	default:
		m_HallDir = HALL_ERROR;
#ifdef ENABLE_ERROR_REC
		m_HallErrorCnt++;
		ErrRec_Push_Error(ONE_BB_ERR_HALL, m_HallErrorCnt);
#endif
		break;
	}
	if(old_HallDir != m_HallDir)
		s_bDirChangedLastTime = 1;
	else
		s_bDirChangedLastTime = 0;
}

void Hall_TIM_IRQHandler(void)
{ 
	static s32 s_prevHallDir = 0;
	static s32 s_prevCapPeriod = 0;
	
	u32 wCaptBuf = 0;
    u32 hPrscBuf = 0;
	u8 bPRCChangedLastTime = 0;
	u32 temp32 = 0;

	m_bCaptProcess = 1;
	if ( TIM_GetFlagStatus(HALL_TIMER, TIM_FLAG_Update) == RESET )   	//�����ж�
	{
		s_prevHallDir = m_HallDir;
		Hall_SynElectricAngleAtHallEdge();					//ͬ����Ƕ�
		TIM_ClearFlag(HALL_TIMER, TIM_FLAG_CC1);		//�����־
		m_bHallTimeOut = 0;
		
		if (hCaptCounter < U16_MAX)		//ͳ�Ʋ������
			hCaptCounter++;
		
		// Compute new array index
		if (m_PeriodFIFO_Index < HALL_PERIOD_FIFO_SIZE-1)
			m_PeriodFIFO_Index++;
		else
			m_PeriodFIFO_Index = 0;

		wCaptBuf = (u32)TIM_GetCapture1(HALL_TIMER);    //��ȡ����ֵ  
		hPrscBuf = HALL_TIMER->PSC;						//��ȡԤ��Ƶֵ
		if (g_TimerPSCChanged)  		//��һ�β���ʱ��Ƶֵ�ı���
		{
			hPrscBuf = m_OldPRCReg;		//Ԥ��Ƶ�Ĵ�����Ԥװ�صģ���һ�β����¼��Ż������ã���ѵPPT2/49ҳ��
			bPRCChangedLastTime = 1;
			g_TimerPSCChanged = 0;
		}	  // */

		if (bGP1_OVF_Counter != 0)	// ����ǰ���������
		{
			wCaptBuf += m_OvfPeriod/(hPrscBuf+1);	//�������ʱ���Ч�Ĳ���ֵ
			if(!bPRCChangedLastTime)
			{
				if ((HALL_TIMER->PSC) < HALL_MAX_RATIO) // Avoid OVF w/ very low freq
				{
					m_OldPRCReg = HALL_TIMER->PSC; 					//�Ĵ����仯ǰ��¼��ǰֵ
					temp32 = (wCaptBuf<<8)/BEST_CAP_VALUE;			//����ֵ�����ֵ�ı������Ŵ�256��
					temp32 = ((hPrscBuf+1)*temp32)>>8; 				//������ѷ�Ƶϵ��
					(HALL_TIMER->PSC)= temp32 > HALL_MAX_RATIO ? HALL_MAX_RATIO : (temp32-1);		// Increase accuracy by decreasing prsc
					g_TimerPSCChanged = 1;
				}
			}
			g_PeriodMeas[m_PeriodFIFO_Index].wPeriod = wCaptBuf*(hPrscBuf+1);		
			g_PeriodMeas[m_PeriodFIFO_Index].bDirection = m_HallDir;
			bGP1_OVF_Counter = 0;
			m_OvfPeriod = 0;
		}
		else		// û���������
		{  
			//����Ƿ���Ҫ�ı��Ƶֵ
			if(!bPRCChangedLastTime)
			{
				if (wCaptBuf < LOW_RES_THRESHOLD && HALL_TIMER->PSC > 0)
				{
					m_OldPRCReg = HALL_TIMER->PSC; 					//�Ĵ����仯ǰ��¼��ǰֵ
					temp32 = (wCaptBuf<<10)/BEST_CAP_VALUE;			//����ֵ�����ֵ�ı������Ŵ�1024��
				//	temp32 = ((hPrscBuf+1)*temp32)>>10; 			//������ѷ�Ƶϵ��������ʵ�ʷ��ֻᵼ���������֪��Ϊ�Σ�
					temp32 = ((hPrscBuf+1)*temp32)>>9;			    //ȡ���ۼ���ֵ��2�������÷�Ƶֵ̫С��
					(HALL_TIMER->PSC)= temp32 > 0 ? (temp32-1): 0;		// Increase accuracy by decreasing prsc
					g_TimerPSCChanged = 1;
				}
				else if(wCaptBuf > HIGH_RES_THRESHOLD && HALL_TIMER->PSC < HALL_MAX_RATIO)
				{
					m_OldPRCReg = HALL_TIMER->PSC; 					//�Ĵ����仯ǰ��¼��ǰֵ
					temp32 = (wCaptBuf<<10)/BEST_CAP_VALUE;			//����ֵ�����ֵ�ı������Ŵ�1024��
					temp32 = ((hPrscBuf+1)*temp32)>>10; 			//������ѷ�Ƶϵ��
					(HALL_TIMER->PSC) = temp32 > HALL_MAX_RATIO ? HALL_MAX_RATIO : (temp32-1);		// Increase accuracy by decreasing prsc
					g_TimerPSCChanged = 1;
				}	   
			}
			if(m_HallDir*s_prevHallDir < 0)	//�������˸ı䣬ͬʱû�з������������һ�εĲ���ֵ����ֹ�����ٶ�ͻ��
				g_PeriodMeas[m_PeriodFIFO_Index].wPeriod = s_prevCapPeriod;
			else
				g_PeriodMeas[m_PeriodFIFO_Index].wPeriod = wCaptBuf*(hPrscBuf+1);
			g_PeriodMeas[m_PeriodFIFO_Index].bDirection = m_HallDir;
		}
		s_prevCapPeriod = g_PeriodMeas[m_PeriodFIFO_Index].wPeriod;
		//�����ٶ�
		HALL_GetRotorFreq();
        
        if(OnHallEdgeCallback != NULL)
            OnHallEdgeCallback(m_HallDir);
	}
	else	 //��ʱ�������Ҳ����¶�ʱ����Ƶϵ��
	{
		TIM_ClearFlag(HALL_TIMER, TIM_FLAG_Update);	 	//�����־	
		if (bGP1_OVF_Counter < HALL_MAX_OVFCNT)
		{
			bGP1_OVF_Counter++;
		}
		if(g_TimerPSCChanged)		//���ǰ��Ƶϵ���ı��ˣ�ʹ�øı�ǰ��ֵ
		{
	//		if(m_OvfPeriod < HALL_ZERO_OVFTIME)		//
			if(m_OvfPeriod < HALL_MAX_OVFTIME)
				m_OvfPeriod += 0x10000uL*(m_OldPRCReg+1);
			g_TimerPSCChanged = 0;
		}
		else
		{
			if(m_OvfPeriod < HALL_MAX_OVFTIME)
				m_OvfPeriod += 0x10000uL*((HALL_TIMER->PSC)+1);
		}
		if(bGP1_OVF_Counter >= 2)				//����2�����
		{
			m_bHallTimeOut = 1;
			if (m_PeriodFIFO_Index < HALL_PERIOD_FIFO_SIZE-1)
				m_PeriodFIFO_Index++;
			else
				m_PeriodFIFO_Index = 0;
	//		g_PeriodMeas[m_PeriodFIFO_Index].wPeriod = m_OvfPeriod*2;
			g_PeriodMeas[m_PeriodFIFO_Index].wPeriod = m_OvfPeriod;
			g_PeriodMeas[m_PeriodFIFO_Index].bDirection = m_HallDir;
			HALL_GetRotorFreq();	
		}			  // */
		if(m_OvfPeriod >= HALL_MAX_OVFTIME && (HALL_TIMER->PSC) < HALL_MAX_RATIO) //�ۼ����ʱ�䳬�����ֵ������Ƶϵ����Ϊ���
		{
			m_OldPRCReg = (HALL_TIMER->PSC);
			(HALL_TIMER->PSC) = HALL_MAX_RATIO;
			g_TimerPSCChanged = 1;
		}
		if(m_OvfPeriod >= HALL_MAX_OVFTIME)
		{
//			(*g_pTagPos2) = 100;
		}
	}
	m_bCaptProcess = 0;
}

s32 HALL_GetAvrSpeed(void)
{
//	s32 temp32 = 0;
  s32 result = 0;
	static s32 s_oldSpeed = 0;
	
	if(m_bHallTimeOut)
		return 0;
	
	if(!m_bCaptProcess)			//���ڲ��������
	{
		s_oldSpeed = m_HallSpeed_Avr;
		result = m_HallSpeed_Avr;
	}
	else
	{
		result = s_oldSpeed;
	}
	
	return (result);
}

s32 HALL_GetDirSpeed(void)
{
	PeriodMeas_S PeriodMeasAux;
	s32 temp32 = 0;
	s32 result = 0;
	static s32 s_oldSpeed = 0;
	
	if(m_bHallTimeOut)
		return 0;
	
	if(!m_bCaptProcess)			//���ڲ��������
	{
		PeriodMeasAux = g_PeriodMeas[m_PeriodFIFO_Index];
		temp32 = 10*CKTIM/(PeriodMeasAux.wPeriod);		//60/(6*PeriodMeasAux.wPeriod/CKTIM);	//ת����rpm
		temp32 *= PeriodMeasAux.bDirection;
		s_oldSpeed = temp32;
		result = temp32;
	}
	else
	{
		result = s_oldSpeed;
	}
	
	return (result);
}

u16 HALL_IncElectricalAngle(void)
{
	static s32 s_olddpp = 0;
	
	u16 Electrical_Angle = 0;
	s32 temp32 = 0;
//	static s32 s_oldangle = 0;	//m_Hall_dpp_forsee
	
	m_forsee_time += (CKTIM/FOC_FREQ);		//���ڼ���Ԥ���ٶȵ�ʱ���ۼ�
	if(m_forsee_cnt > 0)
	{
		m_forsee_cnt--;
	}
	else if(!m_bHallTimeOut)	
	{
		m_Hall_dpp_forsee = PSEUDO_FREQ_CONV/m_forsee_time;	//�����ٶ�Ԥ��
		m_Hall_dpp_forsee *= m_HallDir; 
	}
	
//	temp32 = m_Hall_dpp;
	temp32 = m_Hall_dpp_forsee;
	
	if(!m_bCaptProcess)			//���ڲ����жϹ�����
	{
		m_Electrical_Angle += temp32;
		
		//������ǽ��Χ��
		if(m_Electrical_Angle > m_Elec_Angle_Wall_H)
		{
			m_Electrical_Angle = m_Elec_Angle_Wall_H;
		}
		else if(m_Electrical_Angle < m_Elec_Angle_Wall_L)
		{
			m_Electrical_Angle = m_Elec_Angle_Wall_L;
		}
	}
	else										//�ڲ����ж��У�m_Hall_dpp���ܴ��ڲ�ȷ��״̬������һ�ε��ٶ�ֵ
	{
		m_Electrical_Angle += s_olddpp;
		if(m_Electrical_Angle >= EANGLE_360)
		{
			m_Electrical_Angle -= EANGLE_360;
		}
		else if(m_Electrical_Angle < 0)
		{
			m_Electrical_Angle += EANGLE_360;
		}
	}	
		
	if(!m_bCaptProcess)
		s_olddpp = temp32; // */

	Electrical_Angle = (u16)((m_Electrical_Angle + 7)>>4);		//ת��Ϊ2^16
/*	Electrical_Angle = m_Electrical_Angle*EN_360/EANGLE_360;		//ת��Ϊ0~1000
	if(Electrical_Angle >= EN_360)
		Electrical_Angle -= EN_360;
	else if(Electrical_Angle < EN_0)
		Electrical_Angle += EN_360;// */
/*
	temp32 = Electrical_Angle - s_oldangle;
	if(temp32 > 40000)
		temp32 -= EANGLE_360>>4;
	else if(temp32 < -40000)
		temp32 += EANGLE_360>>4;
	
	if(ABS(temp32) > 50)
		*g_pCur = temp32;
//	*g_pTagPos = m_Hall_dpp*10;
//	*g_pPos = m_bHalfWall*5000;
	*g_pPos = m_Hall_dpp*10;// */
		
//	s_oldangle = Electrical_Angle;
	return Electrical_Angle;
}


