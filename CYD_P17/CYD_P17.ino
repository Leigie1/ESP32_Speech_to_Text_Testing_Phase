#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>
#include <vector>

// ======================================
// WIFI CONFIGURATION
// ======================================

const char* WIFI_SSID = "Raphael's A56";
const char* WIFI_PASSWORD = "1223456789";

const char* SERVER_URL = "http://10.237.106.160:8000";

// ======================================
// DISPLAY AND TOUCHSCREEN
// ======================================

TFT_eSPI tft = TFT_eSPI();

SPIClass touchSPI(HSPI);

#define TOUCH_CS 33
#define TOUCH_IRQ 36

XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

// Previous working calibration

#define TOUCH_X_MIN 466
#define TOUCH_X_MAX 3726

#define TOUCH_Y_MIN 435
#define TOUCH_Y_MAX 3666

// ======================================
// APPLICATION DATA
// ======================================

struct Conversation {
    int id;
    String title;
    String date;
};

std::vector<Conversation> conversations;

String selectedTitle = "";
String selectedTranscription = "";
String selectedSummary = "";

std::vector<String> textLines;

int currentPage = 0;
int scrollPosition = 0;

const int ITEMS_PER_PAGE = 5;
const int VISIBLE_LINES = 12;

enum Screen {
    HISTORY_SCREEN,
    TRANSCRIPTION_SCREEN,
    SUMMARY_SCREEN
};

Screen currentScreen = HISTORY_SCREEN;

// ======================================
// COLORS
// ======================================

#define COLOR_BACKGROUND TFT_BLACK
#define COLOR_HEADER TFT_NAVY
#define COLOR_BUTTON TFT_DARKCYAN
#define COLOR_TEXT TFT_WHITE
#define COLOR_SELECTED TFT_BLUE

// ======================================
// WIFI CONNECTION
// ======================================

void connectWiFi() {

    tft.fillScreen(COLOR_BACKGROUND);

    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);

    tft.drawString("Connecting WiFi...", 15, 130);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;

    while (
        WiFi.status() != WL_CONNECTED &&
        attempts < 30
    ) {
        delay(500);
        attempts++;
    }

    tft.fillScreen(COLOR_BACKGROUND);

    if (WiFi.status() == WL_CONNECTED) {

        tft.drawString("WiFi Connected!", 15, 130);

        Serial.println("WiFi Connected!");
        Serial.println(WiFi.localIP());

    } else {

        tft.drawString("WiFi Failed!", 15, 130);

        Serial.println("WiFi connection failed!");

    }

    delay(1500);
}

// ======================================
// HTTP REQUEST
// ======================================

int httpRequest(
    String endpoint,
    String method,
    String &response
) {

    if (WiFi.status() != WL_CONNECTED) {

        Serial.println("WiFi disconnected!");

        return -1;
    }

    HTTPClient http;

    String url = String(SERVER_URL) + endpoint;

    Serial.println("\nHTTP REQUEST");
    Serial.println(url);

    http.begin(url);

    // AI generation can take several minutes
    if (method == "POST") {
        http.setTimeout(300000);
    } else {
        http.setTimeout(15000);
    }

    int httpCode;

    if (method == "GET") {

        httpCode = http.GET();

    } else {

        httpCode = http.POST("");

    }

    response = http.getString();

    Serial.print("HTTP Status: ");
    Serial.println(httpCode);

    http.end();

    return httpCode;
}

// ======================================
// LOAD CONVERSATIONS
// ======================================

bool loadConversations() {

    String response;

    conversations.clear();

    int status = httpRequest(
        "/conversations",
        "GET",
        response
    );

    if (status != 200) {
        return false;
    }

    DynamicJsonDocument doc(16384);

    DeserializationError error =
        deserializeJson(doc, response);

    if (error) {

        Serial.println("JSON parsing failed!");

        return false;
    }

    JsonArray list =
        doc["conversations"].as<JsonArray>();

    for (JsonObject item : list) {

        Conversation conversation;

        conversation.id = item["id"] | 0;

        conversation.title =
            item["title"].as<String>();

        conversation.date =
            item["created_at"].as<String>();

        conversations.push_back(conversation);
    }

    Serial.print("Conversations loaded: ");
    Serial.println(conversations.size());

    return true;
}

// ======================================
// LOAD SPECIFIC CONVERSATION
// ======================================

bool loadConversation(int conversationID) {

    String response;

    String endpoint =
        "/conversations/" + String(conversationID);

    int status = httpRequest(
        endpoint,
        "GET",
        response
    );

    if (status != 200) {
        return false;
    }

    DynamicJsonDocument doc(32768);

    DeserializationError error =
        deserializeJson(doc, response);

    if (error) {

        Serial.println("JSON parsing failed!");

        return false;
    }

    JsonObject conversation =
        doc["conversation"];

    selectedTitle =
        conversation["title"].as<String>();

    selectedTranscription =
        conversation["transcription"].as<String>();

    Serial.println("\nSelected Conversation:");
    Serial.println(selectedTitle);
    Serial.println(selectedTranscription);

    return true;
}

// ======================================
// GET SAVED SUMMARY
// ======================================

int loadSavedSummary(int conversationID) {

    String response;

    String endpoint =
        "/conversations/" +
        String(conversationID) +
        "/summary";

    int status = httpRequest(
        endpoint,
        "GET",
        response
    );

    if (status != 200) {
        return status;
    }

    DynamicJsonDocument doc(16384);

    DeserializationError error =
        deserializeJson(doc, response);

    if (error) {

        Serial.println("Summary JSON parsing failed!");

        return -2;
    }

    selectedSummary =
        doc["summary"].as<String>();

    Serial.println("\nSaved Summary:");
    Serial.println(selectedSummary);

    return 200;
}

// ======================================
// GENERATE SUMMARY USING QWEN
// ======================================

bool generateSummary(int conversationID) {

    String response;

    String endpoint =
        "/conversations/" +
        String(conversationID) +
        "/summarize";

    Serial.println("\nRequesting AI summary...");

    int status = httpRequest(
        endpoint,
        "POST",
        response
    );

    if (status != 200) {

        Serial.println("Summary generation failed!");

        Serial.println(response);

        return false;
    }

    DynamicJsonDocument doc(16384);

    DeserializationError error =
        deserializeJson(doc, response);

    if (error) {

        Serial.println("Summary JSON parsing failed!");

        return false;
    }

    selectedSummary =
        doc["summary"].as<String>();

    Serial.println("\nAI Summary:");
    Serial.println(selectedSummary);

    return true;
}

// ======================================
// TOUCHSCREEN
// ======================================

bool readTouch(int &x, int &y) {

    if (!ts.touched()) {
        return false;
    }

    TS_Point p = ts.getPoint();

    x = map(
        p.x,
        TOUCH_X_MIN,
        TOUCH_X_MAX,
        0,
        239
    );

    y = map(
        p.y,
        TOUCH_Y_MIN,
        TOUCH_Y_MAX,
        0,
        319
    );

    x = constrain(x, 0, 239);
    y = constrain(y, 0, 319);

    return true;
}

// ======================================
// WAIT FOR TOUCH RELEASE
// ======================================

void waitForRelease() {

    while (ts.touched()) {
        delay(10);
    }

    delay(150);
}

// ======================================
// DRAW HEADER
// ======================================

void drawHeader(String title) {

    tft.fillRect(
        0, 0, 240, 45,
        COLOR_HEADER
    );

    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);

    tft.drawCentreString(
        title,
        120,
        14,
        1
    );
}

// ======================================
// DRAW BUTTON
// ======================================

void drawButton(
    int x,
    int y,
    int w,
    int h,
    String label
) {

    tft.fillRoundRect(
        x, y, w, h,
        5,
        COLOR_BUTTON
    );

    tft.drawRoundRect(
        x, y, w, h,
        5,
        TFT_WHITE
    );

    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);

    tft.drawCentreString(
        label,
        x + w / 2,
        y + h / 2 - 4,
        1
    );
}

// ======================================
// PREPARE TEXT FOR DISPLAY
// ======================================

void prepareTextLines(String text) {

    textLines.clear();

    text.replace("\r", "");

    while (text.length() > 0) {

        int splitPosition;

        if (text.length() <= 30) {

            splitPosition = text.length();

        } else {

            splitPosition = text.lastIndexOf(' ', 30);

            if (splitPosition <= 0) {
                splitPosition = 30;
            }
        }

        String line =
            text.substring(0, splitPosition);

        line.trim();

        textLines.push_back(line);

        text = text.substring(splitPosition);

        text.trim();
    }

}

// ======================================
// DRAW TEXT CONTENT
// ======================================

void drawTextContent() {

    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);

    for (int i = 0; i < VISIBLE_LINES; i++) {

        int lineIndex = scrollPosition + i;

        if (lineIndex >= (int)textLines.size()) {
            break;
        }

        tft.drawString(
            textLines[lineIndex],
            8,
            90 + (i * 15),
            1
        );
    }

}

// ======================================
// DRAW HISTORY SCREEN
// ======================================

void drawHistoryScreen() {

    currentScreen = HISTORY_SCREEN;

    tft.fillScreen(COLOR_BACKGROUND);

    drawHeader("Conversation History");

    drawButton(10, 50, 105, 30, "REFRESH");

    int totalPages =
        ((int)conversations.size() + ITEMS_PER_PAGE - 1)
        / ITEMS_PER_PAGE;

    if (totalPages == 0) {
        totalPages = 1;
    }

    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);

    tft.drawRightString(
        "Page " + String(currentPage + 1) +
        "/" + String(totalPages),
        230,
        60,
        1
    );

    if (conversations.empty()) {

        tft.setTextSize(2);

        tft.drawCentreString(
            "No conversations",
            120,
            145,
            1
        );

        drawButton(10, 305, 100, 30, "PREVIOUS");
        drawButton(130, 305, 100, 30, "NEXT");

        return;
    }

    int startIndex =
        currentPage * ITEMS_PER_PAGE;

    int endIndex =
        min(
            startIndex + ITEMS_PER_PAGE,
            (int)conversations.size()
        );

    for (int i = startIndex; i < endIndex; i++) {

        int row = i - startIndex;

        int y = 95 + (row * 42);

        tft.fillRoundRect(
            10, y, 220, 36,
            5,
            TFT_DARKGREY
        );

        tft.setTextColor(TFT_WHITE);
        tft.setTextSize(1);

        tft.drawString(
            String(conversations[i].id) +
            ". " +
            conversations[i].title.substring(0, 25),
            18,
            y + 12,
            1
        );
    }

    drawButton(10, 305, 100, 30, "PREVIOUS");

    drawButton(130, 305, 100, 30, "NEXT");
}

// ======================================
// DRAW TRANSCRIPTION SCREEN
// ======================================

void drawTranscriptionScreen() {

    currentScreen = TRANSCRIPTION_SCREEN;

    tft.fillScreen(COLOR_BACKGROUND);

    drawHeader("Transcription");

    tft.setTextColor(TFT_YELLOW);
    tft.setTextSize(2);

    tft.drawString(
        selectedTitle.substring(0, 20),
        8,
        52
    );

    tft.drawLine(
        5, 80, 235, 80,
        TFT_WHITE
    );

    drawTextContent();

    // Bottom buttons

    drawButton(5, 285, 55, 32, "BACK");

    drawButton(65, 285, 95, 32, "SUMMARIZE");

    drawButton(165, 285, 32, 32, "UP");

    drawButton(202, 285, 33, 32, "DOWN");
}

// ======================================
// DRAW SUMMARY SCREEN
// ======================================

void drawSummaryScreen() {

    currentScreen = SUMMARY_SCREEN;

    tft.fillScreen(COLOR_BACKGROUND);

    drawHeader("AI Summary");

    tft.setTextColor(TFT_YELLOW);
    tft.setTextSize(2);

    tft.drawString(
        selectedTitle.substring(0, 20),
        8,
        52
    );

    tft.drawLine(
        5, 80, 235, 80,
        TFT_WHITE
    );

    drawTextContent();

    // Bottom buttons

    drawButton(5, 285, 55, 32, "BACK");

    drawButton(65, 285, 95, 32, "REGEN");

    drawButton(165, 285, 32, 32, "UP");

    drawButton(202, 285, 33, 32, "DOWN");
}

// ======================================
// LOADING SCREEN
// ======================================

void showLoading(String message) {

    tft.fillScreen(COLOR_BACKGROUND);

    drawHeader("Please Wait");

    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);

    tft.drawCentreString(
        message,
        120,
        135,
        1
    );

    tft.setTextSize(1);

    tft.drawCentreString(
        "Processing...",
        120,
        170,
        1
    );
}

// ======================================
// SHOW ERROR
// ======================================

void showError(String message) {

    tft.fillScreen(COLOR_BACKGROUND);

    tft.setTextColor(TFT_RED);
    tft.setTextSize(2);

    tft.drawCentreString(
        "ERROR",
        120,
        120,
        1
    );

    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);

    tft.drawCentreString(
        message,
        120,
        160,
        1
    );

    delay(2000);
}

// ======================================
// OPEN SUMMARY
// ======================================

void openSummary() {

    showLoading("Checking summary");

    int status = loadSavedSummary(
        conversations[
            currentPage * ITEMS_PER_PAGE
        ].id
    );

    // This function is not used for selection.
    // The selected conversation ID is stored below.
}

// ======================================
// CURRENT CONVERSATION ID
// ======================================

int selectedConversationID = -1;

// ======================================
// SUMMARIZE BUTTON ACTION
// ======================================

void handleSummarize() {

    showLoading("Checking summary");

    int status = loadSavedSummary(
        selectedConversationID
    );

    if (status == 200) {

        Serial.println("Using saved summary.");

        prepareTextLines(selectedSummary);

        scrollPosition = 0;

        drawSummaryScreen();

        return;
    }

    if (status != 404) {

        showError("Server connection error");

        drawTranscriptionScreen();

        return;
    }

    showLoading("Generating summary");

    Serial.println("No saved summary found.");
    Serial.println("Generating new summary...");

    if (generateSummary(selectedConversationID)) {

        prepareTextLines(selectedSummary);

        scrollPosition = 0;

        drawSummaryScreen();

    } else {

        showError("Summary generation failed");

        drawTranscriptionScreen();
    }
}

// ======================================
// REGENERATE SUMMARY
// ======================================

void handleRegenerate() {

    showLoading("Regenerating summary");

    if (generateSummary(selectedConversationID)) {

        prepareTextLines(selectedSummary);

        scrollPosition = 0;

        drawSummaryScreen();

    } else {

        showError("Generation failed");

        drawSummaryScreen();
    }
}

// ======================================
// HISTORY TOUCH CONTROLS
// ======================================

void handleHistoryTouch(int x, int y) {

    // Refresh
    if (
        x >= 10 && x <= 115 &&
        y >= 50 && y <= 80
    ) {

        showLoading("Loading history");

        if (loadConversations()) {

            currentPage = 0;

            drawHistoryScreen();

        } else {

            showError("Failed to load history");

            drawHistoryScreen();
        }

        return;
    }

    // Previous page
    if (y >= 300 && x <= 115) {

        if (currentPage > 0) {

            currentPage--;

            drawHistoryScreen();
        }

        return;
    }

    // Next page
    if (y >= 300 && x >= 125) {

        int totalPages =
            ((int)conversations.size() + ITEMS_PER_PAGE - 1)
            / ITEMS_PER_PAGE;

        if (currentPage + 1 < totalPages) {

            currentPage++;

            drawHistoryScreen();
        }

        return;
    }

    // Select conversation
    if (y >= 95 && y < 305) {

        int row = (y - 95) / 42;

        int index =
            currentPage * ITEMS_PER_PAGE + row;

        if (
            index >= 0 &&
            index < (int)conversations.size()
        ) {

            selectedConversationID =
                conversations[index].id;

            showLoading("Loading transcription");

            if (loadConversation(selectedConversationID)) {

                prepareTextLines(selectedTranscription);

                scrollPosition = 0;

                drawTranscriptionScreen();

            } else {

                showError("Failed to load");

                drawHistoryScreen();
            }
        }
    }
}

// ======================================
// TRANSCRIPTION TOUCH CONTROLS
// ======================================

void handleTranscriptionTouch(int x, int y) {

    if (y >= 285) {

        // Back
        if (x <= 60) {

            drawHistoryScreen();

            return;
        }

        // Summarize
        if (x >= 65 && x <= 160) {

            handleSummarize();

            return;
        }

        // Scroll up
        if (x >= 165 && x <= 197) {

            if (scrollPosition > 0) {

                scrollPosition--;

                drawTranscriptionScreen();
            }

            return;
        }

        // Scroll down
        if (x >= 202) {

            if (
                scrollPosition + VISIBLE_LINES <
                (int)textLines.size()
            ) {

                scrollPosition++;

                drawTranscriptionScreen();
            }

            return;
        }
    }
}

// ======================================
// SUMMARY TOUCH CONTROLS
// ======================================

void handleSummaryTouch(int x, int y) {

    if (y >= 285) {

        // Back
        if (x <= 60) {

            prepareTextLines(selectedTranscription);

            scrollPosition = 0;

            drawTranscriptionScreen();

            return;
        }

        // Regenerate
        if (x >= 65 && x <= 160) {

            handleRegenerate();

            return;
        }

        // Scroll up
        if (x >= 165 && x <= 197) {

            if (scrollPosition > 0) {

                scrollPosition--;

                drawSummaryScreen();
            }

            return;
        }

        // Scroll down
        if (x >= 202) {

            if (
                scrollPosition + VISIBLE_LINES <
                (int)textLines.size()
            ) {

                scrollPosition++;

                drawSummaryScreen();
            }

            return;
        }
    }
}

// ======================================
// SETUP
// ======================================

void setup() {

    Serial.begin(115200);

    // Display
    tft.init();

    tft.setRotation(0);

    tft.invertDisplay(true);

    tft.fillScreen(TFT_BLACK);

    // Touchscreen
    touchSPI.begin(
        25,
        39,
        32,
        TOUCH_CS
    );

    ts.begin(touchSPI);

    ts.setRotation(0);

    // WiFi
    connectWiFi();

    // Load conversations
    if (loadConversations()) {

        currentPage = 0;

        drawHistoryScreen();

    } else {

        showError("Server connection failed");

        tft.setTextColor(TFT_WHITE);

        tft.drawCentreString(
            "Check WiFi and server IP",
            120,
            165,
            1
        );
    }
}

// ======================================
// MAIN LOOP
// ======================================

void loop() {

    int x, y;

    if (readTouch(x, y)) {

        Serial.print("Touch X: ");
        Serial.print(x);

        Serial.print(" | Touch Y: ");
        Serial.println(y);

        waitForRelease();

        if (currentScreen == HISTORY_SCREEN) {

            handleHistoryTouch(x, y);

        } else if (
            currentScreen == TRANSCRIPTION_SCREEN
        ) {

            handleTranscriptionTouch(x, y);

        } else if (
            currentScreen == SUMMARY_SCREEN
        ) {

            handleSummaryTouch(x, y);
        }
    }
}