/******************************************************************/
//	Copyright (C), 2014-2016, �������� 
//  Author   	  : ����Ԫ  
//  Update Date   : 2014/06/25
//  Version   	  : 20140625          
//  Description   :  
/******************************************************************/

#ifndef FOC_CTRL_PARAM_H
#define FOC_CTRL_PARAM_H

#include "FOC_TYPE.h"

#define PID_CURR_KP_DEFAULT  (s16)800       
#define PID_CURR_KI_DEFAULT  (s16)500
#define PID_CURR_KD_DEFAULT  (s16)0

#define PID_CURR_KPDIV ((u16)(2048))
#define PID_CURR_KIDIV ((u16)(8192))
#define PID_CURR_KDDIV ((u16)(2048))

//��ʽ����  2000   1250  3000  2000
//�˶��������2000   4000  3000  3000
//���ʰ棺    2000   400   2000  1000
#define PID_SPD_KP_DEFAULT  (s16)2000       //����470ת����2000
#define PID_SPD_KI_DEFAULT  (s16)1250
#define PID_SPD_KD_DEFAULT  (s16)3000

#define PID_SPD_KPDIV ((u16)(2048))
#define PID_SPD_KIDIV ((u16)(8192))
#define PID_SPD_KDDIV ((u16)(2048))


#define PID_CURR_OUTPUT_LIMIT (S16_MAX*98/100)	   //Q�����PID������Ϊ��ֵ��98%
#define PID_CURR_D_OUTPUT_LIMIT (S16_MAX*20/100)	//D�����PID������Ϊ��ֵ��98%

#define PID_SPD_OUTPUT_LIMIT (S16_MAX*96/100)	   //����PID������Ϊ��ֵ��96%


#endif

