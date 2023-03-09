### mTroll MIDI controller changelog / process milestones


#### 2023.03.08
- Support for automatic repeat of `A` group commands specified in `repeatingToggle` and `repeatingMomentary` patch types (enabling a repeating patch type creates a unique thread for the patch that runs the commands in a loop -- use the [Sleep](docs.md#patchCommands) patch command as needed)
- Added support for Dynamic Distortion blocks added in Axe-FX III firmware version 20.00
- Fixed expression pedal handling when restricting MIDI CC output range
- Fixed expression pedal virtual button activation failure to trigger half-way through deadzone
- Updated monitoring of expression pedals (both simple and detailed display variants)
- Allow multiple patch definitions for Axe-FX III effect blocks so that multiple patch types can exist for each effect block (for example, separate toggle and momentary patches for Flanger 1)
- Allow multiple MIDI device names delimited by `;` in `in` and `out` attributes of `midiDevice` definitions

#### 2022.01.12
- Support dark mode titlebar when window is inactive

#### 2022.01.10
- Added support for monitoring external control change messages on patches of type `toggleControlChange`
- Added support for selecting MIDI out port in manual/interactive program change, control change, and beat clock modes
- Added MIDI beat clock engine mode for setting tempo and enabling/disabling generation of MIDI beat clock
- Added support for optional auto-generation of all patch and bank numbers to eliminate need for manual management of the numbers
- Added support for `toggleControlChange` and `momentaryControlChange` patch types as shortcuts for toggle and momentary patch types that simply modify a single controller between 127 and 0 (patch requires device and controller attributes)

#### 2021.06.30
- Added support for Echoplex Digital Pro and Pro Plus global and local state display
- Added new patch command type, `EdpProgramChange`, which displays local/preset state after sending the requested program change
- Added new meta commands `EdpShowGlobalState` and `EdpShowLocalState` for displaying state independent of program change commands
- Added support for assigning patches to switches by patch name in bank switch definitions
- Added support for specifying target of `LoadBank` command by bank name in bank switch definitions

#### 2021.06.27
- Fix for inadvertent propagation of secondary switch action defined in default bank when target bank contains primary switch definition

#### 2021.05.06
- Axe-Fx III effect block patches now display active block channel
- Changed long press duration from 500ms to 300ms, and extended press duration from 3sec to 2sec

#### 2021.04.20
- Added compositeToggle patch type that creates a toggle patch composed of other patches

#### 2021.03.27
- Updated Axe-Fx III example config
- Report invalid banks referenced by system config
- Removed unnecessary dependency on Axe-Fx III effect blocks in patches for Axe-Fx III effect block channels
- Fixed display of patch state when Axe-Fx III effect block channel patch is executed twice (channels do not become inactive by being pressed a second time)

#### 2021.03.23
- Patches that select Axe-Fx III scenes automatically update with the names of the scenes in the Axe-Fx III preset that has been activated

#### 2021.01.14
- Reduced main display flicker when updating after Axe-Fx query

#### 2020.12.17
- Fixed updating of Axe-Fx III scene info (possibly broken by timer changes in a Windows update)

#### 2020.10.30
- Support for RGB LEDs (using [modified monome firmware](https://github.com/sean-e/monome40hFirmware))
- Support for Axe-Fx III

#### 2020.08.12
- Updated to Qt 5.15.0

#### 2020.07.12
- Added support for automatic grid layout
- Added View menu

#### 2020.06.27
- Added new meta patch to reset patch that changes state of patch without exec of deactivation (for reset of a `patchListSequence`).
- Added new optional `gaplessRestart` attribute to `patchListSequence` for restart without deactivated state (use the new meta patch to reset the sequence).
- Added Settings menu to main menubar.
- Updated expression pedal status message display (added option to use old detailed status in Settings menu).
- Updated styling of menus, menubar, and scrollbars.
- Preliminary support for Axe-Fx III (completely untested, no hardware yet)

#### 2018.05.06  
- Changed expression pedal status message display.  
- Added application manifest for Windows 10.  
- Renamed configuration `bank\patchmap` to `bank\switch` (old name still supported for backwards compatibility).  
- Updated to Qt 5.10.1 resulting in ~50% reduction in size of installer.  
- Modernized some of the code.  

#### 2017.04.02  
- Fix for inadvertent toggle patch state changes when processing exclusive group switches (if a patch is assigned to multiple switches in the group, or if a switch is pressed multiple times).  
- When pressing a button, patch name is no longer displayed for inactive patches in main display window. Patch number continues to be displayed but without name since it is confusing to turn off a patch and then see its name show up in the main display.  

#### 2016.10.29  
- Fix for RefirePedal command failing to work as expected when activating a patch (RefirePedal didn't work when transitioning from one patch to another).  

#### 2016.06.21  
- Prevent propagation of switch groups from default bank if any buttons in target banks are already assigned to another group (banks should define entire switch groups rather than rely on partial propagation).  

#### 2015.08.30  
- Renamed the "Time Display" page to "Time + utils."  
- Elapsed time is now calculated from application start up rather than from file load (so reload does not reset timer).  
- Added command to reset the timer manually.  
- Added support for pause/resume of elapsed time.  
- Added multiple application exit commands to the "Time + utils" page; 1) exit application, 2) exit and sleep, 3) exit and hibernate  

#### 2015.05.24  
- Automatically sync Axe-Fx effects/state at startup.

#### 2015.05.03  
- Fix `persistentPedalOverride` failure when a `PatchListSequence` is activated.

#### 2015.03.27  
- Added new patch type `persistentPedalOverride` that is a toggle patch used to reassign expression pedals such that the reassignment sticks until the patch is toggled off or until another `persistentPedalOverride` patch is activated. It overrides pedal assignments made in subsequently activated patches that are not of type `persistentPedalOverride`.  
- Secondary switch functions no longer inadvertently deactivate expression pedal settings of primary switch functions.  

#### 2015.03.20  
- Updated for Axe-Fx II firmware version 18 (Rotary X/Y).  

#### 2015.03.14  
- Fixed LED mapping after loading a bank via the Bank Direct Access command/mode.  
- Fixed another case where secondary function was not properly displayed in the main display in certain circumstances.  
- Added sysex ID support for Axe-Fx II XL and XL+ (untested).  

#### 2015.02.15  
- Added 1280x800 ui config for use on Dell Venue 8 Pro.  
- Updated Main display window text formatting of bank patchmaps for more consistent columns when using fixed-width font.  
- Fix for font names with spaces not being read from ui config files (see tinyxml QueryValueAttribute() vs Attribute()).  
- Fixed case where loadbank in a secondary function caused update to switch label text after new bank was loaded.  
- Made secondary functions update main display (if bank is still loaded) even if no action is taken, to note that the new function state was registered.  
- Support for width and height overrides in ui config inside of switchAssembly on the individual sub-elements.  
- Support for putting vOffset and hOffset in the switchAssemblyConfig elements rather than duplicating for every switchAssembly element.  
- Changed "Mode select" to "Menu" and Mode button to Menu button.  
- Deprecate EngineMetaPatch patch type in favor of PatchMap "command" attribute.  
- Deprecate PatchMap "name" attribute in favor of "label" attribute.  

#### 2014.11.30  
- MIDI port in `localExpr` blocks now use the midi port of the specified device if port is not explicitly set.  
- Support for optional name on PatchMap entries to eliminate need for separate name placeholder patches.  
- All LEDs turned off before releasing monome device.  

#### 2014.05.30  
- MidiDevice port binding routine supports multiple attempts with different names (create multiple entries for the same port each with a different device name).  
- MidiDevice port binding doesn't bind same port once bound successfully.  
- GlobalExpr now supports port binding (port is still required on parent expression tag for default binding of GlobalExprs that don't explicitly override port).  
- Axe-FX support: the entire main display is updated by Axe-FX preset name and scene number instead of appended to text already present.  
- Axe-FX support: 'LED' UI of present but inactive effect is dimmed (in software UI, not hardware).  
- Axe-FX II support: inverted X/Y LED handling such that X is LED off (dimmed in software UI) and Y is LED on.  

#### 2014.03.16  
- Added support for specifying computer MIDI device by name rather than OS index for portability of config file to different systems.  

#### 2013.09.22  
- Added menu command to Suspend/Resume MIDI device connections.  
- Added elapsed time to the time display engine mode.  

#### 2013.07.20  
- Improved support for Axe-Fx II scenes.  
- Added support for Axe-Fx II looper modes.  
- Displays Axe-Fx II preset number when name is displayed.  
- Created installer which includes all dependencies and sample files.  
- Moved samples files to .\config sub-directory.  
- Created config file for Axe-Fx II.  
- Added support for a new meta-patch: [ResetExclusiveGroup](docs.md#metaPatches).  
- Buttons support long-press and momentary press when invoked via touch (previously touch caused immediate press and release events).  
- Menus displayed in alternate style more suitable for fingers if touch input is detected.  
- Moved to Qt 5.0.2 (unfortunately causing significant increase in size of dependencies).  
- Fixed situation where some LEDs in an exclusive switch group did not turn off when they should have.  
- Fixed propagation of exclusive switch groups defined in the default bank to other banks where applicable (when switches are not otherwise redefined in the target banks).  

#### 2013.01.15  
- Added support for Axe-Fx II scenes (introduced in firmware version 9).  
- Normal-type patches are deactivated, when stepping through a PatchListSequencePatch.  
- Fix for crash when loading/reloading configuration in some circumstances.  
- Fix for incorrect initial size of window after refresh/reload when maximized.  

#### 2012.08.20  
- Fix for bug where switch release was routed to a different bank than switch press if the press caused the load of a different bank (bank loads now occur on switch release instead of press).  
- Display and switch LEDs update when changing presets via Axe-Fx II front panel if using recent versions of firmware that send notification of preset loads.  

#### 2012.07.21  
- Added Axe-Fx II support.  
- Created [patchListSequence](docs.md#patches) patch type that steps through a list of patches.  
- Added [secondary function](docs.md#secondFunction) switch support accessed via long-press of switches (defined in a `PatchMap` of a `Bank`).  
- Mode switch invokes [Backward](docs.md#metaPatches) metapatch on long-press.  
- Updated LED illumination of mode, next and back switches.  

#### 2011.10.15  
- Fixed inadvertent change of LED state of first LED.  
- Fixed handling of loadState/unloadState/override/sync attributes (loadState was also inadvertently being set for unloadState).  
- Improved timing of sync after AxeFxProgramChange.  

#### 2011.03.26  
- Added interactive control change engine mode.  

#### 2011.02.13  
- Added support for [Axe-Fx](axe.html) Feedback Return block bypass synchronization (via external ctrl modifier).  
- Fixed off by one channel error when defining local expression pedal using implicit device default channel (as opposed to explicit channel).  
- Fixed occasional expression pedal status text corruption.  

#### 2010.12.28  
- Created project and uploaded source to [SourceForge](http://sourceforge.net/projects/mtroll/).  
- Added support for [Axe-Fx processors](axe.html) (via special device name, new patch types, patch commands and metapatch).  
- Allow expression pedals to be active while in some engine modes other than the default bank mode.  
- DeviceChannelMap supports device to MIDI port binding.  
- Status updates to main display by expression pedals are prepended to existing text rather than overwriting it.  
- Expression pedals are bound only to the first patch if there are multiple patches assigned to the same switch.  
- Patches can optionally define a default channel for all commands (that can be overridden on individual commands).  
- Added a [Sleep](docs.md#patchCommands) patch command.  
- [PatchMap](docs.md#patchmaps) accepts empty patch number in order to defeat a default mapping without actually overriding it.  
- Mappings in PatchMaps have new optional `override` and `sync` attributes.  
- Added time display engine mode.  
- Added interactive program change engine mode.  
- Switch mappings during Mode Select are customizable.  
- Added support for direct access to any mTroll bank during mode select (reducing need for the `LoadBank` metapatch).  
- Made the `recall`, `backward` and `forward` metapatches available during mode select.  
- Reorganized default switches used in mode select.  
- Created new [LoadNextBank](docs.md#metaPatches) and [LoadPreviousBank](docs.md#metaPatches) metapatches that load the next/previous bank (without confirmation, in contrast to the `Next` and `Prev` commands of Bank Navigation mode).  
- Added support for spaces in button labels.  
- Made UI background and element frame colors customizable.  
- Updated default colors.  
- Fixed problem opening MIDI device during reload.  

#### 2010.09.15  
- Added support for expression pedal [response curves](docs.md#curves).  

#### 2010.08.29  
- Added support for expression pedal [virtual toggle footswitches](docs.md#virtualToggles) at toe and/or heel positions.  
- Added interactive engine commands that are accessible from the hardware via [engine mode](docs.md#engineModes) select:  
	- toggle display of trace window (previously only available in the GUI)  
	- set ADC overrides (previously only available in the GUI)  
	- invert LEDs (previously only available via the config data file)  
	- test LEDs (previously only occurred during hardware handshake)  
- Fixed failure to load some elements that followed comments in XML files  
- Minor update to expression pedal jitter control.  

#### 2010.01.03  
- Many changes to XML data files:  
	- Added support for multiple commands per patch group (instead of a single, long hex string per patch).  
	- Added support for simpler non-binary commands for [ProgramChange](docs.md#patchCommands), [ControlChange](docs.md#patchCommands), [NoteOn](docs.md#patchCommands), [NoteOff](docs.md#patchCommands) (which can be used instead of [midiByteString](docs.md#patchCommands)s).  
	- Added support for new patch command: [RefirePedal](docs.md#patchCommands).
	- Added config data file validation messages.  
	- Fixed crash loading malformed data files.  
- Improved expression pedal jitter control.  

#### 2009.05.17  
- Fixed handling of interrupted USB reads which would cause occasional unknown command trace statements.  

#### 2009.03.14  
- Added menu items to disable ADC ports/expression pedals independently of the config files. These items either let the file-defined state pass untouched, or override them to force the ADCs off. Useful if, for example, you load a config file that has defined ADC behavior but you don't actually have any pedals hooked up. The overrides are retained across file loads and sessions.  

#### 2008.12.06  
- Added support for double byte expression pedal control changes (though the monome hardware only supports 1024 steps - at least it's an improvement over the 128 steps possible using a single byte control change) (use `doubleByte` attribute).  
- Fixed crash that occurred when opening a config file with a bank that maps to a non-existent patch.  
- Created XSL file for nicer display of config files in web browsers (compare [without XSL](downloads/testdata.config.xml) to [with XSL](downloads/testdataWithXsl.config.xml)).  

#### 2008.11.09  
- Added "Toggle trace window visibility" command (Ctrl+T) that hides/shows the trace window and resizes the main display window appropriately.  

#### 2008.11.02  
- A handful of changes resulting from experience on the Asus eee PC Model 900.  
- Added 1024x600 ui profile for 8.9" display.  
- Added auto-scrollbars to main display.  
- Previous builds apparently didn't work if Visual Studio 2008 wasn't installed due to the damn SxS manifest crap. It's addressed now but requires an update of the Qt libraries.  

#### 2008.08.23  
- Qt build released.  
- Released source code.

#### 2008.07.05  
- User-defined banks can make patch mappings to the switches reserved for Next Bank and Previous Bank. The switches are still reserved in the alternate operating modes. You can now override and use the Next and Previous switches in none/some/all banks. If no mapping is specified, the switches default to Next and Previous functionality (to switch from standard Bank mode to Bank Navigation mode).

#### 2008.06.29  
- Finished port to [Qt 4.4](http://trolltech.com/downloads/opensource/appdev/windows-cpp) including Qt-based monome client implementation. Now, a Mac or Linux developer will only need to write their own MIDI out implementation (and create their own makefile).

#### 2008.06.07  
- Started port from Windows-only WTL to cross-platform [Qt 4.4](http://trolltech.com/downloads/opensource/appdev/windows-cpp)

#### 2008.05.08  
- Added support for new meta-patches: [Backward](docs.md#metaPatches), [Forward](docs.md#metaPatches) and [Recall](docs.md#metaPatches)  

#### 2008.02.11  
- Automatically disable screensaver and system sleep/shutdown timers while application is running if monome is found at startup (previous settings restored at exit)

#### 2007.12.23  
- Added support for [exclusive switch groups](docs.md#exclusiveGroup) (for radio button functionality)  
- Added support for a new meta-patch: [Load Bank](docs.md#metaPatches)  
- Added support for inverse LED display (specified in the `hardware` node of the .ui.xml file)

#### 2007.11.17  
- Added support for [instant access switches](docs.md#instantAccess)

#### 2007.11.03  
- After biting the bullet and ordering a firmware programmer, it arrived this week; tweaked the adc filtering/smoothing. Happy now.

#### 2007.10.21  
- Automatically loads last opened files at startup  
- Fixed flicker in the GUI  
- Made LED intensity user definable  
- Cleaned up GUI (removed unused default elements)  
- Added Raw ADC Value engine mode  

#### 2007.09.16  
- Added first meta-patch: [Reset patches in active bank](docs.md#metaPatches)

#### 2007.09.08  
- Started on this page

#### 2007.09.01  
- Worked out expression pedal calibration and load of pedal settings from file

#### 2007.08.19  
- Wired up the switches  
- Initial pass at expression pedal support

#### 2007.08.11  
- Wired up the LEDs

#### 2007.08.05  
- Finally got around to assembling the logic board  
- Fixed some bugs in the monome software framework, app now works with the board  
- My neighbor, [Brian Barron](http://www.barronknives.com/), drilled extra holes in the Ground Control Pro case

#### 2007.07.12  
- Met Josh Fiden at [Voodoo Lab](http://www.voodoolab.com/) in Santa Rosa and scored a Ground Control Pro chassis - this was huge! Not being too handy with metal, without this the alternatives would have been:
	-   plywood box
	-   find and gut a used board
	-   one of those plastic project enclosures
	-   shoebox
	-   give up
	-   who knows...
- Many thanks to Josh and Harmony at [Voodoo Lab](http://www.voodoolab.com/)!

#### 2007.07.08  
- Initial pass at FTDI and monome software support

#### 2007.07.06  
- Read about and ordered the monome logic kit after having looked at other USB bridge boards (like [Phidgets](http://www.phidgets.com/), [EleXol](http://www.elexol.com/) and [CREATE USB Interface](http://www.create.ucsb.edu/~dano/CUI/))

#### 2007.06.16  
- Initial pass at midi out device, UI and file loader implementation

#### 2007.05.30  
- Completed initial UI and got it all to compile

#### 2007.05.26  
- Completed first pass at engine control logic

#### 2007.05.19  
- Exhibits at [Maker Faire](http://makerfaire.com/bayarea/2007/) provide inspiration and get the gears turning

