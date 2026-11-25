import os
import time

from faster_whisper import WhisperModel

# =====================================
# CONFIGURATION
# =====================================

MODEL_SIZE = "small"

AUDIO_FILE = "recording.wav"

OUTPUT_FILE = "transcription.txt"

# =====================================
# LOAD MODEL
# =====================================

print("\n==============================")
print("     SPEECH-TO-TEXT TESTER")
print("==============================")

print("\nLoading Whisper model...")

model = WhisperModel(
    MODEL_SIZE,
    device="cpu",
    compute_type="int8"
)

print("Model loaded successfully!")

# =====================================
# CHECK AUDIO FILE
# =====================================

if not os.path.exists(AUDIO_FILE):

    print("\nERROR: Audio file not found!")

    print("Expected file:", AUDIO_FILE)

    exit()

# =====================================
# TRANSCRIBE AUDIO
# =====================================

print("\nProcessing audio...")
print("Please wait...\n")

start_time = time.time()

segments, info = model.transcribe(
    AUDIO_FILE,
    beam_size=5
)

# =====================================
# COLLECT TRANSCRIPTION
# =====================================

transcription = []

print("==============================")
print("       TRANSCRIPTION")
print("==============================\n")

for segment in segments:

    text = segment.text.strip()

    transcription.append(text)

    print(text)

# =====================================
# SAVE TRANSCRIPTION
# =====================================

final_text = " ".join(transcription)

with open(OUTPUT_FILE, "w", encoding="utf-8") as file:

    file.write(final_text)

# =====================================
# PERFORMANCE INFORMATION
# =====================================

end_time = time.time()

processing_time = end_time - start_time

print("\n==============================")
print("       PROCESSING INFO")
print("==============================")

print("Detected language:", info.language)

print("Language probability:", round(
    info.language_probability, 4
))

print("Audio duration:", round(
    info.duration, 2
), "seconds")

print("Processing time:", round(
    processing_time, 2
), "seconds")

print("\nTranscription saved to:", OUTPUT_FILE)

print("==============================")
