#include "industrialli_modbus_rtu.h"
#include <HardwareSerial.h>
#include <Arduino.h>

void IndustrialliModbusRTU::create_register(uint16_t address, uint16_t value){
    Register *new_register;

	new_register = (Register *) malloc(sizeof(Register));
	new_register->address = address;
	new_register->value	  = value;
	new_register->next	  = NULL;

	if(registers_head == NULL) {
        registers_head = new_register;
        registers_last = registers_head;
    } else {
        registers_last->next = new_register;
        registers_last = new_register;
    }
}

void IndustrialliModbusRTU::set_register(uint16_t _address, uint16_t _value){
    Register *reg = search_register(_address);
    reg->value = _value;
}

uint16_t IndustrialliModbusRTU::get_register(uint16_t _address){
    Register *reg = search_register(_address);
    return reg->value;
}

Register* IndustrialliModbusRTU::search_register(uint16_t address){
    Register *registers = registers_head;

    if(registers == NULL){
        return NULL;
    }
    
    do {
        if (registers->address == address){
            return(registers);
        }
        
        registers = registers->next;
	} while(registers != NULL);
	
    return NULL;
}

void IndustrialliModbusRTU::read_coils(uint16_t _start_address, uint16_t _n_coils){
    for(int reg = _n_coils - 1, bitpos = 0; reg >= 0; reg--, _start_address++, bitpos++){
        if(bitpos == 8) bitpos = 0;

        if(get_input_coil(_start_address)){
            // bitSet(frame[(3 + (_n_coils-reg)/8)], bitpos);
        }else {
            // bitClear(frame[(3 + (_n_coils-reg)/8)], bitpos);
        }
    }
}

void IndustrialliModbusRTU::read_input_coils(uint16_t _start_address, uint16_t _n_coils){
    for(int reg = _n_coils - 1, bitpos = 0; reg >= 0; reg--, _start_address++, bitpos++){
        if(bitpos == 8) bitpos = 0;

        if(get_status_coil(_start_address)){
            // bitSet(frame[(3 + (_n_coils-reg)/8)], bitpos);
        }else {
            // bitClear(frame[(3 + (_n_coils-reg)/8)], bitpos);
        }
    }
}

void IndustrialliModbusRTU::read_holding_register(uint16_t _start_address, uint16_t _n_registers){
    uint16_t value;
    
    for (int i = 0; i < _n_registers; i++){
        value = get_holding_register(_start_address + i);
        // frame[?] = value >> 8;
        // frame[? + 1] = value | 0xFF;
    }
}



void IndustrialliModbusRTU::read_input_register(uint16_t _start_address, uint16_t _n_registers){
    uint16_t value;
    
    for (int i = 0; i < _n_registers; i++){
        value = get_input_register(_start_address + i);
        // frame[?] = value >> 8;
        // frame[? + 1] = value | 0xFF;
    }
}

void IndustrialliModbusRTU::write_single_coil(uint16_t _address, uint16_t _value){
    set_status_coil(_address, (bool)_value);
}

void IndustrialliModbusRTU::write_single_register(uint16_t _address, uint16_t _value){
    this->set_holding_register(_address, _value);
}

void IndustrialliModbusRTU::write_multiple_coils(uint16_t _start_address, uint16_t _n_coils){
    for(int reg = _n_coils - 1, bitpos = 0; reg >= 0; reg--, _start_address++, bitpos++){
        if(bitpos == 8) bitpos = 0;
        set_status_coil(_start_address, bitRead(frame[(7 + (_n_coils-reg)/8)], bitpos));
    }
}

void IndustrialliModbusRTU::write_multiple_registers(uint16_t _start_address, uint16_t _n_registers, uint8_t _byte_count){
    for (uint16_t address = 7, index = 0; index < _n_registers; address += 2, index++){
        this->set_holding_register(_start_address + index, (frame[address] << 8) | frame[address + 1]);
    }
}

uint16_t IndustrialliModbusRTU::crc(uint8_t address, uint8_t *pdu, int pdusize){
    uint8_t uchCRCHi = 0xFF;
    uint8_t uchCRCLo = 0xFF;
    uint8_t uIndex;

    uIndex   = uchCRCLo ^ address;
    uchCRCLo = uchCRCHi ^ auchCRCHi[uIndex];
    uchCRCHi = auchCRCLo[uIndex];
    
    while(pdusize--){
        uIndex   = uchCRCLo ^ *pdu++;
        uchCRCLo = uchCRCHi ^ auchCRCHi[uIndex];
        uchCRCHi = auchCRCLo[uIndex];
    }

    return (uchCRCHi << 8 | uchCRCLo);
}

void IndustrialliModbusRTU::begin(HardwareSerial *_serial){
    serial = _serial;
    registers_head = NULL;
    registers_last = NULL;

    t15 = 16500000/9600; 
    t35 = t15 * 2;
}

void IndustrialliModbusRTU::set_slave_id(uint8_t _address){
    client_address = _address;
}

uint8_t IndustrialliModbusRTU::get_slave_id(){
    return client_address;
}

void IndustrialliModbusRTU::task(){
    if(receive_frame()){
        process_frame();
    }

    // send(frame[0], &frame[1], framesize - 2); // send response
}

bool IndustrialliModbusRTU::receive_frame(){
    frame_size = 0;

    if(serial->available()){
        SerialUSB.println("Here we go again.");
        while (serial->available()){
            frame[frame_size++] = serial->read();
            delayMicroseconds(t15);
        }

        uint16_t crc_frame = (frame[frame_size - 2] << 8) | (frame[frame_size - 1]);

        if(crc_frame == crc(frame[0], &frame[1], frame_size - 3)){
            return true;
        }
    }
    
    SerialUSB.println("Here we go again.");

    return false;
}

void IndustrialliModbusRTU::process_frame(){
    uint8_t _address = frame[0];
    uint8_t function = frame[1];
    uint16_t field_1 = (frame[2] << 8) | frame[3];
    uint16_t field_2 = (frame[4] << 8) | frame[5];
   
    switch (function){
        case FC_READ_COILS:
            read_coils(field_1, field_2);
            break;
        case FC_READ_INPUT_COILS:
            read_input_coils(field_1, field_2);
            break;
        case FC_READ_HOLDING_REGISTERS:
            read_holding_register(field_1, field_2);
            break;
        case FC_READ_INPUT_REGISTERS:
            read_input_register(field_1, field_2);
            break;
        case FC_WRITE_SINGLE_COIL:
            write_single_coil(field_1, field_2);
            break;
        case FC_WRITE_SINGLE_REGISTER:
            write_single_register(field_1, field_2);
            break;
        case FC_WRITE_MULTIPLE_COILS:
            write_multiple_coils(field_1, field_2);
            break;
        case FC_WRITE_MULTIPLE_REGISTERS:
            write_multiple_registers(field_1, field_2, frame[6]);
            break;
        default:
            break;
    }
}

void IndustrialliModbusRTU::send_frame(uint8_t _address, uint8_t *_pdu, int _pdusize){
    serial->write(_address);

    for (int i = 0; i < _pdusize; i++){
        serial->write(_pdu[i]);
    }

    uint16_t crc_value = crc(_address, _pdu, _pdusize);

    serial->write(crc_value >> 8);
    serial->write(crc_value & 0xFF);

    serial->flush();

    delayMicroseconds(t35);
}

void IndustrialliModbusRTU::create_status_coil(uint16_t _address, bool _value){
    create_register(_address + 1, _value);
}

void IndustrialliModbusRTU::create_input_coil(uint16_t _address, bool _value){
    create_register(_address + 10001, _value);
}

void IndustrialliModbusRTU::create_input_register(uint16_t _address, uint16_t _value){
    create_register(_address + 30001, _value);
}

void IndustrialliModbusRTU::create_holding_register(uint16_t _address, uint16_t _value){
    create_register(_address + 40001, _value);
}

void IndustrialliModbusRTU::set_status_coil(uint16_t _address, bool _value){
    set_register(_address + 1, _value);
}

void IndustrialliModbusRTU::set_input_coil(uint16_t _address, bool _value){
    set_register(_address + 10001, _value);
}

void IndustrialliModbusRTU::set_input_register(uint16_t _address, uint16_t _value){
    set_register(_address + 30001, _value);
}

void IndustrialliModbusRTU::set_holding_register(uint16_t _address, uint16_t _value){
    set_register(_address + 40001, _value);
}

bool IndustrialliModbusRTU::get_status_coil(uint16_t _address){
    return get_register(_address + 1);
}

bool IndustrialliModbusRTU::get_input_coil(uint16_t _address){
    return get_register(_address + 10001);
}

uint16_t IndustrialliModbusRTU::get_input_register(uint16_t _address){
    return get_register(_address + 30001);
}

uint16_t IndustrialliModbusRTU::get_holding_register(uint16_t _address){
    return get_register(_address + 40001);
}