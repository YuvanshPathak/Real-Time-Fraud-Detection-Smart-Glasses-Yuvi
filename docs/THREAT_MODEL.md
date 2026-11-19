# Threat Model and Success Targets (Layer 1)

**Status: draft, 2026-11-20.** It builds on [PROBLEM_STATEMENT.md](PROBLEM_STATEMENT.md) and feeds [EVALUATION_PLAN.md](EVALUATION_PLAN.md). Items marked *(confirm with mentor)* are team proposals that still need her sign-off.

## 1. What counts as an attack

| Module | Genuine | Attack | Out of scope (for now) |
|---|---|---|---|
| **Voice** | A live human speaking, whatever the channel (call codec, loudspeaker, room, glasses mic) | Synthetic speech: text-to-speech or voice conversion / cloning | **Replay** of a real recording of the official. Through a call it is physically the same as live speech, since every call voice already passes through a speaker. |
| **Face** (calls only) | A real person on camera | **Primary:** deepfakes (face swap, reenactment, fully synthetic face). **Secondary:** presentation attacks (photo or screen held up to the camera). | The in-person scenario: the person is physically present, so face liveness is switched off there. Realistic masks are also out of scope. |
| **Document** | An authentic ID | A tampered ID (edited text fields, swapped photo) or a fully forged one, shown on screen or in hand | **Official letters and orders:** future work. |
| **Face-to-ID** | The live person matches the photo on the ID they present | A genuine ID presented by the wrong person (stolen or borrowed credential) | Identical twins and look-alikes: a stated limitation. |

**Label rule for voice (fixed):** "real human vs. synthetic voice, regardless of channel". Playback through a speaker is never by itself an attack.

## 2. Attacker model

**The attacker can:**
- Use freely available real-time tools: voice cloning from a few seconds of public audio, live face swap in video-call software, editing of ID templates.
- Fully control everything on their side of the call: camera, microphone, background, the documents shown.
- Choose the timing and apply social pressure (urgency, authority).

**The attacker cannot:**
- Tamper with the glasses, the gateway or our backend. Protecting those is the job of the security layer (plan Layer 6), not the detectors.

**Out of scope, stated as limitations:**
- Adaptive attackers who craft inputs specifically to fool our models (adversarial examples).
- Genuine insiders, who really are who they claim to be.
- Judging the request itself (coercion or urgency language).

## 3. Datasets implied by this threat model

| Module | Benchmark (T1) | Real-world / cross-dataset (T2) | Own device (T3) |
|---|---|---|---|
| Voice | ASVspoof 2019 LA | In-the-Wild; Indian sets (Kathbath real + IndicSynth fake, Hindi) | Team speech plus synthetic clips played to the glasses mic |
| Face | FaceForensics++ | Celeb-DF or another cross-dataset set | Team members on video calls, genuine plus face-swapped |
| Document | SIDTD (European ID templates; no public tampered Indian-ID set exists) | Cross-template split of SIDTD | **Mock Indian-style IDs** made by the team: fictional people and a fictional issuing authority, marked SPECIMEN, genuine plus tampered |
| Face-to-ID | LFW (standard face-verification pairs) | Cross-pose / cross-age pairs | Team members against their own and each other's mock IDs |

T3 sets are test-only. Indian voice sets are test-only. IndicSynth's licence is non-commercial.

## 4. Operating policy *(confirm with mentor)*

In this setting the two errors don't cost the same:
- Trusting an impostor can compromise critical systems.
- Doubting a genuine official costs a callback.

So the fused verdict is tuned to rarely say **Trusted** to an attack. Uncertain cases go to **Needs secondary verification** rather than a forced guess.

## 5. Success targets

| What | Target |
|---|---|
| Fused verdict | Attacks reach "Trusted" in ≤ 5% of cases; uncertain cases go to "Needs secondary verification" |
| Voice | **Minimum:** beat the learned-weight heuristic on In-the-Wild (currently 32.0% EER) and stay under 5% EER on ASVspoof LA eval. Also report on T3 own-device recordings. |
| Face, document | Beat both the hand-set heuristic and the off-the-shelf model on the real-world tier, with non-overlapping confidence intervals. Absolute targets are set only after the baselines are measured. |
| Face-to-ID | False match rate (impostor accepted) ≤ 1%, with the false non-match rate reported at that point |
| Latency | Voice verdict within ~2 s of the end of a 4 s clip; full fused verdict within ~10 s of the other party starting to speak |
| Privacy | No stored biometrics; every upload is deleted when its request ends (already enforced) |

## 6. How approaches are compared and chosen *(confirm with mentor)*

Every module climbs the same ladder, and **every rung is reported, including the ones that lose**:

1. Hand-set heuristic (the prototype)
2. The same heuristic features with weights learned from training data
3. Pretrained model, used as-is
4. Fine-tuned model
5. Combinations of the above

The deployed approach for each module is the one that wins on the **real-world and own-device tiers**, a rule fixed before the results are seen. The benchmark tier only checks that our setup reproduces published numbers. A module may end up deploying a learned heuristic if that is what wins.

**Voice, as of 2026-11-20** (In-the-Wild EER; full table in `eval/results/voice.md`):
- hand-set heuristic: 50.5%
- learned weights: **32.0%**
- pretrained AASIST-L: 44.5%
- fine-tuned AASIST-L: 43.8%
- score fusion, learned-weight heuristic + fine-tuned AASIST-L: 39.3%. The combiner was fit on ASVspoof dev, where AASIST is near-perfect, so it leaned on AASIST; it was deliberately not re-tuned on In-the-Wild.

Still to run: AASIST-L from scratch (training) and an SSL front-end (wav2vec2 / XLS-R + AASIST).
