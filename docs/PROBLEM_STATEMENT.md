# Problem Statement

**Status: adopted 2026-11-19 as the capstone's problem statement — the single source of truth.** The research paper, the MeitY proposal, the patent (IDF-2) and the final report all build on this. If the framing changes, change it here first, then propagate.

**Title:** AI-Driven Multimodal Cybersecurity and Risk Assessment Framework for Critical Information Infrastructure: Real-Time Detection of Authority-Impersonation Attacks on Personnel

## Statement

Attackers increasingly breach critical information infrastructure through people rather than networks. They pose as a senior official, officer, vendor or contractor, over a video call, a voice call, or in person with a forged credential. Then they pressure an employee into a harmful action: granting access, approving a payment, sharing credentials, or letting someone into a facility. Generative AI has made the face, voice and documents in such an interaction cheap to fake in real time, and the targeted employee has no reliable way to check, in the moment, that the person is who they claim to be.

Existing defences miss this gap. Network and endpoint security never see the human conversation. Biometric verification systems compare against a pre-enrolled database, which doesn't cover outsiders and becomes a high-value target itself.

We propose a wearable system that verifies the interaction itself. It analyses the other party's voice, face and presented documents as they reach the employee. It fuses these into a single risk verdict with a recommended action, and alerts both the employee and their security team. It checks only against what is presented in that interaction, never against stored biometrics. The same mechanism protects citizens from "digital arrest" scams, where attackers impersonate the same kinds of authority.

## Who it protects

- **Primary:** government and critical-infrastructure personnel (e-governance, power, banking backbone, PSUs) targeted by authority impersonation.
- **Secondary:** citizens targeted by "digital arrest" scams — the same attack (impersonating police/officials) aimed at the public. Covered by the same system and by the patent; used as a familiar example in demos.

## Scenarios

| Scenario | Example | Modules used |
|---|---|---|
| **Video call** (flagship) | A deepfaked "senior official" orders an urgent transfer or access grant | Face (deepfake) + voice (synthetic speech) |
| **Voice call** | A cloned voice of a known officer asks for credentials | Voice |
| **In person** | A "contractor" at a facility gate presents a forged ID | Document authenticity + face-to-ID match |

## Scope

**In scope:** the human / social-engineering attack surface: verifying that the face, voice and documents of the other party in an interaction are genuine and consistent, and turning that into a verdict and recommended action.

**Out of scope:**
- Network intrusion detection, SCADA/ICS monitoring, endpoint security — conventional CII security that this system complements, not replaces.
- Insiders who really are who they claim to be (an authorised but malicious person passes identity checks by design).
- Judging the request itself (urgency/coercion language) — future work.
- A full security-operations (SOC) dashboard — described in the paper, not built.

## Why a wearable

- Works across every calling platform and phone, with no integration per app.
- Needs no software installed on locked-down government systems.
- Observes only what the employee already sees and hears; never intercepts call data.
- Hands-free, and the same device covers the in-person scenario.

## Design principles (must not regress)

- **Fresh analysis only:** every check compares against what is presented in that interaction, never a stored biometric reference. No biometric database exists to leak, which fits data-minimisation law such as India's DPDP Act 2023.
- **Honest evaluation:** every claimed accuracy comes from the protocol in [EVALUATION_PLAN.md](EVALUATION_PLAN.md), on test sets never used for training.

## What this changes in the build

The detection modules (voice, face, document, face-to-ID) and their evaluation are unchanged — detecting a fake voice works the same whoever is being targeted. The framing adds:

- **Fusion (plan Phase 4):** verdicts become *Trusted / Needs secondary verification / High-risk impersonation / Insufficient evidence*, each with a concrete recommended action ("call back on a known number", "get a second officer's approval").
- **Security layer (plan Phase 5):** threat-intel lookup (known-bad phone numbers, links, document hashes), security-team alert on high-risk events, tamper-evident audit log, login/roles and HTTPS.

## History

- **Proposal / mid-sem (Aug 2026):** framed around protecting citizens from digital-arrest scams; institutional users were future work. The mid-sem report reflects that and is kept as a snapshot.
- **2026-09-13:** the mentor (Dr. Tarunpreet Bhatia) pointed to the MeitY call on AI-driven cybersecurity for critical information infrastructure and asked to reframe toward the government-employee side, suggesting the title above.
- **Late Sept 2026:** Mannat's `PATENT WORK.txt` added the security layer.
- **2026-11-19:** adopted as the capstone's problem statement (not just the paper's), with digital arrest kept as the secondary, citizen-facing application.
