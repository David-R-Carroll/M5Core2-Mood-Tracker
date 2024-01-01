

//
//
// ***********************************************************
// *  M5Core2 Mood Logger (C) David R. Carroll 2023          *
// ***********************************************************
// *  This program is for tracking your mood.                *
// *  Each day you can select from five sad to happy icons.  *
// *  You can use a calendar to see mood trends.             *
// *  This program shows how to use                          *
// *  flash memory save/recall, touch buttons,               *
// *  the real time clock and sprite graphics.               *
// ***********************************************************


// Includes.

#include <M5Core2.h>
#include <SPI.h>
#include <SPIFFS.h>


#include "Mood_Icons100.h"
#include "Mood_Icons50.h"
#include "Mood_Icons45x40.h"

#include "Calendar_Icon.h"
#include "Power_Icon.h"
#include "Free_Fonts.h"

RTC_DateTypeDef RTCDate;
RTC_TimeTypeDef RTCTime;

TFT_eSprite Sprite_1 = TFT_eSprite(&M5.Lcd);

// Variables.

#define moodFilepath "/Mood"


int moodIconInt[10];  // Holds the four home page icon values.
bool summaryMode;     // True if calendar is being displayed.
int summaryYear;      // Used to hold year and month when calculating current calendar.
int summaryMonth;

String moodHistory;       // String stored in flash memory containing mood values and dates.
int moodHistoryLen;       // Length of moodHistory.
String moodHistoryDates;  // moodHistoryDates minus today's mood.
int moodHistoryDatesLen;  // Length of moodHistoryDates.

String searchMoodHistoryDates; // moodHistory Dates plus current mood/date.

char chBuffer[128]; // Used with string manipulations.
String temp1;  
String temp2;

// Month lengths and names.
int monthLength[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
String monthName[] = { "", "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };

File file;  // Used by SPIFFS

// See https://github.com/m5stack/M5Core2/blob/master/src/utility/M5Button.h
// For an explanation on how M5 touch buttons work.
ButtonColors off_clrs = { NODRAW, NODRAW, NODRAW };  // Defines what color touch buttons should be.
Button nextButton(0, 0, 0, 0, false, "", off_clrs);             // Changes current mood.
Button summaryButton(0, 0, 0, 0, false, "", off_clrs);          // Enters calendar mode.
Button previousSummaryButton(0, 0, 0, 0, false, "", off_clrs);  // Goes to previous month.
Button nextSummaryButton(0, 0, 0, 0, false, "", off_clrs);      // Goes to next month.
Button powerButton(0, 0, 0, 0, false, "", off_clrs);            // Saves current mood and powers off M5Core2.

// Standard file get routine.
String fileGet(String sFile) {
  String fileText;
  char temp;
  file = SPIFFS.open("/" + sFile + ".txt");
  if (!file) {
    Serial.println("Failed to open " + sFile + "for reading");
    return "--";
  } else {
    fileText = "";
    while (file.available()) {
      temp = (char)file.read();
      fileText.concat(String(temp));
    }
  }
  file.close();
  return fileText;
}

// Standard file save routine.
void fileSave(String sFile, String fileText) {
  file = SPIFFS.open("/" + sFile + ".txt", FILE_WRITE);
  file.print(fileText);
  file.close();
}

void PushUp(int g_From) {  // Animate up to the next icon.
  int g_To;
  int wipe_y;
  int x;
  int y;
  int xOffset = 197;
  int yOffset = 30;
  int32_t moodIconPixel;

  g_To = g_From + 1;
  if (g_To > 5) {
    g_To = 0;
  }

  for (wipe_y = 0; wipe_y < 108; wipe_y = wipe_y + 16) {
    for (y = 0; y < 108 - wipe_y; y++) {
      for (x = 0; x < 108; x++) {
        moodIconPixel = Mood_Icons100[g_From][x + (y + wipe_y) * 108];
        Sprite_1.drawPixel(xOffset + x, yOffset + y, moodIconPixel);
      }
    }

    for (y = 0; y < wipe_y; y++) {
      for (x = 0; x < 108; x++) {
        moodIconPixel = Mood_Icons100[g_To][x + y * 108];
        Sprite_1.drawPixel(xOffset + x, yOffset + y + 108 - wipe_y, moodIconPixel);
      }
    }

    Sprite_1.pushSprite(0, 0);
  }
}

// Calculate the start day for a given month by adding month lengths starting with Sunday January 1, 2023.
int getWeekDay(int currentYear, int currentMonth) {
  int weekDay;
  int tempMonth = 1;
  int tempYear = 2023;
  int totalDays = 0;

  // Count the number of days from Sunday Jan1, 2023 to currentMonth/CurrentYear.
  while (!(tempMonth == currentMonth && tempYear == currentYear)) {
    totalDays = totalDays + monthLength[tempMonth];
    tempMonth++;

    if (tempMonth > 12) {
      tempMonth = 1;
      tempYear++;
    }

    if ((tempYear % 4) == 0 && tempMonth == 3) {  // Leap year.
      totalDays++;
    }
  }
  // divide totalDays by 7 and add 1 to the remainder.
  weekDay = (totalDays % 7) + 1;
  
  return weekDay;
}


// Draw the calendar for the selected month and year.
void DrawCalendar(int currentMonth, int currentYear) {

  int x;                      //General for next loop counters
  int y;
  int day = 1;                // Used to print the days of the month.
  bool blankCalendar = true;  // If this is true no history exists for this month, so return to the home page.

  int monthStartDay;
  int tempYear;
  int tempMonth;
  int tempDate;
  int tempIcon;

  // Set the target area for the touch buttons.
  nextButton.set(0, 0, 0, 0); // Disable these buttons by making the target size zero by zero.
  summaryButton.set(0, 0, 0, 0);
  powerButton.set(0, 0, 0, 0);
  previousSummaryButton.set(161, 0, 160, 240); // Set the target to be the left side of the screen
  nextSummaryButton.set(0, 0, 160, 240);       // Set the target to be the right side.

  // Draw the blue background and setup the font size and color.
  Sprite_1.fillSprite(BLUE);
  Sprite_1.setFreeFont(FSSB9);
  Sprite_1.setTextColor(TFT_BLACK);  // Black

  // Find out what day the month starts with.
  monthStartDay = getWeekDay(currentYear,currentMonth); 

  // Print the three letter month name.
  // Print at the top left if the start day is greater than Tuesday.
  // Else print at the bottom right. 
  Sprite_1.setFreeFont(FSSB18);
  Sprite_1.setTextColor(TFT_WHITE);  // Black
  if (monthStartDay > 3) {
    Sprite_1.drawString(String(monthName[currentMonth]), 10, 10, GFXFF);
  } else {
    Sprite_1.drawString(String(monthName[currentMonth]), 240, 205 - 40, GFXFF);
  }

  // Set up the font for the day's number.
  Sprite_1.setFreeFont(FSSB9);
  Sprite_1.setTextColor(TFT_BLACK);  // Black

  // Draw the first row of the calendar.
  for (x = monthStartDay - 1; x <= 6; x++) {
    // look up the mood icon for the current date.
    tempIcon = findMood(currentYear, currentMonth, day);

    if (tempIcon != 0) {
      blankCalendar = false;
    }
    // Draw the mood icon, the date's box and number.
    Sprite_1.pushImage(x * 45, 0, 45, 40, Mood_Icons45x40[tempIcon]);  //moodIconInt[3]
    Sprite_1.drawRect(x * 45, 0, 45, 40, BLACK);
    Sprite_1.drawString(String(day), x * 45, 0, GFXFF);
    day++;
  }

  // Draw the rest of the calendar
  for (y = 0; y <= 5; y++) {
    for (x = 0; x <= 6; x++) {
      // If the drawn date matches the current date, then use the current icon.
      if (currentYear == RTCDate.Year && currentMonth == RTCDate.Month && day == RTCDate.Date) {
        tempIcon = moodIconInt[0];
      } else {
        tempIcon = findMood(currentYear, currentMonth, day);
      }
      tempDate++;

      if (tempIcon != 0) {
        blankCalendar = false;
      }

      Sprite_1.pushImage(x * 45, 40 + y * 40, 45, 40, Mood_Icons45x40[tempIcon]);
      Sprite_1.drawRect(x * 45, 40 + y * 40, 45, 40, BLACK);
      Sprite_1.drawString(String(day), x * 45, 40 + y * 40, GFXFF);
      day++;

      if (day > monthLength[currentMonth]) {
        // If there are no icons drawn in this month, then exit summary mode and go back to the home page.
        if (blankCalendar) {
          summaryMode = false;
          DrawIcons();
        } else {
          // Send the drawn calendar to the LCD screen.
          Sprite_1.pushSprite(0, 0);
        }
        return;
      }
    }
  }
}

// Draw the home page.
void DrawIcons() {

  // If the battery is running low, use a red background.
  if (M5.Axp.GetWarningLevel() == 0) {
    Sprite_1.fillSprite(BLUE);
    Sprite_1.setTextColor(WHITE, BLUE);
  } else {
    Sprite_1.fillSprite(RED);
    Sprite_1.setTextColor(WHITE, RED);
  }

  // Draw the three smaller boxes and the previous 3 days of mood icons.
  Sprite_1.fillRect(15 - 4, 57 - 4, 54 + 8, 54 + 8, BLACK);
  Sprite_1.pushImage(15, 57, 54, 54, Mood_Icons50[moodIconInt[3]]);

  Sprite_1.fillRect(20 + 54 * 1 - 4, 57 - 4, 54 + 8, 54 + 8, BLACK);
  Sprite_1.pushImage(20 + 54 * 1, 57, 54, 54, Mood_Icons50[moodIconInt[2]]);

  Sprite_1.fillRect(25 + 54 * 2 - 4, 57 - 4, 54 + 8, 54 + 8, BLACK);
  Sprite_1.pushImage(25 + 54 * 2, 57, 54, 54, Mood_Icons50[moodIconInt[1]]);

  // Draw the current day's icon.
  nextButton.set(191, 24, 120, 120);
  Sprite_1.fillRect(191,24, 120, 120, BLACK);
  Sprite_1.pushImage(197, 30, 108, 108, Mood_Icons100[moodIconInt[0]]);

  // Draw the calendar icon, and set the summaryButton touch area to surround it.
  summaryButton.set(10, 170, 70, 64);
  Sprite_1.pushImage(10, 170, 70, 64, Calendar_Icon[0]);

  // Draw the power icon, and set the powerButton touch area to surround it.
  powerButton.set(245, 173, 60, 60);
  Sprite_1.pushImage(245, 173, 60, 60, Power_Icon[0]);

  // Disable these buttons by setting their touch area to zero.
  previousSummaryButton.set(0, 0, 0, 0);
  nextSummaryButton.set(0, 0, 0, 0);

  // Send the graphics to the LCD screen.
  Sprite_1.pushSprite(0, 0);
  M5.Buttons.draw();
}

void setup() {
  int i;
  int CurrentIcon;
  int tempYear;
  int tempMonth;
  int tempDate;

  // Init Serial.print.
  Serial.begin(115200);
  while (!Serial) {
    Serial.print('.');
  }
  delay(100);

  // Init the core2 display.
  M5.begin();
  M5.Lcd.setRotation(1);
  summaryMode = false;  // True if calendar is being displayed.

  // Init flash memory save/recall.
  if (!SPIFFS.begin()) {
    Serial.println("An Error has occurred while mounting SPIFFS");
  }

  // All graphics are written to Sprite1 and then pushed to the LCD.
  Sprite_1.createSprite(320, 240);
  Sprite_1.setRotation(3);

  // Define each button's subroutine.
  nextButton.addHandler(nextMood, E_ALL - E_MOVE);                    // Next current mood.
  summaryButton.addHandler(showSummary, E_ALL - E_MOVE);              // Enter calendar mode.
  powerButton.addHandler(powerOff, E_ALL - E_MOVE);                   // Save mood and power off the M5.
  previousSummaryButton.addHandler(previousSummary, E_ALL - E_MOVE);  // Go to previous month.
  nextSummaryButton.addHandler(nextSummary, E_ALL - E_MOVE);          // Go to next month.

  // Init Real Time Clock
  M5.Rtc.GetDate(&RTCDate);
  M5.Rtc.GetTime(&RTCTime);

  if (false) {  // Set Real Time Clock time and date if true.
    RTCDate.Year = 2024;
    RTCDate.Month = 1;
    RTCDate.Date = 1;
    RTCDate.WeekDay = 1;  // 1= Sunday

    RTCTime.Hours = 1;  // We don't use the time, but it should be set so the date changes at the right time.
    RTCTime.Minutes = 00;
    RTCTime.Seconds = 00;
    if (!M5.Rtc.SetDate(&RTCDate)) {  // Set and validate date and time.
      Serial.println("Bad Date");
    }
    if (!M5.Rtc.SetTime(&RTCTime)) {
      Serial.println("Bad Time");
    }
  }

  tempYear = RTCDate.Year;
  tempMonth = RTCDate.Month;
  tempDate = RTCDate.Date;

  //*********************************************************************
  //                              Initialize Mood History              //
  //*********************************************************************

  // TODO: Eliminate this altogether. Have the program deal with a non existant mood history.

  if (false) {  // If true, init Mood History. 
 
    moodHistory = "0000";
    moodHistoryLen = moodHistory.length();

    // Set the previous 4 days to have a blank mood icon.
    moodHistoryDates = "";
    for (i = moodHistoryLen; i > 0; i--) {
      temp1 = moodHistory.substring(i - 1, i);
      sprintf(chBuffer, "%04d-%02d-%02d-%02d:", tempYear, tempMonth, tempDate, temp1.toInt());
      String temp1(chBuffer);
      moodHistoryDates = temp1 + moodHistoryDates;

      tempDate = tempDate - 1;  // ToDo this should include year.
      if (tempDate < 1) {
        tempMonth = tempMonth - 1;
        if (tempMonth < 1) {
          tempMonth = 12;
        }
        tempDate = monthLength[tempMonth];
      }
    }
    fileSave("moodHistoryDates", moodHistoryDates);
    delay(100);
  }

  // Retrieve the mood history from flash memory.
  moodHistoryDates = fileGet("moodHistoryDates"); 
  moodHistoryDatesLen = moodHistoryDates.length();
  searchMoodHistoryDates = moodHistoryDates;
  
  sprintf(chBuffer, "%04d-%02d-%02d", RTCDate.Year, RTCDate.Month, RTCDate.Date);
  String temp1(chBuffer);

  CurrentIcon = findMood(RTCDate.Year, RTCDate.Month, RTCDate.Date);
  if (CurrentIcon != 0) {  //trim off current date if it is present.
    moodHistoryDates = moodHistoryDates.substring(1, moodHistoryDatesLen - 14);
  }

  moodIconInt[0] = CurrentIcon;

  // Work backwards in the mood history and populate the
  // three smaller home page icons with the previous 3 moods. 
  tempYear = RTCDate.Year;
  tempMonth = RTCDate.Month;
  tempDate = RTCDate.Date;
  for (i = 1; i <= 3; i++) {
    tempDate = tempDate - 1;
    if (tempDate < 1) {
      tempMonth = tempMonth - 1;
      tempDate = monthLength[tempMonth];
      if (tempMonth < 1) {
        tempMonth = 12;
        tempYear = tempYear - 1;
      }
      tempDate = getWeekDay(tempYear, tempMonth);
    }

    CurrentIcon = findMood(tempYear, tempMonth, tempDate);
    moodIconInt[i] = CurrentIcon;
  }

  DrawIcons();
}


// Use indexOf to find the needed mood by year, month and day.
int findMood(int year, int month, int date) {
  int datePos;
  String searchMood;
  sprintf(chBuffer, ":%04d-%02d-%02d", year, month, date);
  String searchDate(chBuffer);
  
  datePos = searchMoodHistoryDates.indexOf(searchDate);
  if (datePos == -1) {
    return 0;
  } else {
    searchMood = searchMoodHistoryDates.substring(datePos + 13, datePos + 14);
    return searchMood.toInt();
  }
}

// Display the initial most recent calendar page.
void showSummary(Event& e) {

  if (e.typeName() == "E_TAP") { // only act on tap events.
    summaryMode = true; // Keep showing calendars until this is false, then go back to the home page.

    summaryMonth = RTCDate.Month;
    summaryYear = RTCDate.Year;

    DrawCalendar(summaryMonth, summaryYear);
  }
}

// When the user taps the right side of the calendar screen, show the next month.
void previousSummary(Event& e) {

  if (e.typeName() == "E_TAP") {
    if (summaryMonth == RTCDate.Month && summaryYear == RTCDate.Year) {
      summaryMode = false;
    }
    // Increment the month and increment the year if it's January.
    if (summaryMode == true) {
      summaryMonth++;
      if (summaryMonth > 12) {
        summaryYear++;
        summaryMonth = 1;
      }
      DrawCalendar(summaryMonth, summaryYear);
    } else { // Summary mode is false so go to the home page.
      DrawIcons();
    }
  }
}

// When the user taps the left side of the calendar screen, show the previous month.
void nextSummary(Event& e) {

  if (e.typeName() == "E_TAP") {
    if (summaryMode == true) {
      // Decrement the month and decrement the year if the month is December.
      summaryMonth--;
      if (summaryMonth < 1) {
        summaryYear--;
        summaryMonth = 12;
      }
      DrawCalendar(summaryMonth, summaryYear);

    } else { // Summary mode is false so go to the home page.
      DrawIcons();
    }
  }
}

// Advance the current icon to the next icon.
void nextMood(Event& e) {
  int ButtonClick;

  if (e.typeName() == "E_TAP") {
    PushUp(moodIconInt[0]);
    moodIconInt[0] = moodIconInt[0] + 1;
    if (moodIconInt[0] > 5) {
      moodIconInt[0] = 0;
    }
  }

  DrawIcons();
}

// Append the current mood to the mood history and save it. Then power down the M5.
void powerOff(Event& e) {
  temp2 = moodHistoryDates;

  if (moodIconInt[0] != 0) {
    sprintf(chBuffer, "%04d-%02d-%02d-%02d:", RTCDate.Year, RTCDate.Month, RTCDate.Date, moodIconInt[0]);
    String temp1(chBuffer);
    temp2.concat(temp1);
  }

  fileSave("moodHistoryDates", temp2);

  M5.shutdown();
}


void loop() {
  M5.update();
}
