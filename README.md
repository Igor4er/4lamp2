# 4LAMP2 (aka *flamp2*)

![4LAMP2 Example](4lamp_example.gif)

**An actually-smart lighting controller for ESP32, rewritten from scratch and upgraded to behave like a grown-up system.**

4LAMP2 turns your light **on only when two conditions are met**:
1. Your home has working internet access
2. *You* are physically present (your phone responds to local network pings)

No apps. No buttons. No rituals.  
Just logic.

## 🔧 What It Does

4LAMP2 continuously monitors three things:

- **Wi-Fi state** — connects, reconnects, and survives drops
- **Internet reachability** — via ICMP ping to a public DNS server
- **Phone presence** — via ICMP ping to your phone’s local IP

The external lamp is turned **ON** only when:
- Internet is reachable  
- Your phone is reachable  

If *either* condition fails — the light turns **OFF**.

## Hardware

- **ESP32**
- External load via GPIO (relay or N-channel MOSFET)
- Onboard LED used for boot indication

Default pins:
- External lamp: **GPIO 7**
- Onboard LED: **GPIO 8**

## Presence Detection (No Cloud, No Tracking)

Your phone is detected purely by:
- Local network ICMP ping
- No apps
- No Bluetooth
- No vendor lock-in

If your phone stops responding for **45 minutes**, it is considered *gone*.

> ⚠️ iOS devices usually stop responding to pings when the screen is off.  
> Android *may* work better, but hasn’t been tested.

## Internet-Aware Auto Shutoff

If internet connectivity disappears for longer than ~90 seconds:
- The lamp shuts off automatically

This is intentional.

Some houses become *unlivable* at night when the internet is down and a bright light stays on forever.  
4LAMP2 respects sleep.

## ⚙️ Smart Ping Management

To avoid unnecessary network spam:

- Phone pings run aggressively when presence matters
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

## "Smart" Devices

If a light:
- needs an app
- needs you to press a button
- needs cloud servers to exist

…it’s not smart.  
It’s just remote-controlled.

**4LAMP2 is automated.**  
It observes reality and reacts.

That’s the whole point.

**4LAMP2 doesn’t ask you what to do.  
It already knows.**
