
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

// ---------------- PIN CONFIG ----------------
#define PIN_JOY_X   A0
#define PIN_JOY_Y   A1
#define PIN_JOY_SW  2
#define PIN_BUZZER  3
#define TFT_CS      7
#define TFT_DC      6
#define TFT_RST     5

// ---------------- DISPLAY ----------------
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
#define SCR_W 128
#define SCR_H 160

// ---------------- COLORS ----------------
#define C_BG    ST77XX_BLACK
#define C_FG    ST77XX_WHITE
#define C_SNAKE ST77XX_GREEN
#define C_FOOD  ST77XX_RED
#define C_DINO  ST77XX_WHITE
#define C_OBST  ST77XX_YELLOW
#define C_SEL   ST77XX_CYAN

// ---------------- JOYSTICK ----------------
int joyCenterX = 512, joyCenterY = 512;
const int JOY_DEADZONE = 150;

enum Dir { NONE, UP, DOWN, LEFT, RIGHT };

Dir readDir() {
  int x = analogRead(PIN_JOY_X) - joyCenterX;
  int y = analogRead(PIN_JOY_Y) - joyCenterY;
  if (abs(x) < JOY_DEADZONE && abs(y) < JOY_DEADZONE) return NONE;
  if (abs(x) > abs(y)) {
    return (x > 0) ? RIGHT : LEFT;
  } else {
    return (y > 0) ? DOWN : UP;
  }
}

bool buttonPressed() {
  static bool lastState = HIGH;
  bool state = digitalRead(PIN_JOY_SW);
  bool pressed = (lastState == HIGH && state == LOW);
  lastState = state;
  return pressed;
}

// ---------------- BUZZER ----------------
void beep(int freq, int dur) {
  tone(PIN_BUZZER, freq, dur);
}
void beepGameOver() {
  tone(PIN_BUZZER, 400, 150); delay(150);
  tone(PIN_BUZZER, 300, 150); delay(150);
  tone(PIN_BUZZER, 200, 300); delay(300);
  noTone(PIN_BUZZER);
}

// ---------------- APP STATE ----------------
enum AppState { MENU, SNAKE_GAME, TETRIS_GAME, DINO_GAME, GAME_OVER };
AppState state = MENU;
int menuIndex = 0;
const char* menuItems[3] = {"Snake", "Tetris", "Dino"};
unsigned long lastGameOverTime = 0;
int lastScore = 0;
const char* lastGameName = "";

void calibrateJoystick() {
  long sx = 0, sy = 0;
  for (int i = 0; i < 20; i++) {
    sx += analogRead(PIN_JOY_X);
    sy += analogRead(PIN_JOY_Y);
    delay(5);
  }
  joyCenterX = sx / 20;
  joyCenterY = sy / 20;
}

// ================= SETUP =================
void setup() {
  pinMode(PIN_JOY_SW, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);

  tft.initR(INITR_BLACKTAB); // change to INITR_GREENTAB / INITR_144GREENTAB if colors look off
  tft.setRotation(0);        // 0 = portrait (128x160). Use 1 or 3 for landscape.
  tft.fillScreen(C_BG);

  calibrateJoystick();
  drawMenu();
}

// ================= MAIN LOOP =================
void loop() {
  switch (state) {
    case MENU:        loopMenu();   break;
    case SNAKE_GAME:   loopSnake();  break;
    case TETRIS_GAME:  loopTetris(); break;
    case DINO_GAME:    loopDino();   break;
    case GAME_OVER:    loopGameOver(); break;
  }
}

// ================= MENU =================
void drawMenu() {
  tft.fillScreen(C_BG);
  tft.setTextSize(2);
  tft.setTextColor(C_FG);
  tft.setCursor(10, 10);
  tft.print("GAME HUB");
  tft.drawFastHLine(0, 32, SCR_W, C_FG);

  tft.setTextSize(2);
  for (int i = 0; i < 3; i++) {
    tft.setCursor(20, 50 + i * 25);
    if (i == menuIndex) {
      tft.setTextColor(C_SEL);
      tft.print("> ");
    } else {
      tft.setTextColor(C_FG);
      tft.print("  ");
    }
    tft.print(menuItems[i]);
  }
}

unsigned long lastMenuMove = 0;
void loopMenu() {
  if (millis() - lastMenuMove > 200) {
    Dir d = readDir();
    if (d == UP) {
      menuIndex = (menuIndex + 2) % 3;
      beep(600, 40);
      drawMenu();
      lastMenuMove = millis();
    } else if (d == DOWN) {
      menuIndex = (menuIndex + 1) % 3;
      beep(600, 40);
      drawMenu();
      lastMenuMove = millis();
    }
  }
  if (buttonPressed()) {
    beep(900, 60);
    if (menuIndex == 0) { initSnake(); state = SNAKE_GAME; }
    else if (menuIndex == 1) { initTetris(); state = TETRIS_GAME; }
    else { initDino(); state = DINO_GAME; }
  }
}

void goToGameOver(const char* gameName, int score) {
  lastGameName = gameName;
  lastScore = score;
  lastGameOverTime = millis();
  beepGameOver();
  tft.fillScreen(C_BG);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_RED);
  tft.setCursor(15, 40);
  tft.print("GAME OVER");
  tft.setTextSize(1);
  tft.setTextColor(C_FG);
  tft.setCursor(20, 70);
  tft.print(gameName);
  tft.setCursor(20, 85);
  tft.print("Score: ");
  tft.print(score);
  tft.setCursor(10, 120);
  tft.print("Press button");
  tft.setCursor(10, 132);
  tft.print("to return to menu");
  state = GAME_OVER;
}

void loopGameOver() {
  if (millis() - lastGameOverTime > 400 && buttonPressed()) {
    beep(700, 50);
    drawMenu();
    state = MENU;
  }
}

// ======================================================
//                        SNAKE
// ======================================================
#define CELL 8
#define GRID_W (SCR_W / CELL)      // 16
#define GRID_TOP 16                 // leave room for score bar
#define GRID_H ((SCR_H - GRID_TOP) / CELL) // 18

#define MAX_SNAKE 100
int snakeX[MAX_SNAKE], snakeY[MAX_SNAKE];
int snakeLen;
Dir snakeDir, snakeNextDir;
int foodX, foodY;
int snakeScore;
unsigned long lastSnakeMove;
int snakeSpeed; // ms per move

void placeFood() {
  bool ok;
  do {
    ok = true;
    foodX = random(0, GRID_W);
    foodY = random(0, GRID_H);
    for (int i = 0; i < snakeLen; i++) {
      if (snakeX[i] == foodX && snakeY[i] == foodY) { ok = false; break; }
    }
  } while (!ok);
}

void initSnake() {
  snakeLen = 3;
  snakeX[0] = GRID_W / 2;     snakeY[0] = GRID_H / 2;
  snakeX[1] = GRID_W / 2 - 1; snakeY[1] = GRID_H / 2;
  snakeX[2] = GRID_W / 2 - 2; snakeY[2] = GRID_H / 2;
  snakeDir = RIGHT;
  snakeNextDir = RIGHT;
  snakeScore = 0;
  snakeSpeed = 180;
  placeFood();
  lastSnakeMove = millis();

  tft.fillScreen(C_BG);
  drawSnakeScoreBar();
  for (int i = 0; i < snakeLen; i++) drawCell(snakeX[i], snakeY[i], C_SNAKE);
  drawCell(foodX, foodY, C_FOOD);
}

void drawCell(int gx, int gy, uint16_t color) {
  tft.fillRect(gx * CELL, GRID_TOP + gy * CELL, CELL - 1, CELL - 1, color);
}

void drawSnakeScoreBar() {
  tft.fillRect(0, 0, SCR_W, GRID_TOP, C_BG);
  tft.setTextSize(1);
  tft.setTextColor(C_FG);
  tft.setCursor(2, 4);
  tft.print("Score: ");
  tft.print(snakeScore);
}

void loopSnake() {
  Dir d = readDir();
  if (d != NONE) {
    // prevent reversing directly into itself
    if (!((d == UP && snakeDir == DOWN) || (d == DOWN && snakeDir == UP) ||
          (d == LEFT && snakeDir == RIGHT) || (d == RIGHT && snakeDir == LEFT))) {
      snakeNextDir = d;
    }
  }

  if (millis() - lastSnakeMove < snakeSpeed) return;
  lastSnakeMove = millis();
  snakeDir = snakeNextDir;

  int nx = snakeX[0], ny = snakeY[0];
  switch (snakeDir) {
    case UP: ny--; break;
    case DOWN: ny++; break;
    case LEFT: nx--; break;
    case RIGHT: nx++; break;
    default: break;
  }

  // wall collision
  if (nx < 0 || nx >= GRID_W || ny < 0 || ny >= GRID_H) {
    goToGameOver("Snake", snakeScore);
    return;
  }
  // self collision
  for (int i = 0; i < snakeLen; i++) {
    if (snakeX[i] == nx && snakeY[i] == ny) {
      goToGameOver("Snake", snakeScore);
      return;
    }
  }

  bool grow = (nx == foodX && ny == foodY);

  // erase tail if not growing
  if (!grow) {
    tft.fillRect(snakeX[snakeLen - 1] * CELL, GRID_TOP + snakeY[snakeLen - 1] * CELL, CELL - 1, CELL - 1, C_BG);
  }

  // shift body
  int limit = grow ? snakeLen : snakeLen - 1;
  for (int i = limit; i > 0; i--) {
    snakeX[i] = snakeX[i - 1];
    snakeY[i] = snakeY[i - 1];
  }
  snakeX[0] = nx; snakeY[0] = ny;

  if (grow) {
    if (snakeLen < MAX_SNAKE) snakeLen++;
    snakeScore += 10;
    beep(1200, 30);
    if (snakeSpeed > 70) snakeSpeed -= 4;
    placeFood();
    drawCell(foodX, foodY, C_FOOD);
    drawSnakeScoreBar();
  }

  drawCell(snakeX[0], snakeY[0], C_SNAKE);
}

// ======================================================
//                        TETRIS
// ======================================================
#define TET_COLS 8
#define TET_ROWS 16
#define TET_CELL 12
#define TET_OX 4     // origin x offset in px
#define TET_OY 8     // origin y offset in px

uint16_t tetBoard[TET_ROWS][TET_COLS]; // 0 = empty, else color

// 7 tetrominoes, 4 rotations each, 4x4 grid, bit-encoded rows
const uint16_t TETROMINOES[7][4] = {
  {0x0F00, 0x2222, 0x00F0, 0x4444}, // I
  {0x8E00, 0x6440, 0x0E20, 0x44C0}, // J
  {0x2E00, 0x4460, 0x0E80, 0xC440}, // L
  {0x6600, 0x6600, 0x6600, 0x6600}, // O
  {0x06C0, 0x4620, 0x06C0, 0x4620}, // S
  {0x0E40, 0x4C40, 0x4E00, 0x4640}, // T
  {0x0C60, 0x2640, 0x0C60, 0x2640}  // Z
};
const uint16_t TET_COLORS[7] = {
  ST77XX_CYAN, ST77XX_BLUE, ST77XX_ORANGE, ST77XX_YELLOW,
  ST77XX_GREEN, ST77XX_MAGENTA, ST77XX_RED
};

int curPiece, curRot, curX, curY;
int tetScore;
unsigned long lastTetDrop;
int tetDropInterval;

bool cellFilled(int piece, int rot, int px, int py) {
  uint16_t shape = TETROMINOES[piece][rot];
  int bitIndex = py * 4 + px;
  return (shape >> (15 - bitIndex)) & 1;
}

bool checkCollision(int piece, int rot, int ox, int oy) {
  for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
      if (cellFilled(piece, rot, px, py)) {
        int bx = ox + px;
        int by = oy + py;
        if (bx < 0 || bx >= TET_COLS || by >= TET_ROWS) return true;
        if (by >= 0 && tetBoard[by][bx] != 0) return true;
      }
    }
  }
  return false;
}

void newPiece() {
  curPiece = random(0, 7);
  curRot = 0;
  curX = TET_COLS / 2 - 2;
  curY = -1;
  if (checkCollision(curPiece, curRot, curX, curY)) {
    goToGameOver("Tetris", tetScore);
  }
}

void initTetris() {
  for (int r = 0; r < TET_ROWS; r++)
    for (int c = 0; c < TET_COLS; c++)
      tetBoard[r][c] = 0;
  tetScore = 0;
  tetDropInterval = 500;
  tft.fillScreen(C_BG);
  drawTetrisFrame();
  newPiece();
  lastTetDrop = millis();
  drawTetrisBoard();
}

void drawTetrisFrame() {
  tft.setTextSize(1);
  tft.setTextColor(C_FG);
  tft.setCursor(TET_OX, 1);
  tft.print("Score:0");
  int bw = TET_COLS * TET_CELL;
  int bh = TET_ROWS * TET_CELL;
  tft.drawRect(TET_OX - 1, TET_OY - 1, bw + 2, bh + 2, C_FG);
}

void drawTetrisScore() {
  tft.fillRect(TET_OX, 1, 90, 8, C_BG);
  tft.setTextSize(1);
  tft.setTextColor(C_FG);
  tft.setCursor(TET_OX, 1);
  tft.print("Score:");
  tft.print(tetScore);
}

void drawBoardCell(int bx, int by, uint16_t color) {
  int px = TET_OX + bx * TET_CELL;
  int py = TET_OY + by * TET_CELL;
  if (color == 0) tft.fillRect(px, py, TET_CELL - 1, TET_CELL - 1, C_BG);
  else tft.fillRect(px, py, TET_CELL - 1, TET_CELL - 1, color);
}

void drawTetrisBoard() {
  for (int r = 0; r < TET_ROWS; r++)
    for (int c = 0; c < TET_COLS; c++)
      drawBoardCell(c, r, tetBoard[r][c]);
  drawCurrentPiece(TET_COLORS[curPiece]);
}

void drawCurrentPiece(uint16_t color) {
  for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
      if (cellFilled(curPiece, curRot, px, py)) {
        int bx = curX + px, by = curY + py;
        if (by >= 0) drawBoardCell(bx, by, color);
      }
    }
  }
}

void eraseCurrentPiece() {
  for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
      if (cellFilled(curPiece, curRot, px, py)) {
        int bx = curX + px, by = curY + py;
        if (by >= 0) drawBoardCell(bx, by, tetBoard[by][bx]);
      }
    }
  }
}

void lockPiece() {
  for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
      if (cellFilled(curPiece, curRot, px, py)) {
        int bx = curX + px, by = curY + py;
        if (by >= 0 && by < TET_ROWS && bx >= 0 && bx < TET_COLS)
          tetBoard[by][bx] = TET_COLORS[curPiece];
      }
    }
  }
  // clear full lines
  int cleared = 0;
  for (int r = TET_ROWS - 1; r >= 0; r--) {
    bool full = true;
    for (int c = 0; c < TET_COLS; c++) if (tetBoard[r][c] == 0) { full = false; break; }
    if (full) {
      cleared++;
      for (int rr = r; rr > 0; rr--)
        for (int c = 0; c < TET_COLS; c++)
          tetBoard[rr][c] = tetBoard[rr - 1][c];
      for (int c = 0; c < TET_COLS; c++) tetBoard[0][c] = 0;
      r++; // re-check same row index after shift
    }
  }
  if (cleared > 0) {
    tetScore += cleared * 100;
    beep(1000 + cleared * 200, 100);
    if (tetDropInterval > 150) tetDropInterval -= cleared * 15;
    drawTetrisBoard();
    drawTetrisScore();
  }
}

unsigned long lastTetInput = 0;
void loopTetris() {
  Dir d = readDir();
  if (d != NONE && millis() - lastTetInput > 150) {
    lastTetInput = millis();
    eraseCurrentPiece();
    if (d == LEFT && !checkCollision(curPiece, curRot, curX - 1, curY)) curX--;
    else if (d == RIGHT && !checkCollision(curPiece, curRot, curX + 1, curY)) curX++;
    else if (d == DOWN && !checkCollision(curPiece, curRot, curX, curY + 1)) { curY++; }
    else if (d == UP) {
      int nrot = (curRot + 1) % 4;
      if (!checkCollision(curPiece, nrot, curX, curY)) curRot = nrot;
    }
    drawCurrentPiece(TET_COLORS[curPiece]);
  }

  if (buttonPressed()) {
    // hard drop
    eraseCurrentPiece();
    while (!checkCollision(curPiece, curRot, curX, curY + 1)) curY++;
    lockPiece();
    beep(500, 40);
    newPiece();
    drawTetrisBoard();
    lastTetDrop = millis();
    return;
  }

  if (millis() - lastTetDrop > (unsigned long)tetDropInterval) {
    lastTetDrop = millis();
    eraseCurrentPiece();
    if (!checkCollision(curPiece, curRot, curX, curY + 1)) {
      curY++;
      drawCurrentPiece(TET_COLORS[curPiece]);
    } else {
      drawCurrentPiece(TET_COLORS[curPiece]);
      lockPiece();
      newPiece();
      drawTetrisBoard();
    }
  }
}

// ======================================================
//                     DINO RUNNER
// ======================================================
#define DINO_GROUND_Y 130
#define DINO_X 20
#define DINO_W 12
#define DINO_H 14

float dinoY, dinoVY;
bool dinoJumping;
int dinoScore;
unsigned long lastDinoFrame;
int dinoFrameInterval = 30;

#define MAX_OBST 3
struct Obstacle { float x; bool active; int w; int h; };
Obstacle obstacles[MAX_OBST];
float gameSpeed;
unsigned long dinoScoreTimer;

void initDino() {
  dinoY = DINO_GROUND_Y;
  dinoVY = 0;
  dinoJumping = false;
  dinoScore = 0;
  gameSpeed = 2.5;
  for (int i = 0; i < MAX_OBST; i++) obstacles[i].active = false;
  obstacles[0].active = true;
  obstacles[0].x = SCR_W + 20;
  obstacles[0].w = 8;
  obstacles[0].h = 16;
  tft.fillScreen(C_BG);
  tft.drawFastHLine(0, DINO_GROUND_Y + DINO_H, SCR_W, C_FG);
  dinoScoreTimer = millis();
  lastDinoFrame = millis();
}

void drawDinoScore() {
  tft.fillRect(0, 0, SCR_W, 10, C_BG);
  tft.setTextSize(1);
  tft.setTextColor(C_FG);
  tft.setCursor(2, 2);
  tft.print("Score:");
  tft.print(dinoScore);
}

int prevDinoYi = -999;
int prevObstXi[MAX_OBST];

void loopDino() {
  if (millis() - lastDinoFrame < (unsigned long)dinoFrameInterval) return;
  lastDinoFrame = millis();

  // input: jump
  Dir d = readDir();
  if ((d == UP || buttonPressed()) && !dinoJumping) {
    dinoJumping = true;
    dinoVY = -6.5;
    beep(800, 40);
  }

  // physics
  if (dinoJumping) {
    dinoY += dinoVY;
    dinoVY += 0.45; // gravity
    if (dinoY >= DINO_GROUND_Y) {
      dinoY = DINO_GROUND_Y;
      dinoJumping = false;
      dinoVY = 0;
    }
  }

  // erase old dino
  int oldYi = (int)dinoY;
  tft.fillRect(DINO_X, 0, DINO_W, DINO_GROUND_Y + DINO_H, C_BG);
  tft.drawFastHLine(0, DINO_GROUND_Y + DINO_H, SCR_W, C_FG);

  // update obstacles
  for (int i = 0; i < MAX_OBST; i++) {
    if (obstacles[i].active) {
      tft.fillRect((int)obstacles[i].x, DINO_GROUND_Y + DINO_H - obstacles[i].h,
                   obstacles[i].w, obstacles[i].h, C_BG);
      obstacles[i].x -= gameSpeed;
      if (obstacles[i].x < -obstacles[i].w) {
        obstacles[i].active = false;
      }
    }
  }
  // spawn new obstacle if none active and randomly
  bool anyActive = false;
  for (int i = 0; i < MAX_OBST; i++) if (obstacles[i].active) anyActive = true;
  if (!anyActive) {
    for (int i = 0; i < MAX_OBST; i++) {
      if (!obstacles[i].active) {
        obstacles[i].active = true;
        obstacles[i].x = SCR_W + random(10, 40);
        obstacles[i].w = random(6, 10);
        obstacles[i].h = random(12, 22);
        break;
      }
    }
  }

  // draw dino (simple rectangle body)
  tft.fillRect(DINO_X, (int)dinoY - DINO_H, DINO_W, DINO_H, C_DINO);

  // draw obstacles + collision check
  for (int i = 0; i < MAX_OBST; i++) {
    if (obstacles[i].active) {
      int ox = (int)obstacles[i].x;
      int oy = DINO_GROUND_Y + DINO_H - obstacles[i].h;
      tft.fillRect(ox, oy, obstacles[i].w, obstacles[i].h, C_OBST);
      // AABB collision
      int dinoTop = (int)dinoY - DINO_H;
      bool overlapX = (DINO_X < ox + obstacles[i].w) && (DINO_X + DINO_W > ox);
      bool overlapY = (dinoTop < oy + obstacles[i].h) && ((int)dinoY > oy);
      if (overlapX && overlapY) {
        goToGameOver("Dino", dinoScore);
        return;
      }
    }
  }

  // score & speed ramp
  if (millis() - dinoScoreTimer > 100) {
    dinoScoreTimer = millis();
    dinoScore += 1;
    if (dinoScore % 50 == 0) gameSpeed += 0.2;
  }
  drawDinoScore();
}
