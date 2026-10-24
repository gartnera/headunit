# Unofficial Mazda Connect (tm) (*) Android Auto Headunit App

Unofficial port of Android Auto Headunit App to Mazda Connect CMU. The app makes extensive use of jni functions that were originally developed by Mike Reid as part of his Android app. The Mazda specific source code is under the /mazda folder.

(*) - Mazda and Mazda Connect are trademarks of Mazda NA

The goal of this fork is to update the code to modernize it, and to use libprotobuf instead of manually parsing the messages so that it will be easier to add more complete integration with car systems.

## Configuration

Settings are stored in `headunit.json`; see [the Mazda defaults](mazda/headunit.json).
The Mazda app reads `/tmp/root/headunit.json` at runtime. Use JSON booleans
(`true`/`false`, without quotes) except for `phoneIpAddress`, which is a string.
Omitted options use the defaults below at startup.

| Option | Default | Description |
| --- | --- | --- |
| `launchOnDevice` | `true` | Accepted and stored, but currently unused by the app; changing it has no effect on automatic launch. |
| `carGPS` | `true` | Send the Mazda's GPS data to the phone. Set to `false` to stop sending vehicle location updates. |
| `wifiTransport` | `false` | Use Wi-Fi/TCP instead of USB for the phone connection. Requires an existing network connection and a phone running the Android Auto headunit server. |
| `phoneIpAddress` | `"192.168.43.1"` | Phone IPv4 address used with `wifiTransport: true`. |
| `reverseGPS` | `false` | Correct reversed GPS heading by adding 180 degrees when the navigation SD card content directory (`/tmp/mnt/sd_nav/content`) exists. Applies to vehicle GPS updates. |
| `disableTouchscreen` | `false` | Omit the touchscreen capability from the phone's service-discovery response and discard touchscreen events. Buttons and the commander knob remain available. |

Restart the headunit app after editing the file and reconnect the phone to apply
the settings and refresh the advertised capabilities. Enabling `disableTouchscreen`
through the configuration API blocks touch events immediately; reconnecting is
still required to update the capabilities on the phone.
