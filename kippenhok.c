// GEBRUIK:
// Cijfers: tijd instellen
//  - Formaat: DDMMYYYYhhmm
//  - Voorbeeld: 060120221734 = 6 januari 2022 17u34
//  - Lichtjes:
//    * Oranje: cijfer geregistreerd
//    * Groen: tijd correct ingesteld
//    * Rood: Fout in input => begin opnieuw
// Hekje: begin opnieuw met tijd instellen
// Sterretje: Voltmeter
//  - Lichtjes:
//    * Groen: > 3.75V
//    * Oranje: > 3.25V < 3.75V
//    * Rood: < 3.25V
// Knop: Manuele poort bediening
//  - Doorloopt cyclus van 4 staten:
//    1. Naar boven
//    2. Stop boven
//    3. Naar beneden
//    4. Stop beneden
// Magneet sensoren: Zet motor in Stop Boven of Beneden

 

#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/rtc.h"
#include "hardware/rosc.h"
#include "hardware/structs/scb.h"
#include "hardware/clocks.h"
#include "pico/sleep.h"
#include "Dusk2Dawn.h"
#include <stdbool.h>
#include "pico/sync.h"
#include "hardware/adc.h"

const uint LED_PIN = PICO_DEFAULT_LED_PIN;
const uint MOTOR_SLEEP = 18;
const uint MOTOR_AIN1= 17;
const uint MOTOR_AIN2 = 16;
const uint VOLT_SENSOR = 28;
const uint VOLT_SENSOR_ADC = 2; // Fixed for pin 28
const uint BUTTON = 19;
const uint SENSOR_TOP = 21;
const uint SENSOR_BOTTOM= 20;
const uint LED_RED = 13;
const uint LED_ORANGE = 12;
const uint LED_GREEN = 11;
struct Dusk2Dawn d2d;

const uint KEYPAD[4][3] = {
        {1,2,3},
        {4,5,6},
        {7,8,9},
        {10,0,11}
    };

const uint KEYPAD_ROW_PINS[4] = {1,2,3,4};
const uint KEYPAD_COL_PINS[3] = {5,6,7};

uint keypad_last_key;
absolute_time_t keypad_last_event;
uint keypad_inpud_step = 0;
datetime_t keypad_input_datetime = {
        .year  = 0,
        .month = 0,
        .day   = 0,
        .hour  = 0,
        .min   = 0,
        .sec   = 0
};

// states:
//   0 top
//   1 moving down
//   2 bottom
//   3 moving up
uint state = 0;

// wake_up_reason
//   0 sunrise
//   1 sunset
//   3 keypad trigger
//   6 back to sleep
uint wake_up_reason = 6;

absolute_time_t button_last_event;

bool use_daylight_savings;

void led_init();
void led_blink_number(int number);
void led_blink(uint led);


void motor_init();
void motor_up();
void motor_down();
void motor_stop();

void button_init();
void sensor_init();
void button_trigger();
void sensor_top_trigger();
void sensor_bottom_trigger();

void gpio_callback(uint gpio, uint32_t events);

void clock_init();
void clock_set_datetime(datetime_t *t);
void sunrise_callback();
void sunset_callback();
int get_days_per_month(int month, int year); 
bool is_daylight_savings(int year, int month, int day);

void volt_sensor_init();
void volt_sensor_measure();

void keypad_init();
void keypad_cancel_input();
void keypad_trigger(uint row_pin);
void keypad_input(uint key);

datetime_t wake_up_datetime;

//   0 sunrise
//   1 sunset
uint wake_up_reason_clock = 0;


void wake_up_callback(){
    wake_up_reason = wake_up_reason_clock;
}

void set_wake_up(uint min, uint wake_up_reason){
    wake_up_datetime.year  = -1;
    wake_up_datetime.month = -1;
    wake_up_datetime.day   = -1; 
    wake_up_datetime.hour  = min/60;
    wake_up_datetime.min   = min%60;
    wake_up_datetime.sec   = -1;
    wake_up_datetime.dotw  = -1;
    wake_up_reason_clock = wake_up_reason;
}

void recover_from_sleep(uint scb_orig, uint clock0_orig, uint clock1_orig){

    //Re-enable ring Oscillator control
    rosc_write(&rosc_hw->ctrl, ROSC_CTRL_ENABLE_BITS);

    //reset procs back to default
    scb_hw->scr = scb_orig;
    clocks_hw->sleep_en0 = clock0_orig;
    clocks_hw->sleep_en1 = clock1_orig;

    //reset clocks
    clocks_init();
    stdio_init_all();

    return;
}

void goto_sleep(){
    //save values for later
    uint scb_orig = scb_hw->scr;
    uint clock0_orig = clocks_hw->sleep_en0;
    uint clock1_orig = clocks_hw->sleep_en1;

    sleep_run_from_xosc(); // choose wich oscilator to keep active in sleep mode and disable all others
    sleep_goto_sleep_until(&wake_up_datetime, &wake_up_callback);

    //reset processor and clocks back to defaults
    recover_from_sleep(scb_orig, clock0_orig, clock1_orig);
}


int main() {
    led_init();
    led_blink(LED_PIN);
    led_blink(LED_GREEN);
    led_blink(LED_ORANGE);
    led_blink(LED_RED);


    motor_init();

    motor_up();
    sleep_ms(500);
    motor_stop();

    button_init();
    sensor_init();
    volt_sensor_init();

    clock_init();

    keypad_init();

    goto_sleep();

    while(1){
        switch(wake_up_reason){
            case 0: sunrise_callback();
                    break;
            case 1: sunset_callback();
                    break;
            case 3: if(keypad_last_key == 10){
                        volt_sensor_measure();
                    } else if( keypad_last_key == 11){
                        keypad_cancel_input();
                    } else if (keypad_last_key != 12){
                        keypad_input(keypad_last_key);
                    }
                    break;
            case 6: break; // do nothing go back to sleep mode
        }

        wake_up_reason = 6; // default next wake up to do nothing
        goto_sleep();
        
    }
    return 0;
}

void gpio_callback(uint gpio, uint32_t events){
    if(gpio == BUTTON ){
        button_trigger();
    } else if(gpio == SENSOR_TOP){
        sensor_top_trigger();
    } else if(gpio == SENSOR_BOTTOM){
        sensor_bottom_trigger();
    } 
    else if(gpio == 1 || gpio == 2 || gpio == 3 || gpio == 4){
        if(absolute_time_diff_us(keypad_last_event, get_absolute_time()) > 500000){
            keypad_last_event = get_absolute_time();
            keypad_trigger(gpio);
        }
    }

}


// LED FUNCTIONS

void led_blink_number(int number){
    int i = 0;

    while (i < 12) {
        if (number & 0x01) {
            gpio_put(LED_PIN, 1);
            sleep_ms(2000);
            gpio_put(LED_PIN, 0);
            sleep_ms(1000);
        } else {
            gpio_put(LED_PIN, 1);
            sleep_ms(500);
            gpio_put(LED_PIN, 0);
            sleep_ms(500);
            gpio_put(LED_PIN, 1);
            sleep_ms(500);
            gpio_put(LED_PIN, 0);
            sleep_ms(1500);
        }

        i++;
        number = number >> 1;
    }
}

void led_blink(uint led){
    gpio_put(led, 1);
    sleep_ms(500);
    gpio_put(led, 0);
}


void led_init(){
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_init(LED_RED);
    gpio_set_dir(LED_RED, GPIO_OUT);
    gpio_init(LED_GREEN);
    gpio_set_dir(LED_GREEN, GPIO_OUT);
    gpio_init(LED_ORANGE);
    gpio_set_dir(LED_ORANGE, GPIO_OUT);
}

// VOLT SENSOR FUNCTIONS

void volt_sensor_init(){
    adc_init();
    // Make sure GPIO is high-impedance, no pullups etc
    adc_gpio_init(VOLT_SENSOR);
    adc_select_input(VOLT_SENSOR_ADC);
}

void volt_sensor_measure(){
    // 12-bit conversion, assume max value == ADC_VREF == 3.3 V
    const float conversion_factor = 3.3f / (1 << 12);
    uint16_t result = adc_read();
    // The voltage is divided by two by the divider
    float voltage = result * conversion_factor * 2;

    if(voltage > 3.75){
        led_blink(LED_GREEN);
    } else if(voltage > 3.25){
        led_blink(LED_ORANGE);
    } else {
        led_blink(LED_RED);
    }

}

// MOTOR FUNCTIONS

void motor_init(){
    gpio_init(MOTOR_SLEEP);
    gpio_set_dir(MOTOR_SLEEP, GPIO_OUT);
    gpio_init(MOTOR_AIN1);
    gpio_set_dir(MOTOR_AIN1, GPIO_OUT);
    gpio_init(MOTOR_AIN2);
    gpio_set_dir(MOTOR_AIN2, GPIO_OUT);
}

void motor_up(){
    gpio_put(LED_GREEN, 1);
    gpio_put(MOTOR_SLEEP, 1);
    gpio_put(MOTOR_AIN1, 1);
    gpio_put(MOTOR_AIN2, 0);
    gpio_set_irq_enabled_with_callback(SENSOR_TOP, GPIO_IRQ_LEVEL_HIGH, true, &gpio_callback);
}

void motor_down(){
    gpio_put(LED_RED, 1);
    gpio_put(MOTOR_SLEEP, 1);
    gpio_put(MOTOR_AIN1, 0);
    gpio_put(MOTOR_AIN2, 1);
    gpio_set_irq_enabled_with_callback(SENSOR_BOTTOM, GPIO_IRQ_LEVEL_HIGH, true, &gpio_callback);
}

void motor_stop(){
    gpio_put(LED_RED, 0);
    gpio_put(LED_GREEN, 0);
    gpio_put(MOTOR_SLEEP, 0);
    gpio_put(MOTOR_AIN1, 0);
    gpio_put(MOTOR_AIN2, 0);
    gpio_set_irq_enabled(SENSOR_BOTTOM, GPIO_IRQ_LEVEL_HIGH, false);
    gpio_set_irq_enabled(SENSOR_TOP, GPIO_IRQ_LEVEL_HIGH, false); 
}

// BUTTON AND SENSOR FUNCTIONS

void button_init(){
    gpio_init(BUTTON);
    gpio_set_dir(BUTTON, GPIO_IN);
    gpio_pull_down(BUTTON);  
    gpio_set_irq_enabled_with_callback(BUTTON, GPIO_IRQ_LEVEL_HIGH, true, &gpio_callback);
    button_last_event = get_absolute_time();
}

void sensor_init(){
    gpio_init(SENSOR_TOP);
    gpio_set_dir(SENSOR_TOP, GPIO_IN);
    gpio_pull_down(SENSOR_TOP);  
    gpio_set_irq_enabled_with_callback(SENSOR_TOP, GPIO_IRQ_LEVEL_HIGH, true, &gpio_callback);
    gpio_init(SENSOR_BOTTOM);
    gpio_set_dir(SENSOR_BOTTOM, GPIO_IN);
    gpio_pull_down(SENSOR_BOTTOM);  
    gpio_set_irq_enabled_with_callback(SENSOR_BOTTOM, GPIO_IRQ_LEVEL_HIGH, true, &gpio_callback);
}

void button_trigger(){
    if(absolute_time_diff_us(button_last_event, get_absolute_time()) > 500000){
        switch(state){
            case 0: motor_down();
                    break;
            case 1: motor_stop();
                    break;
            case 2: motor_up();
                    break;
            case 3: motor_stop();
                    break;
            default: motor_stop();
        }
        state = (state + 1) % 4;
        button_last_event = get_absolute_time();
    }
}

void sensor_top_trigger(){
    if(state == 3){
        motor_stop();
        state = 0;
    }
}

void sensor_bottom_trigger(){
    if(state == 1){
        motor_stop();
        state = 2;
    }
}

// CLOCK FUNCTIONS

void clock_init(){

    datetime_t t = {
            .year  = 2022,
            .month = 1,
            .day   = 1,
            .hour  = 12,
            .min   = 15,
            .sec   = 0
    };

    d2d.latitude = 51.022437;
    d2d.longitude = 3.714940;
    d2d.timezone = 1;
 
    // Start the RTC
    rtc_init();
    clock_set_datetime(&t);
}

void clock_set_datetime(datetime_t *t){
    rtc_set_datetime(t);
    
    use_daylight_savings = is_daylight_savings(t->year, t->month, t->day);

    uint current_minutes = t->hour * 60 + t->min;
    uint sunset_minutes = sunset(d2d, t->year, t->month, t->day, use_daylight_savings);
    uint sunrise_minutes = sunrise(d2d, t->year, t->month, t->day, use_daylight_savings);


    // Sunrise is never earlier then 7am
    if(sunrise_minutes < 420){
        sunrise_minutes = 420;
    }

    if(current_minutes < sunrise_minutes || current_minutes > sunset_minutes + 60){
        // it is night, run sunset to make sure the gate is closed and the morning alarm is set
        sunset_callback();
    } else {
        // it is light, run sunrise to make sure the gate is open and evening alarm is set
        sunrise_callback();
    }

}

void sunrise_callback() {
    int minutes = 0;
    datetime_t now;

    state = 3;
    motor_up();

    // safety break
    sleep_ms(7000);
    motor_stop();
    state = 0;

    rtc_get_datetime(&now);
    minutes = sunset(d2d, now.year, now.month, now.day, use_daylight_savings); 

    // close door 60 minutes after sunset
    minutes = minutes + 60; 

    set_wake_up(minutes, 1);
}

void sunset_callback() {
    int minutes = 0;
    datetime_t now;

    state = 1;
    motor_down();

    // safety break
    sleep_ms(7000);
    motor_stop();
    state = 2;

    rtc_get_datetime(&now);
    minutes = sunrise(d2d, now.year, now.month, now.day, use_daylight_savings); 


    // Sunrise is never earlier then 7am
    if(minutes < 420){
        minutes = 420;
    }

    set_wake_up(minutes, 0);
}

int get_days_per_month(int month, int year){                                                                                                                                                                                             
    int days_per_month = 31;                                                                                                                                                                                                             
    if (month == 4 || month == 6 || month == 9 || month == 11) {                                                                                                                                                                         
        days_per_month = 30;                                                                                                                                                                                                             
    } else if (month == 2) {                                                                                                                                                                                                             
        days_per_month = 28;                                                                                                                                                                                                             
        if (year % 4 == 0) {                                                                                                                                                                                                             
            days_per_month = 29;                                                                                                                                                                                                         
            if (year % 100 == 0) {                                                                                                                                                                                                       
                days_per_month = 28;
                if (year % 400 == 0) {
                  days_per_month = 29;
                }
            }
        }                                                                                                                                                                                                                                
    }
    return days_per_month;
}

bool is_daylight_savings(int year, int month, int day){
    // TODO IMPROVE!!!!
    return (month == 3 && day > 28) || (month > 3 && month < 11);
}

// KEYPAD FUNCTIONS

void keypad_init(){
    uint i;

    for(i = 0; i< 4; i++){
        gpio_init(KEYPAD_ROW_PINS[i]);
        gpio_set_dir(KEYPAD_ROW_PINS[i], GPIO_IN);
        gpio_pull_down(KEYPAD_ROW_PINS[i]);  
        gpio_set_irq_enabled_with_callback(KEYPAD_ROW_PINS[i], GPIO_IRQ_LEVEL_HIGH, true, &gpio_callback);
    }

    for(i = 0; i< 3; i++){
        gpio_init(KEYPAD_COL_PINS[i]);
        gpio_set_dir(KEYPAD_COL_PINS[i], GPIO_OUT);
        gpio_put(KEYPAD_COL_PINS[i], 1);
    }

    keypad_last_event = get_absolute_time();
    keypad_last_key = 12;
}

void keypad_cancel_input(){
    keypad_inpud_step = 0;
    keypad_input_datetime.year  = 0;
    keypad_input_datetime.month = 0;
    keypad_input_datetime.day   = 0; 
    keypad_input_datetime.hour  = 0;
    keypad_input_datetime.min   = 0;
    keypad_input_datetime.sec   = 0;

    led_blink(LED_RED);
}

void keypad_trigger(uint row_pin){

    const uint *row = KEYPAD[row_pin - 1];
    uint key = 12;

    for(int i = 0; i< 3 && key == 12; i++){
        gpio_put(KEYPAD_COL_PINS[i], 0);
        absolute_time_t n = get_absolute_time();
        while(n + 20000 > get_absolute_time() );
        if(gpio_get(row_pin) == 0){
            key = row[i];
        }
        gpio_put(KEYPAD_COL_PINS[i], 1);
    }

    absolute_time_t n = get_absolute_time();
    while(n + 20000 > get_absolute_time() );

    if(gpio_get(row_pin) == 0){
        return; // Key is no longer pressed, so result is uncertain
    }

    keypad_last_key = key;
    wake_up_reason = 3;
    return;

}


void keypad_input(uint key){
    switch(keypad_inpud_step){
        case 0: keypad_input_datetime.day = key * 10;
                break;
        case 1: keypad_input_datetime.day += key;
                break;
        case 2: keypad_input_datetime.month = key * 10;
                break;
        case 3: keypad_input_datetime.month += key;
                break;
        case 4: keypad_input_datetime.year = key * 1000;
                break;
        case 5: keypad_input_datetime.year += key * 100;
                break;
        case 6: keypad_input_datetime.year += key * 10;
                break;
        case 7: keypad_input_datetime.year += key;
                break;
        case 8: keypad_input_datetime.hour = key * 10;
                break;
        case 9: keypad_input_datetime.hour += key;
                break;
        case 10: keypad_input_datetime.min = key * 10;
                break;
        case 11: keypad_input_datetime.min += key;
                break;
        default: keypad_cancel_input();
                 return;
    }

    // when month is complet, check it
    if(keypad_inpud_step == 3 && keypad_input_datetime.month > 12){
        keypad_cancel_input();
        return;
    }

    // when date is complete, check number of days in month
    else if(keypad_inpud_step == 7 && keypad_input_datetime.day > get_days_per_month(keypad_input_datetime.month, keypad_input_datetime.year)){
        keypad_cancel_input();
        return;
    }

    // when hour is complete check value
    else if(keypad_inpud_step == 9 && keypad_input_datetime.hour >= 24){
        keypad_cancel_input();
        return;
    }

    // when minutes are complete check value
    else if(keypad_inpud_step == 11 && keypad_input_datetime.min >= 60){
        keypad_cancel_input();
        return;
    }


    else if(keypad_inpud_step == 11){
        led_blink(LED_GREEN);
        clock_set_datetime(&keypad_input_datetime); 
        keypad_inpud_step = 0;
        keypad_input_datetime.year  = 0;
        keypad_input_datetime.month = 0;
        keypad_input_datetime.day   = 0; 
        keypad_input_datetime.hour  = 0;
        keypad_input_datetime.min   = 0;
        keypad_input_datetime.sec   = 0;
        return;
    }

    else{
        keypad_inpud_step += 1;
        led_blink(LED_ORANGE);
        return;
    }

}