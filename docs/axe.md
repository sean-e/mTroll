# mTroll MIDI Controller Axe-Fx Support

## Overview
mTroll has special support for [Fractal Audio Axe-Fx](https://www.fractalaudio.com/) processors (Axe-Fx, Axe-Fx II, Axe-Fx III).
It includes 
a default [config.xml data file](../data/axefx3v2.config.xml) that targets Axe-Fx III processors, 
a default [config.xml data file](../data/axefx2.config.xml) that targets Axe-Fx II processors, and
a default [config.xml data file](../data/axefx.config.xml) that targets Axe-Fx processors.

When using mTroll with an Axe-Fx, first edit the applicable `axefx.config.xml` file to make sure the channel 
specified in the `DeviceChannelMap` matches the channel of your Axe-Fx.
Then open the file in mTroll.

###  Axe-Fx and Axe-Fx II Notes
These are my notes which need to be fleshed out.  Check out the config.xml data files referenced above for actual use.

#### Enhanced Axe-Fx support is dependent upon:
- minimum of Axe-Fx firmware version 10 (any Axe-Fx II firmware version)
- a `DeviceChannelMap` mapping with device name "AxeFx" or "Axe-Fx"
- "AxeFx" `device` name being mapped to the same channel as your actual Axe-Fx
- the use of that `device` name in defined patches (ie, `device="AxeFx"` in the `patch` definition)
- Axe-Fx to mTroll sync support is dependent upon both MIDI In and Out being connected between mTroll and the Axe-Fx (Axe-Fx II USB connection is sufficient)

#### Main window display
- Axe-Fx preset names are displayed automatically on the last line of the display whenever an Axe-Fx program change is issued
- Axe-Fx II preset names are displayed automatically in the display whenever
a preset on the Axe-Fx II is loaded or whenever a scene change occurs
- Axe-Fx II scene numbers are displayed automatically whenever the preset name is displayed
- When using the Axe-Fx II looper block, looper status is displayed on the first line of the display window.  Status is sent by the Axe-Fx II.

#### patch types: `AxeToggle` and `AxeMomentary`
- automatic conversion from `Toggle` and `Momentary` patch types if `device` specified in patch is "Axe-Fx" (continue to use 
`Toggle` and `Momentary` patch types if you use use `device="AxeFx"`, otherwise use `AxeToggle` and `AxeMomentary` explicitly)
- effect block bypass CCs are automatically assigned to the Axe-Fx default values if effect name is found (and cc is not explicitly specified)
- attribute `singlestate="1"` used to prevent automatic generation of B group (for example, to use with Volume Increment and Volume Decrement 
assignments in momentary patches - "Axe Volume Increment" in config data file).
- attribute `cc="x"` to define the control change number (or override the default) for a sync patch (required for use with Feedback Return 
bypass assignment - see "Axe Fdbk Return" in config data file).
- attribute `invert="1"` used to invert the A (127) and B (0) groups that are automatically generated.

#### patch type: `AxeFxTapTempo`
- same as `Momentary` but switch LED syncs to specified Axe-Fx device MIDI in port 
(assuming Axe-Fx unit is set to send realtime tempo sysex to its MIDI out)
- no command strings are necessary (if not present, Axe-Fx default is used)

#### metapatch: `SyncAxeFx`
- forces a sync of the bypass and X/Y state for all `AxeToggle` patches that are mapped to Axe-Fx effect blocks
- use this command if you manually change presets on the Axe-Fx front panel to sync up LEDs for effect blocks in mTroll (not necessary for Axe-Fx II firmware version 9+)
- automatically invoked after each program change when using interactive program change engine mode on the channel specified by "AxeFx" device map
- automatically invoked for `AxeToggle` and `AxeMomentary` patch types

#### patch command: `AxeProgramChange`
- similar to `ProgramChange` but does automatic patch name and effect block state sync
- supports program change numbers of 0-383 (via automatic bank select and program number conversion)
- same as standard `ProgramChange` command + `Sleep` command + `SyncAxeFx` metapatch
- note that the Patches in the Axe-Fx config.xml data files do not have names; 
When `AxeProgramChange` command is invoked, a sync will occur and the name of the Axe-Fx patch will be appended to the Main Display window automatically

###  Axe-Fx III Notes
Many of the Axe-Fx and Axe-Fx II notes also apply to Axe-Fx III.
Refer to the Axe-Fx III optimized [axefx3v2.config.xml](../data/axefx3v2.config.xml) config file.
