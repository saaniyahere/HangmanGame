#include "raylib.h"
#include <string>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <vector>

using std::string;
using std::vector;

// Global background color that fits the hangman vibe
const Color BG_COLOR = { 18, 22, 40, 255 }; // dark blue-ish

// ---------------- Game Screens ----------------
enum class GameScreen {
    START,
    SETTINGS,     // pre-game setup
    SOUND_SETTINGS, // Sound Configuration Screen
    LEADERBOARD,   // Leaderboard/High Scores Screen
    ENTER_WORD,   // setter enters word + hint
    PLAYING,      // guesser plays
    SUMMARY       // after all rounds
};

// ---------------- Leaderboard Data Structure ----------------
struct PlayerScore {
    string name;
    int score;

    // Helper for sorting
    bool operator>(const PlayerScore& other) const {
        return score > other.score;
    }
};

// ---------------- Hangman Game Class ----------------
class HangmanGame {
public:
    HangmanGame(int W, int H)
        : screenWidth(W), screenHeight(H) {

        // Default settings
        player1Name = "Player 1";
        player2Name = "Player 2";
        starterIsP1 = true;
        totalRounds = 3;
        maxLivesSetting = 7;      // max wrong guesses (1–7)
        timeLimitSeconds = 60;    // default 60s per word

        // Sound settings
        soundEnabled = true;
        musicEnabled = false; 

        currentRound = 1;

        // Scores
        player1Score = 0;
        player2Score = 0;

        // Start game in START screen
        currentScreen = GameScreen::START;

        // Other state init
        player1IsSetter = true;
        ResetRoundState();
        ResetWordInput();
        ResetSettingsInput();
        soundSettingsFieldIndex = 0;
        
        // --- Use built-in font but make it smoother ---
        uiFont = GetFontDefault();

        // Load visual asset
        startBg = LoadTexture("start_bg.jpg");
        
        // NEW: Load and Set Window Icon
        // NOTE: The file 'icon.png' must exist in your project root.
        windowIcon = LoadImage("hangman_img.png");
        if (windowIcon.data != NULL) {
            TraceLog(LOG_INFO, "ICON: Image loaded successfully. Setting window icon.");
            SetWindowIcon(windowIcon);
        } else {
            TraceLog(LOG_ERROR, "ICON: FAILED to load icon.png! Check filename and format.");
        }

        // Load audio assets
        if (IsAudioDeviceReady()) {
            clickSound = LoadSound("click.wav");
            // NOTE: Replace "music.ogg" with your actual music file name.
            backgroundMusic = LoadMusicStream("music.ogg"); 
            
            if (backgroundMusic.frameCount > 0) {
                PlayMusicStream(backgroundMusic);
            }
        } 
    }

    ~HangmanGame() {
        if (startBg.id != 0) {
            UnloadTexture(startBg);
        }
        if (clickSound.frameCount > 0) {
            UnloadSound(clickSound);
        }
        
        if (backgroundMusic.frameCount > 0) {
            UnloadMusicStream(backgroundMusic);
        }
        
        // NEW: Unload Window Icon
        if (windowIcon.data != NULL) {
            UnloadImage(windowIcon);
        }
    }

    // Called each frame
    void Update() {
        if (backgroundMusic.frameCount > 0) {
            UpdateMusicStream(backgroundMusic);
            SetMusicVolume(backgroundMusic, musicEnabled ? 0.5f : 0.0f);
        }
        
        switch (currentScreen) {
            case GameScreen::START:      UpdateStart();      break;
            case GameScreen::SETTINGS:   UpdateSettings();   break;
            case GameScreen::SOUND_SETTINGS: UpdateSoundSettings(); break;
            case GameScreen::LEADERBOARD: UpdateLeaderboard(); break;
            case GameScreen::ENTER_WORD: UpdateEnterWord();  break;
            case GameScreen::PLAYING:    UpdatePlaying();    break;
            case GameScreen::SUMMARY:    UpdateSummary();    break;
        }
    }

    // Called each frame to draw
    void Draw() {
        switch (currentScreen) {
            case GameScreen::START:      DrawStart();      break;
            case GameScreen::SETTINGS:   DrawSettings();   break;
            case GameScreen::SOUND_SETTINGS: DrawSoundSettings(); break;
            case GameScreen::LEADERBOARD: DrawLeaderboard(); break;
            case GameScreen::ENTER_WORD: DrawEnterWord();  break;
            case GameScreen::PLAYING:    DrawPlaying();    break;
            case GameScreen::SUMMARY:    DrawSummary();    break;
        }
    }

private:
    // ---------------- Window / global state ----------------
    int screenWidth, screenHeight;
    GameScreen currentScreen;
    Font uiFont; // built-in font, smoothed

    // ---------------- Settings (user-configurable) ----------------
    string player1Name, player2Name;
    bool starterIsP1;
    int totalRounds;
    int maxLivesSetting;
    int timeLimitSeconds;
    
    // Sound settings
    bool soundEnabled;
    bool musicEnabled;

    // For settings input
    int settingsFieldIndex;
    string p1NameInput, p2NameInput;
    string settingsMessage;
    
    // For sound settings input (0=sound, 1=music)
    int soundSettingsFieldIndex; 

    // ---------------- Game State ----------------
    bool player1IsSetter;
    int currentRound;
    int player1Score;
    int player2Score;
    
    // Leaderboard
    vector<PlayerScore> leaderboard; // Stores top scores
    
    // For word entry
    string inputWord;
    string inputHint;
    int inputStep;
    string inputErrorMsg;

    // For guessing phase
    string secretWord;
    string hint;
    string shownWord;
    string triedLetters;
    int lives;
    bool gameOver;
    bool win;
    float timeLeft;
    // Animation state (purely visual — does NOT affect logic)
    float wrongShakeTimer = 0.0f;  
    float winJumpTimer    = 0.0f;  

    // ---------------- Assets ----------------
    Texture2D startBg{};    // background image for start page
    Sound clickSound{};     // Button click sound
    Music backgroundMusic{};// Background Music Stream
    Image windowIcon{};     // NEW: Window Icon Image

    // ---------------- Helper methods ----------------
    
    void PlayClick() {
        if (soundEnabled && clickSound.frameCount > 0) {
            PlaySound(clickSound);
        }
    }

    void UpdateLeaderboardScores() {
        // 1. Add current players' final scores
        if (player1Score > 0) leaderboard.push_back({player1Name, player1Score});
        if (player2Score > 0) leaderboard.push_back({player2Name, player2Score});

        // 2. Sort the list descending by score
        std::sort(leaderboard.begin(), leaderboard.end(), 
                  [](const PlayerScore& a, const PlayerScore& b) {
                      return a.score > b.score;
                  });

        // 3. Keep only the top 3 scores
        if (leaderboard.size() > 3) {
            leaderboard.resize(3);
        }
    }


    void DrawTextSmooth(const string& txt, int x, int y, int fontSize,
                        Color color, float spacing = 1.0f) {
        DrawTextEx(uiFont, txt.c_str(),
                   Vector2{ (float)x, (float)y },
                   (float)fontSize, spacing, color);
    }

    void DrawTextSmooth(const char* txt, int x, int y, int fontSize,
                        Color color, float spacing = 1.0f) {
        DrawTextEx(uiFont, txt,
                   Vector2{ (float)x, (float)y },
                   (float)fontSize, spacing, color);
    }
    void DrawTextCentered(const string& txt, int centerX, int y, int fontSize, Color color, float spacing = 1.0f) {
        Vector2 size = MeasureTextEx(uiFont, txt.c_str(), (float)fontSize, spacing);
        float x = centerX - size.x / 2.0f;
        DrawTextEx(uiFont, txt.c_str(),
                   Vector2{ x, (float)y },
                   (float)fontSize, spacing, color);
    }

    // Pretty print word as "A _ _" style (visual only)
    string SpacedWord(const string& s) {
        string out;
        for (char c : s) {
            if (c == '*') {
                out.push_back('_');
            } else if (c == ' ') {
                out.push_back(' ');
            } else {
                out.push_back(c);
            }
            out.push_back(' ');
        }
        return out;
    }

    void ResetRoundState() {
        secretWord.clear();
        hint.clear();
        shownWord.clear();
        triedLetters.clear();
        lives = 0;
        gameOver = false;
        win = false;
        timeLeft = (float)timeLimitSeconds;

        wrongShakeTimer = 0.0f;
        winJumpTimer = 0.0f;
    }

    void ResetWordInput() {
        inputWord.clear();
        inputHint.clear();
        inputStep = 0;
        inputErrorMsg.clear();
    }

    void ResetSettingsInput() {
        p1NameInput = player1Name;
        p2NameInput = player2Name;
        settingsFieldIndex = 0;
        settingsMessage.clear();
    }
    
    // Helper for drawing buttons with rounded corners
    void DrawButton(Rectangle r, const char* label, bool primary, bool hover) {
        Color base = primary ? MAROON : DARKGRAY;
        Color fill = hover ? Fade(base, 0.9f) : Fade(base, 0.7f);

        DrawRectangleRounded(r, 0.3f, 10, fill);
        DrawRectangleRoundedLines(r, 0.3f, 10, BLACK);

        int fontSize = 22;
        float spacing = 2.0f;
        Vector2 ts = MeasureTextEx(uiFont, label, (float)fontSize, spacing);
        float tx = r.x + (r.width  - ts.x) / 2.0f;
        float ty = r.y + (r.height - ts.y) / 2.0f;

        DrawTextEx(uiFont, label, Vector2{tx, ty}, (float)fontSize, spacing, RAYWHITE);
    };

    string Upper(const string &s) {
        string r = s;
        std::transform(r.begin(), r.end(), r.begin(), ::toupper);
        return r;
    }

    bool WordValid(const string &w) {
        if (w.empty()) return false;
        for (char c : w) {
            if (std::isalpha((unsigned char)c)) return true;
        }
        return false;
    }

    string SetterName()  const { return player1IsSetter ? player1Name : player2Name; }
    string GuesserName() const { return player1IsSetter ? player2Name : player1Name; }

    void SwapRoles() {
        player1IsSetter = !player1IsSetter;
    }

    void AwardScore(bool guesserWon) {
        if (guesserWon) {
            if (player1IsSetter) player2Score++;
            else                 player1Score++;
        } else {
            if (player1IsSetter) player1Score++;
            else                 player2Score++;
        }
    }

    // ============================================================
    // START SCREEN
    // ============================================================

    void UpdateStart() {
        // --- Button Geometry ---
        int btnW = 260;
        int btnH = 60;
        int btnX = screenWidth / 2 - btnW / 2; // Horizontally centered

        // --- Stacked Buttons Y Positions (Centered Below Figure) ---
        int gap = 20;
        int yStart = screenHeight / 2 + 100; // Start the stack around the middle bottom
        
        // Button 1: START GAME
        int btnY1 = yStart; 
        Rectangle btnStartRect = { (float)btnX, (float)btnY1, (float)btnW, (float)btnH };
        
        // Button 2: SOUND SETTINGS
        int btnY2 = btnY1 + btnH + gap;
        Rectangle btnSoundRect = { (float)btnX, (float)btnY2, (float)btnW, (float)btnH };
        
        // Button 3: LEADERBOARD
        int btnY3 = btnY2 + btnH + gap;
        Rectangle btnLeaderRect = { (float)btnX, (float)btnY3, (float)btnW, (float)btnH };


        Vector2 mouse = GetMousePosition();

        bool hoverStart = CheckCollisionPointRec(mouse, btnStartRect);
        bool clickStart = hoverStart && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        
        bool hoverSound = CheckCollisionPointRec(mouse, btnSoundRect);
        bool clickSound = hoverSound && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        
        bool hoverLeader = CheckCollisionPointRec(mouse, btnLeaderRect);
        bool clickLeader = hoverLeader && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

        bool keyStart = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);

        if (clickStart || keyStart) {
            PlayClick();
            currentScreen = GameScreen::SETTINGS;
            return;
        }
        
        // Handle sound settings button click
        if (clickSound) {
            PlayClick();
            currentScreen = GameScreen::SOUND_SETTINGS;
            soundSettingsFieldIndex = 0;
            return;
        }
        
        // Handle LEADERBOARD button click
        if (clickLeader) {
            PlayClick();
            currentScreen = GameScreen::LEADERBOARD;
            return;
        }


        // ESC from start screen quits the game
        if (IsKeyPressed(KEY_ESCAPE)) {
            CloseWindow();
        }
    }

    void DrawStart() {
        ClearBackground(BLACK);

        // Draw background image stretched to window
        if (startBg.id != 0) {
            Rectangle src = { 0, 0, (float)startBg.width, (float)startBg.height };
            Rectangle dst = { 0, 0, (float)screenWidth, (float)screenHeight };
            DrawTexturePro(startBg, src, dst, Vector2{0, 0}, 0.0f, WHITE);
        }

        // Dark overlay to make text readable
        DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.35f));

        // --- Title: SPLIT HANG MAN (Top Center) ---
        
        // Define common properties
        int titleFontSize = 52;
        float titleSpacing = 3.0f;
        Color titleColor = RAYWHITE;
        
        // Position variables
        int titleY = 62; 
        
        // Calculate size of "HANG" and "MAN"
        Vector2 sizeHang = MeasureTextEx(uiFont, "HANG", (float)titleFontSize, titleSpacing);
        Vector2 sizeMan = MeasureTextEx(uiFont, "MAN", (float)titleFontSize, titleSpacing);
        
        // Desired gap for the rope/gallows image
        int gapWidth = 30; // Small gap
        
        // Calculate total width of the 'HANG' + GAP + 'MAN' block
        float totalTitleWidth = sizeHang.x + gapWidth + sizeMan.x;
        
        // Start X position for the entire block to be horizontally centered
        float startX = (screenWidth / 2.0f) - (totalTitleWidth / 2.0f);
        
        // Apply the previous 0.5 cm right shift to the centered block
        startX += 19; 
        
        // Apply HANG ADJUSTMENTS
        float xHang = startX - 13; 
        
        // Apply MAN ADJUSTMENTS
        float xManBase = startX + sizeHang.x + gapWidth;
        float xMan = xManBase - 14; 

        // Draw HANG (Left side)
        DrawTextSmooth("HANG", (int)xHang, titleY, titleFontSize, titleColor, titleSpacing); 
        
        // Draw MAN (Right side)
        DrawTextSmooth("MAN", (int)xMan, titleY, titleFontSize, titleColor, titleSpacing); 

        // --- Button Geometry ---
        int btnW = 260;
        int btnH = 60;
        int btnX = screenWidth / 2 - btnW / 2; // Horizontally centered

        // --- Stacked Buttons Y Positions ---
        int gap = 20;
        int yStart = screenHeight / 2 + 100; // Start the stack around the middle bottom
        
        // Button 1: START GAME
        int btnY1 = yStart; 
        Rectangle btnStartRect = { (float)btnX, (float)btnY1, (float)btnW, (float)btnH };
        
        // Button 2: SOUND SETTINGS
        int btnY2 = btnY1 + btnH + gap;
        Rectangle btnSoundRect = { (float)btnX, (float)btnY2, (float)btnW, (float)btnH };
        
        // Button 3: LEADERBOARD
        int btnY3 = btnY2 + btnH + gap;
        Rectangle btnLeaderRect = { (float)btnX, (float)btnY3, (float)btnW, (float)btnH };


        Vector2 mouse = GetMousePosition();
        int fontSize = 26;
        float spacing = 2.0f;
        Color textColor = RAYWHITE;

        // --- Drawing Helper (Uses MAROON hover fill for all) ---
        auto drawSecondaryButton = [&](Rectangle r, const char* label, bool hover) {
            Color base = DARKGRAY; 
            
            Color fill = hover ? Fade(MAROON, 0.9f) : Fade(base, 0.7f);
            
            DrawRectangleRounded(r, 0.3f, 10, fill);
            DrawRectangleRoundedLines(r, 0.3f, 10, BLACK);

            int currentFontSize = 26;
            float currentSpacing = 2.0f;
            Vector2 ts = MeasureTextEx(uiFont, label, (float)currentFontSize, currentSpacing);
            float tx = r.x + (r.width  - ts.x) / 2.0f;
            float ty = r.y + (r.height - ts.y) / 2.0f;

            DrawTextEx(uiFont, label, Vector2{tx, ty}, (float)currentFontSize, currentSpacing, textColor);
        };

        // --- Draw START GAME button (Primary, Top of Stack) ---
        bool hoverStart = CheckCollisionPointRec(mouse, btnStartRect);
        Color startBtnColor = hoverStart ? MAROON : DARKGRAY;

        DrawRectangleRounded(btnStartRect, 0.3f, 10, startBtnColor);
        DrawRectangleRoundedLines(btnStartRect, 0.3f, 10, BLACK);

        const char* btnTextStart = "START GAME";
        Vector2 textSizeStart = MeasureTextEx(uiFont, btnTextStart, (float)fontSize, spacing);

        int textXStart = btnX + (btnW - (int)textSizeStart.x) / 2;
        int textYStart = btnY1 + (btnH - (int)textSizeStart.y) / 2;
        DrawTextEx(uiFont, btnTextStart,
                   Vector2{ (float)textXStart, (float)textYStart },
                   (float)fontSize, spacing, textColor);
                   
        // --- Draw SOUND SETTINGS button (Middle of Stack) ---
        bool hoverSound = CheckCollisionPointRec(mouse, btnSoundRect);
        drawSecondaryButton(btnSoundRect, "SOUND SETTINGS", hoverSound);

        // --- Draw LEADERBOARD button (Bottom of Stack) ---
        bool hoverLeader = CheckCollisionPointRec(mouse, btnLeaderRect); 
        drawSecondaryButton(btnLeaderRect, "LEADERBOARD", hoverLeader); 

        // --- Footer text ---
        // (Section intentionally empty)
    }

    // ============================================================
    // SETTINGS SCREEN
    // ============================================================

    void UpdateSettings() {
        // Navigate fields with UP/DOWN (0..4) - NO SOUND
        if (IsKeyPressed(KEY_UP)) {
            settingsFieldIndex--;
            if (settingsFieldIndex < 0) settingsFieldIndex = 4;
        }
        if (IsKeyPressed(KEY_DOWN)) {
            settingsFieldIndex++;
            if (settingsFieldIndex > 4) settingsFieldIndex = 0;
        }

        // Edit based on current field: (TYPING AND BACKSPACE - NO SOUND)
        int ch = GetCharPressed();
        while (ch > 0) {
            char c = (char)ch;

            if (settingsFieldIndex == 0) {
                if (p1NameInput.size() < 15 && c >= 32 && c <= 126) {
                    p1NameInput.push_back(c);
                }
            } else if (settingsFieldIndex == 1) {
                if (p2NameInput.size() < 15 && c >= 32 && c <= 126) {
                    p2NameInput.push_back(c);
                }
            }
            ch = GetCharPressed();
        }

        if (IsKeyPressed(KEY_BACKSPACE)) {
            if (settingsFieldIndex == 0 && !p1NameInput.empty()) {
                p1NameInput.pop_back();
            }
            else if (settingsFieldIndex == 1 && !p2NameInput.empty()) {
                p2NameInput.pop_back();
            }
        }

        // Left/right to change non-text fields (NO SOUND)
        if (settingsFieldIndex == 2) { // starter
            if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT)) {
                starterIsP1 = !starterIsP1;
            }
        } else if (settingsFieldIndex == 3) { // rounds
            if (IsKeyPressed(KEY_LEFT)) {
                if (totalRounds > 1) { totalRounds--; }
            }
            if (IsKeyPressed(KEY_RIGHT)) {
                if (totalRounds < 10) { totalRounds++; }
            }
        } else if (settingsFieldIndex == 4) { // time per round
            if (IsKeyPressed(KEY_LEFT)) {
                if (timeLimitSeconds > 10) { timeLimitSeconds -= 10; }
            }
            if (IsKeyPressed(KEY_RIGHT)) {
                if (timeLimitSeconds < 300) { timeLimitSeconds += 10; }
            }
        }

        // Geometry for card + buttons
        int margin = 20;
        int cardX = margin;
        int cardY = margin;
        int cardW = screenWidth - 2 * margin;
        int cardH = screenHeight - 2 * margin;

        int btnW = 200;
        int btnH = 50;
        int btnY = cardY + cardH - 130;

        Rectangle backBtn  = { (float)(cardX + 80), (float)btnY, (float)btnW, (float)btnH };
        Rectangle startBtn = { (float)(cardX + cardW - 80 - btnW), (float)btnY, (float)btnW, (float)btnH };

        Vector2 mouse = GetMousePosition();

        // Mouse selection of fields + buttons
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            // Click on fields (NO SOUND)
            int xValue = cardX + 550;
            int yStart = cardY + 120;
            int dy = 60;

            for (int i = 0; i <= 4; i++) {
                int y = yStart + i * dy;
                Rectangle box = { (float)(xValue - 10), (float)(y - 5), 320.0f, 36.0f };
                if (CheckCollisionPointRec(mouse, box)) {
                    settingsFieldIndex = i;
                }
            }

            // Click on BACK button (SOUND)
            if (CheckCollisionPointRec(mouse, backBtn)) {
                PlayClick();
                currentScreen = GameScreen::START;
                return;
            }

            // Click on START ROUND button (SOUND)
            if (CheckCollisionPointRec(mouse, startBtn)) {
                PlayClick();
                ApplySettingsAndStartRound();
                return;
            }
        }

        // ENTER: apply settings & start game (SOUND)
        if (IsKeyPressed(KEY_ENTER)) {
            PlayClick();
            ApplySettingsAndStartRound();
        }

        // ESC from settings: back to START screen (SOUND)
        if (IsKeyPressed(KEY_ESCAPE)) {
            PlayClick();
            currentScreen = GameScreen::START;
        }
    }

    void ApplySettingsAndStartRound() {
        if (!p1NameInput.empty()) player1Name = p1NameInput;
        else player1Name = "Player 1";

        if (!p2NameInput.empty()) player2Name = p2NameInput;
        else player2Name = "Player 2";

        player1IsSetter = starterIsP1;
        player1Score = 0;
        player2Score = 0;
        currentRound = 1;

        ResetRoundState();
        ResetWordInput();
        currentScreen = GameScreen::ENTER_WORD;
    }

    void DrawSettings() {
        ClearBackground(BG_COLOR);

        int margin = 20;
        int cardX = margin;
        int cardY = margin;
        int cardW = screenWidth - 2 * margin;
        int cardH = screenHeight - 2 * margin;

        DrawRectangle(cardX + 6, cardY + 8, cardW, cardH, Fade(BLACK, 0.18f));
        DrawRectangle(cardX, cardY, cardW, cardH, RAYWHITE);
        DrawRectangleLines(cardX, cardY, cardW, cardH, BLACK);

        // Title
        DrawTextSmooth("BEFORE YOU HANG PAGE", cardX + 340, cardY + 30, 32, BLACK, 2.0f);

        int xLabel = cardX + 140;
        int xValue = cardX + 550;
        int yStart = cardY + 120;
        int dy = 60;

        auto drawField = [&](int index, const char* label, const string& value) {
            int y = yStart + index * dy;

            DrawTextSmooth(label, xLabel, y, 24, BLACK, 2.0f);

            // Active vs normal border
            if (settingsFieldIndex == index) {
                DrawRectangleLinesEx(
                    { (float)(xValue - 10), (float)(y - 5), 320, 36 },
                    2.5f, MAROON
                ); 
            } else {
                DrawRectangleLines(
                    xValue - 10, y - 5, 320, 36,
                    DARKGRAY
                );
            }

            // Draw field text
            int valueFontSize = 22;
            float valueSpacing = 1.5f;
            DrawTextSmooth(value.c_str(), xValue, y, valueFontSize, BLACK, valueSpacing);

            // Blinking cursor for editable text fields (name fields: 0 and 1)
            if (settingsFieldIndex == index && (index == 0 || index == 1)) {
                float t = GetTime();
                bool showCursor = ((int)(t * 2)) % 2 == 0;  // blink ~2 times per second

                if (showCursor) {
                    Vector2 textSize = MeasureTextEx(
                        uiFont, value.c_str(),
                        (float)valueFontSize, valueSpacing
                    );

                    int cursorX = xValue + (int)textSize.x + 3;
                    int cursorY = y - 2;
                    int cursorH = valueFontSize + 6;

                    DrawRectangle(cursorX, cursorY, 2, cursorH, BLACK);
                }
            }
        };

        drawField(0, "YOUR COOL NAME :", p1NameInput);
        drawField(1, "SIDEKICK / RIVAL NAME :", p2NameInput);

        {
            string starterStr = starterIsP1 ? player1Name : player2Name;
            drawField(2, "WHO HIDES THE WORD FIRST? :", starterStr);
        }

        {
            string roundsStr = std::to_string(totalRounds);
            drawField(3, "HOW MANY BATTLES? : ", roundsStr);
        }

        {
            string timeStr = std::to_string(timeLimitSeconds) + " seconds";
            drawField(4, "TIME PRESSURE PER ROUND :", timeStr);
        }

        // Buttons at bottom: BACK and START ROUND
        int btnW = 200;
        int btnH = 50;
        int btnY = cardY + cardH - 130;

        Rectangle backBtn  = { (float)(cardX + 80), (float)btnY, (float)btnW, (float)btnH };
        Rectangle startBtn = { (float)(cardX + cardW - 80 - btnW), (float)btnY, (float)btnW, (float)btnH };

        Vector2 mouse = GetMousePosition();

        DrawButton(backBtn,  "BACK",        false, CheckCollisionPointRec(mouse, backBtn));
        DrawButton(startBtn, "START ROUND", true, CheckCollisionPointRec(mouse, startBtn));

        string tip = "Tip: Use ↑/↓ or click to change fields. ENTER or START button to begin.";
        DrawTextCentered(tip, cardX + cardW / 2, cardY + cardH - 40, 18, DARKGRAY);

    }

    // ============================================================
    // SOUND SETTINGS SCREEN
    // ============================================================

    void UpdateSoundSettings() {
        // Navigate fields with UP/DOWN (0..1) - NO SOUND
        if (IsKeyPressed(KEY_UP)) {
            soundSettingsFieldIndex--;
            if (soundSettingsFieldIndex < 0) soundSettingsFieldIndex = 1;
        }
        if (IsKeyPressed(KEY_DOWN)) {
            soundSettingsFieldIndex++;
            if (soundSettingsFieldIndex > 1) soundSettingsFieldIndex = 0;
        }
        
        // Left/right/ENTER to toggle - NO SOUND
        if (soundSettingsFieldIndex == 0) { // soundEnabled
            if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_ENTER)) {
                soundEnabled = !soundEnabled;
            }
        } else if (soundSettingsFieldIndex == 1) { // musicEnabled
            if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_ENTER)) {
                musicEnabled = !musicEnabled;
            }
        }

        // Geometry for card + buttons
        int margin = 20;
        int cardX = margin;
        int cardY = margin;
        int cardW = screenWidth - 2 * margin;
        int cardH = screenHeight - 2 * margin;

        int btnW = 200;
        int btnH = 50;
        int btnY = cardY + cardH - 130;

        Rectangle backBtn  = { (float)(cardX + cardW / 2 - btnW / 2), (float)btnY, (float)btnW, (float)btnH };

        Vector2 mouse = GetMousePosition();

        // Mouse selection of fields + buttons
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            // Click on fields (NO SOUND)
            int xValue = cardX + 550;
            int yStart = cardY + 120;
            int dy = 60;

            for (int i = 0; i <= 1; i++) {
                int y = yStart + i * dy;
                Rectangle box = { (float)(xValue - 10), (float)(y - 5), 320.0f, 36.0f };
                if (CheckCollisionPointRec(mouse, box)) {
                    soundSettingsFieldIndex = i;
                    if (i == 0) soundEnabled = !soundEnabled;
                    if (i == 1) musicEnabled = !musicEnabled;
                }
            }

            // Click on BACK button (SOUND)
            if (CheckCollisionPointRec(mouse, backBtn)) {
                PlayClick();
                currentScreen = GameScreen::START;
                return;
            }
        }

        // ESC from settings: back to START screen (SOUND)
        if (IsKeyPressed(KEY_ESCAPE)) {
            PlayClick();
            currentScreen = GameScreen::START;
        }
    }

    void DrawSoundSettings() {
        ClearBackground(BG_COLOR);

        int margin = 20;
        int cardX = margin;
        int cardY = margin;
        int cardW = screenWidth - 2 * margin;
        int cardH = screenHeight - 2 * margin;

        DrawRectangle(cardX + 6, cardY + 8, cardW, cardH, Fade(BLACK, 0.18f));
        DrawRectangle(cardX, cardY, cardW, cardH, RAYWHITE);
        DrawRectangleLines(cardX, cardY, cardW, cardH, BLACK);

        // Title
        DrawTextSmooth("SOUND SETTINGS", cardX + 340, cardY + 30, 32, BLACK, 2.0f);

        int xLabel = cardX + 140;
        int xValue = cardX + 550;
        int yStart = cardY + 120;
        int dy = 60;

        auto drawField = [&](int index, const char* label, bool enabled) {
            int y = yStart + index * dy;
            string value = enabled ? "ENABLED" : "DISABLED";

            DrawTextSmooth(label, xLabel, y, 24, BLACK, 2.0f);

            // Active vs normal border
            if (soundSettingsFieldIndex == index) {
                DrawRectangleLinesEx(
                    { (float)(xValue - 10), (float)(y - 5), 320, 36 },
                    2.5f, MAROON
                ); 
            } else {
                DrawRectangleLines(
                    xValue - 10, y - 5, 320, 36,
                    DARKGRAY
                );
            }

            // Draw field text
            Color valColor = enabled ? DARKGREEN : RED;
            DrawTextSmooth(value.c_str(), xValue, y, 22, valColor, 1.5f);
        };

        drawField(0, "GAME SOUNDS :", soundEnabled);
        drawField(1, "BACKGROUND MUSIC :", musicEnabled);


        // Button at bottom: BACK
        int btnW = 200;
        int btnH = 50;
        int btnY = cardY + cardH - 130;

        Rectangle backBtn  = { (float)(cardX + cardW / 2 - btnW / 2), (float)btnY, (float)btnW, (float)btnH };

        Vector2 mouse = GetMousePosition();

        DrawButton(backBtn,  "BACK TO START", true, CheckCollisionPointRec(mouse, backBtn));

        string tip = "Tip: Use ↑/↓ or click to select. ←/→/ENTER to toggle on/off. ESC to go back.";
        DrawTextCentered(tip, cardX + cardW / 2, cardY + cardH - 40, 18, DARKGRAY);

    }

    // ============================================================
    // LEADERBOARD SCREEN
    // ============================================================

    void UpdateLeaderboard() {
        // Geometry for card + buttons
        int margin = 20;
        int cardX = margin;
        int cardY = margin;
        int cardW = screenWidth - 2 * margin;
        int cardH = screenHeight - 2 * margin;
        int btnW = 200;
        int btnH = 50;
        int btnY = cardY + cardH - 130;

        Rectangle backBtn  = { (float)(cardX + cardW / 2 - btnW / 2), (float)btnY, (float)btnW, (float)btnH };
        Vector2 mouse = GetMousePosition();

        // Handle BACK button click (SOUND)
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mouse, backBtn)) {
            PlayClick();
            currentScreen = GameScreen::START;
            return;
        }

        // Return to start screen on ESC or ENTER (SOUND)
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER)) {
            PlayClick();
            currentScreen = GameScreen::START;
        }
    }

    void DrawLeaderboard() {
        ClearBackground(BG_COLOR);

        int margin = 20;
        int cardX = margin;
        int cardY = margin;
        int cardW = screenWidth - 2 * margin;
        int cardH = screenHeight - 2 * margin;

        DrawRectangle(cardX + 6, cardY + 8, cardW, cardH, Fade(BLACK, 0.18f));
        DrawRectangle(cardX, cardY, cardW, cardH, RAYWHITE);
        DrawRectangleLines(cardX, cardY, cardW, cardH, BLACK);
        
        DrawTextCentered("LEADERBOARD", screenWidth / 2, cardY + 40, 40, MAROON, 2.5f);
        
        DrawTextCentered("Top 3 Scores", screenWidth / 2, cardY + 100, 24, BLACK, 2.0f);
        
        int yStart = cardY + 180;
        int dy = 50;
        
        if (leaderboard.empty()) {
            DrawTextCentered("No scores recorded yet!", screenWidth / 2, yStart, 22, DARKGRAY, 1.5f);
        } else {
            for (size_t i = 0; i < leaderboard.size(); ++i) {
                const PlayerScore& entry = leaderboard[i];
                string rankStr = "#" + std::to_string(i + 1);
                string scoreStr = std::to_string(entry.score) + " pts";
                
                Color rankColor = RAYWHITE;
                // Assign metal colors for the top 3 ranks
                if (i == 0) rankColor = GOLD;
                else if (i == 1) rankColor = LIGHTGRAY; // Silver-ish
                else if (i == 2) rankColor = BROWN; // Bronze-ish

                // Draw Rank Box (Centered)
                int xRank = screenWidth / 2 - 200;
                DrawRectangle(xRank - 10, yStart + i * dy - 5, 420, 36, LIGHTGRAY);
                DrawRectangleLines(xRank - 10, yStart + i * dy - 5, 420, 36, DARKGRAY);

                DrawTextSmooth(rankStr.c_str(), xRank, yStart + i * dy, 24, rankColor, 1.5f);
                DrawTextSmooth(entry.name.c_str(), xRank + 80, yStart + i * dy, 24, BLACK, 1.5f);
                DrawTextSmooth(scoreStr.c_str(), xRank + 300, yStart + i * dy, 24, MAROON, 1.5f);
            }
        }

        // --- BACK BUTTON ---
        int btnW = 200;
        int btnH = 50;
        int btnY = cardY + cardH - 130;
        Rectangle backBtn  = { (float)(cardX + cardW / 2 - btnW / 2), (float)btnY, (float)btnW, (float)btnH };
        Vector2 mouse = GetMousePosition();

        DrawButton(backBtn,  "BACK TO START", true, CheckCollisionPointRec(mouse, backBtn));

        DrawTextCentered("Press ENTER or ESC to return to Start Screen.", 
                         screenWidth / 2, cardY + cardH - 40, 20, DARKGRAY);
    }


    // ============================================================
    // ENTER WORD SCREEN
    // ============================================================

    void UpdateEnterWord() {
        if (IsKeyPressed(KEY_ESCAPE)) {
            PlayClick();
            currentScreen = GameScreen::SETTINGS;
            return;
        }

        // Character input: (TYPING AND BACKSPACE - NO SOUND)
        int ch = GetCharPressed();
        while (ch > 0) {
            char c = (char)ch;
            if (inputStep == 0) 
            {
                // allow letters + space in secret word
                if ((std::isalpha((unsigned char)c) || c == ' ') && inputWord.size() < 20) {
                    inputWord.push_back(c);
                }
            }

            else {
                if (inputHint.size() < 40 && c >= 32 && c <= 126) {
                    inputHint.push_back(c);
                }
            }
            ch = GetCharPressed();
        }

        if (IsKeyPressed(KEY_BACKSPACE)) {
            if (inputStep == 0 && !inputWord.empty()) {
                inputWord.pop_back();
            }
            else if (inputStep == 1 && !inputHint.empty()) {
                inputHint.pop_back();
            }
        }

        // Geometry for card, inputs, and buttons
        int margin = 20;
        int cardX = margin;
        int cardY = margin;
        int cardW = screenWidth - 2 * margin;
        int cardH = screenHeight - 2 * margin;

        int xValue = cardX + 280;

        Rectangle wordBox = { (float)(xValue - 10), (float)(cardY + 115), 360.0f, 40.0f };
        Rectangle hintBox = { (float)(xValue - 10), (float)(cardY + 185), 480.0f, 40.0f };

        int btnW = 200;
        int btnH = 50;
        int btnY = cardY + cardH - 130;

        Rectangle backBtn  = { (float)(cardX + 80), (float)btnY, (float)btnW, (float)btnH };
        Rectangle nextBtn  = { (float)(cardX + cardW - 80 - btnW), (float)btnY, (float)btnW, (float)btnH };

        Vector2 mouse = GetMousePosition();

        bool triggerEnter = false;

        // Mouse interactions
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            // Click in word/hint boxes to switch step (NO SOUND)
            if (CheckCollisionPointRec(mouse, wordBox)) {
                inputStep = 0;
            } else if (CheckCollisionPointRec(mouse, hintBox)) {
                inputStep = 1;
            }

            // Click BACK (SOUND)
            if (CheckCollisionPointRec(mouse, backBtn)) {
                PlayClick();
                currentScreen = GameScreen::SETTINGS;
                return;
            }

            // Click NEXT / START (SOUND)
            if (CheckCollisionPointRec(mouse, nextBtn)) {
                PlayClick();
                triggerEnter = true;
            }
        }

        // Keyboard ENTER
        if (IsKeyPressed(KEY_ENTER)) {
            triggerEnter = true;
        }

        if (triggerEnter) {
            inputErrorMsg.clear();

            if (inputStep == 0) {
                if (!WordValid(inputWord)) {
                    inputErrorMsg = "Word must contain at least one letter!";
                } else {
                    inputStep = 1;
                }
            } else {
                if (inputHint.empty()) {
                    inputErrorMsg = "Hint cannot be empty!";
                } else {
                    secretWord = inputWord;
                    hint = inputHint;
                    shownWord = secretWord;

                    // Mask all non-space characters
                    for (size_t i = 0; i < shownWord.size(); ++i) {
                         if (std::isalpha((unsigned char)shownWord[i]))
                            shownWord[i] = '*';
                    }
                    
                    triedLetters.clear();
                    lives = 0;
                    gameOver = false;
                    win = false;
                    timeLeft = (float)timeLimitSeconds;

                    currentScreen = GameScreen::PLAYING;
                }
            }
        }
    }

    void DrawEnterWord() {
        ClearBackground(BG_COLOR);

        int margin = 20;
        int cardX = margin;
        int cardY = margin;
        int cardW = screenWidth - 2 * margin;
        int cardH = screenHeight - 2 * margin;

        DrawRectangle(cardX + 6, cardY + 8, cardW, cardH, Fade(BLACK, 0.18f));
        DrawRectangle(cardX, cardY, cardW, cardH, RAYWHITE);
        DrawRectangleLines(cardX, cardY, cardW, cardH, BLACK);

        string title = "WORD ENTRY - " + SetterName();
        DrawTextSmooth(title, cardX + 40, cardY + 30, 30, BLACK, 2.0f);

        string roundStr = "ROUND " + std::to_string(currentRound) +
                          " OF " + std::to_string(totalRounds);
        DrawTextSmooth(roundStr.c_str(), cardX + cardW - 260, cardY + 35, 22, DARKGRAY, 2.0f);

        int xLabel = cardX + 60;
        int xValue = cardX + 280;

        DrawTextSmooth("SECRET WORD:", xLabel, cardY + 120, 24, BLACK, 2.0f);
        if (inputStep == 0) {
            DrawRectangleLinesEx(
                { (float)(xValue - 10), (float)(cardY + 115), 360, 40 },
                3.0f,
                MAROON
            );
        } else {
            DrawRectangleLinesEx(
                { (float)(xValue - 10), (float)(cardY + 115), 360, 40 },
                2.0f,
                DARKGRAY
            );
        }
        DrawTextSmooth(inputWord.c_str(), xValue, cardY + 120, 24, BLACK);

        DrawTextSmooth("HINT:", xLabel, cardY + 190, 24, BLACK, 2.0f);
        if (inputStep == 1) {
            DrawRectangleLinesEx(
                { (float)(xValue - 10), (float)(cardY + 185), 480, 40 },
                3.0f,
                MAROON
            );
        } else {
            DrawRectangleLinesEx(
                { (float)(xValue - 10), (float)(cardY + 185), 480, 40 },
                2.0f,
                DARKGRAY
            );
        }

        DrawTextSmooth(inputHint.c_str(), xValue, cardY + 190, 24, BLACK);

        if (!inputErrorMsg.empty())
            DrawTextSmooth(inputErrorMsg.c_str(), xLabel, cardY + 250, 22, RED);

        // Buttons at bottom: BACK and NEXT/START
        int btnW = 200;
        int btnH = 50;
        int btnY = cardY + cardH - 130;

        Rectangle backBtn  = { (float)(cardX + 80), (float)btnY, (float)btnW, (float)btnH };
        Rectangle nextBtn  = { (float)(cardX + cardW - 80 - btnW), (float)btnY, (float)btnW, (float)btnH };

        Vector2 mouse = GetMousePosition();

        DrawButton(backBtn, "BACK", false, CheckCollisionPointRec(mouse, backBtn));
        DrawButton(nextBtn, "NEXT", true, CheckCollisionPointRec(mouse, nextBtn));

        string tip = "Click a box to edit. Press NEXT or ENTER to continue. ESC returns to settings.";
        DrawTextCentered(tip, cardX + cardW / 2, cardY + cardH - 40, 18, DARKGRAY);

    }

    // ============================================================
    // PLAYING SCREEN
    // ============================================================

    void UpdatePlaying() {
        if (IsKeyPressed(KEY_ESCAPE)) {
            PlayClick();
            currentScreen = GameScreen::SETTINGS;
            return;
        }

        // Geometry (used for mouse-button 'Next' and keyboard clicks)
        int margin = 20;
        int cardX = margin;
        int cardY = margin;
        int cardW = screenWidth - 2 * margin;
        int cardH = screenHeight - 2 * margin;

        int sidebarW = 220;
        int mainX = cardX + sidebarW + 30;
        int mainY = cardY + 20;
        int mainW = cardW - sidebarW - 50;

        int kbStartX = mainX + 40;
        int kbStartY = mainY + 60 + 80;  // wordY = mainY + 60, then +80

        int btnW = 200;
        int btnH = 50;
        int btnY = cardY + cardH - 130;
        Rectangle nextBtn = { (float)(cardX + cardW - 80 - btnW), (float)btnY, (float)btnW, (float)btnH };
        Vector2 mouse = GetMousePosition();

        float dt = GetFrameTime();

        // update animation timers
        if (wrongShakeTimer > 0.0f) {
            wrongShakeTimer -= dt;
            if (wrongShakeTimer < 0.0f) wrongShakeTimer = 0.0f;
        }
        if (winJumpTimer > 0.0f) {
            winJumpTimer -= dt;
            if (winJumpTimer < 0.0f) winJumpTimer = 0.0f;
        }

        if (!gameOver) {
            timeLeft -= dt;
            if (timeLeft <= 0.0f) {
                timeLeft = 0.0f;
                gameOver = true;
                win = false;
                AwardScore(false);
                return;
            }

            // Keyboard input
            HandleGuessInput();

            // Mouse clicking on on-screen keyboard
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                int boxSize = 40;
                int boxGap  = 10;
                int kbCols  = 7;

                for (int i = 0; i < 26; ++i) {
                    int row = i / kbCols;
                    int col = i % kbCols;
                    int bx = kbStartX + col * (boxSize + boxGap);
                    int by = kbStartY + row * (boxSize + boxGap);

                    Rectangle keyBox = { (float)bx, (float)by, (float)boxSize, (float)boxSize };
                    if (CheckCollisionPointRec(mouse, keyBox)) {
                        char letter = 'A' + i;
                        if (triedLetters.find(letter) == string::npos) {
                            triedLetters.push_back(letter);
                            ProcessGuess(letter);
                            // PlayClick(); // Removed: No sound for guesses
                        }
                        break;
                    }
                }
            }
        } else {
            // gameOver = true: allow ENTER or clicking NEXT button (SOUND)
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (CheckCollisionPointRec(mouse, nextBtn)) {
                    PlayClick();
                    AdvanceRoundOrSummary();
                    return;
                }
            }

            if (IsKeyPressed(KEY_ENTER)) {
                PlayClick();
                AdvanceRoundOrSummary();
            }
        }
    }

    void AdvanceRoundOrSummary() {
        if (currentRound < totalRounds) {
            currentRound++;
            SwapRoles();
            ResetRoundState();
            ResetWordInput();
            currentScreen = GameScreen::ENTER_WORD;
        } else {
            currentScreen = GameScreen::SUMMARY;
        }
    }

    void DrawPlaying() {
        ClearBackground(BG_COLOR);

        int margin = 20;
        int cardX = margin;
        int cardY = margin;
        int cardW = screenWidth - 2 * margin;
        int cardH = screenHeight - 2 * margin;

        DrawRectangle(cardX + 6, cardY + 8, cardW, cardH, Fade(BLACK, 0.18f));
        DrawRectangle(cardX, cardY, cardW, cardH, RAYWHITE);
        DrawRectangleLines(cardX, cardY, cardW, cardH, BLACK);

        // ---- Left sidebar: round + scores ----
        int sidebarW = 220;   // narrower now
        DrawRectangle(cardX, cardY, sidebarW, cardH, LIGHTGRAY);
        DrawRectangleLines(cardX, cardY, sidebarW, cardH, BLACK);

        // Header bar
        DrawRectangle(cardX, cardY, sidebarW, 40, BLACK);
        string puzzleHeader = "ROUND " + std::to_string(currentRound) +
                              " OF " + std::to_string(totalRounds);
        DrawTextSmooth(puzzleHeader.c_str(), cardX + 10, cardY + 10, 20, RAYWHITE, 2.0f);

        // Player rows
        int rowY = cardY + 60;
        int rowH = 70;

        auto drawPlayerRow = [&](const string& name, int score, bool isGuesser) {
            Color nameColor = isGuesser ? BLACK : DARKGRAY;
            DrawTextSmooth(name.c_str(), cardX + 16, rowY, 22, nameColor, 1.8f);

            string scoreStr = std::to_string(score) + " pts";
            DrawTextSmooth(scoreStr.c_str(), cardX + 16, rowY + 26, 18, nameColor);

            rowY += rowH;
        };

        bool p1Guesser = !player1IsSetter;
        drawPlayerRow(player1Name, player1Score, p1Guesser);
        drawPlayerRow(player2Name, player2Score, !p1Guesser);

        // ---- Right main play area ----
        int mainX = cardX + sidebarW + 30;
        int mainY = cardY + 20;
        int mainW = cardW - sidebarW - 50;
        int mainH = cardH - 40;

        // Top title (hint or default)
        string title = hint.empty() ? "HANGMAN" : ("HINT: " + hint);
        float titleSize = 30.0f;
        float titleSpacing = 2.0f;
        Vector2 titleMeasure = MeasureTextEx(uiFont, title.c_str(), titleSize, titleSpacing);
        int titleX = mainX + (mainW - (int)titleMeasure.x) / 2;
        DrawTextEx(uiFont, title.c_str(),
                   Vector2{ (float)titleX, (float)mainY }, titleSize, titleSpacing, BLACK);

        // Word display
        string spacedWord = SpacedWord(shownWord);
        float wordSize = 36.0f;
        float wordSpacing = 6.0f;
        Vector2 wordMeasure = MeasureTextEx(uiFont, spacedWord.c_str(),
                                            wordSize, wordSpacing);
        int wordX = mainX + (mainW - (int)wordMeasure.x) / 2;
        int wordY = mainY + 60;
        DrawTextEx(uiFont, spacedWord.c_str(),
                   Vector2{ (float)wordX, (float)wordY }, wordSize, wordSpacing, BLACK);

        // Keyboard grid A-Z (visual)
        int kbStartX = mainX + 40;
        int kbStartY = wordY + 80;
        int boxSize = 40;
        int boxGap = 10;
        int kbCols = 7;  // more columns now for better width use

        for (int i = 0; i < 26; ++i) {
            int row = i / kbCols;
            int col = i % kbCols;
            int bx = kbStartX + col * (boxSize + boxGap);
            int by = kbStartY + row * (boxSize + boxGap);

            char letter = 'A' + i;
            bool used = (triedLetters.find(letter) != string::npos);

            if (used) {
                DrawRectangle(bx, by, boxSize, boxSize, LIGHTGRAY);
            }
            DrawRectangleLines(bx, by, boxSize, boxSize, BLACK);

            char txt[2] = { letter, '\0' };
            DrawTextSmooth(txt, bx + 13, by + 9, 22, BLACK, 1.5f);
        }

        int infoY = kbStartY + 4 * (boxSize + boxGap) + 8;
        string livesStr = "Wrong guesses: " + std::to_string(lives) +
                          "/" + std::to_string(maxLivesSetting);
        DrawTextSmooth(livesStr.c_str(), kbStartX, infoY, 20, BLACK);

        string timeStr = "Time left: " + std::to_string((int)timeLeft) + " s";
        DrawTextSmooth(timeStr.c_str(), kbStartX, infoY + 24, 20, BLACK);

        Vector2 mouse = GetMousePosition();

        // Game over / instructions at bottom
        int bottomY = cardY + cardH - 45;
        if (!gameOver) {
            DrawTextSmooth("Type A-Z or click letters to guess. ESC = settings.",
                           kbStartX, bottomY, 18, DARKGRAY);
        } else {
            if (win) {
                string winMsg = "YOU WIN, " + GuesserName() + "!";
                DrawTextSmooth(winMsg.c_str(), kbStartX, bottomY - 70, 24, GREEN, 2.0f);
            } else {
                string loseMsg = "YOU LOSE, " + GuesserName() + "!";
                DrawTextSmooth(loseMsg.c_str(), kbStartX, bottomY - 70, 24, RED, 2.0f);

                string wordMsg = "Word was: " + secretWord;
                DrawTextSmooth(wordMsg.c_str(), kbStartX, bottomY - 45, 20, DARKGRAY);
            }

            // NEXT button (for next round or summary)
            int btnW = 200;
            int btnH = 50;
            int btnY = cardY + cardH - 130;
            Rectangle nextBtn = { (float)(cardX + cardW - 80 - btnW), (float)btnY, (float)btnW, (float)btnH };

            DrawButton(nextBtn, "NEXT", true, CheckCollisionPointRec(mouse, nextBtn));

            DrawTextSmooth("Press ENTER or click NEXT.", kbStartX, bottomY, 18, DARKGRAY);
        }

        // Hangman on the right
        int hangmanX = mainX + mainW - 220;
        int hangmanY = kbStartY + 10;

        DrawHangman(hangmanX, hangmanY);              // main hangman
    }

    void HandleGuessInput() {
        // Handle individual key presses A-Z (NO SOUND)
        for (int key = KEY_A; key <= KEY_Z; key++) {
            if (IsKeyPressed(key)) {
                char ch = 'A' + (key - KEY_A);
                if (triedLetters.find(ch) == string::npos) {
                    triedLetters.push_back(ch);
                    ProcessGuess(ch);
                }
            }
        }
        
        // Handle other character inputs (less crucial for click sound here)
        int c = GetCharPressed();
        while (c > 0) {
            // Processing logic...
            c = GetCharPressed();
        }
    }

    void ProcessGuess(char ch) {
        bool found = false;
        string upperSecret = Upper(secretWord);

        for (size_t i = 0; i < upperSecret.size(); ++i) 
        {
            if (secretWord[i] == ' ') 
            {
                shownWord[i] = ' ';        // auto-show space
                continue;
            }
            if (upperSecret[i] == ch) 
            {
                shownWord[i] = secretWord[i];
                found = true;
            }
        }


        if (!found) {
            lives++;

            // start shake animation (if not hanged yet)
            if (lives < maxLivesSetting) {
                wrongShakeTimer = 0.35f; // ~0.35 seconds of wiggle
            }
            if (lives >= maxLivesSetting) {
                gameOver = true;
                win = false;
                AwardScore(false);
            }
        } else {
            // Check if the word is fully guessed (no remaining '*')
            bool allGuessed = true;
            for (char s_char : shownWord) {
                if (s_char == '*') {
                    allGuessed = false;
                    break;
                }
            }
            
            if (allGuessed) {
                gameOver = true;
                win = true;
                AwardScore(true);

                winJumpTimer = 0.6f; // brief jump animation
            }
        }
    }

    void DrawHangman(int x, int y) {
        // Make him bigger and slightly left/down from anchor
        x -= 40;   // move a bit left
        y += 20;   // move a bit down

        int steps = (lives > 7 ? 7 : lives);

        float T = 7.0f;      // line thickness
        float S = 1.35f;     // overall scale

        // Shake on wrong guess
        float shakeOffsetX = 0.0f;
        if (wrongShakeTimer > 0.0f) {
            float t = GetTime() * 40.0f;
            float amp = 6.0f;
            shakeOffsetX = sinf(t) * amp * (wrongShakeTimer / 0.35f);
        }

        // Jump on win (little vertical bounce)
        float jumpOffsetY = 0.0f;
        if (win && winJumpTimer > 0.0f) {
            float t = (0.6f - winJumpTimer) * 10.0f;
            float amp = 14.0f;
            jumpOffsetY = -sinf(t) * amp * (winJumpTimer / 0.6f);
        }

        x += (int)shakeOffsetX;
        y += (int)jumpOffsetY;

        auto P = [&](float v) { return v * S; };

        // ---- Gallows (thick black) ----
        DrawLineEx(
            { (float)x,            (float)(y + P(120)) },
            { (float)(x + P(120)), (float)(y + P(120)) },
            T, BLACK
        );
        DrawLineEx(
            { (float)(x + P(60)),  (float)(y + P(120)) },
            { (float)(x + P(60)),  (float)(y - P(40))  },
            T, BLACK
        );
        DrawLineEx(
            { (float)(x + P(60)),  (float)(y - P(40))  },
            { (float)(x + P(130)), (float)(y - P(40))  },
            T, BLACK
        );
        DrawLineEx(
            { (float)(x + P(130)), (float)(y - P(40))  },
            { (float)(x + P(130)), (float)(y + P(10))  },
            T, BLACK
        );

        // rounded ends on the vertical post
        DrawCircle((int)(x + P(60)), (int)(y - P(40)),  T * 0.6f, BLACK);
        DrawCircle((int)(x + P(60)), (int)(y + P(120)), T * 0.6f, BLACK);

        // ---- Hangman body ----
        // Head
        if (steps >= 1)
            DrawCircle((int)(x + P(130)), (int)(y + P(10)), P(18), BLACK);

        // Body
        if (steps >= 2)
            DrawLineEx(
                { (float)(x + P(130)), (float)(y + P(25)) },
                { (float)(x + P(130)), (float)(y + P(60)) },
                T, BLACK
            );

        // Left arm
        if (steps >= 3)
            DrawLineEx(
                { (float)(x + P(130)), (float)(y + P(35)) },
                { (float)(x + P(110)), (float)(y + P(50)) },
                T, BLACK
            );

        // Right arm
        if (steps >= 4)
            DrawLineEx(
                { (float)(x + P(130)), (float)(y + P(35)) },
                { (float)(x + P(150)), (float)(y + P(50)) },
                T, BLACK
            );

        // Left leg
        if (steps >= 5)
            DrawLineEx(
                { (float)(x + P(130)), (float)(y + P(60)) },
                { (float)(x + P(115)), (float)(y + P(85)) },
                T, BLACK
            );

        // Right leg
        if (steps >= 6)
            DrawLineEx(
                { (float)(x + P(130)), (float)(y + P(60)) },
                { (float)(x + P(145)), (float)(y + P(85)) },
                T, BLACK
            );

        // X eyes when fully hanged
        if (steps >= 7) {
            float ox = P(130);
            float oy = P(10);
            DrawLineEx({(float)(x + ox - P(8)), (float)(y + oy - P(5))},
                       {(float)(x + ox - P(2)), (float)(y + oy + P(5))}, 3, RED);
            DrawLineEx({(float)(x + ox - P(2)), (float)(y + oy - P(5))},
                       {(float)(x + ox - P(8)), (float)(y + oy + P(5))}, 3, RED);

            DrawLineEx({(float)(x + ox + P(2)), (float)(y + oy - P(5))},
                       {(float)(x + ox + P(8)), (float)(y + oy + P(5))}, 3, RED);
            DrawLineEx({(float)(x + ox + P(8)), (float)(y + oy - P(5))},
                       {(float)(x + ox + P(2)), (float)(y + oy + P(5))}, 3, RED);
        }
    }

    // ============================================================
    // SUMMARY SCREEN
    // ============================================================
    void UpdateSummary() {
        int margin = 20;
        int cardX = margin;
        int cardY = margin;
        int cardW = screenWidth - 2 * margin;
        int cardH = screenHeight - 2 * margin;

        int btnW = 220;
        int btnH = 50;
        int btnY = cardY + cardH - 130;

        Rectangle lobbyBtn = { (float)(cardX + 40), (float)btnY, (float)btnW, (float)btnH };
        Rectangle quitBtn  = { (float)(cardX + cardW - 40 - btnW), (float)btnY, (float)btnW, (float)btnH };

        Vector2 mouse = GetMousePosition();

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (CheckCollisionPointRec(mouse, lobbyBtn)) {
                PlayClick();
                UpdateLeaderboardScores();
                ResetToLobby();
                return;
            }
            if (CheckCollisionPointRec(mouse, quitBtn)) {
                PlayClick();
                CloseWindow();
                return;
            }
        }

        if (IsKeyPressed(KEY_ENTER)) {
            PlayClick();
            UpdateLeaderboardScores();
            ResetToLobby();
        }
        if (IsKeyPressed(KEY_ESCAPE)) {
            CloseWindow();
        }
    }

    void ResetToLobby() {
        currentRound = 1;
        player1Score = 0;
        player2Score = 0;
        player1IsSetter = starterIsP1;
        ResetRoundState();
        ResetWordInput();
        ResetSettingsInput();
        currentScreen = GameScreen::START; // Go to Start Menu
    }
    

    void DrawSummary() {
        ClearBackground(BG_COLOR);

        int margin = 20;
        int cardX = margin;
        int cardY = margin;
        int cardW = screenWidth - 2 * margin;
        int cardH = screenHeight - 2 * margin;

        DrawRectangle(cardX + 6, cardY + 8, cardW, cardH, Fade(BLACK, 0.18f));
        DrawRectangle(cardX, cardY, cardW, cardH, RAYWHITE);
        DrawRectangleLines(cardX, cardY, cardW, cardH, BLACK);

        DrawTextSmooth("GAME OVER", cardX + 40, cardY + 40, 34, BLACK, 2.0f);

        string rStr = "Rounds played: " + std::to_string(totalRounds);
        DrawTextSmooth(rStr.c_str(), cardX + 40, cardY + 100, 24, BLACK);

        string p1Str = player1Name + "  -  " + std::to_string(player1Score) + " pts";
        string p2Str = player2Name + "  -  " + std::to_string(player2Score) + " pts";
        DrawTextSmooth(p1Str.c_str(), cardX + 40, cardY + 150, 24, BLACK);
        DrawTextSmooth(p2Str.c_str(), cardX + 40, cardY + 190, 24, BLACK);

        // Winner text (bigger + colored + centered)
        string winnerMsg;
        Color winnerColor = BLACK;

        if (player1Score > player2Score) {
            winnerMsg = "WINNER: " + player1Name;
            winnerColor = GREEN;
        } else if (player2Score > player1Score) {
            winnerMsg = "WINNER: " + player2Name;
            winnerColor = GREEN;
        } else {
            winnerMsg = "IT'S A TIE!";
            winnerColor = DARKGRAY;
        }

        int winnerFontSize = 40;
        float winnerSpacing = 2.0f;
        Vector2 textSize = MeasureTextEx(uiFont, winnerMsg.c_str(),
                                        (float)winnerFontSize, winnerSpacing);

        int winnerX = cardX + (cardW - (int)textSize.x) / 2;
        int winnerY = cardY + 240;

        DrawTextEx(uiFont, winnerMsg.c_str(),
                Vector2{ (float)winnerX, (float)winnerY },
                (float)winnerFontSize, winnerSpacing, winnerColor);

        // Buttons: back to lobby & quit
        int btnW = 220;
        int btnH = 50;
        int btnY = cardY + cardH - 130;

        Rectangle lobbyBtn = { (float)(cardX + 40), (float)btnY, (float)btnW, (float)btnH };
        Rectangle quitBtn  = { (float)(cardX + cardW - 40 - btnW), (float)btnY, (float)btnW, (float)btnH };

        Vector2 mouse = GetMousePosition();

        DrawButton(lobbyBtn, "MAIN MENU", true, CheckCollisionPointRec(mouse, lobbyBtn));
        DrawButton(quitBtn,  "QUIT GAME",     false, CheckCollisionPointRec(mouse, quitBtn));

        DrawTextSmooth("ENTER / MAIN MENU   |   ESC / QUIT GAME",
                    cardX + 40, cardY + cardH - 40, 20, DARKGRAY);

        // ---- Hangman standing on the right ----
        int oldLives = lives;
        bool oldWin = win;

        lives = 6;   // draw full body (steps 1..6, 7 adds X eyes)
        win = false; // avoid jump animation

        int hangmanX = cardX + cardW - 260;
        int hangmanY = cardY + 140;
        DrawHangman(hangmanX, hangmanY);

        lives = oldLives;
        win = oldWin;
    }
};

// ============================================================
// main
// ============================================================
int main() {
    const int W = 1200;   // bigger window
    const int H = 680;

    InitWindow(W, H, "Hangman - 2 Player (OOP + Raylib)");
    SetTargetFPS(60);
    
    // Initialize audio device
    InitAudioDevice(); 

    HangmanGame game(W, H);

    while (!WindowShouldClose()) {
        game.Update();

        BeginDrawing();
        game.Draw();
        EndDrawing();
    }
    
    // Close audio device
    CloseAudioDevice(); 

    CloseWindow();
    return 0;
}
