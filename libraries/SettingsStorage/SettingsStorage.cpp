/*
  SettingsStorage.cpp - ThingRelay EEPROM Settings Storage

  Copyright (c) 2015 Spring Creek Makers. All rights reserved.
  Author Shane E. Bryan <shane.bryan@scttco.com>
  This file is part of the ThingRelay library, which relies on
  the esp8266 core for Arduino environment.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "Arduino.h"
#include "SettingsStorage.h"
#include "EEPROM.h"

SettingsStorage::SettingsStorage(int blockSize) {
	_blockSize = blockSize; //Byte size of the EEPROM
  _blockStart = 224; //This is just a DEC char to confirm that a block has been created
  _blockReferenceStart  = 1; //Hard coded location (byte) of the start to the block reference section
  _blockReferenceLength = 3; //Each BlockID Ref is currently 3 bytes BlockID, Location, Size
}

SettingsStorage::start(char blockIDs) {
  _blockIDs = blockIDs;
  __blockIDCount = sizeof(_blockIDs) - 1; //Currently 8 block IDs
  _blockReferenceStop = (_blockReferenceLength * _blockIDCount) + _blockReferenceLength - 1;
  //Start EEPROM Library 
  EEPROM.begin(_blockSize);
}

SettingsStorage::clear() {
  for (int i = 0; i < _blockSize; i++)
    EEPROM.write(i, 0);
    EEPROM.commit();
}

SettingsStorage::initializeBlockReference() { //Initializes the EEPROM block if: it is blank, is corrupt, or is being refreshed
  if (EEPROM.read(0) != _blockStart) {// ASCII lowercase Alpha
    int i = 0;
    for (int x = _blockReferenceLength; (x <= _blockReferenceStop) && (i <= (_blockIDCount - 1)); x += _blockReferenceLength) {
      EEPROM.write(x, _blockIDs[i]);
      EEPROM.commit();
      for (int j = 1; j <= 2; j++) {
        char a = 0;
        EEPROM.write(x + j, a);
        EEPROM.commit();
      }
      i++;
    }
    EEPROM.write(0, _blockStart);
    EEPROM.commit();
    return true; 
  }
  return false;
}

SettingsStorage::setBlockRef(char blockID, char _blockStart, char blockLength) {
  //Initializing Local Variables
  char _blockID = blockID;
  char __blockStart = blockStart;
  char _blockLength = blockLength;

  //Setting Block Reference information
  if (!initializeBlockReference()) {
    Serial1.println("setBlockRef has been initialized");
    for (int x = _blockReferenceLength; x <= _blockReferenceStop; x += _blockReferenceLength) {
      if (EEPROM.read(x) == _blockID) {
        EEPROM.write(x + 1, _blockStart);
        EEPROM.write(x + 2, _blockLength);
        EEPROM.commit();
        Serial1.println("setBlockRef returned true");
        return true;
      }
    }
  }
  else {
    Serial1.println("setBlockRef returned false");
    //In order for this function to have been called Block Reference should have already been Initialized
    return false; 
  }
}

SettingsStorage::availableBlock(char blockID, char bytesReq) { //Locates the first available big enough block
  char _blockID = blockID;
  char _bytesReq = bytesReq;
  Serial1.print("bytesReq Variable: ");
  Serial1.println(_bytesReq, DEC);
  
  bool _currentBlock = false; //Is there a current block 
  char _currentBlockLength;
  char _currentBlockStart;
  bool _currentBlockAvailable = false; //Can we use the current block
  char _availableBlock;
  
  if (EEPROM.read(0) == blockStart) { // 11100000 is Binary for ASCII lowercase Alpha 
    for (int x = _blockReferenceLength; x <= _blockReferenceStop; x += _blockReferenceLength) { //Checking to see if there is a current block and if so what it's length is
      if (EEPROM.read(x) == _blockID) {
        _currentBlock = true;
        _currentBlockStart  = (EEPROM.read(x + 1));
        Serial1.print("_currentBlockStart Variable: ");
        Serial1.println(_currentBlockStart, DEC);
        _currentBlockLength = (EEPROM.read(x + 2));
        Serial1.print("_currentBlockLength Variable: ");
        Serial1.println(_currentBlockLength, DEC);
        if (_currentBlockLength >= _bytesReq) { //If this is true we can use the current block
          _currentBlockAvailable = true;
          _availableBlock = _currentBlockStart;
          Serial1.println("Using current block");
          return _availableBlock;
        }
        break;
      }
    }
    if ((_currentBlockAvailable == false) || (_currentBlock == false)) {
      //Since current block is not big enough or is not available we must locate one that is
      char _blocks[_blockIDCount * 2];
      int _blocksPointer = 0;
      for (int x = _blockReferenceLength; x < _blockReferenceStop; x += _blockReferenceLength) {
        Serial1.print("For Loop: ");
        Serial1.println(x);
        Serial1.print("_blocksPointer Variable: ");
        Serial1.println(_blocksPointer);
        char _startByte;
        char _blockLength;
        _startByte = (EEPROM.read(x + 1));
        Serial1.print("For Loop _startByte Variable: ");
        Serial1.println(_startByte, DEC);
        _blockLength = (EEPROM.read(x + 2));
        Serial1.print("For Loop _blockLength Variable: ");
        Serial1.println(_blockLength, DEC);
        _blocks[_blocksPointer] = _startByte;
        _blocksPointer++;
        _blocks[_blocksPointer] = _blockLength;
        _blocksPointer++;
      }
      char _highestStartByte = _blockReferenceStop + 1;
      char _highestTotalBytes = 0;
      Serial1.print("_highestStartByte Variable before loop: ");
      Serial1.println(_highestStartByte, DEC);
      for (int x = 0; x < (_blockIDCount * 2);) {
        if (_blocks[x] >= _highestStartByte) {
          _highestStartByte = _blocks[x];
          x++;
          _highestTotalBytes = _blocks[x];
          x++;
        }
        else {
          x = x + 2;
        }
      }
      Serial1.print("_highestStartByte Variable after loop: ");
      Serial1.println(_highestStartByte, DEC);
      Serial1.print("_highestTotalByte Variable after loop: ");
      Serial1.println(_highestTotalBytes, DEC);
      if (((_highestStartByte - 1) + _highestTotalBytes + _bytesReq) < blockSize) {
        _availableBlock = _highestStartByte + _highestTotalBytes;
        return _availableBlock;
      }
    }
    return -1;
  }
  return -1;
}
SettingsStorage::setBlock(String blockString, char blockID) {
  //Initializing Local Variables
  char _blockStringLength = blockString.length();
  char _blockSize = _blockStringLength; //Length of blockString without trailing null char
  char _blockBytes[_blockSize + 1];
  blockString.toCharArray(_blockBytes, sizeof(_blockBytes)); //Converting String Object to Char array
  char _blockID = blockID;
  int _startBlock = availableBlock(_blockID, _blockBytes);
  //Writing converted String Object to memory
    //By just using _blockSize this loop will not include trailing null char added when converting to char array
    //To do so use "x <= sizeof(_blockBytes) - 1". Minus 1 is to account for 0 start position in array
  for (int x = 0; x <= _blockSize - 1; x++) { 
    int _block = _startBlock + x;
    EEPROM.write(_block, _blockBytes[x]);
    EEPROM.commit();
    delay(100); //This is a time consuming function. Delay allows pending background tasks to complete
  }

  //Confirming String Object was properly written to memory
  for (int x = 0; x <= _blockSize - 1; x++) {
    int _block = _startBlock + x;
    char _value;
    _value = EEPROM.read(_block);
    delay(100);
    if (_value != _blockBytes[x]) {
      Serial1.println("setBlock could not be confirmed");
      return false;
    } 
  }
  //Updating Block Reference
  if (setBlockRef(_blockID, _startBlock, _blockSize)) {
    Serial1.println("setBlock returned true");
    return true; 
  }
  else {
    Serial1.println("setBlock returned false");
    return false;
  }
}
