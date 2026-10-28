# 4Story for Mac — version 0.1 (beta)

This is a port of the 4Story 5.0 game to Mac.
It has two parts: the game for players, and a server that you run yourself.

Both parts need a Mac with Apple silicon (M1 or later) and macOS 14 or newer.

## What is in this folder

| What | For whom | What to do |
| --- | --- | --- |
| **4Story macOS.dmg** | Every player | Open it and drag **4Story (Beta)** to **Applications** |
| **Server** folder | One person who runs the server | Read **Server/README.md** |
| **SHA256SUMS.txt** | Anyone | Checks that the disk image downloaded correctly |

## Players

1. Install the game from **4Story macOS.dmg**.
2. Ask the server owner for two things:
   - the server address,
   - your account name and password.
3. Open the game and follow its instructions.
   The game has a README with all the steps: click **View README** when the game asks for the server address.

You cannot create an account in the game. The server owner creates it for you.

## Server owner

1. Read **Server/README.md**.
2. Run **Install.command**, then **Start.command**.
3. Create an account for each player with **Create Account.command**.
4. Give each player the server address and their account.

## Good to know

- **Game master:** the first character created on a server becomes a game master (GM).
  This happens the next time the server starts. Create your own character first.
- **Accounts:** only the server owner can create accounts.
- **Beta:** this is a test version. Expect problems. Save the logs when you report one (both READMEs explain where they are).
- **Apple warnings:** the game and the server are not signed by Apple.
  The first time you open them, right-click and choose **Open**.
  On newer macOS versions, use **System Settings** → **Privacy & Security** → **Open Anyway**.
  Both READMEs show the steps.

## Check the download

In Terminal, in this folder:

```
shasum -a 256 -c SHA256SUMS.txt
```

It prints `4Story macOS.dmg: OK` when the file is complete.
The Server folder has its own **SHA256SUMS.txt** for its files.
