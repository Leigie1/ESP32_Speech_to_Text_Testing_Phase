from faster_whisper import WhisperModel

print("Downloading/loading Whisper Small model...")

model = WhisperModel(
    "small",
    device="cpu",
    compute_type="int8"
)

print("Whisper Small model is ready!")
