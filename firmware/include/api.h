#ifndef API_H
#define API_H

#include <cstddef>
#include <cstdint>
#include <cmath>

struct VoiceCheckResult {
  bool ok = false;              // true only if the backend returned a usable score
  float voiceLivenessScore = 0.0f;
  bool spoofDetected = false;
};

struct RiskScoreResult {
  bool ok = false;
  float fraudRiskScore = 0.0f;
  const char *riskLevel = "";   // points into a static buffer, valid until next call
};

// POSTs a WAV-encoded PCM buffer to /voice-check as multipart/form-data.
// samples/sampleCount describe raw 16-bit mono PCM; this function builds the
// WAV header itself.
VoiceCheckResult postVoiceCheck(const int16_t *samples, size_t sampleCount, int sampleRate);

// POSTs to /risk-score. voiceLivenessScore is the only modality this firmware
// evaluates this phase — face/doc/face-id are omitted from the JSON body
// entirely (not sent as 0 or 1.0), matching the backend's fusion contract:
// an omitted modality is excluded from the weighted average, not assumed
// genuine or fraudulent.
RiskScoreResult postRiskScore(float voiceLivenessScore);

// Scores for a multi-modality /risk-score call. NAN = not evaluated this
// cycle: the field is omitted from the JSON body (same contract as above).
struct RiskInputs {
  float voiceLivenessScore = NAN;
  float faceLivenessScore = NAN;
  float docAuthenticityScore = NAN;
};
RiskScoreResult postRiskScore(const RiskInputs &inputs);

struct ImageCheckResult {
  bool ok = false;
  float score = 0.0f;
};

// POSTs a JPEG to a backend image endpoint as multipart/form-data and reads
// one score from the JSON reply, e.g.
//   postImageCheck("/face-check", "image", "face_liveness_score", jpg, len)
//   postImageCheck("/document-check", "document", "doc_authenticity_score", jpg, len)
ImageCheckResult postImageCheck(const char *path, const char *fieldName, const char *scoreKey,
                                const uint8_t *jpg, size_t len);

#endif
