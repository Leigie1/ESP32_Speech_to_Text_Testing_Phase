import sqlite3

DATABASE = "conversations.db"


# Initialize database
def initialize_database():
    connection = sqlite3.connect(DATABASE)
    cursor = connection.cursor()

    cursor.execute("""
        CREATE TABLE IF NOT EXISTS conversations (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            title TEXT NOT NULL,
            transcription TEXT NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    """)

    connection.commit()
    connection.close()

    print("Database initialized successfully!")


# Save a transcription
def save_conversation():
    title = input("\nEnter conversation title: ")
    filename = input("Enter transcription filename: ")

    try:
        with open(filename, "r", encoding="utf-8") as file:
            transcription = file.read().strip()

    except FileNotFoundError:
        print("File not found!")
        return

    if not transcription:
        print("The transcription file is empty!")
        return

    connection = sqlite3.connect(DATABASE)
    cursor = connection.cursor()

    cursor.execute("""
        INSERT INTO conversations (title, transcription)
        VALUES (?, ?)
    """, (title, transcription))

    connection.commit()

    print("\nConversation saved successfully!")
    print("Assigned ID:", cursor.lastrowid)

    connection.close()


# View all conversations
def view_conversations():
    connection = sqlite3.connect(DATABASE)
    cursor = connection.cursor()

    cursor.execute("""
        SELECT id, title, created_at
        FROM conversations
        ORDER BY id DESC
    """)

    conversations = cursor.fetchall()

    print("\n========== CONVERSATION HISTORY ==========")

    if not conversations:
        print("No conversations found.")

    else:
        for conversation in conversations:
            print("\nID:", conversation[0])
            print("Title:", conversation[1])
            print("Date:", conversation[2])

    connection.close()


# View a specific conversation
def view_conversation():
    conversation_id = input("\nEnter conversation ID: ")

    connection = sqlite3.connect(DATABASE)
    cursor = connection.cursor()

    cursor.execute("""
        SELECT * FROM conversations WHERE id = ?
    """, (conversation_id,))

    conversation = cursor.fetchone()

    if conversation:
        print("\n========== CONVERSATION ==========")
        print("ID:", conversation[0])
        print("Title:", conversation[1])
        print("Date:", conversation[3])

        print("\nTranscription:")
        print(conversation[2])

    else:
        print("Conversation not found.")

    connection.close()


# Delete a conversation
def delete_conversation():
    conversation_id = input("\nEnter conversation ID to delete: ")

    connection = sqlite3.connect(DATABASE)
    cursor = connection.cursor()

    cursor.execute("""
        DELETE FROM conversations WHERE id = ?
    """, (conversation_id,))

    if cursor.rowcount > 0:
        print("Conversation deleted successfully!")

    else:
        print("Conversation not found.")

    connection.commit()
    connection.close()


# Main menu
def main():
    initialize_database()

    while True:
        print("\n================================")
        print("       DATABASE MANAGER")
        print("================================")
        print("1. Save transcription")
        print("2. View all conversations")
        print("3. View a conversation")
        print("4. Delete a conversation")
        print("5. Exit")

        choice = input("\nSelect an option: ")

        if choice == "1":
            save_conversation()

        elif choice == "2":
            view_conversations()

        elif choice == "3":
            view_conversation()

        elif choice == "4":
            delete_conversation()

        elif choice == "5":
            print("Exiting database manager...")
            break

        else:
            print("Invalid option!")


if __name__ == "__main__":
    main()
