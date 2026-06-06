# 4LAMP2 (aka *flamp2*)

Here's the GIF for you to see (may take some time to load):
![4LAMP2 Example](4lamp_example.gif)

**An actually-smart lighting controller for ESP32, rewritten from scratch and upgraded to behave like a grown-up system.**

4LAMP2 turns your light **on** when two conditions are met:
1. Your home has working **internet access**
2. Your **phone is physically present** at home (phone responds to local network pings)

No eldritch apps and configs, it's an install once forget forver device.

## "Smart" Devices

If a light:
- needs an app
- needs you to press a button
- needs cloud servers to exist

then it’s not smart. It’s just remote-controlled.

**4LAMP2 is automated.**  
It observes and reacts and that's why i it's so cool.

4LAMP continuously monitors three things:

- **Wi-Fi state** connects, reconnects, and survives drops
- **Internet reachability** via ICMP ping to a public DNS server
- **Phone presence** via ICMP ping to your phone’s local IP

## Hardware
- **ESP32** (C3, but compatibe with any of C and S series)
- External load via GPIO (relay or N-channel MOSFET)
- Onboard LED used for boot indication

Default pins:
- External lamp: **GPIO 7**
- Onboard LED: **GPIO 8**

## Local Presence Detection

Your phone is detected purely by ICMP ping
No need to install shady apps or connect to unknown bluetooth devices.

If your phone stops responding for **45 minutes**, it is considered *gone*.

> ⚠️ iOS devices usually stop responding to pings when the screen is off.  
> Android *may* work better, but hasn’t been tested.

## Internet-Aware Auto Shutoff

If internet connectivity disappears for longer than ~90 seconds:
- The lamp shuts off automatically

This is intentional and should work like that, at least in my house where internet is turned off at night.

To avoid unnecessary network spam, phone pings run aggressively when presence matters:
- After **15 minutes of continuous presence**, phone pings are reduced
- Pings resume automatically when needed again

This keeps the system responsive *without* being noisy.

## Architecture Highlights

- Event-driven design using **FreeRTOS Event Groups**
- Separate logic for:
  - Wi-Fi state
  - Internet reachability
  - Phone presence
- Infinite ping sessions with timeout-based validation
- Safe recovery from Wi-Fi disconnects and IP loss
- really really "hope it works" logic.

**4LAMP2 doesn’t ask you what to do.  
It already knows.**
