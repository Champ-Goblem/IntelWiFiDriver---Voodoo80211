# IntelWiFiDriver *WIP*
Current progress: ~= 15-20%

For now do not worry about trying to run this, as it is nowhere near finished yet!

When ready a series of testing phases will be made available.

## Basis:
This work is based mainly upon the [Voodoo80211](https://github.com/mercurysquad/Voodoo80211) project created by mercurysquad as it contained the best codebase to work off of and should end up integrating into Mac OS properly.
I had inspirtion from [Black80211](https://github.com/rpeshkov/black80211).
Also contributing some components to it from the linux side is [AppleIntelWiFiMVM](https://github.com/ammulder/AppleIntelWiFiMVM), this has provided the linux header files found under [net80211/wpi/iwlwifi_headers/](net80211/wpi/iwlwifi_headers)
The rest is based upon work already implemented in linux in the [iwlwifi source code](https://git.kernel.org/pub/scm/linux/kernel/git/iwlwifi/iwlwifi-fixes.git) and my own work to port it to Mac OS.

## Building from source:
Open the project in Xcode and build the project using `cmd+B`.
Now navigate to the directory that net80211.kext is in and in a new terminal window run:
```
sudo chown -R root:wheel net80211.kext
sudo kextload -v 6 net80211.kext
```
And check check `console.app` for any problems
or try:
```
sudo chown -R root:wheel net80211.kext
sudo kextutil net80211.kext
```
to get error printed to the console.

## Issues
Any issues please check the `Issues` tab, as usual create a new issue if there isnt something similar and provide the output from `console.app` filtering by `net80211` so as not to get a full system log.
Please list your hardware as well and any steps that happened previous that could be a cause for concern.

## Not Recommended
Please do *NOT* install this to `/Library/Extensions` or `/System/Library/Extensions` as it may irreversably damage your system and will need to be thourghly tested before release.
Updates on release will follow shortly.

## Current Known Issues
