#include <RGBmatrixPanel.h>  // LEDマトリクスのライブラリ
#include <TimerThree.h>

// 以下LEDマトリクスのポートの設定
const int CLK = 11;
const int OE = 9;
const int LAT = 10;
const int A = A0;
const int B = A1;
const int C = A2;
const int D = A3;

int pointerX, pointerY;  // 現在のポインタが指す場所を管理(X -> 横, Y -> 縦)
int directX, directY;    // joystickの倒している方向を管理
unsigned long baseTime;  // センタークリックの時間管理に用いる
int leftEdge = -3;       // 左端の位置
int nLeftEdge;  // playingモードからeditモードに復帰する際に使うleftEdge

RGBmatrixPanel matrix(A, B, C, D, CLK, LAT, OE, false,
                      64);  // RGBmatrixPanelのインスタンスを生成
uint16_t black = matrix.Color888(0, 0, 0);  // 黒色の設定
const uint16_t pointerColor =
    matrix.Color888(230, 230, 230);  // ポインタの色の設定
const uint16_t vLine1Color =
    matrix.Color888(90, 100, 20);  // 小節の線(粗めに分割)の色
const uint16_t vLine2Color =
    matrix.Color888(85, 65, 55);  // 小節の線(細かく分割)の色

const int pitchColor[12][3] = {{240, 0, 0},   {240, 120, 0}, {240, 240, 0},
                               {120, 240, 0}, {0, 240, 0},   {0, 240, 120},
                               {0, 240, 240}, {0, 120, 240}, {0, 0, 240},
                               {120, 0, 240}, {240, 0, 240}, {240, 0, 120}};
// ↑音階(C~A)による色の設定
uint16_t midiColor[32];
// ↑一番下から上までの行ごとのLEDの色の設定

const int miniNote = 32;    // 音の長さの最小単位(miniNote分音符)
// ↓measureSizeの宣言をこの間に記述
const int measureSize = 4;
// ↑measureSizeの宣言をこの間に記述
const int noteSize = miniNote * measureSize;  // notesのサイズ

// ↓notesの宣言をこの間に記述
int notes[noteSize] = {3, 3, 3, 3, 3, 3, 3, 3, 8, 8, -1, -1, 7, 7, 7, 7, 5, 5, 5, 5, 3, 3, 5, 5, 7, 7, -1, -1, 8, 8, 8, 8, 10, 10, 10, 10, 10, 10, -1, -1, 10, 10, 10, 10, 7, 7, 7, 7, 3, 3, 3, 3, 3, 3, 3, 3, 5, 5, -1, -1, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, -1, -1, -1, -1, 7, 7, 7, 7, 5, 5, 5, 5, -1, -1, -1, -1, 3, 3, 3, 3, 3, 3, 3, 3, 7, 7, 7, 7, -1, -1, -1, -1, 10, 10, 10, 10, -1, -1, -1, -1, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10};
// ↑notesの宣言をこの間に記述
// ↑楽譜を格納。各列当たりどの行にノーツが入っているか保持する。これが-1のときどの行にもノーツはない。

// 以下joystickのピンの設定
const int XPIN = A8;
const int YPIN = A9;
const int SCPIN = 8;
int nowSC;
const int maxSizeX = 64;
const int maxSizeY = 32;

// 以下演奏の際の設定
bool playing = false;
const int BPM = 180;
unsigned long tmptime;
unsigned long base;
int playLine = 0;
float waveF[32];
volatile int mainWave = LOW;
bool isStop = false;  // 一時停止中かどうか
bool mute = true; // 無音中かどうか
bool isTest = false; // 試聴中かどうか
const int testTime = 400;
unsigned long testBase;
int basePlaySw = HIGH;

const int wavePin = 7;
const int playPin = 6;

bool firstInput = true;
int baseSC = LOW;

int vLine1[measureSize];
int vLine2[3 * measureSize];

void setup() {
    pinMode(XPIN, INPUT);      // A0ピンを入力に(省略可)
    pinMode(YPIN, INPUT);      // A1ピンを入力に(省略可)
    pinMode(SCPIN, INPUT);     // D2ピンをプルアップして入力に
    pinMode(wavePin, OUTPUT);  // 波形出力のピンを設定
    pinMode(playPin, INPUT);
    directX = 0;
    directY = 0;
    pointerX = 0;
    pointerY = 0;
    void drawVLine();
    for (int i = 0; i < 32; i++) {
        midiColor[i] =
            matrix.Color888(pitchColor[i % 12][0], pitchColor[i % 12][1],
                            pitchColor[i % 12][2]);
    }
    decideVLine();
    matrix.begin();
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 32; j++) {
            matrix.drawPixel(i, j, midiColor[j]);
        }
    }
    Serial.begin(9600);
    drawVeLine();

    for (int i = 0; i < 32; i++) {
        waveF[i] = getWaveF(31 - i);
    }

    base = 60000 / (BPM * 8);

    Timer3.initialize(1000000);  // マイクロ秒単位で設定
    Timer3.attachInterrupt(reverse);
    firstView(playing);
}

void loop() {
    if (playing) {
        if (isStop) {
            int nowPlaySw = digitalRead(playPin);
            if (nowPlaySw == LOW && basePlaySw == HIGH) {
                isStop = false;
            }
            basePlaySw = nowPlaySw;
            return;
        }
        if (playLine < noteSize - 1) {
            Serial.println(1);
            unsigned long now = millis();
            int nowPlaySw = digitalRead(playPin);
            if (nowPlaySw == LOW && basePlaySw == HIGH) {
                isStop = true;
                basePlaySw = nowPlaySw;
                mute = true;
                return;
            }
            basePlaySw = nowPlaySw;
            if (now - tmptime >= base) {
                tmptime = now;
                if ((playLine << 11) == 0) {
                    for (int j = 0; j < 32; j++) {
                        matrix.drawPixel(playLine - leftEdge, j, vLine1Color);
                    }
                } else if ((playLine << 13) == 0) {
                    for (int j = 0; j < 32; j++) {
                        matrix.drawPixel(playLine - leftEdge, j, vLine2Color);
                    }
                } else {
                    for (int j = 0; j < 32; j++) {
                        matrix.drawPixel(playLine - leftEdge, j, black);
                    }
                }
                if (notes[playLine] != -1) {
                    matrix.drawPixel(playLine - leftEdge, notes[playLine],
                                     midiColor[notes[playLine]]);
                }
                playLine++;
                if (playLine - leftEdge >= 61 && leftEdge < noteSize - 61) {
                    wholeMove(32);
                }
                for (int j = 0; j < 32; j++) {
                    matrix.drawPixel(playLine - leftEdge, j, pointerColor);
                }
            }
            if (notes[playLine] == -1) {
                mute = true;
            } else {
                mute = false;
                Timer3.initialize(m(waveF[notes[playLine]]));
            }
        } else {
            mute = true;
            leftEdge = nLeftEdge;
            playing = false;
            firstView(false);
        }
    } else {
        int nowPlaySw = digitalRead(playPin);
        if (nowPlaySw == LOW && basePlaySw == HIGH) {
            tmptime = millis();
            playLine = 0;
            nLeftEdge = leftEdge;
            leftEdge = -3;
            base = 60000 / (BPM * 8);
            firstView(true);
            playing = true;
            basePlaySw = nowPlaySw;
            return;
        }
        basePlaySw = nowPlaySw;

        int newPosX = analogRead(XPIN);
        int newPosY = analogRead(YPIN);
        nowSC = digitalRead(SCPIN);
        /* // デバッグ用
        if (nowSC == HIGH){
          Serial.println(1);
        } else {
          Serial.println(0);
        }
        */
        if (isTest){
            unsigned long now = millis();
            if (now - testBase >= testTime){
                isTest = false;
                mute = true;
                Serial.println(now);
            }
        }
        if (nowSC == LOW && baseSC == HIGH) {
            if (notes[pointerX] == pointerY) {
                notes[pointerX] = -1;
            } else if (notes[pointerX] != -1) {
                if ((pointerX << 11) == 0) {
                    matrix.drawPixel(pointerX - leftEdge, notes[pointerX],
                                     vLine1Color);
                } else if ((pointerX << 13) == 0) {
                    matrix.drawPixel(pointerX - leftEdge, notes[pointerX],
                                     vLine2Color);
                } else {
                    matrix.drawPixel(pointerX - leftEdge, notes[pointerX],
                                     black);
                }
                notes[pointerX] = pointerY;
            } else {
                notes[pointerX] = pointerY;
            }
            Timer3.initialize(m(waveF[pointerY]));
            testBase = millis();
            mute = false;
            isTest = true;
            Serial.println(testBase);
        }
        baseSC = nowSC;
        int newDirectX = (newPosX - 512 >= 0);
        int newDirectY = (newPosY - 512 <= 0);

        if (newDirectX) {
            if (abs(newPosX - 512) <= 200) {
                newDirectX = 0;
            }
        } else {
            if (abs(newPosX - 512) > 200) {
                newDirectX = -1;
            }
        }

        if (newDirectY) {
            if (abs(newPosY - 512) <= 200) {
                newDirectY = 0;
            }
        } else {
            if (abs(newPosY - 512) > 200) {
                newDirectY = -1;
            }
        }

        if (newDirectX != directX || newDirectY != directY) {
            baseTime = millis();
            firstInput = true;
            directX = newDirectX;
            directY = newDirectY;
            pixelMove();
        } else if (firstInput) {
            unsigned long nowTime = millis();
            if (nowTime - baseTime > 400) {
                pixelMove();
                firstInput = false;
                baseTime = nowTime;
            }
        } else {
            unsigned long nowTime = millis();
            if (nowTime - baseTime > 50) {
                pixelMove();
                baseTime = nowTime;
            }
        }
    }
}

void wholeMove(int moveVel) {
    matrix.fillScreen(black);
    leftEdge += moveVel;
    drawVeLine();

    for (int i = 0; i < 64; i++) {
        if (i + leftEdge <= -1 || i + leftEdge >= noteSize) {
            for (int j = 0; j < 32; j++) {
                matrix.drawPixel(i, j, midiColor[j]);
            }
        } else if (notes[i + leftEdge] != -1) {
            matrix.drawPixel(i, notes[i + leftEdge],
                             midiColor[notes[i + leftEdge]]);
        }
    }
    if (moveVel < 0 && noteSize - 1 < leftEdge + 64) {
        for (int j = 0; j < 32; j++) {
            matrix.drawPixel(noteSize - 1 - leftEdge, j, black);
        }
    }
}

void pixelMove() {
    int tempPointerX = max(min(pointerX + directX, noteSize - 1), 0);
    int tempPointerY = max(min(pointerY + directY, maxSizeY - 1), 0);
    matrix.drawPixel(pointerX - leftEdge, pointerY, black);
    if (tempPointerX - leftEdge >= 61 && leftEdge < noteSize - 61) {
        wholeMove(32);
    } else if (tempPointerX - leftEdge < 3 && leftEdge > -3) {
        wholeMove(-32);
    } else if (notes[pointerX] == pointerY) {
        matrix.drawPixel(pointerX - leftEdge, pointerY, midiColor[pointerY]);
    } else if ((pointerX << 11) == 0) {
        matrix.drawPixel(pointerX - leftEdge, pointerY, vLine1Color);
    } else if ((pointerX << 13) == 0) {
        matrix.drawPixel(pointerX - leftEdge, pointerY, vLine2Color);
    }
    pointerX = tempPointerX;
    pointerY = tempPointerY;
    matrix.drawPixel(pointerX - leftEdge, pointerY, pointerColor);
    if (directX != 0 && directY == 0) {
        if (nowSC == LOW) {
            if (notes[pointerX] == pointerY) {
                notes[pointerX] = -1;
            } else if (notes[pointerX] != -1) {
                if ((pointerX << 11) == 0) {
                    matrix.drawPixel(pointerX - leftEdge, notes[pointerX],
                                     vLine1Color);
                } else if ((pointerX << 13) == 0) {
                    matrix.drawPixel(pointerX - leftEdge, notes[pointerX],
                                     vLine2Color);
                } else {
                    matrix.drawPixel(pointerX - leftEdge, notes[pointerX],
                                     black);
                }
                notes[pointerX] = pointerY;
            } else {
                notes[pointerX] = pointerY;
            }
        }
    }
}

void drawVeLine() {
    int t = decideVLine();
    while (t < leftEdge + 64 && t < noteSize) {
        if ((t << 11) == 0) {
            for (int j = 0; j < 32; j++) {
                matrix.drawPixel(t - leftEdge, j, vLine1Color);
            }
        } else {
            for (int j = 0; j < 32; j++) {
                matrix.drawPixel(t - leftEdge, j, vLine2Color);
            }
        }
        t += 8;
    }
}

int decideVLine() {
    int firstVLine1 = ((leftEdge + 7) / 8) * 8;
    return firstVLine1;
}

float getWaveF(int i) {
    float f = 220;
    return pow(2, (float)(i - 9) / 12) * f;
}
int m(float f) { return int(1000000 / (2 * f)); }

void reverse() {
    if (mute){
        return;
    }
    if (mainWave == HIGH) {
        mainWave = LOW;
    } else {
        mainWave = HIGH;
    }
    digitalWrite(wavePin, mainWave);
}

void firstView(bool isPlaying) {  // 状態が変わる際に最初の画面を出す。
    matrix.fillScreen(black);
    if (leftEdge <
        0) {  // 左端の虹色(右側の虹色は未実装なのでよろしくお願いします)(←やりました)
        for (int i = leftEdge; i < 0; i++) {
            for (int j = 0; j < 32; j++) {
                matrix.drawPixel(i - leftEdge, j, midiColor[j]);
            }
        }
    }
    if (leftEdge + 31 >= noteSize) {
        for (int i = 31; i + leftEdge >= noteSize; i--) {
            for (int j = 0; j < 32; j++) {
                matrix.drawPixel(i, j, midiColor[j]);
            }
        }
    }
    drawVeLine();
    if (isPlaying) {
        for (int i = 0; i < 32; i++) {
            matrix.drawPixel(playLine - leftEdge, i, pointerColor);
        }
    } else {
        matrix.drawPixel(pointerX - leftEdge, pointerY, pointerColor);
    }
    for (int i = 0; i < 64; i++) {
        if (i + leftEdge < 0 || i + leftEdge >= noteSize) {
            continue;
        }
        matrix.drawPixel(i, notes[i + leftEdge],
                         midiColor[notes[i + leftEdge]]);
    }
}
