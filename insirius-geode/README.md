# insirius's mod menu (Geode version)

This is the same menu as the standalone PC version, rebuilt as a [Geode](https://geode-sdk.org) mod. Geode does the loading and hooking, so one source tree builds for both **Windows and Android**. On Android it runs through the official Geode launcher on top of your Play Store copy of Geometry Dash, so you don't need a modified APK.

## Getting the .geode file (free, using GitHub)
1. Create a new repository on github.com, for example `insirius-modmenu`.
2. Upload everything in this folder to it. Use **Add file → Upload files** and drag the files in, including the hidden `.github` folder. If the browser hides that folder, create `.github/workflows/build.yml` by hand and paste the contents in.
3. Open the **Actions** tab. A "Build Geode Mod" run starts by itself. You can also click **Run workflow**.
4. When the run finishes (about 10 minutes), download the **Build Output** artifact. It contains `insirius.modmenu.geode`, a single file that works on Windows, Android 32-bit and Android 64-bit.

If a build fails, open the failed job, copy the red error lines, and send them to me.

## Installing
**Android**
1. Install the **Geode launcher** from https://geode-sdk.org. It loads your Play Store copy of Geometry Dash; the game itself is not modified.
2. Open Geometry Dash through the launcher once, so Geode creates its folders.
3. Copy `insirius.modmenu.geode` to `Android/media/com.geode.launcher/game/geode/mods/`.
4. Restart the game. Tap the **insirius** button to open the menu. You can drag the button anywhere.

**PC**
1. Install Geode from https://geode-sdk.org.
2. Copy `insirius.modmenu.geode` into `Geometry Dash/geode/mods/`.
3. Press **Tab** in game. Right-click any toggle to bind it to a key.
4. Delete `XInput1_4.dll` and `insirius.dll` from the GD folder. Those are the old standalone menu, and Geode replaces `XInput1_4.dll` with its own loader.

## Game version
`mod.json` targets **GD 2.2081**, the version Geode currently supports. GD is already at 2.2091, and Geode 5.11.2 only runs on 2.209 in a limited "forward compatibility" mode. When Geode releases full support for the new version:
1. Change both `"2.2081"` entries in `mod.json` to the new version, for example `"2.2091"`.
2. Push the change. GitHub rebuilds the mod for that version automatically.

None of the code uses hardcoded addresses. Everything goes through Geode's named game functions and members, so the code itself doesn't need to change for a new version unless Geode renames something.

## Input buffer
When you click, the menu simulates "click now" on a hidden copy of your player, using the game's real movement and collision code (the same technique trajectory hacks use).
- **Safe clicks** are sent immediately, with no delay.
- **Deadly clicks** (too early for the jump, or the wave would clip a spike) are held back and re-checked every physics tick (240 per second). The click is sent on the first tick where it survives, with the hold length you actually used. "Hold back up to" sets the longest wait.
- **Wave, ship, UFO and swing** are only held back if the click itself is what kills you.
- **Orbs, pads and portals** are left out of the simulation so it can never change the real level. Early clicks in the air are kept held until an orb or a landing uses them.
- **Prediction check** (under Advanced): every few seconds the menu compares a prediction with what really happened and writes the difference to the Geode log.

## Not in this version
- Auto deafen. It sends Discord keystrokes, which only makes sense on Windows.
- The editor's 80,000 object limit. On the standalone version this was a byte patch at a fixed address, which isn't portable.

## Building locally
You need the Geode SDK and CLI: https://docs.geode-sdk.org/getting-started/. Then run `geode build`.
