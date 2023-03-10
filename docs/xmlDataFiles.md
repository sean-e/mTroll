# mTroll MIDI controller XML Data Files

I think these files are pretty straightforward.  
If anything can't be deduced from the example files and aren't explained below or on 
the [docs/usage page](docs.md), open an issue.

## *.ui.xml ([example](../data/testdata.ui.xml))
ui.xml files define the GUI of the application.

Item position can be specified via top and left coordinates.  You can also
specify relative positions which makes it easier to position a lot of elements.  
`incrementalVpos` adjusts the top coordinate relative to the previous sibling 
element.  `incrementalHpos` adjusts the left coordinate	relative to the 
previous element.  Do not specify both relative and absolute positions for the same 
dimension (`incrementalVpos` and `top`, or `incrementalHpos` 
and `left`); mixing relative and absolute is fine for different dimensions 
(`incrementalVpos` and `left`, or `incrementalHpos` 
and `top`).

The `SwitchMappings` section is required if using the software with monome-based hardware.
It is used to associate on screen switch displays with monome switches (the software uses ordinal 
based switch identifiers that can be mapped to any monome switch identified by a `row` and 
`column` coordinate).
		
The `switchAssemblyConfig` section is required for the `switchAssemblies` 
section to load; it defines global properties for the switchAssemblies.

A `switchAssembly` may contain a `switch`, `switchTextDisplay` 
and `switchLED`; none of these items are required.

## *.config.xml ([example](../data/testdata.config.xml) or [with XSL](../data/testdataWithXsl.config.xml))
config.xml files define the banks, patches and expression pedal settings.

The `DeviceChannelMap` section is optional and is used to map text names to MIDI 
channels.  This makes it easier to read commands and makes it easier to update commands without
editing them when you move a device from one channel to another.  Any patch or pedal command 
that uses a channel attribute also supports a device attribute.  The text specified in a 
command device attribute must have been defined/mapped in the `DeviceChannelMap`.  
The map is required for Axe-Fx synchronization (which itself is dependent upon the device name 
being either "AxeFx" or "Axe-Fx").

The `SystemConfig`|`switches` section should contain a `switch` 
entry for each of the three operating switches.  The `id` attributes in these entries 
correlate to the `number` attributes of switchAssemblies in the ui.xml file.

The `SystemConfig`|`midiDevices` section contains a `midiDevice` 
entry for each MIDI output device (MIDI out port on the computer) referenced by the rest of the 
file.  If your computer only has one MIDI out, then there will only be one `midiDevice`
entry.  The `port` attribute is required and can be any number; it is used as an alias 
in the patch definitions.  The `out` attribute is optional and is the name of your MIDI
out device as reported by your OS.  If the `out` attribute is not specified, then the
`outIdx` attribute is required and corresponds to the 
zero-based system assigned device index (0 is typically the default MIDI out mapper on Windows).  
The `activityIndicatorId` is optional and specifies the number of a 
`switchAssemby` which contains an LED that should be used to indicate MIDI out 
activity (the corresponding switch assemby need only contain a `switchLED`).  If you move
the config file to another computer that has different MIDI out capabilities, you only
need to edit the `midiDevice` entry to map the port to the new device index (you don't need
to edit every single patch; they are isolated from the actual index by way of the port alias).

The `SystemConfig`|`expression` section contains up to 4 `adc` 
entries and up to 8 `globalExpr` entries.

The `adc` entries map to each of the monome adc outputs.  The adcs can be explicitly 
enabled or 	disabled.  If an adc port is not explicitly disabled, it will be automatically enabled 
if there are any `globalExpr` or `localExpr` settings for that port defined 
in the file.  The monome adcs have a range of 0 - 1023.  Calibrate your pedals by specifying 
different minimums and maximums.

The `globalExpr` entries define the MIDI data that is sent in response to adc 
changes on a global basis.  There can be a maximum of two settings (`assignmentNumber` 
1 or 2) per adc port.  Select the MIDI port used for the global expression definitions by using 
the `port` attribute on the `expression` node.

Patches can have up to 8 `localExpr` entries (up to 2 assignments for each of the 4 ports) 
and are configured identically to the `globalExpr` entries in the 
`SystemConfig`|`expression` section.

By default, the global `globalExpr` settings are active for all patches.  The 
`globalExpr` settings can be defeated on a patch by patch basis through the use 
of a truncated `globalExpr` entry within	the `patch` definition (and 
setting `enable="0"`).  The `globalExpr` settings are defeated by adc port;
it is not possible to defeat only one of two assignments made to an adc port.  A 
`patch` can have a maximum of 4 `globalExpr` defeating entries 
(one for each adc port).

The `LedPresetColors` section optionally overrides any of the 32 predefined color slots.

The `LedDefaultColors` section defines colors for devices or categories of patches.  Assignment of 
colors to patches occurs via lookup/matching in this order (where `type` could be patch or command type): 
- `device+type`
- `Axe-Fx type`
- `device`
- `type`

`LedDefaultColors` are used if a patch does not explicitly set its own color attributes.  That is, 
by default, patches do not need to set their own color.  mTroll will map either the patch type or device
used in the patch to a color defined in `LedDefaultColors`.  Patches can explicitly set their own color
directly or via `LedPresetColors` preset slot.
- Example direct color attributes: `ledColor="00000f" ledInactiveColor="000004"`
- Example preset color attributes: `ledColorPreset="1" ledInactiveColorPreset="30"`
