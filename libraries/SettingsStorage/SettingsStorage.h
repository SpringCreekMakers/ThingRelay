/*
  SettingsStorage.cpp - ThingRelay EEPROM Settings Storage

  Copyright (c) 2015 Spring Creek Think Tank, LLC. All rights reserved.
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

#ifndef SettingsStorage_h
#define SettingsStorage_h

#include <Arduino.h>
#include <EEPROM.h>


class SettingsStorage {

	public:
		SettingsStorage(int blockSize);
		bool setBlockIDs(char blockIDs);
		bool setBlock(String blockString, char blockID); // need to use availableBlock
		String getBlock(char blockID);
	private:
		char _blockStart;
		int _blockSize;
		int _blockRefStart; 
		int _blockRefLength;
		int _blockRefStop;
		char _blockIDs;
		int _blockIDCount;
		void clearBlock();
		bool initBlockRef();
		bool setBlockRef(char blockID, char blockStart, char blockLength);
		int availableBlock(char blockID, char bytesReq);
};

#endif