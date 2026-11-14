# AmigaOS 3.x client — experimental

Build with `make amiga`, or `make -C clients/amiga AMIGA_TOOLS=/path/to/toolchain CPU=68020`. The default shared compiler is `/Users/frossmant/Development/tools/amiga/m68k-amigaos-gcc-16.2/bin/m68k-amigaos-gcc`. No extra utilities were needed. The executable is `clients/amiga/LineClash` (Amiga Hunk, libnix, no ixemul, no FPU).

## Runtime

Use a 68020 and adequate Fast RAM; the test profile has 2 MB Chip plus 8 MB Fast. AmigaOS 3.x, graphics/intuition/keymap libraries and Roadshow/AmiTCP/Miami's bsdsocket.library are required. HTTPS public discovery also needs AmiSSL 4.10+ with a valid certificate store, `AmiSSL:` assigned, and AmiSSL libraries in `LIBS:`. Set the system date correctly. TLS is used for directory discovery and native account sign-in; account creation is native and email password recovery uses the HTTPS website when its operator has configured SMTP. Without SMTP, recovery is disabled. Registration signs in immediately without a recovery-code screen. The game stream remains plain TCP.

Keep the executable named `LineClash` in its drawer. It launches itself as an asynchronous directory worker using temporary files in `T:`. Preferences and server entries are stored beside it. No account password is passed to that worker. The native frontend currently has no audio playback or modern gamepad integration. Gameplay/runtime verification is still in progress; compilation is not evidence of hardware compatibility.

## Shared OpenFP emulator workflow

Build into the shared `Builds:` drawer:

```sh
make amiga
mkdir -p /Users/frossmant/Development/builds/LineClash/AmigaOS
cp clients/amiga/LineClash /Users/frossmant/Development/builds/LineClash/AmigaOS/LineClash
python3 /Users/frossmant/Development/tools/amiga/bin/amiga-machine a1200-020 LineClash/AmigaOS/LineClash --bridge en0
```

This uses the custom `tools/amiga/fs-uae-3.2.35-pcap/bin/fs-uae`, the shared AmigaOS 3.2.3 machine profile, A2065, Roadshow and vmnet-helper. Close an existing machine first: the profiles share the guest system volume. `en0` is this Mac's active Wi-Fi bridge interface. For a VPN/tunnel test, the same launcher supports `--nat`, but that does not test LAN broadcast discovery.

The bridge helper starts successfully in the current environment. The guest log confirms bsdsocket.library, timer.device and the Intuition window open. `Builds:LineClash/AmigaOS/LineClash.log` records native startup and discovery status. Full network gameplay, display/input review and audio remain unverified or incomplete. Do not launch the unrelated `/Applications/FS-UAE.app` when checking this build.

The Account screen supports one remembered username/email, native registration, password login and PIN sign-in through AmiSSL. Choose **Sign in with a PIN**, open `https://lineclash.kladhest.se/account/pin/` on another computer or phone, enter the displayed eight-digit PIN, sign in, then approve the client. It polls over HTTPS every five seconds until approved or expired; gameplay remains ordinary TCP. Cancel with Escape or **Use username / password**. This lets you authenticate without typing the central password on the Amiga. If the website requires activation after registering, confirm the emailed link in a browser, then sign in. **Activation help** displays the resend URL. Pending accounts cannot use password or PIN login until confirmed, unless the operator disables the requirement. Passwords and sessions are exchanged with a separate native worker using public-memory Exec messages; they are never written to temporary files or command-line arguments. Sessions stay in memory and are reused over HTTPS for public-server joins. TLS is used for account/directory requests, while the game stream remains ordinary TCP. This path cross-compiles; guest networking and hardware runtime checks are still required.
