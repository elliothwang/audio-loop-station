# Install and run

Audio Loop Station runs on **Windows 10 or 11 (64-bit) only**. It is a single `.exe` with nothing to
install: the C++ runtime is built into it, so you don't need the Visual C++ Redistributable.

## What you need

- A Windows 10 or 11 (64-bit) computer
- A microphone (a built-in laptop mic works)
- Headphones, so the speakers don't bleed into the microphone while you record over a loop

## 1. Download

1. Open the [latest release](https://github.com/elliothwang/audio-loop-station/releases/latest).
2. Under **Assets**, download `AudioLoopStation.exe`.
3. If your browser warns that the file isn't commonly downloaded, choose **Keep**.

## 2. Run it

1. Double-click `AudioLoopStation.exe`.
2. The exe isn't signed, so Windows may show **Windows protected your PC**. Click **More info**, then
   **Run anyway**. Windows only asks the first time.

The window opens with four empty tracks:

![The main window on first launch, with four empty tracks](images/main-window-empty.png)

## 3. Check the audio device

The app uses your Windows default microphone and speakers. The bar along the bottom shows the device
it opened, its sample rate, its buffer size, and the round-trip latency:

![The footer showing the audio device, sample rate, buffer size, and latency](images/footer-device-info.png)

If it says **No audio device**, or recording stays silent:

1. **Allow microphone access.** Open **Settings > Privacy & security > Microphone**, and turn on both
   **Microphone access** and **Let desktop apps access your microphone**.
2. **Pick your devices.** Open **Settings > System > Sound** and choose the **Output** and **Input**
   devices you want.
3. **Restart the app**, so it opens the new defaults.

## Uninstall

Delete `AudioLoopStation.exe`. Saved loops are kept in `Documents\Audio Loop Station\Loops`; delete
that folder too if you don't want them.

## Troubleshooting

| Problem | Fix |
|---|---|
| "No audio device" in the footer, or **Record failed** with "No audio device is running." | Follow step 3 above, then restart the app. |
| Recording is silent | Check microphone access and the default **Input** device (step 3). |
| You hear yourself or the loop twice in a new take | Use headphones, so the speakers don't reach the microphone. |
| **Load failed** with "This loop was saved at … Hz, but the audio device runs at … Hz." | Loops only load at the sample rate they were saved at. Change the device's rate in **Settings > System > Sound**, under the device's properties, then restart the app. |
| Windows won't run the exe at all | Some managed computers block unsigned downloads. Try a computer you control. |

Next, see the [user guide](user-guide.md) for a step-by-step tour.
