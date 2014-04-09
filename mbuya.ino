/*
Monitoring Unit – prototype installed at Mbuya UNICEF Innovation Center.
22/06/2012, Uganda.
Author: Joao Marcos Barguil
jbarguil@gmail.com
Prototype for one door only. Uses one Teensy 2.0 board.
Modified to also send the time the door was closed.
*/
#include <EEPROM.h>    // For using the EEPROM (o rly?)
#include <MsTimer2.h>  // For time controlling (scheduling periodical tasks based on Timer 2 interruption)
#include <Bounce.h>    // Makes our life easier when working with contact switches


// ———————————————————————
// HARDWARE DEFINITIONS

const int GPRS_pin   = 6;    // the pin to switch on the module (without press on button)
const int btn_send   = 15;    // Sends data immediately
const int btn_reset  = 14;    // Resets data count
const int led_send   = 13;    // Indicates that data is being sent
const int led_reset  = 12;    // Indicates that data has been reset
const int led_sensor = 11;    // Indicates that at least one sensor has been activated (on board LED).
const int latrine_pin = 0;      // Latrine input (Digital)
const int shower_pin  = 1;      // Shower  input (Digital)

Bounce button_send(btn_send, 5);    // 5ms debounce
Bounce button_reset(btn_reset, 5);
Bounce b0(latrine_pin, 5);

//Bounce b1(shower_pin, 5);
Bounce input[] = {b0};

// must be equal to the size of input
const int input_count = 1;


// ———————————————————————
// SOFTWARE CONSTANT DEFINITIONS

// Uncomment one (and only one) of the lines below.
//unsigned const long DATA_SEND_PERIOD = 30;          // Data send period in seconds (t)
//unsigned const long DATA_SEND_PERIOD = 60 * 30;     // Data send period in minutes (60 * t)
unsigned const long DATA_SEND_PERIOD = 3600 * 2;  // Data send period in hours (3600 * t)

unsigned const long UPDATES_PER_DAY = 60*60*24 / DATA_SEND_PERIOD;

unsigned long time_closed[input_count];

const String felix = “+256773174122″;
const String joao  = “+256706830879″;
const String twitter_phone = “+3584573950042″;
const String server_phone = joao;

// Time a door must stay closed to be a valid input (in milliseconds)
const int timeout_min = 10000;
const int timeout_max = 300000;

// This line defines an “Uart” object to access the serial port
HardwareSerial Uart = HardwareSerial();


// ———————————————————————
//     TIME MANAGEMENT
// ———————————————————————
/*
Time counters. These are declared as volatile because they are modified inside the
interrupt procedure and also outside it.
IMPORTANT NOTE: they cannot be initialized with zero. Seriously, DON’T DO IT! It took me
many hours to discover that. :)
*/
volatile int gprs_timer = -1;  // GPRS needs some time to connect to network. -1 means the module hasn’t been turned on.
volatile int tmr_led_reset = 5;
volatile int tmr_send_data = DATA_SEND_PERIOD;


// It’s called every second. Manages stuff related to real-time.
void clock_tick() {
    // TODO: No periodical updates so far. Uncomment here to change.
    // Checks if it’s time to send data
    //  if (tmr_send_data < DATA_SEND_PERIOD) {
    //    tmr_send_data++;
    //  }
    // LED: reset
    if (tmr_led_reset  < 5) {
        digitalWrite(led_reset, HIGH);
        tmr_led_reset++;
    } else if (tmr_led_reset == 5) {
        digitalWrite(led_reset, LOW);
        tmr_led_reset++;
    }
    if (gprs_timer < 30 && gprs_timer >= 0) {
        gprs_timer++;
    }
}

// ———————————————————————
//          GPRS
// ———————————————————————
void turnOnGPRS() {
    Uart.begin(115200);  // the GPRS baud rate
    delay(2000);
    // Switches the module on
    digitalWrite(GPRS_pin,HIGH);
    digitalWrite(13, HIGH);
    delay(2000);
    digitalWrite(13, LOW);
    digitalWrite(GPRS_pin,LOW);
    gprs_timer = 0;
}

void turnOffGPRS() {
    Uart.println(“AT*PSCPOF”);
    delay(3000);
    Uart.end();
    gprs_timer = -1;
}

boolean GPRS_ready() {
    return gprs_timer >= 30;
}

boolean GPRS_on() {
    return gprs_timer > 0;
}

void sendSMS(String number, String msg) {
    Uart.println(“AT+CMGF=1″);  // set the SMS mode to text
    delay(2000);
    // send the SMS number
    Uart.print(“AT+CMGS=\””);
    Uart.print(number);
    Uart.println(“\””);
    delay(1500);
    // SMS body
    Uart.print(msg);
    delay(500);
    Uart.write(char(26));  // end of message command 1A (hex)
    delay(2000);
}

void debugModule() {
    while(Uart.available()) {
        Serial.write(Uart.read());
    }
}

// ———————————————————————
//         EEPROM
/*
Arduino’s EEPROM offers 512 bytes of non-volatile memory. Normal access is for each byte
individually. In this case, our EEPROM is organized to work with integers. Since integers
are two bytes big, these functions offer an interface so we can consider our EEPROM size
as 256 integers.
We are using big-endian organization (most significant byte first): http://en.wikipedia.org/wiki/Big-endian
*/
// ———————————————————————

// Memory addresses of things
const int reading_address = 0;
const int total_address = 10;
const int tweet_id_address = 20;
const int global_address = 22;

// Resets the whole EEPROM. Very dangerous!
void EEPROM_reset() {
    for (int i = 0; i < 512; i++) {
        EEPROM.write(i, 0);
    }
}

void EEPROM_write(int address, int data) {
    EEPROM.write(address+1, data);      // Lower  byte
    EEPROM.write(address, data >> 8);   // Higher byte
}

int EEPROM_read(int address) {
    return (EEPROM.read(address) << 8) | EEPROM.read(address+1);
}

// ———————————————————————
//     DATA MANAGEMENT
// ———————————————————————
/*
The values read are stored in non-volatile memory (EEPROM) in “vectors”.
They are not real vectors, but to make it simpler, we can think of them as vectors.
We have reading[], which are the newest values since last update, and total[],
which is the total for the day.
In our hypothetical vectors, the values are stored like this:
[0] – latrine
[1] – shower
*/

// Returns the values stored in the EEPROM Arrays (read the VARIABLES section for more info)
int get_reading(int i) {
    // reading[]
    return EEPROM_read(reading_address + 2*i);
}

void set_reading(int index, int value) {
    // reading[]
    EEPROM_write(reading_address + 2*index, value);
}

void reset_reading() {
    for (int i = 0; i <= 1; i++) {
        // resets reading[]
        set_reading(i, 0);
    }
}

// —————————————
int get_total(int i) {
    // total[]
    return EEPROM_read(total_address + 2*i);
}

void set_total(int index, int value) {
    // total[]
    EEPROM_write(total_address + 2*index, value);
}

// Updates the total values with the data from reading[]
void update_total() {
    for (int i = 0; i <= 1; i++) {
        set_total(i, get_total(i) + get_reading(i));
    }
}

void reset_total() {
    for (int i = 0; i <= 1; i++) {
        // resets total[] (in the EEPROM)
        set_total(i, 0);
    }
}

// ———————————————————————

// Time of the last reading
unsigned long last_time[input_count];

// Active sensors
int active_count = 0;

// Updates the values in reading[].
int update_values() {
    for (int i = 0; i < input_count; i++) {
        if (input[i].update() ) {
            // The line above is true only if the state of the Bounce changed since last update.
            // IMPORTANT NOTE: SENSORS ARE ACTIVE LOW!
            // (This means that when they are pressed, the signal is low).
            if (input[i].read() == LOW) {
                // Door has been closed
                last_time[i] = millis();
                active_count++;
            } else if (input[i].read() == HIGH) {
                // Door has been opened
                time_closed[i] = millis() – last_time[i];
                // Ignores the reading if the person was inside for a too short time
                if ( (time_closed[i] > timeout_min) && (time_closed[i] < timeout_max) ) {
                    // Someone used a latrine or urinal. Increments the counter
                    set_reading(i, get_reading(i) + 1);
                    // Sends a message every time someone uses the toilet
                    tmr_send_data = DATA_SEND_PERIOD;
                }
                if (active_count)
                    active_count–;
            }
        }
    }
    if (active_count) {
        digitalWrite(led_sensor, HIGH);
    } else {
        digitalWrite(led_sensor, LOW);
    }
}

// ———————————————————————
//      DATA SENDING
// ———————————————————————

String get_state() {
    String ret = “”;
    ret += “Today: “;
    ret += get_reading(0);
    ret += ” \nTotal: “;
    ret += get_total(0);
    ret += ” \nTime closed: “;
    ret += time_closed[0];
    return ret;
}

// Sends relevant data to the server. It is called every DATA_SEND_PERIOD.
void send_data() {
    static long counter = 0;

    // Turns on LED
    digitalWrite(led_send, HIGH);

    // If we just started, we gotta wait for the GPRS module to connect to the network.
    if (!GPRS_ready()) {
        if (!GPRS_on) {
            turnOnGPRS();
        }
        return;
    }

    sendSMS(joao, get_state());
    sendSMS(felix, get_state());

    // If a full day has gone (or we just started), resets daily counters.
    // TODO: not being done
    //  if (counter == 0) {
    //    reset_reading();
    //  }

    // Updates daily counter
    counter = (counter + 1) % UPDATES_PER_DAY;

    // Resets timer
    tmr_send_data = 1;

    // Turns off LED
    digitalWrite(led_send, LOW);
}

// ———————————————————————
//      MAIN PROGRAM
// ———————————————————————
void setup() {
    pinMode(btn_send,  INPUT_PULLUP);
    pinMode(btn_reset, INPUT_PULLUP);
    pinMode(led_send, OUTPUT);
    pinMode(led_reset, OUTPUT);
    pinMode(led_sensor, OUTPUT);
    pinMode(GPRS_pin, OUTPUT);
    pinMode(latrine_pin, INPUT_PULLUP);
    pinMode(shower_pin, INPUT_PULLUP);

    // Sets the time counter
    MsTimer2::set(1000, clock_tick); // Starts Timer2, calling clock_tick() every 1s
    MsTimer2::start();

    turnOnGPRS();

    // Major change #4: updates global[] and resets total[] and reading[] when turning on.
    update_total();
    reset_reading();

    Serial.begin(115200);
    Uart.begin(115200);

    for (int i = 0; i < input_count; i++)
        time_closed[i] = 0;
}

// ———————————————————————
void loop() {
    // Reads control buttons
    // Write the current state on serial port
    if (button_send.update())
        if (button_send.fallingEdge()) {
            digitalWrite(led_send, HIGH);
            Serial.println(get_state());
            digitalWrite(led_send, LOW);
        }

    // Resets our current values
    if (button_reset.update())
        if (button_reset.fallingEdge()) {
            tmr_led_reset = 0;  // Resets the LED counter (so it flashes for a few seconds)
            // Resets reading[]
            reset_reading();
            reset_total();
        }

    // Reads the sensors.
    update_values();

    // If it should send the data, does it.
    if (tmr_send_data >= DATA_SEND_PERIOD) {
        send_data();
    }
}