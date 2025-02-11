# Pet Your Dolphin
The **Pet Your Dolphin** app for the Flipper Zero let's you check in your dolphin buddy daily and give them some well-deserved pets. Petting your dolphin improves its mood and provides some XP but these effects are capped by the firmware so you're limited to two pets a day.

## Installing the App
You can install the app on your Flipper by simply downloading the latest [release](https://github.com/dwight9339/pet_your_dolphin/releases) and loading the `pet_your_dolphin.fap` file onto your Flipper's SD card using [qFlipper](https://flipperzero.one/update) or the [Flipper Labs](https://lab.flipper.net/) browser utility.

## Building the App
The app can be built using [FBT](https://github.com/flipperdevices/flipperzero-firmware/blob/dev/documentation/fbt.md). 

1. Clone this repo and the Flipper [firmware](https://github.com/flipperdevices/flipperzero-firmware/) repo.
2. Copy this repo and all of its contents into the `/applications_user/` directory of the firmware source.
3. Navigate to the root of the firmware source and run `.fbt fap_pet_your_dolphin`.
4. The build output can be found in the `build/f7-firmware-D/.extapps/` directory of the firmware source and uploaded to your Flipper as described in the previous section.

You might be able to build the app with [uFBT](https://github.com/flipperdevices/flipperzero-ufbt) as well. Idk, I haven't checked.

## Contributing
If you want to contribute, you can clone the repo and open a pull request. Don't be shy!
