# 4Story (Beta) for Mac

Version 0.1 (beta). For Apple Silicon Macs with macOS 14 or newer.

## Install

1. Open **4Story macOS.dmg**.
2. Drag **4Story (Beta)** to **Applications**.
3. Eject the disk image.

## First start

macOS blocks apps that are not from the App Store or a known developer.
This is normal for 4Story.

1. Open your **Applications** folder.
2. Right-click **4Story (Beta)** and choose **Open**.
3. Click **Open** again in the message.

If the message has no **Open** button (newer macOS versions):

1. Click **Done**.
2. Open **System Settings** → **Privacy & Security**.
3. Scroll down. Next to "4Story (Beta) was blocked", click **Open Anyway**.
4. Confirm with your password, then click **Open Anyway** again.

You only do this once.

## F1–F12 keys without Fn

The keyboard button in the game lets you use F1–F12 without holding Fn.
macOS asks for permission the first time.

1. Click the keyboard button and choose **Enable**.
2. In **System Settings** → **Privacy & Security** → **Accessibility**, turn on **4Story (Beta)**.
3. Click the keyboard button again.

If the switch is on but the game still asks:

1. Quit 4Story.
2. In the same **Accessibility** list, select **4Story (Beta)** and click **−**.
3. Open 4Story and do steps 1–3 again.

You may need to do this once after updating from an older beta.

## You must know: the server address

4Story needs a server to play. The game asks for its address the first time.

1. Open this file:
   `~/Library/Application Support/4Story/Settings.plist`
   (In Finder: **Go** menu → **Go to Folder…**, then paste the path.)
2. Next to **ServerAddress**, type the IP address of the Mac that runs the server.
   Ask the server owner for it. Example: `192.168.1.20`
3. Save the file and open 4Story again.

Important: use the server Mac's real IP address.
Do not use `127.0.0.1` or `localhost`. It does not work, even on the server Mac itself.

To find a Mac's IP address: **System Settings** → **Wi-Fi** (or **Network**) → **Details**.

## Settings

All settings are in `Settings.plist`.

| Setting | What it does |
| --- | --- |
| ServerAddress | The server Mac's IP address |
| ServerPort | The server port. Keep 4815 unless told otherwise |
| Channel | The game channel. Keep 1 unless told otherwise |
| WindowedMode | `false`: the game starts fullscreen. `true`: it starts in a window. The game updates this when you switch in game |
| WriteLogFile | `true`: the game writes a log file (helps with bug reports) |

## Check your installation

If the game does not start, run the checker:

1. In **Applications**, right-click **4Story (Beta)** → **Show Package Contents**.
2. Open **Contents** → **Resources** → **tools**.
3. Double-click **Client Checker.command**.

It lists every check with ✔ or ✘ and tells you what to fix.

## Logs and bug reports

The game writes logs here:

- `~/Library/Logs/4Story/4Story.log` (this game session)
- `~/Library/Logs/4Story/4Story.previous.log` (the session before)
- other files in the same folder (graphics and game logs)

To open the folder: in Finder, **Go** → **Go to Folder…**, paste `~/Library/Logs/4Story`.
You can also open the **Console** app and look under **Log Reports**.

If the game crashes, macOS saves a crash report here:
`~/Library/Logs/DiagnosticReports` (files that start with `4Story`).

When you report a bug, please send:

1. What you did, step by step.
2. What you expected, and what happened.
3. `4Story.log` and, after a crash, the newest `4Story` crash report.

## Remove the game

1. Drag **4Story (Beta)** from Applications to the Trash.
2. To also remove settings and logs, delete these folders:
   `~/Library/Application Support/4Story`, `~/Library/Logs/4Story`, `~/Library/Caches/4Story`.

## Notice

The game data in this package belongs to its original owners.
This package is for private, non-commercial use.
