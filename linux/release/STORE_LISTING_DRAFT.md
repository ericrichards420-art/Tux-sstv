# Tux-SSTV product listing draft

**Owner review draft — not a published offer.** Finalize the application license,
support contact, refund/update terms and validated installer before taking
payment. The proposed price below is a launch experiment, not evidence of
customer demand.

## Suggested headline

**Tux-SSTV — SSTV and remote voice for your Linux shack**

## Customer description

Bring your home shack to your laptop.

Tux-SSTV combines SSTV picture transmission and reception with radio controls
and remote voice operation over your home network. Prepare a picture, watch
incoming signals on the waterfall, and operate the radio connected to your
shack PC from another room.

Listen to the receiver through your laptop's selected audio output, check your
microphone, and click PTT to start or stop a voice transmission. The same
application runs at the shack and on the laptop.

### Included capabilities

- 43 implemented SSTV modes and automatic receive-mode detection.
- Picture preparation with callsigns, text, templates and emoji.
- Live waterfall, saved receive pictures, and WAV encoding/decoding.
- Guided audio setup, Hamlib radio controls and named radio profiles.
- Home-network remote SSTV, receive audio and click-on/click-off voice PTT.
- Local operation without an account.

Developed by **Eric Richards — KI5BZO**. Eric reports successful 20-meter
operation with his Kenwood TS-990S. Radio features depend on the radio and its
Hamlib backend; the implementation and verification status of each SSTV mode
are documented with the software.

### Requirements to state beside the Buy button

- For the current installer target: **Ubuntu 26.04, 64-bit Intel/AMD (amd64)**.
- A suitable amateur radio and audio interface; CAT hardware if required by
  the chosen radio-control setup.
- For remote operation: Tux-SSTV on both computers, a working home network,
  and the shack PC left running with the radio connected.

Do not label a build as compatible with other distributions or Raspberry Pi
until that target has its own built and checked package.

## Proposed first offer

**Test price: US $39, one-time purchase.**

Proposed scope: one operator's own shack PC and remote laptop, the named
release's installer and quickstart, and a clearly stated support/update policy.
This is a proposal for Eric to approve, not an application license or a promise
already made to customers.

Before publication, replace this section with the chosen license, support
address, refund terms and exact update entitlement. Avoid a lifetime-support
promise unless that is a commitment Eric specifically wants to make.

## Evidence to add before launch

Use a short demonstration showing a received picture, a transmission, and
remote listening/PTT from the living-room laptop. Show the actual supported
Ubuntu version and describe any extra radio/audio hardware. Obtain an install
result from another operator using the intended release package.

The code review and automated tests are engineering evidence. They do not
establish that Tux-SSTV is the best Linux SSTV program, guarantee compatibility
with every radio, or replace an independent on-air interoperability check.
