#if !defined(PICO_RP2350)
#error "You *MUST* use RP2350 board. This cannot work on RP2040 board"
#endif

#define DBG_BEGIN() {Serial.begin(9600); for (int dbg_lp1 = 0; dbg_lp1 < 10; dbg_lp1++) { if (Serial) {break;} delay(10);}}
#define DBG_PRINT(msg) {if (Serial) {Serial.print(msg);}}
#define DBG_PRINTLN(msg) {if (Serial) {Serial.println(msg);}}
#define DBG_PRINTF(...) {if (Serial) {Serial.printf(__VA_ARGS__);}}
#define DBG_PRINTFLN(...) {if (Serial) {Serial.printf(__VA_ARGS__);Serial.println();}}

#include <SdFat.h>
#if !defined(USE_UTF8_LONG_NAMES) || (USE_UTF8_LONG_NAMES == 0)
#error "You must define USE_UTF8_LONG_NAMES = 1 in libraries/SdFat/src/SdFatConfig.h"
#endif
#include <SPI.h>

#include "char_table.h"

#define ENABLE_EXFAT
#ifdef ENABLE_EXFAT
#define SD_OBJ SdFs
#define FILE_OBJ FsFile
#else
#define SD_OBJ SdFat
#define FILE_OBJ FsFile
#endif
SD_OBJ SD;
FILE_OBJ file;
FILE_OBJ w_file;

unsigned long m_lop = 128;
char m_name[40];
char m_name_dos[120];
char f_name[40];
char f_name_dos[120];
char new_name[40];
char new_name_dos[120];

char c_name[40];
//#define S_DATA_LEN 256
#define S_DATA_LEN 1024
byte s_data[S_DATA_LEN];

uint8_t ASCIItoUTF8(char ascii_char, char *utf8_char);
void ASCIItoUTF8_str(char *ascii_str, char *utf8_str);
void UTF8toASCII_str(char *utf8_str, char *ascii_str);

#define CSPIN_SD  (PIN_SPI0_SS) // 17
#define CHKPIN    (2)
#define PB0PIN    (3)
#define PB1PIN    (4)
#define PB2PIN    (5)
#define PB3PIN    (6)
#define PB4PIN    (7)
#define PB5PIN    (8)
#define PB6PIN    (9)
#define PB7PIN    (10)
#define FLGPIN    (11)
#define PA0PIN    (12)
#define PA1PIN    (13)
#define PA2PIN    (14)
#define PA3PIN    (15)

// ファイル名は、ロングファイルネーム形式対応
boolean eflg;

// Raspberry Pi Pico2: CSn(SS)=GPIO17, RX(MISO)=GPIO16, TX(MOSI)=GPIO19, SCK=GPIO18
#define SD_SPI_SPEED 16
#define SD_CONFIG SdSpiConfig(CSPIN_SD, SHARED_SPI, SD_SCK_MHZ(SD_SPI_SPEED))

void sdinit(void) {
  DBG_PRINTFLN("sdinit()");

  // SD初期化
  for (uint8_t lp1 = 0; lp1 < 10; lp1++) {
    if (SD.begin(SD_CONFIG)) {
      DBG_PRINTFLN("sdinit() SD opened");
      eflg = false;
      return;
    }

    if (lp1 == 10) {
      DBG_PRINTFLN("sdinit() SD open failed. retrying...");
    }

    delay(100);
  }

  DBG_PRINTFLN("sdinit() SD open error");
  eflg = true;
}

void setup() {
  DBG_BEGIN();
  DBG_PRINTFLN("setup()");

  pinMode(CSPIN_SD, OUTPUT);
  pinMode(CHKPIN, INPUT);  //CHK
  pinMode(PB0PIN, OUTPUT); //送信データ
  pinMode(PB1PIN, OUTPUT); //送信データ
  pinMode(PB2PIN, OUTPUT); //送信データ
  pinMode(PB3PIN, OUTPUT); //送信データ
  pinMode(PB4PIN, OUTPUT); //送信データ
  pinMode(PB5PIN, OUTPUT); //送信データ
  pinMode(PB6PIN, OUTPUT); //送信データ
  pinMode(PB7PIN, OUTPUT); //送信データ
  pinMode(FLGPIN, OUTPUT); //FLG

  pinMode(PA0PIN, INPUT_PULLUP); //受信データ
  pinMode(PA1PIN, INPUT_PULLUP); //受信データ
  pinMode(PA2PIN, INPUT_PULLUP); //受信データ
  pinMode(PA3PIN, INPUT_PULLUP); //受信データ

  digitalWrite(PB0PIN, LOW);
  digitalWrite(PB1PIN, LOW);
  digitalWrite(PB2PIN, LOW);
  digitalWrite(PB3PIN, LOW);
  digitalWrite(PB4PIN, LOW);
  digitalWrite(PB5PIN, LOW);
  digitalWrite(PB6PIN, LOW);
  digitalWrite(PB7PIN, LOW);
  digitalWrite(FLGPIN, LOW);

  sdinit();
}

//4BIT受信
byte rcv4bit(void) {
//HIGHになるまでループ
  while (digitalRead(CHKPIN) != HIGH) {}

//受信
  byte j_data = digitalRead(PA0PIN)+digitalRead(PA1PIN)*2+digitalRead(PA2PIN)*4+digitalRead(PA3PIN)*8;

//FLGをセット
  digitalWrite(FLGPIN,HIGH);

//LOWになるまでループ
  while (digitalRead(CHKPIN) == HIGH) {}

//FLGをリセット
  digitalWrite(FLGPIN,LOW);
  return(j_data);
}

//1BYTE受信
byte rcv1byte(void) {
  byte i_data = (rcv4bit() << 4) | rcv4bit();
  return(i_data);
}

//1BYTE送信
void snd1byte(byte i_data) {
//下位ビットから8ビット分をセット
  digitalWrite(PB0PIN, (i_data) & 0x01);
  digitalWrite(PB1PIN, (i_data >> 1) & 0x01);
  digitalWrite(PB2PIN, (i_data >> 2) & 0x01);
  digitalWrite(PB3PIN, (i_data >> 3) & 0x01);
  digitalWrite(PB4PIN, (i_data >> 4) & 0x01);
  digitalWrite(PB5PIN, (i_data >> 5) & 0x01);
  digitalWrite(PB6PIN, (i_data >> 6) & 0x01);
  digitalWrite(PB7PIN, (i_data >> 7) & 0x01);
  digitalWrite(FLGPIN, HIGH);

//HIGHになるまでループ
  while (digitalRead(CHKPIN) != HIGH) {}
  digitalWrite(FLGPIN,LOW);

//LOWになるまでループ
  while (digitalRead(CHKPIN) == HIGH) {}
}

//小文字->大文字
char upper(char c) {
  if ('a' <= c && c <= 'z') {
    c = c - ('a' - 'A');
  }

  return c;
}

//ファイル名の最後が「.mzt」でなければ付加
void addmzt(char *f_name) {
  unsigned int lp1 = 0;

  while (f_name[lp1] != 0x0D) {
    lp1++;
  }

  if (f_name[lp1 - 4] != '.' ||
     (f_name[lp1 - 3] != 'M' &&
      f_name[lp1 - 3] != 'm') ||
     (f_name[lp1 - 2] != 'Z' &&
      f_name[lp1 - 2] != 'z') ||
     (f_name[lp1 - 1] != 'T' &&
      f_name[lp1 - 1] != 't')) {
         f_name[lp1++] = '.';
         f_name[lp1++] = 'M';
         f_name[lp1++] = 'Z';
         f_name[lp1++] = 'T';
  }

  f_name[lp1] = 0x00;
}

//SDカードにSAVE
void f_save(void) {
  DBG_PRINTFLN("f_save()");

  char p_name[20];

//保存ファイルネーム取得
  for (unsigned int lp1 = 0; lp1 <= 32; lp1++) {
    f_name[lp1] = rcv1byte();
  }

  addmzt(f_name);
  ASCIItoUTF8_str(f_name, f_name_dos);

//プログラムネーム取得
  for (unsigned int lp1 = 0; lp1 <= 16; lp1++) {
    p_name[lp1] = rcv1byte();
  }

  p_name[15] = 0x0D;
  p_name[16] = 0x00;

//スタートアドレス取得
  int s_adrs1 = rcv1byte();
  int s_adrs2 = rcv1byte();

//スタートアドレス算出
  unsigned int s_adrs = s_adrs1 | (s_adrs2 << 8);

//エンドアドレス取得
  int e_adrs1 = rcv1byte();
  int e_adrs2 = rcv1byte();

//エンドアドレス算出
  unsigned int e_adrs = e_adrs1 | (e_adrs2 << 8);

//実行アドレス取得
  int g_adrs1 = rcv1byte();
  int g_adrs2 = rcv1byte();

//実行アドレス算出
  unsigned int g_adrs = g_adrs1 | (g_adrs2 << 8);

//ファイルサイズ算出
  unsigned int f_length = e_adrs - s_adrs + 1;
  unsigned int f_length1 = f_length & 0xff;
  unsigned int f_length2 = f_length >> 8;

//ファイルが存在すればdelete
  if (SD.exists(f_name_dos)) {
    SD.remove(f_name_dos);
  }

//ファイルオープン
  FILE_OBJ file = SD.open(f_name_dos, FILE_WRITE);
  if (file.isOpen()) {
//状態コード送信(OK)
    snd1byte(0x00);

//ファイルモード設定(01)
    file.write(char(0x01));

//プログラムネーム
    file.write(p_name);
    file.write(char(0x00));

//ファイルサイズ
    file.write(f_length1);
    file.write(f_length2);

//スタートアドレス
    file.write(s_adrs1);
    file.write(s_adrs2);

//実行アドレス
    file.write(g_adrs1);
    file.write(g_adrs2);

//7Fまで00埋め
    for (unsigned int lp1 = 0; lp1 <= 103; lp1++) {
      file.write(char(0x00));
    }

//実データ
    while (f_length > 0) {
      unsigned int f_blk_len = (f_length >= S_DATA_LEN ? S_DATA_LEN : f_length);
      for (unsigned int lp1 = 0; lp1 < f_blk_len; lp1++) {
        s_data[lp1] = rcv1byte();
      }

      file.write(s_data, f_length);
      f_length -= f_blk_len;
    }

    file.close();
   }
   else {
//状態コード送信(ERROR)
    snd1byte(0xF1);
    sdinit();
  }
}

//SDカードから読込
void f_load(void) {
  DBG_PRINTFLN("f_load()");

//ファイルネーム取得
  for (unsigned int lp1 = 0; lp1 <= 32; lp1++) {
    f_name[lp1] = rcv1byte();
  }

  addmzt(f_name);
  ASCIItoUTF8_str(f_name, f_name_dos);

//ファイルが存在しなければERROR
  if (SD.exists(f_name_dos)) {
//ファイルオープン
    FILE_OBJ file = SD.open(f_name_dos, FILE_READ);
    if (file.isOpen()) {

//状態コード送信(OK)
      snd1byte(0x00);
      int wk1 = 0;
      wk1 = file.read();
      for (unsigned int lp1 = 0; lp1 <= 16; lp1++) {
        wk1 = file.read();
        snd1byte(wk1);
      }

//ファイルサイズ取得
      int f_length2 = file.read();
      int f_length1 = file.read();
      unsigned int f_length = (f_length1 << 8) | f_length2;

//スタートアドレス取得
      int s_adrs2 = file.read();
      int s_adrs1 = file.read();
      unsigned int s_adrs = (s_adrs1 << 8) | s_adrs2;

//実行アドレス取得
      int g_adrs2 = file.read();
      int g_adrs1 = file.read();
      unsigned int g_adrs = (g_adrs1 << 8) | g_adrs2;
      snd1byte(s_adrs2);
      snd1byte(s_adrs1);
      snd1byte(f_length2);
      snd1byte(f_length1);
      snd1byte(g_adrs2);
      snd1byte(g_adrs1);
      file.seek(128);

//データ送信
      for (unsigned int lp1 = 0; lp1 < f_length; lp1++) {
          byte i_data = file.read();
          snd1byte(i_data);
      }

      file.close();
      sdinit();
     }
     else {
//状態コード送信(ERROR)
      snd1byte(0xFF);
      sdinit();
     }  
   }
   else {
//状態コード送信(FILE NOT FIND ERROR)
    snd1byte(0xF1);
    sdinit();
  }
}

//ASTART 指定されたファイルをファイル名「0000.mzt」としてコピー
void astart(void) {
  DBG_PRINTFLN("astart()");

  char w_name[] = "0000.mzt";

//ファイルネーム取得
  for (unsigned int lp1 = 0; lp1 <= 32; lp1++) {
    f_name[lp1] = rcv1byte();
  }

  addmzt(f_name);
  ASCIItoUTF8_str(f_name, f_name_dos);

//ファイルが存在しなければERROR
  if (SD.exists(f_name_dos)) {

//0000.mztが存在すればdelete
    if (SD.exists(w_name)) {
      SD.remove(w_name);
    }

//ファイルオープン
    FILE_OBJ file_r = SD.open(f_name_dos, FILE_READ);
    FILE_OBJ file_w = SD.open(w_name, FILE_WRITE);
    if (file_r.isOpen()) {
//実データ
      unsigned int f_length = file_r.size();
      while (f_length > 0) {
        unsigned int f_blk_len = (f_length >= S_DATA_LEN ? S_DATA_LEN : f_length);
        file_r.read(s_data, f_blk_len);
        file_w.write(s_data, f_blk_len);
        f_length -= f_blk_len;
      }

      file_w.close();
      file_r.close();

//状態コード送信(OK)
      snd1byte(0x00);
    }
    else {
//状態コード送信(ERROR)
      snd1byte(0xF1);
      sdinit();
    }
  }
  else {
//状態コード送信(ERROR)
    snd1byte(0xF1);
    sdinit();
  }  
}

// SD-CARDのFILELIST
void dirlist(void) {
  DBG_PRINTFLN("dirlist()");

//比較文字列取得 32+1文字まで
  for (unsigned int lp1 = 0; lp1 <= 32; lp1++) {
    c_name[lp1] = rcv1byte();
    DBG_PRINTFLN("%02X", c_name[lp1]);
  }

//SDのルートディレクトリを開く
  FILE_OBJ file = SD.open("/");
  FILE_OBJ entry = file.openNextFile();
  int cntl2 = 0;
  unsigned int br_chk = 0;
  int page = 1;

//全件出力の場合には20件出力したところで一時停止、キー入力により継続、打ち切りを選択
  while (br_chk == 0) {
    if (entry) {
      entry.getName(f_name_dos, sizeof(f_name_dos) - 1);
      UTF8toASCII_str(f_name_dos, f_name);

//一件送信
//比較文字列でファイルネームを先頭10文字まで比較して一致するものだけを出力
      if (f_match(f_name, c_name)) {
        unsigned int lp1 = 0;
        while (lp1 <= 36 && f_name[lp1] != 0x00) {
          snd1byte(upper(f_name[lp1]));
          lp1++;
        }

        snd1byte(0x0D);
        snd1byte(0x00);
        cntl2++;
      }
    }

    if (!entry || cntl2 > 19) {
//継続・打ち切り選択指示要求
      snd1byte(0xfe);

//選択指示受信(0:継続 B:前ページ 以外:打ち切り)
      br_chk = rcv1byte();
      DBG_PRINTFLN("dirlist(): CMD \"%c\"(%02X)", br_chk, br_chk);

//前ページ処理
      if (br_chk == 0x42) {
//先頭ファイルへ
        file.rewindDirectory();

//entry値更新
        entry = file.openNextFile();

//もう一度先頭ファイルへ
        file.rewindDirectory();
        if (page <= 2) {
//現在ページが1ページ又は2ページなら1ページ目に戻る処理
          page = 0;
        }
        else {
//現在ページが3ページ以降なら前々ページまでのファイルを読み飛ばす
          page -= 2;
          cntl2 = 0;
          while (cntl2 < page * 20) {
            entry = file.openNextFile();
            if (f_match(f_name, c_name)) {
              cntl2++;
            }
          }
        }

        br_chk = 0;
      }

      page++;
      cntl2 = 0;
    }

//ファイルがまだあるなら次読み込み、なければ打ち切り指示
    if (entry) {
      entry = file.openNextFile();
    }
    else{
      br_chk = 1;
    }

//FDLの結果が20件未満なら継続指示要求せずにそのまま終了
    if (!entry && cntl2 < 20 && page == 1) {
      break;
    }
  }

//処理終了指示
  snd1byte(0xFF);
  snd1byte(0x00);
}

//f_nameとc_nameをc_nameに0x00が出るまで比較
//FILENAME COMPARE
boolean f_match(char *f_name, char *c_name) {
  DBG_PRINTFLN("f_match()");

  boolean flg1 = true;
  unsigned int lp1 = 0;
  DBG_PRINTF("%s %s ", f_name, c_name);
  while (lp1 <= 32 && c_name[0] != 0x00 && flg1 == true) {
    DBG_PRINTF("%02X ", f_name[lp1]);
    DBG_PRINTF("%02X ", c_name[lp1 + 1]);
    if (upper(f_name[lp1]) != c_name[lp1 + 1]) {
      flg1 = false;
    }
    lp1++;
    if (c_name[lp1 + 1] == 0x00) {
      break;
    }
  }
  DBG_PRINTLN("");

  return flg1;
}

//FILE DELETE
void f_del(void) {
  DBG_PRINTFLN("f_del()");

//ファイルネーム取得
  for (unsigned int lp1 = 0; lp1 <= 32; lp1++) {
    f_name[lp1] = rcv1byte();
  }

  addmzt(f_name);
  ASCIItoUTF8_str(f_name, f_name_dos);

//ファイルが存在しなければERROR
  if (SD.exists(f_name_dos)) {
//状態コード送信(OK)
    snd1byte(0x00);

//処理選択を受信(0:継続してDELETE 0以外:CANSEL)
    if (rcv1byte() == 0x00) {
      if (SD.remove(f_name_dos)) {
//状態コード送信(OK)
        snd1byte(0x00);
      }
      else{
//状態コード送信(Error)
        snd1byte(0xf1);
        sdinit();
      }
    }
    else{
//状態コード送信(Cansel)
      snd1byte(0x01);
    }
  }
  else{
//状態コード送信(Error)
    snd1byte(0xf1);
    sdinit();
  }
}

//FILE RENAME
void f_ren(void) {
  DBG_PRINTFLN("f_ren()");

//現ファイルネーム取得
  for (unsigned int lp1 = 0; lp1 <= 32; lp1++) {
    f_name[lp1] = rcv1byte();
  }

  addmzt(f_name);
  ASCIItoUTF8_str(f_name, f_name_dos);

//ファイルが存在しなければERROR
  if (SD.exists(f_name)) {
//状態コード送信(OK)
    snd1byte(0x00);

//新ファイルネーム取得
    for (unsigned int lp1 = 0; lp1 <= 32; lp1++) {
      new_name[lp1] = rcv1byte();
    }

    addmzt(new_name);
    ASCIItoUTF8_str(new_name, new_name_dos);

//状態コード送信(OK)
    snd1byte(0x00);

    FILE_OBJ file = SD.open(f_name_dos, FILE_WRITE);
    if (file.isOpen()) {
      if (file.rename(new_name_dos)) {
 //状態コード送信(OK)
        snd1byte(0x00);
      }
      else {
 //状態コード送信(OK)
        snd1byte(0xff);
      }

      file.close();
    }
    else {
//状態コード送信(Error)
      snd1byte(0xf1);
      sdinit();
    }
  }
  else{
//状態コード送信(Error)
    snd1byte(0xf1);
    sdinit();
  }
}

//FILE DUMP
void f_dump(void) {
  DBG_PRINTFLN("f_dump()");

  unsigned int br_chk = 0;

//ファイルネーム取得
  for (unsigned int lp1 = 0; lp1 <= 32; lp1++) {
    f_name[lp1] = rcv1byte();
  }

  addmzt(f_name);
  ASCIItoUTF8_str(f_name, f_name_dos);

//ファイルが存在しなければERROR
  if (SD.exists(f_name_dos)) {
//状態コード送信(OK)
    snd1byte(0x00);

//ファイルオープン
    FILE_OBJ file = SD.open(f_name_dos, FILE_READ);
    if (file.isOpen()) {
//実データ送信(1画面:128Byte)
      unsigned int f_length = file.size();
      long lp1 = 0;
      while (lp1 < f_length) {
//画面先頭ADRSを送信
        snd1byte(lp1 & 0xff);
        snd1byte(lp1 >> 8);
        int i = 0;

//実データを送信
        while (i < 128 && lp1 < f_length) {
          snd1byte(file.read());
          i++;
          lp1++;
        }

//FILE ENDが128Byteに満たなかったら残りByteに0x00を送信
        while (i < 128) {
          snd1byte(0x00);
          i++;
        }

//指示待ち
        br_chk = rcv1byte();

//BREAKならポインタをFILE ENDとする
        if (br_chk == 0xff) {
          lp1 = f_length; 
        }

//B:BACKを受信したらポインタを256Byte戻す。先頭画面なら0に戻してもう一度先頭画面表示
        if (br_chk == 0x42) {
          if (lp1 > 255) {
            if (lp1 & 0x7f == 0) {
              lp1 -= 256;
            }
            else {
              lp1 -= 128 + (lp1 & 0x7f);
            }
            file.seek(lp1);
          }
          else{
            lp1 = 0;
            file.seek(0);
          }
        }
      }

//FILE ENDもしくはBREAKならADRSに終了コード0FFFFHを送信
      if (lp1 > f_length - 1) {
        snd1byte(0xff);
        snd1byte(0xff);
      };

      file.close();

//状態コード送信(OK)
      snd1byte(0x00);
    }
    else {
//状態コード送信(ERROR)
      snd1byte(0xF1);
      sdinit();
    }
  }
  else{
//状態コード送信(Error)
    snd1byte(0xf1);
    sdinit();
  }
}

//FILE COPY
void f_copy(void) {
  DBG_PRINTFLN("f_copy()");

//現ファイルネーム取得
  for (unsigned int lp1 = 0; lp1 <= 32; lp1++) {
    f_name[lp1] = rcv1byte();
  }

  addmzt(f_name);
  ASCIItoUTF8_str(f_name, f_name_dos);

//ファイルが存在しなければERROR
  if (SD.exists(f_name_dos)) {
//状態コード送信(OK)
    snd1byte(0x00);

//新ファイルネーム取得
    for (unsigned int lp1 = 0; lp1 <= 32; lp1++) {
      new_name[lp1] = rcv1byte();
    }

    addmzt(new_name);
    ASCIItoUTF8_str(new_name, new_name_dos);

//新ファイルネームと同じファイルネームが存在すればERROR
    if (SD.exists(new_name)) {
//状態コード送信(OK)
      snd1byte(0x00);

//ファイルオープン
      FILE_OBJ file_r = SD.open(f_name_dos, FILE_READ);
      FILE_OBJ file_w = SD.open(new_name_dos, FILE_WRITE);
      if (file_r.isOpen()) {
//実データコピー
        unsigned int f_length = file_r.size();
        while (f_length > 0) {
          unsigned int f_blk_len = (f_length >= S_DATA_LEN ? S_DATA_LEN : f_length);
          file_r.read(s_data, f_blk_len);
          file_w.write(s_data, f_blk_len);
          f_length -= f_blk_len;
        }

        file_w.close();
        file_r.close();

//状態コード送信(OK)
        snd1byte(0x00);
      }
      else{
//状態コード送信(Error)
        snd1byte(0xf1);
        sdinit();
      }
    }
    else{
//状態コード送信(Error)
      snd1byte(0xf3);
      sdinit();
    }
  }
  else{
//状態コード送信(Error)
    snd1byte(0xf1);
    sdinit();
  }
}

//91hで0436H MONITOR ライト インフォメーション代替処理
void mon_whead(void) {
  DBG_PRINTFLN("mon_whead()");

  char m_info[130];

//インフォメーションブロック受信
  for (unsigned int lp1 = 0; lp1 < 128; lp1++) {
    m_info[lp1] = rcv1byte();
  }

//ファイルネーム取り出し
  for (unsigned int lp1 = 0; lp1 < 17; lp1++) {
    m_name[lp1] = m_info[lp1 + 1];
  }

//DOSファイルネーム用に.MZTを付加
  addmzt(m_name);
  ASCIItoUTF8_str(m_name, m_name_dos);

  m_info[16] = 0x0d;

//ファイルが存在すればdelete
  if (SD.exists(m_name_dos)) {
    SD.remove(m_name_dos);
  }

//ファイルオープン
  FILE_OBJ file = SD.open(m_name_dos, FILE_WRITE);
  if (file.isOpen()) {
//状態コード送信(OK)
    snd1byte(0x00);

//インフォメーションブロックwrite
    file.write(m_info, 128);

    file.close();
  }
  else {
//状態コード送信(ERROR)
    snd1byte(0xF1);
    sdinit();
  }
}

//92hで0475H MONITOR ライト データ代替処理
void mon_wdata(void) {
  DBG_PRINTFLN("mon_wdata()");

//ファイルサイズ取得
  int f_length1 = rcv1byte();
  int f_length2 = rcv1byte();

//ファイルサイズ算出
  unsigned int f_length = f_length1 | (f_length2 << 8);

//ファイルオープン
  FILE_OBJ file = SD.open(m_name_dos, FILE_WRITE);
  if (file.isOpen()) {
//状態コード送信(OK)
    snd1byte(0x00);

//実データ
    while (f_length > 0) {
      unsigned int f_blk_len = (f_length >= S_DATA_LEN ? S_DATA_LEN : f_length);
      for (unsigned int lp1 = 0; lp1 < f_blk_len; lp1++) {
        s_data[lp1] = rcv1byte();
      }

      file.write(s_data, f_blk_len);
      f_length -= f_blk_len;
    }

    file.close();
  }
  else {
//状態コード送信(ERROR)
    snd1byte(0xF1);
  }
}

//04D8H MONITOR リード インフォメーション代替処理
void mon_lhead(void) {
  DBG_PRINTFLN("mon_lhead()");

//リード データ POINTクリア
  m_lop = 128;

//ファイルネーム取得
  for (unsigned int lp1 = 0; lp1 <= 32; lp1++) {
    m_name[lp1] = rcv1byte();
  }

  addmzt(m_name);
  ASCIItoUTF8_str(m_name, m_name_dos);

//ファイルが存在しなければERROR
  if (SD.exists(m_name_dos)) {
    snd1byte(0x00);

//ファイルオープン
    FILE_OBJ file = SD.open(m_name_dos, FILE_READ);
    if (file.isOpen()) {
      snd1byte(0x00);
      for (unsigned int lp1 = 0; lp1 < 128; lp1++) {
        byte i_data = file.read();
        snd1byte(i_data);
      }

      file.close();
      snd1byte(0x00);
    }
    else {
//状態コード送信(ERROR)
      snd1byte(0xFF);
      sdinit();
    }  
  }
  else {
//状態コード送信(FILE NOT FIND ERROR)
    snd1byte(0xF1);
    sdinit();
  }
}

//04F8H MONITOR リード データ代替処理
void mon_ldata(void) {
  DBG_PRINTFLN("mon_ldata()");

  addmzt(m_name);
  ASCIItoUTF8_str(m_name, m_name_dos);

//ファイルが存在しなければERROR
  if (SD.exists(m_name_dos)) {
    snd1byte(0x00);

//ファイルオープン
    FILE_OBJ file = SD.open(m_name_dos, FILE_READ);
    if (file.isOpen()) {
      snd1byte(0x00);
      file.seek(m_lop);

//読み出しサイズ取得
      int f_length2 = rcv1byte();
      int f_length1 = rcv1byte();
      unsigned int f_length = (f_length1 << 8) | f_length2;
      for (unsigned int lp1 = 0; lp1 < f_length; lp1++) {
        byte i_data = file.read();
        snd1byte(i_data);
      }

      file.close();
      m_lop += f_length;
      snd1byte(0x00);
    }
    else {
//状態コード送信(ERROR)
      snd1byte(0xFF);
    }  
  }
  else {
//状態コード送信(FILE NOT FIND ERROR)
    snd1byte(0xF1);
  }
}

//BOOT処理(MZ-2000_SD専用)
void boot(void) {
  DBG_PRINTFLN("boot()");

//ファイルネーム取得
  for (unsigned int lp1 = 0; lp1 <= 32; lp1++) {
    m_name[lp1] = rcv1byte();
  }

  DBG_PRINTFLN("m_name: %s", m_name);
  ASCIItoUTF8_str(m_name, m_name_dos);
  DBG_PRINTFLN("m_name_dos: %s", m_name_dos);

//ファイルが存在しなければERROR
  if (SD.exists(m_name_dos)) {
    snd1byte(0x00);

//ファイルオープン
    FILE_OBJ file = SD.open(m_name_dos, FILE_READ);
    if (file.isOpen()) {
      snd1byte(0x00);

//ファイルサイズ送信
      unsigned long f_length = file.size();
      unsigned int f_len1 = f_length >> 8;
      unsigned int f_len2 = f_length & 0xff;
      snd1byte(f_len2);
      snd1byte(f_len1);

      DBG_PRINTFLN("%08X", f_length);
      DBG_PRINTFLN("%02X", f_len2);
      DBG_PRINTFLN("%02X", f_len1);

//実データ送信
      for (unsigned long lp1 = 1; lp1 <= f_length; lp1++) {
         byte i_data = file.read();
         snd1byte(i_data);
      }
    }
    else {
//状態コード送信(ERROR)
      snd1byte(0xFF);
    }  
  }
  else {
//状態コード送信(FILE NOT FIND ERROR)
    snd1byte(0xF1);
  }
}

void loop()
{
  digitalWrite(PB0PIN, LOW);
  digitalWrite(PB1PIN, LOW);
  digitalWrite(PB2PIN, LOW);
  digitalWrite(PB3PIN, LOW);
  digitalWrite(PB4PIN, LOW);
  digitalWrite(PB5PIN, LOW);
  digitalWrite(PB6PIN, LOW);
  digitalWrite(PB7PIN, LOW);
  digitalWrite(FLGPIN, LOW);

//コマンド取得待ち
  byte cmd = rcv1byte();
  DBG_PRINTFLN("cmd: %02X", cmd);
  DBG_PRINTFLN("eflg: %s", (eflg == true ? "true" : "false"));

  if (eflg == false) {
    switch(cmd) {
//80hでSDカードにsave
      case 0x80:
        DBG_PRINTFLN("SAVE START");

//状態コード送信(OK)
        snd1byte(0x00);
        f_save();
        break;

//81hでSDカードからload
      case 0x81:
        DBG_PRINTFLN("LOAD START");

//状態コード送信(OK)
        snd1byte(0x00);
        f_load();
        break;

//82hで指定ファイルを0000.mztとしてリネームコピー
      case 0x82:
        DBG_PRINTFLN("ASTART START");

//状態コード送信(OK)
        snd1byte(0x00);
        astart();
        break;

//83hでファイルリスト出力
      case 0x83:
        DBG_PRINTFLN("FILE LIST START");

//状態コード送信(OK)
        snd1byte(0x00);
        sdinit();
        dirlist();
        break;

//84hでファイルDelete
      case 0x84:
        DBG_PRINTFLN("FILE Delete START");

//状態コード送信(OK)
        snd1byte(0x00);
        f_del();
        break;

//85hでファイルリネーム
      case 0x85:
        DBG_PRINTFLN("FILE Rename START");

//状態コード送信(OK)
        snd1byte(0x00);
        f_ren();
        break;

//86hでファイルダンプ
      case 0x86:  
        DBG_PRINTFLN("FILE Dump START");

//状態コード送信(OK)
        snd1byte(0x00);
        f_dump();
        break;

//87hでファイルコピー
      case 0x87:  
        DBG_PRINTFLN("FILE Copy START");

//状態コード送信(OK)
        snd1byte(0x00);
        f_copy();
        break;

//91hで0436H MONITOR ライト インフォメーション代替処理
      case 0x91:
        DBG_PRINTFLN("0436H START");

//状態コード送信(OK)
        snd1byte(0x00);
        mon_whead();
        break;

//92hで0475H MONITOR ライト データ代替処理
      case 0x92:
        DBG_PRINTFLN("0475H START");

//状態コード送信(OK)
        snd1byte(0x00);
        mon_wdata();
        break;

//93hで04D8H MONITOR リード インフォメーション代替処理
      case 0x93:
        DBG_PRINTFLN("04D8H START");

//状態コード送信(OK)
        snd1byte(0x00);
        mon_lhead();
        break;

//94hで04F8H MONITOR リード データ代替処理
      case 0x94:
        DBG_PRINTFLN("04F8H START");

//状態コード送信(OK)
        snd1byte(0x00);
        mon_ldata();
        break;

//95hでBOOT LOAD(MZ-2000_SD専用)
      case 0x95:
        DBG_PRINTFLN("BOOT LOAD START");

//状態コード送信(OK)
        snd1byte(0x00);
        boot();
        break;

      default:
//状態コード送信(CMD ERROR)
        snd1byte(0xF4);
    }
  }
  else {
//状態コード送信(ERROR)
    snd1byte(0xF0);
    sdinit();
  }
}

// PC内部コードからUTF8に変換(1文字)
uint8_t ASCIItoUTF8(char ascii_char, char *utf8_char) {
  char_table a_to_u_item;

  if ((uint8_t)ascii_char < MIN_ASCII_CODE || (uint8_t)ascii_char > MAX_ASCII_CODE) {
    *utf8_char = ascii_char;
    return 1;
  }

  memcpy_P(&a_to_u_item, &a_to_u[(uint8_t)ascii_char - MIN_ASCII_CODE], sizeof(char_table));

  if (a_to_u_item.len == 0) {
    *utf8_char = '?';
    return 1;
  }

  memcpy(utf8_char, a_to_u_item.bytes, a_to_u_item.len);
  return a_to_u_item.len;
}

// PC内部コードの文字列をUTF8の文字列に変換
void ASCIItoUTF8_str(char *ascii_str, char *utf8_str) {
  char *ascii_str_tmp = ascii_str;
  char *utf8_str_tmp = utf8_str;
  uint8_t char_len = 0;
  for (unsigned int lp1 = 0; lp1 <= strlen(ascii_str); lp1++){
    char_len = ASCIItoUTF8(*ascii_str_tmp, utf8_str_tmp);
    ascii_str_tmp++;
    utf8_str_tmp += char_len;
  }
}

// UTF8の文字列をPC内部コードの文字列に変換
void UTF8toASCII_str(char *utf8_str, char *ascii_str) {
  char *utf8_str_tmp = utf8_str;
  char *ascii_str_tmp = ascii_str;
  char_table a_to_u_item;
  uint8_t char_len = 0;
  for (unsigned int lp1 = 0; lp1 <= strlen(utf8_str); lp1++){
    char_len = 0;

    // 1バイト文字
    if ((uint8_t)(*utf8_str_tmp & 0x80) == (uint8_t)0) {
      *ascii_str_tmp = *utf8_str_tmp;
      char_len = 1;
    }

    // 2バイト文字/3バイト文字
    else if (((uint8_t)(*utf8_str_tmp & 0xe0) == (uint8_t)0xc0) ||
             ((uint8_t)(*utf8_str_tmp & 0xf0) == (uint8_t)0xe0)) {
      uint8_t check_len = ((uint8_t)(*utf8_str_tmp & 0xf0) == (uint8_t)0xe0 ? 3 : 2);

      for (uint8_t lp2 = 0; lp2 <= MAX_ASCII_CODE - MIN_ASCII_CODE + 1; lp2++){
        memcpy_P(&a_to_u_item, &a_to_u[lp2], sizeof(char_table));
        if (
          a_to_u_item.len == check_len &&
          !memcmp(utf8_str_tmp, a_to_u_item.bytes, check_len)
        ) {
          *ascii_str_tmp = lp2 + MIN_ASCII_CODE;
          lp1 += (check_len - 1);
          char_len = check_len;
          break;
        }
      }
    }

    // UTF8として妥当ではない場合、
    // 2バイト/3バイト文字でテーブルにヒットしなかった場合は"?"
    if (char_len == 0) {
      *ascii_str_tmp = '?';
      char_len = 1;
    }

    utf8_str_tmp += char_len;
    ascii_str_tmp++;
  }

  *ascii_str_tmp = 0;
}
