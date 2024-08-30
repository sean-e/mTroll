/*
 * mTroll MIDI Controller
 * Copyright (C) 2024 Sean Echevarria
 *
 * This file is part of mTroll.
 *
 * mTroll is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * mTroll is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Let me know if you modify, extend or use mTroll.
 * Original project site: http://www.creepingfog.com/mTroll/
 * Contact Sean: "fester" at the domain of the original project site
 */

#ifndef RestCommand_h__
#define RestCommand_h__

#include "BaseSleepCommand.h"
#include "MidiControlEngine.h"


class RestCommand : public BaseSleepCommand
{
public:
	enum class RestNoteValue
	{
		n1,
		n2,
		n4,
		n8,
		n16,
		n32,
		UnknownValue
	};

	static RestNoteValue StringToNoteValue(const std::string &val)
	{
		if (val == "1/1" || val == "whole" || val == "1")
			return RestNoteValue::n1;
		if (val == "1/2" || val == "half" || val == "2")
			return RestNoteValue::n2;
		if (val == "1/4" || val == "quarter" || val == "4")
			return RestNoteValue::n4;
		if (val == "1/8" || val == "eighth" || val == "8")
			return RestNoteValue::n8;
		if (val == "1/16" || val == "sixteenth" || val == "16")
			return RestNoteValue::n16;
		if (val == "1/32" || val == "thirtysecond" || val == "32")
			return RestNoteValue::n32;

		return RestNoteValue::UnknownValue;
	}

	RestCommand(MidiControlEngine *eng, RestNoteValue r) noexcept :
		mEng(eng),
		mRestAmount(r)
	{
	}

protected:
	int GetSleepAmount() noexcept override
	{
		const int tempo = mEng->GetTempo();
		_ASSERTE(tempo > 0);
		// 60,000 (ms) ÷ BPM = duration of a quarter note
		const double kQtr = 60000 / tempo;
		switch (mRestAmount)
		{
		case RestNoteValue::n1:		return kQtr * 4;
		case RestNoteValue::n2:		return kQtr * 2;
		case RestNoteValue::n4:		return kQtr;
		case RestNoteValue::n8:		return kQtr / 2;
		case RestNoteValue::n16:	return kQtr / 4;
		case RestNoteValue::n32:	return kQtr / 8;
		}

		_ASSERTE(!"not possible");
		return 200;
	}

private:
	RestCommand();

private:
	const RestNoteValue		mRestAmount;
	MidiControlEngine		*mEng;
};

#endif // RestCommand_h__
