
/*********************************************************************************************
程序名： 　　 1602 显示 测试程序
编写人： 　　 yuwx.net　
编写时间：　　2011年6月18日
硬件支持：　　STC12C5A60S2 12MHz晶体（不可换其他8051单片机）
接口说明：　　
修改日志：　　
      1.实现DB1直接控制Lcd1602,显示，时间，自定义图像，V1.0. 2014/04/22 01:58:00 TIME: 2014.04.21	
      2.增加红外效果.P3.2 定义LCM2402的RS控制线,改为1.3，P3.2定义为外部红外中断信号线  TIME:2014.05.04    
      3. 添加串口通讯。（电脑和DB1,红外遥控，发串码）
	2014.05.05 01:42 pc->DB1 串口接收Ok
/*********************************************************************************************/

#include <DB1.h>
//#include "lcd.h"

#define MAX_STRING_LEN 16
unsigned char talbe1[MAX_STRING_LEN]=" [www.yuwx.net] ";
//unsigned char talbe1[MAX_STRING_LEN]="0123456789";
unsigned char talbe2[MAX_STRING_LEN]="LCD1602 test ok!";

unsigned char code CDIS1[13]={" Red Ctrl"}; //{" Red Control"};
unsigned char code CDIS2[13]={" IR-CODE --H"};

unsigned char IrValue[6] = {0};
unsigned char Time = 0;
unsigned int nPushCounts = 0;//按键次数计数
unsigned char uType = 0;//0:显示红外 1:显示串口

//void DELAY_MS (unsigned int a);

typedef unsigned char      uint8;          // 无符号8位整型变量 //

/********************************************************************************************
// 引脚定义 // （使用者根据实际更改）
/********************************************************************************************/
#define		LCM2402_DB0_DB7		P2			// 定义LCM2402的数据总线

sbit IRIN	      = P3 ^ 2;					//红外信号线
sbit LCM2402_RS   = P1 ^ 3;					// 定义LCM2402的RS控制线
sbit LCM2402_RW   = P3 ^ 3;					// 定义LCM2402的RW控制线
sbit LCM2402_E    = P3 ^ 4;					// 定义LCM2402的E控制线
sbit LCM2402_Busy = P2 ^ 7;					// 定义LCM2402的测忙线（与LCM2402_DB0_DB7关联）

sbit LED_IR = P1 ^ 7; //定义测试红外LED

data unsigned char TIME_DD,TIME_MO,TIME_YY,TIME_WW,TIME_HH,TIME_MM,TIME_SS;//设置日、月、年、周、时、分、秒和温度存放区
data bit DAY_BIT = 0;//天数增加标志位（用于日期进位的启动）
data unsigned char DIS_BIT = 0; //多种信息的切换显示
data unsigned char cou  = 0;     // 软计数器,对10ms时基信号累加到1s 
/********************************************************************************************
// 定义LCM2402指令集 // （详细请见技术手册）
/********************************************************************************************/
#define			CMD_clear		0x01             // 清除屏幕
#define			CMD_back		0x02             // DDRAM回零位
#define			CMD_dec1		0x04             // 读入后AC（指针）减1，向左写
#define			CMD_add1		0x06             // 读入后AC（指针）加1，向右写
#define			CMD_dis_gb1		0x0f             // 开显示_开光标_开光标闪烁
#define			CMD_dis_gb2		0x0e             // 开显示_开光标_关光标闪烁
#define			CMD_dis_gb3		0x0c             // 开显示_关光标_关光标闪烁
#define			CMD_OFF_dis		0x08             // 关显示_关光标_关光标闪烁
#define			CMD_set82		0x38             // 8位总线_2行显示
#define			CMD_set81		0x30             // 8位总线_1行显示（上边行）
#define			CMD_set42		0x28             // 4位总线_2行显示
#define			CMD_set41		0x20             // 4位总线_1行显示（上边行）
#define			lin_1			0x80             // 4位总线_1行显示（上边行）
#define			lin_2			0xc0             // 4位总线_1行显示（上边行）

/*4. 显示红外键码*/
void DisplayIRCode();
void DelayMs(unsigned int x);   //0.14ms误差 0us
void IrInit();//初始化红外

//显示十进制计数
void DisplayTenCode(int nNumber);
//显示16进制计数
void DisplayHexCode(int nNumber);
//设置串口
void UsartConfiguration();

//读串口数据
unsigned char ReadSerial();
//显示串口数据
void DisplaySerialCode(unsigned char uCode);

//向串口发送一个字符 
void send_char_com(unsigned char ch);
//向串口发送一个字符串,长度不限。//应用：send_string_com("d9887321$"); 
void send_string_com(unsigned char *str);

void TestSerial();
//测试红外按键发串码
void IrKeyTest();

/********************************************************************************************
// 读LCM忙程序 [底层协议] // （所有底层协议都无需关注）
// LCM2402测忙，若LCM2402处于忙状态，本函数将等待至非忙状态 //
/********************************************************************************************/
void LCM2402_TestBusy(void){
   	LCM2402_DB0_DB7 = 0xff;		//设备读状态
   	LCM2402_RS = 0;
   	LCM2402_RW = 1;
   	LCM2402_E = 1;
   	while(LCM2402_Busy);		//等待LCM不忙
   	LCM2402_E = 0;				//
}
/********************************************************************************************
// 写指令程序 //
// 向LCM2402写命令 本函数需要1个指令集的入口参数 //
/********************************************************************************************/
void LCM2402_WriteCMD(uint8 LCM2402_command) { 
  	LCM2402_TestBusy();
  	LCM2402_DB0_DB7 = LCM2402_command;
  	LCM2402_RS = 0;
  	LCM2402_RW = 0;
  	LCM2402_E = 1;
  	LCM2402_E = 0;
}
/********************************************************************************************
// 写数据程序 //
// 向LCM2402写数据 //
/********************************************************************************************/
void LCM2402_WriteData(uint8 LCM2402_data){ 
    LCM2402_TestBusy();
	LCM2402_DB0_DB7 = LCM2402_data;
  	LCM2402_RS = 1;
  	LCM2402_RW = 0;
  	LCM2402_E = 1;
  	LCM2402_E = 0;
}
/********************************************************************************************
// 打印字符串程序 // （本函数调用指针函数）
// 向LCM发送一个字符串,长度48字符之内 
// 第一行位置 0x00~0x17  第二行位置 0x40~0x57 
// 应用举例：print(0x80,"doyoung.net"); //在第一行第一位处从左向右打印doyoung.net字符串
/********************************************************************************************/
void print(uint8 a,uint8 *str){
	LCM2402_WriteCMD(a | 0x80);
	while(*str != '\0'){
		LCM2402_WriteData(*str++);
	}
	*str = 0;
}
/********************************************************************************************
// 打印单字符程序 // 
// 第一行位置 0x00~0x17  第二行位置 0x40~0x57 
// 向LCM发送一个字符,以十六进制（0x00）表示 
// 应用举例：print(0xc0,0x30); //在第二行第一位处打印字符“0”
/********************************************************************************************/
void print2(uint8 a,uint8 t){
		LCM2402_WriteCMD(a | 0x80);
		LCM2402_WriteData(t);
}
/********************************************************************************************
// 定义小汉字 //
// 可写入8个自字义字符，写入后可用其CGRAM代码直接提取显示。
// 字符定义方法请参考技术手册 
/********************************************************************************************/
uint8 code Xword[]={
    0x18,0x18,0x07,0x08,0x08,0x08,0x07,0x00,        //℃，代码 0x00
    0x00,0x00,0x00,0x00,0xff,0x00,0x00,0x00,        //一，代码 0x01
    0x00,0x00,0x00,0x0e,0x00,0xff,0x00,0x00,        //二，代码 0x02
    0x00,0x00,0xff,0x00,0x0e,0x00,0xff,0x00,        //三，代码 0x03
    0x00,0x00,0xff,0xf5,0xfb,0xf1,0xff,0x00,        //四，代码 0x04
    0x00,0xfe,0x08,0xfe,0x0a,0x0a,0xff,0x00,        //五，代码 0x05
    0x00,0x04,0x00,0xff,0x00,0x0a,0x11,0x00,        //六，代码 0x06
    0x00,0x1f,0x11,0x1f,0x11,0x11,0x1f,0x00,        //日，代码 0x07
};

uint8 code HeartWord[]={
	0x03,0x07,0x0f,0x1f,0x1f,0x1f,0x1f,0x1f,
    0x18,0x1E,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,
    0x07,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,
    0x10,0x18,0x1c,0x1E,0x1E,0x1E,0x1E,0x1E,
    0x0f,0x07,0x03,0x01,0x00,0x00,0x00,0x00,
	0x1f,0x1f,0x1f,0x1f,0x1f,0x0f,0x07,0x01,
	0x1f,0x1f,0x1f,0x1f,0x1f,0x1c,0x18,0x00,                     
	0x1c,0x18,0x10,0x00,0x00,0x00,0x00,0x00};//心图案
	
	
/*
void CgramWrite(void) 
{	// 装入CGRAM //
    uint8 i;
	LCM2402_WriteCMD(0x06);			// CGRAM地址自动加1
	LCM2402_WriteCMD(0x40);			// CGRAM地址设为00处
    for(i=0;i<64;i++) {
		LCM2402_WriteData(Xword[i]);// 按数组写入数据
    }
}
*/
	
//void CgramWrite(void) 
void CgramWrite_byType(uint8 type)
{	// 装入CGRAM //
    uint8 i;
	LCM2402_WriteCMD(0x06);			// CGRAM地址自动加1
	LCM2402_WriteCMD(0x40);			// CGRAM地址设为00处
    for(i=0;i<64;i++) {
		if(1 == type)
		{
			LCM2402_WriteData(Xword[i]);// 按数组写入数据
		}
    	else if(2 == type)
		{
			LCM2402_WriteData(HeartWord[i]);// 按数组写入数据
		}
		else{
			//
		}
    }
}
/********************************************************************************************
// LCM2402初始化 //（使用者可自定义，加 * 号程序行必须保留但可修改）
/********************************************************************************************/
void LCM2402_Init(void){
  	LCM2402_WriteCMD(CMD_set82);	//* 显示模式设置：显示2行，每个字符为5*7个像素
  	LCM2402_WriteCMD(CMD_clear);	//  显示清屏
  	LCM2402_WriteCMD(CMD_back);		//* 数据指针指向第1行第1个字符位置
  	LCM2402_WriteCMD(CMD_add1);		//  显示光标移动设置：文字不动，光标右移
  	LCM2402_WriteCMD(CMD_dis_gb3); 	//  显示开及光标设置：显示开，光标开，闪烁开
	//CgramWrite();					//  向CGRAM写入自定义字符
	//CgramWrite_byType(2);
}
/********************************************************************************************/
//			以上是LCM2402驱动程序			//
/*********************************************************************************************/

/*********************************************************************************************/
bit IsLeapYear(void){    //计算本年是否是润年 
	unsigned int a;
	a = 2000+TIME_YY;//加2000表示成完整的年
	if((a%4==0 && a%100!=0)||(a%400==0)){ //润年的计算公式
			return 1;//是润年返回1 
		}else{ 
 			return 0;//不是润年返回0 
		}
} 
/**********************************************************************************************/	
void month_day(void){  
	unsigned char mon_day[]={31,28,31,30,31,30,31,31,30,31,30,31};
	TIME_DD++;//天加1
	TIME_WW++;//星期值加1
	if(TIME_WW > 7){
		TIME_WW = 1;//时期值限定
	}
	if(TIME_DD > mon_day[TIME_MO-1]){//检查天是否大于当月的最大值
		if(IsLeapYear()&&TIME_MO==2){ //计算本月是否是润年的2月份 
			TIME_DD = 29;//如果是润年又是2月，则天为29
		}else{
			TIME_DD = 1; //否则就等于1
			TIME_MO++;//月加1
			if(TIME_MO > 12){
				TIME_MO = 1; //如果月大于12则月等于1
				TIME_YY++;//年加1 （公历年无限积加）
			}
		}
	}
} 
/**********************************************************************************************/	
void init (void){ //上电初始化
	TMOD = 0x11;         // 定时/计数器0,1工作于方式1   
    TH0 = 0x3c;          // 预置产生50ms时基信号   
    TL0 = 0xb0;   
    EA = 1;              // 开总中断   
    ET0 = 1;             // 定时/计数器0允许中断   
    TR0 = 1;             // 开闭定时/计数器0   
////
	TIME_DD = 4; //时间在首次使用的值，之后会在EEPROM自动记录上一天的值
	TIME_MO	= 5; //初始时间：2009年5月18日周一，20时13分40秒
	TIME_YY = 14;
	TIME_WW = 7;
	TIME_HH	= 20;
	TIME_MM = 30;
	TIME_SS = 0;
}
/********************************************************************************************
//显示项目 时间部分 在第一行全行显示时间
*********************************************************************************************/    
void RealTime_Display(void){
	    print(0x80,"20");
	    print2(0x82,TIME_YY/10+0x30);
	    print2(0x83,TIME_YY%10+0x30);
		print(0x84,"/");            // 显示年
		//
	    print2(0x85,TIME_MO/10+0x30);
	    print2(0x86,TIME_MO%10+0x30);
		print(0x87,"/");            // 显示月
		//
	    print2(0x88,TIME_DD/10+0x30);
	    print2(0x89,TIME_DD%10+0x30);

		print(0x8b,"[");            // 显示[
		print2(0x8c,TIME_WW%10); //星期
		print(0x8d,"]");            // 显示]

	    print2(0x40,TIME_HH/10+0x30);//小时
	    print2(0x41,TIME_HH%10+0x30);
		print(0x42,":");            // 显示cgram第一个字模"："
		//
	    print2(0x43,TIME_MM/10+0x30);//分钟
	    print2(0x44,TIME_MM%10+0x30);
		print(0x45,".");            // 显示cgram第一个字模"."
		//
	    print2(0x46,TIME_SS/10+0x30);//秒
	    print2(0x47,TIME_SS%10+0x30);
		//
}

void tiem0(void) interrupt 1{   // T/C0中断服务程序(产生50ms时基信号)   
    cou++;                      // 软计数器加1   
    if(cou > 19){                 // 计数值到100(1s)   
        cou = 0;               // 软计数器清零   
        TIME_SS++;                 // 秒计数器加1(进位10ms*100=1s)   
        if(TIME_SS > 59){          // 秒计数值到60   
           
            TIME_SS = 0;           // 秒计数器清零   
            TIME_MM++;             // 分计数器加1(进位60s=1m)  
            if(TIME_MM > 59){      // 分计数到60   
                TIME_MM = 0;       // 分计数器清零   
                TIME_HH++;         // 时计数器加1(进位60m=1h)   
                if(TIME_HH > 23){  // 时计数到23   
                    TIME_HH = 0;   // 时计数器清零
					DAY_BIT = 1;	//天增加标志位 
				}  
            }   
        }   
    }   
    TH0 = 0x3c;                // 重置定时常数   
    TL0 = 0xb0;   
}
/*********************************************************************************************/


/********************************************************************************************/
/*
// 显示时钟函数 //
void main (void)
{
	init();//初始化                           
	LCM2402_Init();//LCM2402初始化                           
	while(1)
	{		
		RealTime_Display();    	
//		print(0x80,"DoYoung Studio"); //在第一行第一位处从左向右打印doyoung.net字符串
//		print(0x40,"www.DoYoung.net"); //在第一行第一位处从左向右打印doyoung.net字符串

		if(DAY_BIT == 1)
		{ 
			//检查天数是否更新，是则计算公历
			month_day();//计算公历日期	
			DAY_BIT = 0;//计算完成后将日期变更标志位置0
		}
	}
}
*/
/********************************************************************************************/    

/*********************************************************************************************
函数名：毫秒级CPU延时函数
调  用：DELAY_MS (?);
参  数：1~65535（参数不可为0）
返回值：无
结  果：占用CPU方式延时与参数数值相同的毫秒时间
备  注：应用于1T单片机时i<600，应用于12T单片机时i<125
/*********************************************************************************************/
void Delay1ms (unsigned int a) //误差 0us
{
	unsigned int i;
	while( a-- != 0)
	{
		for(i = 0; i < 600; i++);
	}
}

void DisplayLove();

/*******************************************************************************
函数名：主函数
调  用：无
参  数：无
返回值：无
结  果：程序开始处，无限循环
备  注：
*******************************************************************************/
void main()
{	
	uint8 i = 0;
	unsigned char uCodeSerial = 0;
	unsigned int nPushCountsPre = 0;//记录之前的计数，用于不更新时不操作
	
	init();//初始化                           
	LCM2402_Init();//LCM2402初始化	
	IrInit();//初始化红外
	
/* 1.显示test */
	print(0x80, talbe1);
	print(0x40, talbe2);	
	Delay1ms(1000);	
	LCM2402_WriteCMD(CMD_clear);//清屏
	
/*2. 显示Love! */
	DisplayLove();
	
	Delay1ms(1000);	
	LCM2402_WriteCMD(CMD_clear);	//清屏

/*. 显示红外键码前面字符串*/
	//CDIS1
	print(0x80, CDIS1);
	print(0x40, CDIS2);	
	Delay1ms(1000);		
	//LCM2402_WriteCMD(CMD_clear);//清屏
	
	//写入自定义图形
	CgramWrite_byType(1);
	
	//串口设置
	UsartConfiguration();
	
	while(1)
	{		
		//print(0x80,"DoYoung Studio"); //在第一行第一位处从左向右打印doyoung.net字符串
		//print(0x40,"www.DoYoung.net"); //在第一行第一位处从左向右打印doyoung.net字符串		
		//print2(0x4a, TIME_SS % 7);		

		/*3. 显示时间Realtime */
		/*
		RealTime_Display();
		
		if(DAY_BIT == 1)
		{ 
			//检查天数是否更新，是则计算公历
			month_day();//计算公历日期	
			DAY_BIT = 0;//计算完成后将日期变更标志位置0
		}
		*/
		
		/*4. 显示红外键码*/
		DisplayIRCode();
		
		//读串口数据
		uCodeSerial = ReadSerial();
		
		if(0 == uType)
		{
			DisplayHexCode((int)nPushCounts);
			DisplayTenCode((int)nPushCounts);
			
			//send_string_com("温度:");	//显示汉字
			//send_char_com(i/10+0x30);	//显示温度 十位
			//send_char_com(i%10+0x30);	//显示温度 个位
			//send_char_com(0x54);
			//send_string_com("℃");	//度C
			
			if(nPushCountsPre != nPushCounts)
			{
				//测试红外按键发串码
				IrKeyTest();
			}			
			
			//Delay1ms(500);
		}
		else if(1 == uType)
		{
			/*5. 显示串口数据 */
			DisplaySerialCode(uCodeSerial);
			//Delay1ms(1000);
		}
		else{
			//
		}
		
		nPushCountsPre = nPushCounts;
		
		Delay1ms(10);
	}
}

void DisplayLove()
{
	uint8 i = 0;
	
	CgramWrite_byType(2);
	
	print(0x81, "I ");
		
	for(i=0; i<8; i++)
	{
		if(i<4)
		{
			print2(0x83+i, i);
		}
		else
		{
			print2(0x43+(i-4), i);
		}			
	}
	
	print(0x47, "Luo Ying!");
}

/*4. 显示红外键码*/
void DisplayIRCode()
{
	unsigned char uAdrr = 0x00;
	unsigned char uValue = 0x00;
	IrValue[4]=IrValue[2]>>4;	 	 	//高位
	IrValue[5]=IrValue[2]&0x0f;		//低位	

	if(IrValue[4]>9)
	{
		//LcdWriteCom(0xc0+0x09);			//设置显示位置
		//LcdWriteData(0x37+IrValue[4]);	//将数值转换为该显示的ASCII码
		uValue = 0x37+IrValue[4];
	}
	else
	{
		//LcdWriteCom(0xc0+0x09);
		//LcdWriteData(IrValue[4]+0x30);	//将数值转换为该显示的ASCII码
		uValue = 0x30+IrValue[4];
	}
	uAdrr = 0xc0 + 0x09;
	print2(uAdrr, uValue);
	
	if(IrValue[5]>9)
	{
		//LcdWriteCom(0xc0+0x0a);
		//LcdWriteData(IrValue[5]+0x37);		//将数值转换为该显示的ASCII码
		uValue = 0x37+IrValue[5];
	}
	else
	{
		//LcdWriteCom(0xc0+0x0a);
		//LcdWriteData(IrValue[5]+0x30);		//将数值转换为该显示的ASCII码
		uValue = 0x30+IrValue[5];
	}
	uAdrr = 0xc0 + 0x0a;
	print2(uAdrr, uValue);	
}

//显示十进制计数
void DisplayTenCode(int nNumber)
{
	unsigned char uBai = 0x00;//百位
	unsigned char uGe = 0x00;//个位
	unsigned char uShi = 0x00;//十位
	unsigned char uAdrr = 0x00;
	unsigned char uValue = 0x00;
	
	//最后显示按键次数计数,十进制显示
	uBai = nNumber/100;
	uShi = (nNumber%100)/10;
	uGe = nNumber % 10;
	
	if(uBai > 9)
	{
		uBai = 0;//大于999的数从0开始计数
	}
	uAdrr = 0xc0 + 0x0d;
	uValue = 0x30 + uBai;
	print2(uAdrr, uValue);
	
	uAdrr = 0xc0 + 0x0e;
	uValue = 0x30 + uShi;
	print2(uAdrr, uValue);	
	
	uAdrr = 0xc0 + 0x0f;
	uValue = 0x30 + uGe;
	print2(uAdrr, uValue);
}

//显示16进制计数
void DisplayHexCode(int nNumber)
{
	unsigned char uHex01 = 0x00;//十六进制低位
	unsigned char uHex02 = 0x00;//十六进制高位
	unsigned char uAdrr = 0x00;
	unsigned char uValue = 0x00;
	
	//十六进制显示
	uAdrr = 0x80 + 0x0e;//第一行第14个字符
	uHex01 = nNumber % 16;
	uHex02 = nNumber / 16 ;
	if(uHex02 > 9)//先写高位
	{
		uValue = 0x37 + uHex02;
	}
	else
	{
		uValue = 0x30 + uHex02;
	}	
	print2(uAdrr, uValue);
	
	uAdrr = 0x80 + 0x0f;//第一行第15个字符
	if(uHex01 > 9)
	{
		uValue = 0x37 + uHex01;
	}
	else
	{
		uValue = 0x30 + uHex01;
	}	
	print2(uAdrr, uValue);
}

/*******************************************************************************
* 函数名         : DelayMs()
* 函数功能		   : 延时
* 输入           : x
* 输出         	 : 无
*******************************************************************************/
void DelayMs(unsigned int x)   //0.14ms误差 0us
{
	unsigned char i;
	while(x--)
	{
		//for (i = 0; i<13; i++){}
		for(i=0; i<83; i++){}//600*0.14 = 84
	}
}

/*******************************************************************************
* 函数名         : IrInit()
* 函数功能		   : 初始化红外线接收
* 输入           : 无
* 输出         	 : 无
*******************************************************************************/
void IrInit()
{
	int n = 4;
	
	IT0=1;//下降沿触发
	EX0=1;//打开中断0允许
	EA=1;	//打开总中断

	IRIN=1;//初始化端口
	
	//测试LED工作是否正常	
	while(n > 0)
	{
		LED_IR = 1;
		Delay1ms(300);
		LED_IR = 0;
		Delay1ms(300);
		
		--n;
	}	
}

/*******************************************************************************
* 函数名         : ReadIr()
* 函数功能		   : 读取红外数值的中断函数
* 输入           : 无
* 输出         	 : 无
*******************************************************************************/
void ReadIr() interrupt 0
{
	unsigned char j,k;
	unsigned int err;
	Time=0;					 
	DelayMs(70);

	if(IRIN==0)		//确认是否真的接收到正确的信号
	{		
		err=1000;				//1000*10us=10ms,超过说明接收到错误的信号
		/*当两个条件都为真是循环，如果有一个条件为假的时候跳出循环，免得程序出错的时
		侯，程序死在这里*/	
		while((IRIN==0)&&(err>0))	//等待前面9ms的低电平过去  		
		{
			DelayMs(1);
			err--;
		}		
		
		if(IRIN==1)			//如果正确等到9ms低电平
		{
			err=500;
			while((IRIN==1)&&(err>0))		 //等待4.5ms的起始高电平过去
			{
				DelayMs(1);
				err--;
			}
			for(k=0;k<4;k++)		//共有4组数据
			{				
				for(j=0;j<8;j++)	//接收一组数据
				{
					err=60;		
					while((IRIN==0)&&(err>0))//等待信号前面的560us低电平过去
					{
						DelayMs(1);
						err--;
					}
					err=500;
					while((IRIN==1)&&(err>0))	 //计算高电平的时间长度。
					{
						DelayMs(1);//0.14ms
						Time++;
						err--;
						if(Time>30)
						{
							EX0=1;
							return;
						}
					}
					IrValue[k]>>=1;	 //k表示第几组数据
					if(Time>=8)			//如果高电平出现大于565us，那么是1
					{
						IrValue[k]|=0x80;
					}
					Time=0;		//用完时间要重新赋值							
				}
			}
			
			//测试红外LED更改状态
			LED_IR = ~LED_IR;
			nPushCounts++;
			uType = 0;//开红外显示
		}
		if(IrValue[2]!=~IrValue[3])
		{
			return;
		}
	}			
}

/*******************************************************************************
* 函 数 名         :UsartConfiguration()
* 函数功能		   :设置串口
* 输    入         : 无
* 输    出         : 无
*******************************************************************************/
void UsartConfiguration()
{
	/* //PZ init
	SCON=0X50;			//设置为工作方式1
	TMOD=0X20;			//设置计数器工作方式2
	PCON=0X80;			//波特率加倍
	TH1=0XF3;		    //计数器初始值设置，注意波特率是4800的
	TL1=0XF3;
	TR1=1;			  //打开计数器
*/
	//Delay1ms(200); //
	//P1M0 = 0x01;//强推?
	
	TMOD = 0x20;	//定时器T/C1工作方式2
	SCON = 0x50;	//串口工作方式1，允许串口接收（SCON = 0x40 时禁止串口接收）
	TH1 = 0xF3;	//定时器初值高8位设置
	TL1 = 0xF3;	//定时器初值低8位设置
	PCON = 0x80;	//波特率倍频（屏蔽本句波特率为2400）
	TR1 = 1;	//定时器启动
}

//读串口数据
unsigned char ReadSerial()
{
	unsigned char receiveData = 0x00;
	if(RI == 1)				//查看是否接收到数据
	{
		receiveData = SBUF;	//读取数据
		RI = 0;				//清除标志位
		
		uType = 1;//开串口显示		
		//接收到串口数据指示灯
		LED_IR = ~LED_IR;
		
		Delay1ms(20);
	}
	
	return receiveData;
}

//显示串口数据
void DisplaySerialCode(unsigned char uCode)
{
	if(uCode != 0)
	{
		//LcdWriteCom(0xC0);
		//--因为一次接受只能接收到8位数据，最大为255，所以只用显示百位之后--//
		//LcdWriteData('0' + (receiveData / 100));      // 百位
		//LcdWriteData('0' + (receiveData % 100 / 10)); // 十位
		//LcdWriteData('0' + (receiveData % 10));		  // 个位
		DisplayHexCode((int)uCode);
		DisplayTenCode((int)uCode);
	}
}

//向串口发送一个字符 
void send_char_com(unsigned char ch)
{
	SBUF=ch;
	//while(TI == 0);
	//TI=0;
}

//向串口发送一个字符串,长度不限。//应用：send_string_com("d9887321$"); 
void send_string_com(unsigned char *str)
{
	while (*str != '\0')
	{
		send_char_com(*str);
		*str++; 
	}
	*str = 0;
}

/* void TestSerial()
{
	send_char_com(0xDD);
	send_char_com(0xDD);	
	send_char_com(0xDD);
	send_char_com(0xDD);	
	
	for(i=1; i<5; i++)
	{
		send_char_com(IrValue[i]);
	}
	
	send_char_com(0xFF);
	send_char_com(0xFF);
	send_char_com(0xFF);
	send_char_com(0xFF);
} */

//测试红外按键发串码
void IrKeyTest()
{
	//unsigned char uCode = (IrValue[4] << 4) + (IrValue[5] & 0x0f);
	/*
	unsigned char i = 0;
	for(i=0; i<128; i++)
	{
		send_char_com(i);
	}	
	*/
	
	unsigned char uCode = IrValue[2];
	
	switch(uCode)
	{
		case 0x05:
		{
			send_string_com("温度:");	//显示汉字
			break;
		}
		default:
		{
			//send_char_com(uCode);
			//send_string_com("FFFF");
			send_char_com(uCode);
			//send_string_com("DDDD");
			break;
		}			
	}
	
	//Delay1ms(2000);
}