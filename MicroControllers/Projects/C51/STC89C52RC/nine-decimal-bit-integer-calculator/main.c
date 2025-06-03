#include <REG52.H>
#include <string.h>
#include <intrins.h>

#define uint8_t unsigned char
#define uint16_t unsigned int
#define uint32_t unsigned long

// LCD引脚定义
// LCD pin definitions
#define LCM_DATA P0
#define INIT_CMD_LENGTH 5
sbit RS = P2^6;
sbit RW = P2^5;
sbit EN = P2^7;

// 24C02引脚定义
// 24C02 pin definitions
sbit SDA = P2^0;
sbit SCL = P2^1;

// 按键引脚定义
// Key pin definitions
sbit KEY1 = P3^1;
sbit KEY2 = P3^0;
sbit KEY3 = P3^2;
sbit KEY4 = P3^3;

// 矩阵键盘定义
// Matrix key definitions
#define KEY P1
uint8_t matrixKeyValue = 16;
uint8_t j, k, temp;

// TIM0 计数变量
// TIM0 counting variables
volatile uint16_t ntcount;
volatile uint16_t tcount = 0;
volatile uint16_t count = 0;

// 计算器相关变量
// Calculator related variables
volatile long calc_instantNum = 0L;
volatile long calc_accumulator = 0L;
volatile long calc_result = 0L;
volatile uint8_t calc_operator = 0; // 0:+ 1:- 2:* 3:/ 4:=
volatile long calc_isNegitive = 1L; // 1:+ -1:-
#define MAX_NUM 999999999L
#define MIN_NUM -999999999L
#define RESULT_POSITION 0x4f
#define INSTANTNUM_POSITION 0x0f


// 函数声明
// Function declarations
void delay_ms(uint16_t ms);
// LCM相关函数
// LCM related functions
void write_lcm(uint8_t cmd, uint8_t dir, uint8_t is_num);
void lcm_init(void);
void lcm_set_cursor(uint8_t position);
void lcm_display_string(uint8_t position, uint8_t *str);
void lcm_display_number(uint8_t position, long num, uint8_t is_result);
void lcm_clear(void);
void lcm_clear_operator(void);
void lcm_clear_num(void);
void lcm_display_result(long num);

// I2C相关函数
// I2C related functions
void I2C_Start(void);
void I2C_Stop(void);
uint8_t I2C_SendByte(uint8_t sdata, uint8_t ack);
uint8_t I2C_ReadByte();
void AT24C02_Write(uint8_t addr, uint8_t sdata);
uint8_t AT24C02_Read(uint8_t addr);

// TIM0相关函数
// TIM0 related functions
void TIM0_INIT(void);

// 其他相关函数
// Other related functions
void matrixKeyPressingHandler(keyValue);
void buzzer_beep(void);
void keyScan(void);
void matrixKeyScan(void);

void main(void) {

    TIM0_INIT();
    lcm_init();

    lcm_clear();

    KEY1 = 1;
    KEY2 = 1;
    KEY3 = 1;
    KEY4 = 1;
    
    while(1) {

        keyScan();
        matrixKeyScan();
        matrixKeyPressingHandler(matrixKeyValue);
        delay_ms(50);
        

    }
}


void TIM0_INIT(void) {
    TMOD = 0x01;
    TH0 = 0xfc;
    TL0 = 0x67;
    EA = 1;
    ET0 = 1;
    TR0 = 1;
}

void TIM0_ISR(void) interrupt 1 {

    TH0 = 0xfc;
    TL0 = 0x67;
    count++;
}

// 毫秒级延时函数
// Millisecond delay function
void delay_ms(uint16_t ms)
{
    count = 0;
    while(count != ms);
}

// LCM相关函数
// LCM related functions
void write_lcm(uint8_t cmd, uint8_t dir, uint8_t is_num) {
    RS = dir;
    RW = 0;
    EN = 1;
    LCM_DATA = cmd + is_num * 0x30;
    EN = 0;
    delay_ms(1);
}

void lcm_init(void) {
    uint8_t i;
    uint8_t init_cmd[INIT_CMD_LENGTH] = {0x38, 0x0c, 0x06, 0x01, 0x38};
    delay_ms(10);
    for(i = 0; i < INIT_CMD_LENGTH; i++) {
        write_lcm(init_cmd[i], 0, 0);
        delay_ms(10);
    }
}

void lcm_set_cursor(uint8_t position) {
    write_lcm(0x80+position, 0, 0);
}

void lcm_display_string(uint8_t position, uint8_t *str) {
    uint8_t i;
    lcm_set_cursor(position);
    for(i = 0; i < strlen(str); i++) {
        write_lcm(str[i], 1, 0);
    }
}

void lcm_display_number(uint8_t position, long num, uint8_t is_result) {
    uint8_t digits = 0;
    uint8_t i;
    long temp = num;
    uint8_t buffer[16];
    
    // 处理负数
    // Handle negative numbers
    if(temp < (long) 0) {
        if(is_result) {
            lcm_display_string(0x46, "-"); 
        }
        else {
            lcm_display_string(0x06, "-"); 
        }
        temp = -temp;
    }
    
    // 分解数字的每一位
    // Decompose the digits of the number
    do {
        buffer[digits++] = temp % 10;
        temp /= 10;
    } while(temp != 0);
    
    // 从左向右显示
    // Display from left to right
    for(i = 0; i < digits; i++) {
        lcm_set_cursor(position - i);
        write_lcm(buffer[i], 1, 1);
    }
}

void lcm_display_result(long num) {
    lcm_display_string(0x46, "                "); // 清除之前的结果 Clear previous results
    lcm_display_number(RESULT_POSITION, num, 1);
    lcm_set_cursor(0x00);
}

void lcm_clear(void) {
    write_lcm(0x01, 0, 0);
    delay_ms(2); 
}

void lcm_clear_operator(void) {
    lcm_display_string(0x40, "  ");
    lcm_set_cursor(0x00);
}

void lcm_clear_num(void) {
    lcm_display_string(0x45, "           ");
    lcm_set_cursor(0x00);
}

// I2C相关函数
// I2C related functions
void I2C_Start(void) {
    SDA = 1;
    SCL = 1;
    delay_ms(1);
    SDA = 0;
    delay_ms(1);
    SCL = 0;
}

void I2C_Stop(void) {
    SDA = 0;
    SCL = 1;
    delay_ms(1);
    SDA = 1;
    delay_ms(1);
}

uint8_t I2C_SendByte(uint8_t sdata, uint8_t ack) {
    uint8_t i, j;
    for(i = 0; i < 8; i++) {
        SDA = sdata >> 7;
        sdata <<= 1;
        delay_ms(1);
        SCL = 1;
        delay_ms(1);
        SCL = 0;
        delay_ms(1); 
    }
    SDA = 1;
    delay_ms(1);
    SCL = 1;
    while(SDA && (ack == 1)) {
        j++;
        if(j > 200) {
            SCL = 0;
            delay_ms(1);
            return 0;
        }
    }
    SCL = 0;
    delay_ms(1);
    return 1;
}

uint8_t I2C_ReadByte() {
    uint8_t i, rdata;
    SDA = 1;
    delay_ms(1);
    for(i = 0; i < 8; i++) {
        SCL = 1;
        delay_ms(1);
        rdata <<= 1;
        rdata |= SDA;
        delay_ms(1);
        SCL = 0;
        delay_ms(1);
    }
    return rdata;
}

void AT24C02_Write(uint8_t addr, uint8_t sdata) {
    I2C_Start();
    I2C_SendByte(0xa0, 0);
    I2C_SendByte(addr, 0);
    I2C_SendByte(sdata, 0);
    I2C_Stop();
}

uint8_t AT24C02_Read(uint8_t addr) {
    uint8_t rdata;
    I2C_Start();
    I2C_SendByte(0xa0, 0);
    I2C_SendByte(addr, 0);
    I2C_Start();
    I2C_SendByte(0xa1, 1);
    rdata = I2C_ReadByte();
    I2C_Stop();
    return rdata;
}

// 蜂鸣器相关函数
// Buzzer related functions
void buzzer_beep(void) {
    uint16_t cycles = 100;
    uint16_t delayms = 1; 
    uint16_t i;
    
    for(i = 0; i < cycles; i++) {
        RW = ~RW;  // 翻转蜂鸣器状态 Flip the buzzer state
        delay_ms(delayms);
    }
    RW = 0;  // 确保蜂鸣器最后关闭 // Ensure the buzzer is off at the end
}

long is_overflow(long num) {
    if(calc_instantNum < 214748363L) {
        return calc_instantNum * 10L;
    }
    else {
        lcm_display_string(0x40, "ER");
        return 999999999L - (num * calc_isNegitive);
    }
}

long is_accumulator_overflow(long num, uint8_t operator) {
    long result = 0L;
    switch (operator) {
        case 0: // act plus
            result = calc_accumulator + num;
            break;
        case 1: // act minus
            result = calc_accumulator - num;
            break;
        case 2: // act multiply
            if(num == 0) {
                return 0L;
            }
            result = calc_accumulator * num;
            if(result > MAX_NUM || result < MIN_NUM) {
                lcm_display_string(0x40, "ER");
                return (result > 0) ? MAX_NUM : MIN_NUM;
            }
            break;
        case 3: // act divide
            if(num != 0) {
                result = calc_accumulator / num;
            }
            else {
                lcm_display_string(0x40, "ER");
                return MAX_NUM;
            }
            break;
        default:
            result = calc_accumulator;
            break;
    }
    
    // 二次检查结果是否在允许范围内 Check the result again to ensure it is within the allowed range
    if(result > MAX_NUM) {
        lcm_display_string(0x40, "ER");
        return MAX_NUM;
    }
    if(result < MIN_NUM) {
        lcm_display_string(0x40, "ER");
        return MIN_NUM;
    }
    
    return result;
}

void numberHandler(long num) {
    calc_instantNum = is_overflow(num) + (num * calc_isNegitive);
    if(calc_instantNum > MAX_NUM || calc_instantNum < MIN_NUM) {
       lcm_display_string(0x40, "ER");
       return; 
    }
    lcm_display_number(INSTANTNUM_POSITION, calc_instantNum, 0);
    buzzer_beep(); 
}

void operatorHandler(uint8_t op) {
    switch (op) {
        case 0: // act plus
            if(calc_instantNum == 0) {
                calc_isNegitive = 1L; // 重置符号 Reset the sign
            }
            else {
                lcm_clear_operator();
                lcm_display_string(0x40, "+");
                calc_accumulator = is_accumulator_overflow(calc_instantNum, calc_operator);
                calc_isNegitive = 1L; // 重置符号 Reset the sign
                lcm_display_result(calc_accumulator);
                calc_operator = 0;
            }
            break;
        case 1: // act minus
            if(calc_instantNum == 0) {
                calc_isNegitive = -1L; // 设置负号 Set negative sign
            } else {
                lcm_clear_operator();
                lcm_display_string(0x40, "-");
                calc_accumulator = is_accumulator_overflow(calc_instantNum, calc_operator);
                calc_isNegitive = 1L; // 重置符号 Reset the sign
                lcm_display_result(calc_accumulator);
                calc_operator = 1;
            }
            break;
        case 2: // act multiply
            if(calc_instantNum == 0) {
                calc_isNegitive = 1L; // 重置符号 Reset the sign
            } else {
                lcm_clear_operator();
                lcm_display_string(0x40, "*");
                calc_accumulator = is_accumulator_overflow(calc_instantNum, calc_operator);
                calc_isNegitive = 1L; // 重置符号 Reset the sign
                lcm_display_result(calc_accumulator);
                calc_operator = 2;
            }
            break;
        case 3: // act divide
            if(calc_instantNum == 0) {
                calc_isNegitive = 1L; // 重置符号 Reset the sign
            } else {
                lcm_clear_operator();
                lcm_display_string(0x40, "/");
                calc_accumulator = is_accumulator_overflow(calc_instantNum, calc_operator);
                calc_isNegitive = 1L; // 重置符号 Reset the sign
                lcm_display_result(calc_accumulator);
                calc_operator = 3;
            }
            break;
        case 4: // act equal
            lcm_clear_operator();
            lcm_display_string(0x40, "=");
            calc_accumulator = is_accumulator_overflow(calc_instantNum, calc_operator);
            calc_isNegitive = 1L; // 重置符号 Reset the sign
            lcm_display_result(calc_accumulator);
            calc_operator = 4;
            calc_instantNum = 0;
            break;
    }
    
    // 检查计算结果是否溢出
    // Check if the calculation result overflows
    if(calc_accumulator > MAX_NUM || calc_accumulator < MIN_NUM) {
        lcm_display_string(0x40, "ER");
        calc_accumulator = 0;
        return;
    }
    
    calc_instantNum = 0;
    lcm_display_string(0x06, "                ");

    if(calc_isNegitive == -1L) {
        lcm_display_string(0x06, "-");
    } else {
        lcm_display_string(0x06, " ");
    }
}


// 矩阵键盘按键处理函数
// Matrix keyboard key handling function
void matrixKeyPressingHandler(keyValue) {
    
    switch (keyValue)
        {
        case 0: // num 7
            numberHandler(7);
            break;

        case 1: // num 8
            numberHandler(8);
            break;
        case 2: // num 9
            numberHandler(9);
            break;
        case 3: // act time
            operatorHandler(2);
            buzzer_beep();
            break;
        case 4: // num 4
            numberHandler(4);
            break;
        case 5: // num 5
            numberHandler(5);
            break;
        case 6: // num 6
            numberHandler(6);
            break;
        case 7: // act plus
            operatorHandler(0);
            buzzer_beep();
            break;
        case 8: // num 1
            numberHandler(1);
            break;
        case 9: // num 2
            numberHandler(2);
            break;
        case 10: // num 3
            numberHandler(3);
            break;
        case 11: // act minus
            operatorHandler(1);
            buzzer_beep();
            break;
        case 12: // act clear
            calc_instantNum = 0;
            calc_accumulator = 0;
            calc_result = 0;
            calc_operator = 0;
            calc_isNegitive = 1L; // 重置符号 Reset the sign
            lcm_clear_operator();
            lcm_clear_num();
            lcm_display_string(0x00, "                ");
            buzzer_beep();
            break;
        case 13: // num 0
            numberHandler(0);
            break;
        case 14: // act divide
            operatorHandler(3);
            buzzer_beep();
            break;
        case 15: // act equal
            operatorHandler(4);
            buzzer_beep();
            break;
        default:
            
            break;
        }
}

void keyScan(void) {
    if(KEY1 == 0) {     // act MC
        delay_ms(20);
        if(KEY1 == 0) {
            lcm_clear_operator();
            lcm_clear_num();
            AT24C02_Write(0, 0);
            AT24C02_Write(1, 0);
            AT24C02_Write(2, 0);
            AT24C02_Write(3, 0);
            lcm_display_string(0x40, "MC");
            buzzer_beep();
        }
        while(!KEY1);
        KEY1 = 1;
    }

    if(KEY2 == 0) {     // act MR
        delay_ms(20);
        if(KEY2 == 0) {
            uint32_t result_low;
            uint32_t result_high;
            result_low = AT24C02_Read(0) | (AT24C02_Read(1) << 8);
            result_high = AT24C02_Read(2) | (AT24C02_Read(3) << 8);
            lcm_clear_operator();
            lcm_clear_num();
            calc_result = result_low | (result_high << 16);
            lcm_display_result(calc_result);
            lcm_display_string(0x40, "MR");
            buzzer_beep();
        } 
        while(!KEY2);
        KEY2 = 1;
    }

    if(KEY3 == 0) {     // act M+
        delay_ms(20);
        if(KEY3 == 0) {
            long storedValue;
            uint32_t storedValue_low;
            uint32_t storedValue_high;
            storedValue_low = AT24C02_Read(0) | (AT24C02_Read(1) << 8);
            storedValue_high = AT24C02_Read(2) | (AT24C02_Read(3) << 8);
            lcm_clear_operator();
            lcm_clear_num();
            // 先读取当前存储值
            // First read the current stored value
            storedValue = storedValue_low | (storedValue_high << 16);
            // 加上当前数值
            // Add the current value
            if(storedValue + calc_instantNum > MAX_NUM || storedValue + calc_instantNum < MIN_NUM) {
                lcm_display_string(0x40, "ER");
                return; 
            }
            calc_result = storedValue + calc_instantNum;
            // 写回存储器
            // Write back to the memory
            AT24C02_Write(0, calc_result & 0xff);
            AT24C02_Write(1, (calc_result >> 8) & 0xff);
            AT24C02_Write(2, (calc_result >> 16) & 0xff);
            AT24C02_Write(3, (calc_result >> 24) & 0xff);
            lcm_display_string(0x40, "M+");
            buzzer_beep();
        } 
        while(!KEY3);
        KEY3 = 1;
    }

    if(KEY4 == 0) {     // act M-
        delay_ms(20);
        if(KEY4 == 0) {
            long storedValue;
            uint32_t storedValue_low;
            uint32_t storedValue_high;
            storedValue_low = AT24C02_Read(0) | (AT24C02_Read(1) << 8);
            storedValue_high = AT24C02_Read(2) | (AT24C02_Read(3) << 8);
            lcm_clear_operator();
            lcm_clear_num();
            // 先读取当前存储值
            // First read the current stored value
            storedValue = storedValue_low | (storedValue_high << 16);
            // 减去当前数值
            // Subtract the current value
            if(storedValue - calc_instantNum > MAX_NUM || storedValue - calc_instantNum < MIN_NUM) {
                lcm_display_string(0x40, "ER");
                return; 
            }
            calc_result = storedValue - calc_instantNum;
            // 写回存储器
            // Write back to the memory
            AT24C02_Write(0, calc_result & 0xff);
            AT24C02_Write(1, (calc_result >> 8) & 0xff);
            AT24C02_Write(2, (calc_result >> 16) & 0xff);
            AT24C02_Write(3, (calc_result >> 24) & 0xff);
            lcm_display_string(0x40, "M-");
            buzzer_beep();
        }
        while(!KEY4);
        KEY4 = 1;
    } 
}

void matrixKeyScan(void) {
    matrixKeyValue = 16;
    KEY = 0xf0;
    delay_ms(1);
    temp = KEY & 0xf0;
    if(temp != 0xf0) {
        switch (temp)
        {
            case 0x70:
                j = 0;
                break;
            case 0xb0:
                j = 1;
                break;
            case 0xd0:
                j = 2;
                break;
            case 0xe0:
                j = 3;
                break;
            
            default:
                break;
        }
    }
    else {
        matrixKeyValue = 16;
    }
    KEY = 0x0f;
    delay_ms(1);
    temp = KEY & 0x0f;
    if(temp != 0x0f) {
        switch (temp)
        {
            case 0x07:
                k = 0;
                break;
            case 0x0b:
                k = 1;
                break;
            case 0x0d:
                k = 2;
                break;
            case 0x0e:
                k = 3;
                break;
            
            default:
                break;
        }
        matrixKeyValue = j * 4 + k;
        while((KEY & 0x0f) != 0x0f);
    }
    else {
        matrixKeyValue = 16;
    }
}
