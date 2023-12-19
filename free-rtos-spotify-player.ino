/**
 * @file free-rtos-spotify-player.ino
 * @author Jordan Fraser, Nina Okubo
 * @date August 18 23, 2023
 * @brief ECE Lab 4 Assignment, Part C
 * @detail University of Washington ECE 474
 * Acknowledgments: LCD was inspired by: https://www.instructables.com/Arduino-Interfacing-With-LCD-Without-Potentiometer/ 
 * RFID was inspired by: https://www.instructables.com/Interfacing-RFID-RC522-With-Arduino-MEGA-a-Simple-/ 
 * 
 * This program continuously flashes an external LED (Task 1), and waits for an RFID card to be scanned,
 * then sends the scanned card's Spotify code to the Serial to play the song/album on Spotify.
 * Also recieves information about the current song playing on Spotify and displays it on an LCD screen.
 */

#include <Arduino_FreeRTOS.h>
#include <LiquidCrystal.h>
#include <queue.h>
#include <SPI.h>
#include <RFID.h>

LiquidCrystal lcd(12, 11, 5, 4, 3, 2);  ///< Instance of the LiquidCrystal library

#define SDA_DIO 9 ///< constant for the pin number of the SDA (SS)
#define RESET_DIO 8 ///< constant for the pin number of the RST (reset)
RFID RC522(SDA_DIO, RESET_DIO); ///< Instance of the RFID library

#define NUM_CARDS 3 ///< constant for the maximum number of cards to read from
#define LED_PIN_1 47 ///< constant for the pin number of the external LED

const String CARD_VALS[] = {"14724720929168", "35346183176", "911343659194"}; ///< constant holding the RFID card values
const char FEARLESS[] = "spotify:album:4hDok0OAJd57SGIT8xuWJH"; ///< constant holding the "Fearless" Spotify code
const char FOLKLORE[] = "spotify:album:2fenSS68JI1h4Fo296JfGr"; ///< constant holding the "Folklore" Spotify code
const char VAMPIRE[] = "spotify:track:3k79jB4aGmMDUQzEwa46Rz"; ///< constant holding the "Vampire" Spotify code
const String SPOTIFY_ARTIST_VAL[] = {FOLKLORE, VAMPIRE, FEARLESS}; ///< constant holding the Spotify codes with the associated index of the CARD_VALS

// Declare a queue
QueueHandle_t displayQueue; ///< Queue with the maximum length of the 2 strings on the LCD screen

/**
 * @author Jordan Fraser, Nina Okubo
 * @brief Setup the LCD screen
 *
 * Sets up the LCD screen's pins, and sets the initial text on the screen
 * Acknowledgments: Inspired by https://www.instructables.com/Arduino-Interfacing-With-LCD-Without-Potentiometer/
 * @see setup()
 */
void setupLCD() {
  analogWrite(6,75);
  lcd.begin(16, 2);

  lcd.setCursor(0, 0);
  lcd.print("Song: ");
  lcd.setCursor(0, 1);
  lcd.print("Artist: ");
  // setup the initial display length
  int maxDisplayLength = 6;
  xQueueSend(displayQueue, &maxDisplayLength, portMAX_DELAY);
}


/**
 * @author Jordan Fraser, Nina Okubo
 * @brief Setup the RFID scanner
 *
 * Sets up the SPI interface and RFID reader
 * Acknowledgments: Inspired by https://www.instructables.com/Interfacing-RFID-RC522-With-Arduino-MEGA-a-Simple-/
 * @see setup()
 */
void setupRFID() {
  SPI.begin();  // Enable SPI
  RC522.init(); // Initialize RFID reader
}

/**
 * @author Jordan Fraser, Nina Okubo
 * @brief Setup the Serial Communication, External LED, Tasks, LCD, and RFID scanner 
 *
 * Setup the Serial Communication to 9600, External LED, Tasks, LCD, and RFID scanner 
 * @see setupLCD(), setupRFID()
 */
void setup() {
  Serial.begin(9600);
  pinMode(LED_PIN_1, OUTPUT); // Set LED_PIN_1 as an output
  xTaskCreate(vTaskBlink1, "Blink1", 1000, NULL, 1, NULL);
  xTaskCreate(updateText, "updateText", 1000, NULL, 1, NULL);
  xTaskCreate(scrollLCDText, "scrollLCDText", 1000, NULL, 1, NULL);
  xTaskCreate(detectCard, "detectRFIDCard", 1000, NULL, 1, NULL);
  // create a queue capable of holding 10 int values.
  displayQueue = xQueueCreate(10, sizeof(int));
  setupLCD();
  setupRFID();
} 

// Doesn't do anything because FREE RTOS scheduler takes care of everything, but we still need it to avoid errors
void loop() { }

/*
 * @author Jordan Fraser, Nina Okubo
 * @brief Task 1: Flash the LED connected to LED_PIN_1
 * @param pvParameters - void pointer to the pvParameters
 *
 * Repeatedly turns the EXTERNAL_LED on for 100ms, then off for 200ms
 * using pinMode and digitalWrite
 */
void vTaskBlink1(void *pvParameters) {
  for (;;) {
    digitalWrite(LED_PIN_1, HIGH); // Turn LED_PIN_1 on
    vTaskDelay(100 / portTICK_PERIOD_MS);  // Delay for 100ms
    digitalWrite(LED_PIN_1, LOW); // Turn LED_PIN_1 off
    vTaskDelay(200 / portTICK_PERIOD_MS);  // Delay for 200ms
  }
}

/*
 * @author Jordan Fraser, Nina Okubo
 * @brief Scrolls through the text on the LCD
 * @param pvParameters - void pointer to the pvParameters
 *
 * Scrolls through the LCD text being displayed. Shifts to the left once every 600 ms
 */
void scrollLCDText(void *pvParameters) {
  int maxDisplayLength;
  for (;;) {
    if (xQueuePeek(displayQueue, &maxDisplayLength, portMAX_DELAY)==pdTRUE){
      static int positionCounter = 0;
      static unsigned long prevShifted = millis();
      if ((positionCounter < maxDisplayLength) && ((millis() - prevShifted) >= 600)) {
        lcd.scrollDisplayLeft();
        positionCounter++;
        prevShifted = millis();
      } 
      if (positionCounter >= maxDisplayLength) {
        positionCounter = 0;
      }
    }
  }
}

/*
 * @author Jordan Fraser, Nina Okubo
 * @brief Reads from the Serial, and updates the text on the LCD based on the string read
 * @param pvParameters - void pointer to the pvParameters
 *
 * Waits for string to be sent from the Serial. If the string starts with "By", then it updates
 * the artist name on the LCD. If the string starts with "Song", then it updates the song title
 * on the LCD.
 * Acknowledgements: Serial communication inspired by the following github repo: 
 * https://github.com/xanthium-enterprises/CrossPlatform-Serial-Port-Programming-using-Python-and-PySerial/blob/master/
 */
void updateText(void *pvParameters) {
  static String currentArtist = "By: ";
  static String currentSong = "Song: ";
  for (;;) {
    if (Serial.available()) {
      String sentText = Serial.readString();
      if (sentText.substring(0, 3) == "By:") {
        if (currentArtist != sentText) {
          // update artist
          currentArtist = sentText;
          lcd.clear();
          lcd.setCursor(1, 0); // (0, 0)
          lcd.print(currentSong);
          lcd.setCursor(1, 1); // (0, 1)
          lcd.print(currentArtist);
        }
      } else if (sentText.substring(0, 5) == "Song:") {
        if (currentSong != sentText) {
          // update song
          currentSong = sentText;
          lcd.clear();
          lcd.setCursor(1, 0);
          lcd.print(currentSong);
          lcd.setCursor(1, 1);
          lcd.print(currentArtist);
        }
      }
      int temp;
      xQueueReceive(displayQueue, &temp, portMAX_DELAY); // remove from queue
      int maxDisplayLength = max(currentArtist.length(), currentSong.length());
      xQueueSend(displayQueue, &maxDisplayLength, portMAX_DELAY); // add to queue
    } //endof if 
  }
}

/**
 * @author Jordan Fraser, Nina Okubo
 * @brief Waits for the RFID scanner to scan a card
 *
 * Constantly checks if a card has been detected.
 * If a card is detected, gets the serial number value and checks that the card is different from the previous scan
 * If the card is different, then find the associated Spotify code and send the code to Serial.
 * If the card is not recognized then nothing happens
 * Acknowledgments: Inspired by https://www.instructables.com/Interfacing-RFID-RC522-With-Arduino-MEGA-a-Simple-/
 */
void detectCard(void *pvParameters) {
  for (;;) {
    static String value = "";
    /* Has a card been detected? */
    if (RC522.isCard()) {
      /* If so then get its serial number */
      RC522.readCardSerial();
      String pastCardSerial = value;
      value = "";
      for(int i = 0; i < 5; i++) {
        value += RC522.serNum[i];
      }
      bool printed = false;
      if (!pastCardSerial.equals(value)) { // if the card scanned is different from the prev scanned card
        for (int card_index = 0; card_index < NUM_CARDS; card_index++) {
          if (CARD_VALS[card_index] == value) { // find and send the spotify value associated with the card
            Serial.println(SPOTIFY_ARTIST_VAL[card_index]);
            printed = true;
            break;
          }
        }
        if (!printed) { // if the value was not found in the valid card values, then reset to previous valid card value
          value = pastCardSerial;
        }
      }
    }
  }
}