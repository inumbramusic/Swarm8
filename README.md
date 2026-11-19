# Swarm8: a Swarmatron-style VST3 for Cubase

Swarm8 is built around the same idea as the Dewtron Swarmatron:

- **8 sawtooth oscillators all play one pitch.** A **Swarm** control fans them apart into a cluster and pulls them back together.
- **Two ribbons.** One plays continuous pitch with no keys. The other controls the swarm.
- **Two resonant 4-pole ladder filters**, each with its own envelope. Split routing sends the odd oscillators to Filter A and the even ones to Filter B, panned across the stereo field.
- Analog drift, a VCA envelope, an LFO and a large built-in reverb.

It's mono and legato like the original. It has 59 automatable parameters and 23 factory presets.

### Oscillators: classic first, options after
The default is the **Saw**, which is what the original uses. The Init preset and the first 14 presets are pure Saw. The **OSC** section adds:

| Wave | Shape knob controls |
|---|---|
| **Saw** (classic) | — |
| **Pulse** | pulse width (LFO>Shape gives PWM) |
| **Triangle** | — |
| **Sine** | — (the swarm turns into slow beating) |
| **Sync** | hard-sync ratio, 1× to 8× (the classic sync sweep) |
| **Fold** | wavefold depth, from soft to snarling |
| **FM** | FM index, from pure to metallic |

- **Wave 1 / Wave 2 + Wave Mix** blends two waveforms in every oscillator. With Wave Mix at 0%, you hear only Wave 1.
- **Split Waves** sends Wave 1 to the even oscillators and Wave 2 to the odd ones. With Routing set to Split, Filter A gets one waveform and Filter B gets the other, left against right.
- **Sub Osc** adds a square wave one octave below the root. It isn't swarmed, so it anchors the low end while the cluster moves.
- **Noise** adds noise for risers and wind. You can control it with MIDI CC (default mappings plus MIDI Learn) and over OSC from an iPhone or iPad.

---

## 1. Build it

You have two options.

### Option A: GitHub builds it for you (no Xcode needed)
1. Create a new **private** GitHub repo and upload this whole folder, including the hidden `.github` folder. In Finder, press Cmd+Shift+. to show hidden folders before you drag the files into GitHub.
2. Open the repo's **Actions** tab. The "Build macOS" job runs automatically and takes about 10 minutes.
3. Open the finished run and download **Swarm8-mac** from *Artifacts*. Unzip it to get `Swarm8-VST3-mac.zip` and `Swarm8-Standalone-mac.zip`.
4. Unzip the VST3 and move `Swarm8.vst3` to `~/Library/Audio/Plug-Ins/VST3/`.
5. macOS quarantines downloaded plugins, so run this once in Terminal:
   `xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Swarm8.vst3`

### Option B: Build it on your Mac
1. Install the Xcode Command Line Tools (`xcode-select --install`) and CMake (from cmake.org or `brew install cmake`).
2. In Terminal, from this folder, run:
   ```
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build -j8
   ```
   The first build downloads JUCE and takes a few minutes. When it finishes, the plugin is copied to `~/Library/Audio/Plug-Ins/VST3/Swarm8.vst3` automatically.
   If an older Xcode complains about `arm64`, add `-DCMAKE_OSX_ARCHITECTURES=x86_64` to the first command.

### In Cubase
- If Swarm8 doesn't show up, open Studio → VST Plug-in Manager and rescan.
- Add an Instrument Track and choose **Swarm8**. The factory presets appear in Cubase's program list.
- Save your own sounds as `.vstpreset` files through Cubase's preset browser at the top of the plugin window.

---

## 2. Playing it

Every knob name in the plugin window has a **number** next to it if it's mapped to a CC. **Click any knob's name** (or right-click the knob) to MIDI Learn or clear its CC. Double-click a knob to reset it.

| Control | Default MIDI |
|---|---|
| **Swarm** | **CC1 (mod wheel)** and **aftertouch** (amount set by Pressure>Swarm) |
| Shape | CC22 |
| Wave Mix | CC23 |
| Glide | CC5 |
| Volume | CC7 |
| Pitch Ribbon position | CC16 |
| Ribbon Touch (plays with no keys) | CC17 (64 or above = on) |
| Analog Drift | CC18 |
| Reverb | CC19 |
| Swarm Range | CC20 |
| Swarm Lag | CC21 |
| Resonance A / B | CC71 / CC76 |
| Cutoff A / B | CC74 / CC75 |
| Amp Attack / Release | CC73 / CC72 |
| Pitch bend | ± Bend Range |

**Cubase Quick Controls** are another way to map hardware. Every parameter is automatable, so you can assign Swarm, the cutoffs and so on to the 8 Track Quick Controls. Your controller then follows the track, and the mapping is saved with the project.

### Tips
- **Swarm Lag** is the most important knob. With a long lag (2–8 s), moving the swarm makes the oscillators drift apart slowly instead of jumping. That slow pull is the Swarmatron sound.
- **Swarm Mode** sets the spread pattern. *Center* keeps one oscillator on the root and spreads the rest above and below. *Up* and *Down* fan the oscillators in one direction. *Scatter* places them unevenly, which sounds more chordal. **Spread Curve** controls whether the oscillators bunch near the root or push outward.
- **Swarm Range** sets how wide the full swarm is: about 0.3 st for chorus, 12 st for the classic sound, 24 or more for chaos.
- For film-score style parts, hold one low note, keep the mod wheel down, then over 4–8 bars sweep it up and back. Automating Swarm in Cubase works just as well.
- **Routing → Series** passes the sound through both filters for an 8-pole response. With Resonance A turned up, it screams.
- **Drone** keeps the VCA open. Use the ribbon or keys to move the pitch and the filter envelopes to add shape.

---

## 3. Control from an iPhone or iPad

### Option 1: OSC (best for the ribbons)
Use **TouchOSC** or any app that sends OSC.
1. The top-right corner of the plugin window shows your Mac's IP address and the OSC port (default **9000**; click the number to change it). Your iPad and Mac must be on the same Wi-Fi network.
2. In TouchOSC, add an OSC UDP connection with the Mac's IP as the host and **9000** as the send port.
3. Give your controls these addresses (values range from 0 to 1):

| Address | What it does |
|---|---|
| `/swarm/xy` (2 values: x, y) | **XY pad**: x is the pitch ribbon, y is Swarm. This is the Swarmatron in one finger. |
| `/swarm/touch` (1 or 0) | Ribbon gate: send 1 when your finger touches down and 0 when it lifts. Set this as the XY pad's touch message. |
| `/swarm/<id>` | Any parameter, for example `/swarm/cutA` or `/swarm/swLag`. |
| `/swarm/next`, `/swarm/prev`, `/swarm/preset N` | Preset changes. |

Parameter IDs: `tune fine glide bend ribBase ribRange legato ribbon ribGate swarm swRange swMode swCurve swLag oscs drift atSwarm wave1 wave2 waveMix altWaves shape shapeLfo sub noise route drive keyTrack velFilt cutA resA envA cutB resB envB atkA decA susA relA atkB decB susB relB atk dec sus rel drone velAmp lfoRate lfoShape lfoPitch lfoSwarm lfoCut width revMix revSize revDamp volume`

You can also right-click any knob in the plugin to see its OSC address.
- If the swarm feels upside-down, invert the XY pad's Y axis in TouchOSC.
- The first time, macOS may ask whether Cubase can accept incoming network connections. Click **Allow**.
- Only one Swarm8 instance can use a given port. Give a second instance a different port.

### Option 2: Network MIDI (works with any MIDI controller app)
1. Open Audio MIDI Setup → Window → Show MIDI Studio → Network. Create a session and enable it.
2. Connect from the iPad app. The session appears in Cubase as a MIDI input.
3. Send the CCs listed in the table above, for example CC16/CC17 for the ribbon and CC1 for Swarm.

The **Standalone app** (`Swarm8.app`, built alongside the VST3) is useful for testing iPad control without opening Cubase.

---

## Files
- `Source/Params.h`: every parameter, its range, default and default CC.
- `Source/SwarmEngine.h`: the DSP (oscillators, swarm, ladder filters, envelopes, LFO, reverb).
- `Source/Presets.h`: the factory presets. Add your own here.
- `Source/PluginProcessor.*`: the VST3 wrapper, MIDI Learn, OSC and state saving.
- `Source/PluginEditor.*`: the plugin window.
- `tools/render_demo.cpp`: renders all the presets to a WAV without a DAW.

Built with JUCE 8. Personal use is fine. If you ever distribute or sell the plugin, read JUCE's license terms first.
