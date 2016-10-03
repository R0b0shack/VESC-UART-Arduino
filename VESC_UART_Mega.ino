/*
Arduino UART communication with VESC. This software reads VESC telemetr data
and displays it on an OLED display. This code is under development for an
extended ppm remote control.

This version of the software is written for testing and develpment purposes and
uses the Aruino Mega with plenty serial ports and sufficient debugging capabilities.

The "main branch" @ GitHub covers a version for the Arduino Nano without debugging,
as the Arduino Nano is small enough to fit into a small remote control case.

It is written by Sascha Ederer (roboshack.wordpress.com), based on the code
of jenkie (pedelecforum.de), Andreas Chaitidis (Andreas.Chaitidis@gmail.com)
and Benjamin Vedder (www.vedder.se).
Copyright (C) 2016

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "config.h"
#include "printf.h"
#include "datatypes.h"
#include "vesc_uart.h"
#include <SPI.h>

//Library for the OLED Display
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

mc_values VescMeasuredValues;

float current = 0.0;           //measured battery current
float motor_current = 0.0;     //measured motor current
float voltage = 0.0;           //measured battery voltage
float c_speed = 0.0;           //measured rpm * Pi * wheel diameter [km] * 60 [minutes]
float c_dist = 0.00;           //measured odometry tachometer [turns] * Pi * wheel diameter [km] 
double power=0.0;              //calculated power

#define OLED_RESET 4 // not used / nicht genutzt bei diesem Display
Adafruit_SSD1306 display(OLED_RESET);

//Setup---------------------------------------------------------------------------------------------------------------------
void setup()
{
    #ifdef DEBUG
	  DEBUGSERIAL.begin(115200);
    #endif
  
    SERIALIO.begin(115200);
    // initialize with the I2C addr 0x3C / mit I2C-Adresse 0x3c initialisieren
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
}

void loop()
{
    if (vesc_get_values(VescMeasuredValues)) {
      
        voltage = VescMeasuredValues.v_in;
        current = VescMeasuredValues.current_in;
        motor_current = VescMeasuredValues.current_motor;
        power = current*voltage;
        c_speed = (VescMeasuredValues.rpm/38)*3.14159265359*0.000083*60;
        c_dist = (VescMeasuredValues.tachometer/38)*3.14159265359*0.000083;
        
        display.clearDisplay();
	
	// set text color
	display.setTextColor(WHITE);
	// set text size
	display.setTextSize(1);

	// set text cursor position
	display.setCursor(1,0);
	// show text battery voltage
	display.println(voltage);
        display.setCursor(40,0);
        display.println("V ");
        
        // set text cursor position
	display.setCursor(60,0);
        // show text drawn current
        display.println(motor_current);
        display.setCursor(100,0);
	display.println("A ");

        // set text cursor position
	display.setCursor(1,10);
	// show text watt
	display.println(power);
        display.setCursor(40,10);
        display.println("W ");

        // set text cursor position
	display.setCursor(1,20);
	// show text distance
	display.println(c_dist);
        display.setCursor(40,20);
        display.println("km ");
        
        // set text cursor position
	display.setCursor(60,20);
        // show text speed
        display.println(c_speed);
        display.setCursor(100,20);
	display.println("km/h");

        // generate and clear display
        display.display();
	display.clearDisplay();

    }
    else
    {
        // Error message when VESC is not connected or UART data
        // can not be read by the Arduino.
        // One cause for this message can be a wrong (too short or long)
        // buffer setting in function "vesc_get_values(mc_values& values)",
        // see "vesc_uart.cpp" to change it.
        
        display.clearDisplay();
	
	// set text color
	display.setTextColor(WHITE);
	// set text size
	display.setTextSize(1);
	// set text cursor position
	display.setCursor(1,0);
	// show text
	display.println("VESC not connected");
	display.display();
	display.clearDisplay();
    }

}

