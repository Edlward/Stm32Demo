/******************************************************************/
//	Copyright (C), 2014-2016, �������� 
//  Author   	  : ����Ԫ  
//  Update Date   : 2014/06/25
//  Version   	  : 20140625          
//  Description   :  
/******************************************************************/

#ifndef FOC_GLOBLE_H
#define FOC_GLOBLE_H

#include <string.h>
#include "FOC_TYPE.h"

#define CKTIM	((s32)72000000) 			//ʱ��Ƶ��

//#define CKTIM	((s32)64000000) 			//ʱ��Ƶ��
//*
#define FOC_FREQ  	 	(10000)   			//FOC�ŷ�Ƶ��

#define FOC_1MS_CNT		(FOC_FREQ/1000)		//1ms����ֵ
#define FOC_100MS_CNT	(FOC_FREQ/10)		//100ms����ֵ

#define FOC_TIMER_TOP_CCR			1800			//��ʱ������ֵ���������ģʽ������50us
#define TOP_CCR FOC_TIMER_TOP_CCR
#define MAX_CCR			1760		 	//���ռ�ձȼ���ֵ��98%ռ�ձȣ�
/*


#define FOC_FREQ  	 	(8000)   			//FOC�ŷ�Ƶ��

#define FOC_1MS_CNT		(FOC_FREQ/800)		//1ms����ֵ
#define FOC_100MS_CNT	(FOC_FREQ/8)		//100ms����ֵ
#define FOC_TIMER_TOP_CCR			2000			//��ʱ������ֵ���������ģʽ������50us
#define MAX_CCR			1960		 	//���ռ�ձȼ���ֵ��98%ռ�ձȣ�

// */
#define N05_CCR			(FOC_TIMER_TOP_CCR*5/100)		 		//5%ռ�ձ�
#define N08_CCR			(FOC_TIMER_TOP_CCR*8/100)		 		//8%ռ�ձ�
#define N80_CCR			(FOC_TIMER_TOP_CCR*80/100)	 		//80%ռ�ձ�
#define N97_CCR			(FOC_TIMER_TOP_CCR*97/100)	 		//97%ռ�ձ�


//ȫ�ֱ�������
extern PID_Struct_t g_PID_Stru_Iq;
extern PID_Struct_t g_PID_Stru_Id;
extern PID_Struct_t g_PID_Stru_Speed;
extern Timer_CCRx_t g_TimerCCR_Stru;

extern Timer_CCRx_t g_TimerCCR_Stru_2;

extern Curr_Components g_Curr_q_d;
extern Curr_Components g_Curr_q_d_2;


#endif

