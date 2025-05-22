/*
 * mTroll MIDI Controller
 * Copyright (C) 2020,2025 Sean Echevarria
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

#ifndef MetaPatch_AxeFxNav_h__
#define MetaPatch_AxeFxNav_h__

#include "Patch.h"
#include "IAxeFx.h"


class MetaPatch_AxeFxNav : public Patch
{
public:
	MetaPatch_AxeFxNav(int number, const std::string & name) :
		Patch(number, name)
	{ }

	~MetaPatch_AxeFxNav() = default;

	void AddAxeManagers(std::vector<IAxeFxPtr> &mgrs)
	{
		mAxes = mgrs;
	}

	std::string GetPatchTypeStr() const override { return "meta: AxeFxNav"; }

	void SwitchPressed(IMainDisplay *, ISwitchDisplay *) override
	{
		for (const auto &axe : mAxes)
			axe->ForceRefreshAxeState();
	}

	void BankTransitionActivation() override { SwitchPressed(nullptr, nullptr); }
	void BankTransitionDeactivation() override { SwitchPressed(nullptr, nullptr); }

protected:
	std::vector<IAxeFxPtr>	mAxes;
};

class MetaPatch_AxeFxReloadCurrentPreset : public MetaPatch_AxeFxNav
{
public:
	MetaPatch_AxeFxReloadCurrentPreset(int number, const std::string & name) :
		MetaPatch_AxeFxNav(number, name)
	{
	}

	std::string GetPatchTypeStr() const override { return "meta: AxeFxReloadCurrentPreset"; }

	void SwitchPressed(IMainDisplay *, ISwitchDisplay *) override
	{
		for (const auto &axe : mAxes)
			axe->ReloadCurrentPreset();
	}
};

class MetaPatch_AxeFxNextPreset : public MetaPatch_AxeFxNav
{
public:
	MetaPatch_AxeFxNextPreset(int number, const std::string & name) :
		MetaPatch_AxeFxNav(number, name) 
	{ }

	std::string GetPatchTypeStr() const override { return "meta: AxeFxNextPreset"; }

	void SwitchPressed(IMainDisplay *, ISwitchDisplay *) override
	{
		for (const auto &axe : mAxes)
			axe->IncrementPreset();
	}
};

class MetaPatch_AxeFxPrevPreset : public MetaPatch_AxeFxNav
{
public:
	MetaPatch_AxeFxPrevPreset(int number, const std::string & name) :
		MetaPatch_AxeFxNav(number, name)
	{ }

	std::string GetPatchTypeStr() const override { return "meta: AxeFxPrevPreset"; }

	void SwitchPressed(IMainDisplay *, ISwitchDisplay *) override
	{
		for (const auto &axe : mAxes)
			axe->DecrementPreset();
	}
};

class MetaPatch_AxeFxNextScene : public MetaPatch_AxeFxNav
{
public:
	MetaPatch_AxeFxNextScene(int number, const std::string & name) :
		MetaPatch_AxeFxNav(number, name)
	{ }

	std::string GetPatchTypeStr() const override { return "meta: AxeFxNextScene"; }

	void SwitchPressed(IMainDisplay *, ISwitchDisplay *) override
	{
		for (const auto &axe : mAxes)
			axe->IncrementScene();
	}
};

class MetaPatch_AxeFxPrevScene : public MetaPatch_AxeFxNav
{
public:
	MetaPatch_AxeFxPrevScene(int number, const std::string & name) :
		MetaPatch_AxeFxNav(number, name)
	{ }

	std::string GetPatchTypeStr() const override { return "meta: AxeFxPrevScene"; }

	void SwitchPressed(IMainDisplay *, ISwitchDisplay *) override
	{
		for (const auto &axe : mAxes)
			axe->DecrementScene();
	}
};

#endif // MetaPatch_AxeFxNav_h__
