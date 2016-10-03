/*
Copyright 2015 - 2017 Andreas Chaitidis Andreas.Chaitidis@gmail.com

This program is free software : you can redistribute it and / or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.If not, see <http://www.gnu.org/licenses/>.
*/

#include "vesc_uart.h"
#include "buffer.h"
#include "crc.h"
#include "datatypes.h"
#include "config.h"

int process_received_msg(uint8_t *payloadReceived) {

    //Messages <= 255 start with 2. 2nd byte is length
    //Messages >255 start with 3. 2nd and 3rd byte is length combined with 1st >>8 and then &0xFF

    int counter = 0;
    int endMessage = 256;
    bool messageRead = false;
    uint8_t messageReceived[256];
    int lenPayload = 0;

    while (SERIALIO.available()) {

        messageReceived[counter++] = SERIALIO.read();

        if (counter == 2) {//case if state of 'counter' with last read 1

            switch (messageReceived[0])
            {
                case 2:
                    endMessage = messageReceived[1] + 5; //Payload size + 2 for sice + 3 for SRC and End.
                    lenPayload = messageReceived[1];
                    break;
                case 3:
                    //ToDo: Add Message Handling > 255 (starting with 3)
                    break;
                default:
                    break;
            }

        }
        if (counter >= sizeof(messageReceived))
        {
            break;
        }

        if (counter == endMessage && messageReceived[endMessage - 1] == 3) {//+1: Because of counter++ state of 'counter' with last read = "endMessage"
            messageReceived[endMessage] = 0;
#ifdef DEBUG
            DEBUGSERIAL.println("End of message reached!");
#endif
            messageRead = true;
            break; //Exit if end of message is reached, even if there is still more data in buffer.
        }
    }
    bool unpacked = false;
    if (messageRead) {
        unpacked = unpack_payload(messageReceived, endMessage, payloadReceived, messageReceived[1]);
    }
    if (unpacked)
    {
        return lenPayload; //Message was read

    }
    else {
        return 0; //No Message Read
    }
}

bool unpack_payload(uint8_t *message, int lenMes, uint8_t *payload, int lenPay) {
    uint16_t crcMessage = 0;
    uint16_t crcPayload = 0;
    //Rebuild src:
    crcMessage = message[lenMes - 3] << 8;
    crcMessage &= 0xFF00;
    crcMessage += message[lenMes - 2];
#ifdef DEBUG
    DEBUGSERIAL.print("SRC received: "); DEBUGSERIAL.println(crcMessage);
#endif // DEBUG

    //Extract payload:
    memcpy(payload, &message[2], message[1]);

    crcPayload = crc16(payload, message[1]);
#ifdef DEBUG
    DEBUGSERIAL.print("SRC calc: "); DEBUGSERIAL.println(crcPayload);
#endif
    if (crcPayload == crcMessage)
    {
#ifdef DEBUG
        DEBUGSERIAL.print("Received: "); serial_print(message, lenMes); DEBUGSERIAL.println();
	DEBUGSERIAL.print("Payload :      "); serial_print(payload, message[1] - 1); DEBUGSERIAL.println();
#endif // DEBUG

        return true;
    }
    else
    {
        return false;
    }
}

int send_payload(uint8_t* payload, int lenPay) {
    uint16_t crcPayload = crc16(payload, lenPay);
    int count = 0;
    uint8_t messageSend[256];

    if (lenPay <= 256)
    {
        messageSend[count++] = 2;
        messageSend[count++] = lenPay;
    }
    else
    {
        messageSend[count++] = 3;
        messageSend[count++] = (uint8_t)(lenPay >> 8);
        messageSend[count++] = (uint8_t)(lenPay & 0xFF);
    }
    memcpy(&messageSend[count], payload, lenPay);

    count += lenPay;
    messageSend[count++] = (uint8_t)(crcPayload >> 8);
    messageSend[count++] = (uint8_t)(crcPayload & 0xFF);
    messageSend[count++] = 3;
    messageSend[count] = NULL;
    //Sending package
    SERIALIO.write(messageSend, count);
#ifdef DEBUG
    DEBUGSERIAL.print("UART package send: "); serial_print(messageSend, count);

#endif // DEBUG

    //Returns number of send bytes
    return count;
}


bool process_read_package(uint8_t* message, mc_values& values, int len) {
    COMM_PACKET_ID packetId;
    int32_t ind = 0;

    packetId = (COMM_PACKET_ID)message[0];
    message++;//Eliminates the message id
    len--;

    switch (packetId)
    {
        case COMM_GET_VALUES:
            ind = 0;
            values.temp_mos1 = buffer_get_float16(message, 10.0, &ind);
            values.temp_mos2 = buffer_get_float16(message, 10.0, &ind);
            values.temp_mos3 = buffer_get_float16(message, 10.0, &ind);
            values.temp_mos4 = buffer_get_float16(message, 10.0, &ind);
            values.temp_mos5 = buffer_get_float16(message, 10.0, &ind);
            values.temp_mos6 = buffer_get_float16(message, 10.0, &ind);
            values.temp_pcb = buffer_get_float16(message, 10.0, &ind);

            values.current_motor = buffer_get_float32(message, 100.0, &ind);
            values.current_in = buffer_get_float32(message, 100.0, &ind);
            values.duty_now = buffer_get_float16(message, 1000.0, &ind);
            values.rpm = buffer_get_int32(message, &ind);
            values.v_in = buffer_get_float16(message, 10.0, &ind);
            values.amp_hours = buffer_get_float32(message, 10000.0, &ind);
            values.amp_hours_charged = buffer_get_float32(message, 10000.0, &ind);
            ind += 8; //Skip 9 bit
            values.tachometer = buffer_get_int32(message, &ind);
            values.tachometer_abs = buffer_get_int32(message, &ind);
            values.fault_code = (mc_fault_code)message[ind++];

            return true;
            break;

        default:
            return false;
            break;
    }

}

bool vesc_get_values(mc_values& values) {
    uint8_t command[1] = { COMM_GET_VALUES };
    uint8_t payload[256];
    send_payload(command, 1);
    delay(200); //needed, otherwise data is not read
    int lenPayload = process_received_msg(payload);
    if (lenPayload > 0) {
        bool read = process_read_package(payload, values, lenPayload); //returns true if sucessfull
        return read;
    }
    else
    {
        return false;
    }
}

void set_motor_current(float current) {
    int32_t index = 0;
    uint8_t payload[5];

    payload[index++] = COMM_SET_CURRENT ;
    buffer_append_int32(payload, (int32_t)(current * 1000), &index);
    send_payload(payload, 5);
}

void set_brake_current(float brakeCurrent) {
    int32_t index = 0;
    uint8_t payload[5];

    payload[index++] = COMM_SET_CURRENT_BRAKE;
    buffer_append_int32(payload, (int32_t)(brakeCurrent * 1000), &index);
    send_payload(payload, 5);

}

void serial_print(uint8_t* data, int len) {

    //	DEBUGSERIAL.print("Data to display: "); DEBUGSERIAL.println(sizeof(data));

    for (int i = 0; i <= len; i++)
    {
        DEBUGSERIAL.print(data[i]);
        DEBUGSERIAL.print(" ");
    }
    DEBUGSERIAL.println("");
}

void serial_print(const mc_values& values) {

    DEBUGSERIAL.print("Temp MOS1: "); DEBUGSERIAL.println(values.temp_mos1);
    DEBUGSERIAL.print("Temp MOS2: "); DEBUGSERIAL.println(values.temp_mos2);
    DEBUGSERIAL.print("Temp MOS3: "); DEBUGSERIAL.println(values.temp_mos3);
    DEBUGSERIAL.print("Temp MOS4: "); DEBUGSERIAL.println(values.temp_mos4);
    DEBUGSERIAL.print("Temp MOS5: "); DEBUGSERIAL.println(values.temp_mos5);
    DEBUGSERIAL.print("Temp MOS6: "); DEBUGSERIAL.println(values.temp_mos6);
    DEBUGSERIAL.print("Temp PCB: "); DEBUGSERIAL.println(values.temp_pcb);

    DEBUGSERIAL.print("avgMotorCurrent: "); DEBUGSERIAL.println(values.current_motor);
    DEBUGSERIAL.print("avgInputCurrent: "); DEBUGSERIAL.println(values.current_in);
    DEBUGSERIAL.print("dutyCycleNow: "); DEBUGSERIAL.println(values.duty_now);
    DEBUGSERIAL.print("rpm: "); DEBUGSERIAL.println(values.rpm);
    DEBUGSERIAL.print("inputVoltage: "); DEBUGSERIAL.println(values.v_in);
    DEBUGSERIAL.print("ampHours: "); DEBUGSERIAL.println(values.amp_hours);
    DEBUGSERIAL.print("ampHoursCharges: "); DEBUGSERIAL.println(values.amp_hours_charged);
    DEBUGSERIAL.print("tachometer: "); DEBUGSERIAL.println(values.tachometer);
    DEBUGSERIAL.print("tachometerAbs: "); DEBUGSERIAL.println(values.tachometer_abs);
    DEBUGSERIAL.print("Fault Code: "); DEBUGSERIAL.println(values.fault_code);

}

