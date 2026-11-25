import sqlite3
import json
import urllib.request
import urllib.error

from pathlib import Path
from datetime import datetime

from fastapi import FastAPI, HTTPException
from pydantic import BaseModel


app = FastAPI()

# ======================================
# CONFIGURATION
# ======================================

BASE_DIR = Path(__file__).resolve().parent

DATABASE = BASE_DIR / "conversations.db"

OLLAMA_URL = "http://localhost:11434/api/generate"

MODEL_NAME = "qwen2.5:3b"


# ======================================
# DATABASE CONNECTION
# ======================================

def get_connection():

    connection = sqlite3.connect(DATABASE)

    connection.row_factory = sqlite3.Row

    return connection


# ======================================
# SUMMARY TABLE
# ======================================

def initialize_summary_table():

    connection = get_connection()

    cursor = connection.cursor()

    cursor.execute("""
        CREATE TABLE IF NOT EXISTS summaries (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            conversation_id INTEGER NOT NULL UNIQUE,
            summary TEXT NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (conversation_id)
                REFERENCES conversations(id)
        )
    """)

    connection.commit()

    connection.close()


# ======================================
# DEVICE MESSAGE FORMAT
# ======================================

class DeviceMessage(BaseModel):

    device: str
    message: str


# ======================================
# HOME ENDPOINT
# ======================================

@app.get("/")
def home():

    return {
        "status": "online",
        "message": "ESP32 Speech Project Server is running"
    }


# ======================================
# ESP32 MESSAGE ENDPOINT
# ======================================

@app.post("/message")
def receive_message(data: DeviceMessage):

    print("\nMESSAGE RECEIVED FROM ESP32")

    print("Device:", data.device)

    print("Message:", data.message)

    print("Time:", datetime.now())

    return {
        "status": "success",
        "message": "Hello ESP32",
        "received": data.message
    }


# ======================================
# GET ALL CONVERSATIONS
# ======================================

@app.get("/conversations")
def get_conversations():

    connection = get_connection()

    cursor = connection.cursor()

    cursor.execute("""
        SELECT id, title, created_at
        FROM conversations
        ORDER BY id DESC
    """)

    conversations = cursor.fetchall()

    connection.close()

    return {
        "status": "success",
        "count": len(conversations),

        "conversations": [
            {
                "id": row["id"],
                "title": row["title"],
                "created_at": row["created_at"]
            }

            for row in conversations
        ]
    }


# ======================================
# GET SPECIFIC CONVERSATION
# ======================================

@app.get("/conversations/{conversation_id}")
def get_conversation(conversation_id: int):

    connection = get_connection()

    cursor = connection.cursor()

    cursor.execute("""
        SELECT id, title, transcription, created_at
        FROM conversations
        WHERE id = ?
    """, (conversation_id,))

    conversation = cursor.fetchone()

    connection.close()

    if conversation is None:

        raise HTTPException(
            status_code=404,
            detail="Conversation not found"
        )

    return {
        "status": "success",

        "conversation": {
            "id": conversation["id"],
            "title": conversation["title"],
            "transcription": conversation["transcription"],
            "created_at": conversation["created_at"]
        }
    }


# ======================================
# GENERATE AI SUMMARY
# ======================================

def generate_summary(transcription):

    prompt = f"""
You are an AI assistant that summarizes conversations.

Summarize the following transcription.

Instructions:
- Identify the main ideas.
- Include important details.
- Remove unnecessary repetition.
- Use clear and simple language.
- Keep the summary concise.
- Write in the same language as the original transcription.
- Do not invent information.

TRANSCRIPTION:

{transcription}

SUMMARY:
"""

    payload = {
        "model": MODEL_NAME,
        "prompt": prompt,
        "stream": False,
        "options": {
            "temperature": 0.3
        }
    }

    data = json.dumps(payload).encode("utf-8")

    request = urllib.request.Request(
        OLLAMA_URL,
        data=data,
        headers={
            "Content-Type": "application/json"
        },
        method="POST"
    )

    try:

        with urllib.request.urlopen(
            request,
            timeout=300
        ) as response:

            result = json.loads(
                response.read().decode("utf-8")
            )

            return result["response"].strip()

    except urllib.error.URLError as error:

        raise HTTPException(
            status_code=503,
            detail=f"Ollama connection failed: {error}"
        )

    except TimeoutError:

        raise HTTPException(
            status_code=504,
            detail="AI summarization timed out"
        )


# ======================================
# SUMMARIZE CONVERSATION
# ======================================

@app.post("/conversations/{conversation_id}/summarize")
def summarize_conversation(conversation_id: int):

    initialize_summary_table()

    connection = get_connection()

    cursor = connection.cursor()

    # Retrieve transcription

    cursor.execute("""
        SELECT id, title, transcription
        FROM conversations
        WHERE id = ?
    """, (conversation_id,))

    conversation = cursor.fetchone()

    if conversation is None:

        connection.close()

        raise HTTPException(
            status_code=404,
            detail="Conversation not found"
        )

    transcription = conversation["transcription"]

    # Generate summary using Ollama

    print("\nGenerating AI summary...")

    summary = generate_summary(transcription)

    # Save summary to database

    cursor.execute("""
        INSERT INTO summaries (
            conversation_id,
            summary
        )

        VALUES (?, ?)

        ON CONFLICT(conversation_id)
        DO UPDATE SET
            summary = excluded.summary,
            created_at = CURRENT_TIMESTAMP

    """, (conversation_id, summary))

    connection.commit()

    connection.close()

    print("Summary generated successfully!")

    return {
        "status": "success",
        "conversation_id": conversation_id,
        "title": conversation["title"],
        "summary": summary
    }


# ======================================
# RETRIEVE SAVED SUMMARY
# ======================================

@app.get("/conversations/{conversation_id}/summary")
def get_summary(conversation_id: int):

    initialize_summary_table()

    connection = get_connection()

    cursor = connection.cursor()

    cursor.execute("""
        SELECT summary, created_at
        FROM summaries
        WHERE conversation_id = ?
    """, (conversation_id,))

    result = cursor.fetchone()

    connection.close()

    if result is None:

        raise HTTPException(
            status_code=404,
            detail="Summary not found"
        )

    return {
        "status": "success",
        "conversation_id": conversation_id,
        "summary": result["summary"],
        "created_at": result["created_at"]
    }