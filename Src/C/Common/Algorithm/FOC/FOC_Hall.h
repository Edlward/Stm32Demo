#ifndef FOC_HALL_H
#define FOC_HALL_H

#define HALL_TIMER 			TIM5

//#define ENABLE_ERROR_REC			//�Ƿ�ʹ�ú�ϻ�ӹ���

//Ӳ����ʼ��
u32 FOC_Hall_Int(void);						//AD��س�ʼ��

void Hall_TIM_IRQHandler(void);				//�жϴ���
u16 HALL_IncElectricalAngle(void);
s32 HALL_GetAvrSpeed(void);
s32 HALL_GetDirSpeed(void);
u8 Hall_GethalfWall(void);
u8 SVP_ReadHallState(void);

#endif
