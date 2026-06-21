#include <xc.h>
#include <stdio.h> // Thư viện dùng hàm sprintf

// Cấu hình Configuration Bits cho PIC16F877A
#pragma config FOSC = HS        
#pragma config WDTE = OFF       
#pragma config PWRTE = OFF      
#pragma config BOREN = OFF      
#pragma config LVP = OFF        
#pragma config CPD = OFF        
#pragma config WRT = OFF        
#pragma config CP = OFF         

#define _XTAL_FREQ 20000000     

// Định nghĩa các chân ngoại vi
#define LED_WARNING PORTDbits.RD0
#define LED_SAFE    PORTDbits.RD1
#define BUZZER      PORTDbits.RD2

#define BTN_UP      PORTBbits.RB0
#define BTN_DOWN    PORTBbits.RB1

// ==========================================
// THƯ VIỆN LCD CHỐNG LỖI RMW (BLOCK WRITE)
// ==========================================
void LCD_Send(unsigned char data, unsigned char rs_value) {
    PORTC = (data << 2) | rs_value; 
    PORTCbits.RC1 = 1; 
    __delay_us(50);
    PORTCbits.RC1 = 0; 
    __delay_us(50);
}

void LCD_Cmd(unsigned char cmd) {
    LCD_Send(cmd >> 4, 0);   
    LCD_Send(cmd & 0x0F, 0); 
    __delay_ms(2);
}

void LCD_Char(char data) {
    LCD_Send(data >> 4, 1);   
    LCD_Send(data & 0x0F, 1); 
    __delay_us(50);
}

void LCD_Init() {
    TRISC = 0x00;   
    PORTC = 0x00;   
    __delay_ms(20); 
    
    LCD_Send(0x03, 0); __delay_ms(5);
    LCD_Send(0x03, 0); __delay_us(100);
    LCD_Send(0x03, 0); __delay_us(100);
    LCD_Send(0x02, 0); __delay_ms(2); 
    
    LCD_Cmd(0x28); 
    LCD_Cmd(0x0C); 
    LCD_Cmd(0x06); 
    LCD_Cmd(0x01); 
    __delay_ms(5);
}

void LCD_String(const char *str) {
    while(*str) {
        LCD_Char(*str++); 
    }
}

void LCD_Set_Cursor(unsigned char row, unsigned char col) {
    unsigned char address;
    if(row == 1) address = 0x80 + col - 1;
    else address = 0xC0 + col - 1;
    LCD_Cmd(address);
}

// ==========================================
// THƯ VIỆN ADC
// ==========================================
void ADC_Init() {
    ADCON1 = 0x8E;  
    ADCON0 = 0x81;  
}

unsigned int ADC_Read(unsigned char channel) {
    ADCON0 &= 0xC7;                 // Xóa mask đúng cho CHS
    ADCON0 |= (channel << 3);       
    __delay_ms(2);                  
    ADCON0bits.GO_nDONE = 1;        
    while(ADCON0bits.GO_nDONE);     
    return ((ADRESH << 8) + ADRESL); 
}

// ==========================================
// HÀM CHÍNH
// ==========================================
void main(void) {
    // 1. Khởi tạo ngoại vi
    TRISAbits.TRISA0 = 1;   // ADC Input
    TRISDbits.TRISD0 = 0;   // LED Đỏ Output
    TRISDbits.TRISD1 = 0;   // LED Xanh Output
    TRISDbits.TRISD2 = 0;   // Buzzer Output
    PORTD = 0x00;           
    
    TRISBbits.TRISB0 = 1;   // Nút tăng
    TRISBbits.TRISB1 = 1;   // Nút giảm
    OPTION_REGbits.nRBPU = 1; // Tắt điện trở kéo lên nội (Dùng trở ngoài)

    ADC_Init();             
    LCD_Init();             

    int offset_nhiet_do = 0;       
    int fixed_threshold_x10 = 380; // Ngưỡng 38.0 C
    char lcd_buffer[20];           

    while(1) {
        // --- 2. XỬ LÝ NÚT NHẤN TẠO OFFSET ---
        if (BTN_UP == 0) {              
            __delay_ms(20);         
            if (BTN_UP == 0) {          
                offset_nhiet_do++;  
                while(BTN_UP == 0); 
            }
        }
        
        if (BTN_DOWN == 0) {            
            __delay_ms(20);         
            if (BTN_DOWN == 0) {          
                offset_nhiet_do--;  
                while(BTN_DOWN == 0); 
            }
        }

        // --- 3. ĐỌC VÀ TÍNH TOÁN NHIỆT ĐỘ (FIXED-POINT MATH) ---
        unsigned int adc_value = ADC_Read(0); 
        
        // adc_value * 488 / 100 để lấy nhiệt độ x10 (giữ 1 số thập phân)
        unsigned long temp_raw_x10 = ((unsigned long)adc_value * 488) / 100;
        
        // Cộng thêm Offset (Nhân 10 để cùng hệ số)
        int final_temp_x10 = (int)temp_raw_x10 + (offset_nhiet_do * 10); 

        // Tách phần nguyên và phần thập phân để hiển thị
        int temp_int = final_temp_x10 / 10;
        int temp_frac = final_temp_x10 % 10;
        if (temp_frac < 0) temp_frac = -temp_frac; 

        // --- 4. CẬP NHẬT MÀN HÌNH LCD ---
        sprintf(lcd_buffer, "Temp: %d.%d C    ", temp_int, temp_frac);
        LCD_Set_Cursor(1, 1);       
        LCD_String(lcd_buffer);

        if(final_temp_x10 > fixed_threshold_x10) {
            sprintf(lcd_buffer, "Off:%+d   WARN ", offset_nhiet_do); 
        } else {
            sprintf(lcd_buffer, "Off:%+d   SAFE ", offset_nhiet_do);
        }
        LCD_Set_Cursor(2, 1);       
        LCD_String(lcd_buffer);

        // --- 5. LOGIC ĐIỀU KHIỂN CẢNH BÁO ---
        if(final_temp_x10 > fixed_threshold_x10) {
            LED_WARNING = 1;        
            BUZZER = 1;             
            LED_SAFE = 0;           
        } else {
            LED_WARNING = 0;        
            BUZZER = 0;             
            LED_SAFE = 1;           
        }
        
        __delay_ms(50);  
    }
}