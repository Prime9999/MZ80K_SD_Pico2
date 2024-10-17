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

unsigned long r_count = 0;
unsigned long f_length = 0;
char m_name[40];
char c_name[40];
char f_name[40];
char f_name_sd[120];
char w_name[40];
char w_name_sd[120];
byte s_data[260];
unsigned int s_adrs;
unsigned int e_adrs;
unsigned int w_length;
unsigned int w_len1;
unsigned int w_len2;
unsigned int s_adrs1;
unsigned int s_adrs2;
unsigned int b_length;

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

//sd_waopen sd_wnopen sd_wdirectでSAVE用ファイル名を指定なくSAVEされた場合のデフォルトファイル名を設定
  strcpy(w_name, "default.dat");

  sdinit();
}

//4BIT受信
byte rcv4bit(void) {
//  DBG_PRINTFLN("rcv4bit()");

//HIGHになるまでループ
  while (digitalRead(CHKPIN) != HIGH) {}

//受信
  byte j_data = digitalRead(PA0PIN) | (digitalRead(PA1PIN) << 1) | (digitalRead(PA2PIN) << 2) | (digitalRead(PA3PIN) << 3);

//FLGをセット
  digitalWrite(FLGPIN, HIGH);

//LOWになるまでループ
  while (digitalRead(CHKPIN) == HIGH) {}

//FLGをリセット
  digitalWrite(FLGPIN, LOW);
  return(j_data);
}

//1BYTE受信
byte rcv1byte(void) {
//  DBG_PRINTFLN("rcv1byte()");

  byte i_data = (rcv4bit() << 4) | rcv4bit();
  return(i_data);
}

//1BYTE送信
void snd1byte(byte i_data) {
//  DBG_PRINTFLN("snd1byte()");

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
  digitalWrite(FLGPIN, LOW);

//LOWになるまでループ
  while (digitalRead(CHKPIN) == HIGH) {}
}

//小文字->大文字
char upper(char c) {
  DBG_PRINTFLN("upper()");

  if ('a' <= c && c <= 'z') {
    c = c - ('a' - 'A');
  }

  return c;
}

//ファイル名の最後が「.cmt」でなければ付加
void addcmt(char *f_name, char *m_name) {
  DBG_PRINTFLN("addcmt()");

  unsigned int lp1 = 0;
  while (f_name[lp1] != 0x00){
    m_name[lp1] = f_name[lp1];
    lp1++;
  }

  if (f_name[lp1 - 4] != '.' ||
    ( f_name[lp1 - 3] != 'C' &&
      f_name[lp1 - 3] != 'c' ) ||
    ( f_name[lp1 - 2] != 'M' &&
      f_name[lp1 - 2] != 'm' ) ||
    ( f_name[lp1 - 1] != 'T' &&
      f_name[lp1 - 1] != 't' ) ){
         m_name[lp1++] = '.';
         m_name[lp1++] = 'C';
         m_name[lp1++] = 'M';
         m_name[lp1++] = 'T';
  }

  m_name[lp1] = 0x00;
}

//比較文字列取得 32+1文字まで取得、ただしダブルコーテーションは無視する
void receive_name(char *f_name) {
  DBG_PRINTFLN("receive_name()");

  char r_data;
  unsigned int lp2 = 0;
  for (unsigned int lp1 = 0; lp1 <= 32; lp1++) {
    r_data = rcv1byte();
    if (r_data != 0x22) {
      f_name[lp2] = r_data;
      lp2++;
    }
  }
}

// MONITOR Lコマンド .CMT LOAD
void cmt_load(void) {
  DBG_PRINTFLN("cmt_load()");

  boolean flg = false;

//DOSファイル名取得
  receive_name(m_name);

//ファイル名の指定があるか
  if (m_name[0] != 0x00) {
    addcmt(m_name, f_name);
  
//指定があった場合
//ファイルが存在しなければERROR
    ASCIItoUTF8_str(f_name, f_name_sd);
    if (SD.exists(f_name_sd)) {
//ファイルオープン
      file = SD.open(f_name_sd, FILE_READ);

      if (file.isOpen()) {
//f_length設定、r_count初期化
        f_length = file.size();
        r_count = 0;

//状態コード送信(OK)
        snd1byte(0x00);
        flg = true;
      }
      else {
        snd1byte(0xf0);
        sdinit();
        flg = false;
      }
    }
    else{
      snd1byte(0xf1);
      sdinit();
      flg = false;
    }
  }
  else{
//ファイル名の指定がなかった場合
//ファイルエンドになっていないか
    if (f_length > r_count) {
      snd1byte(0x00);
      flg = true;
    }
    else{
      snd1byte(0xf1);
      flg = false;
    }
  }

//良ければファイルエンドまで読み込みを続行する
  if (flg == true) {
    int rdata = 0;
      
//ヘッダーが出てくるまで読み飛ばし
    while (rdata != 0x3a && rdata != 0xd3) {
      rdata = file.read();
      r_count++;
    }

//ヘッダー送信
    snd1byte(rdata);

//ヘッダーが0x3aなら続行、違えばエラー
    if (rdata == 0x3a){
//START ADDRESS HIを送信
      s_adrs1 = file.read();
      r_count++;
      snd1byte(s_adrs1);

//START ADDRESS LOを送信
      s_adrs2 = file.read();
      r_count++;
      snd1byte(s_adrs2);
      s_adrs = (s_adrs1 << 8) | s_adrs2;

//CHECK SUMを送信
      rdata = file.read();
      r_count++;
      snd1byte(rdata);

//HEADERを送信
      rdata = file.read();
      r_count++;
      snd1byte(rdata);

//データ長を送信
      b_length = file.read();
      r_count++;
      snd1byte(b_length);
      
//データ長が0x00でない間ループする
      while (b_length != 0x00) {
        for (unsigned int lp1 = 0; lp1 <= b_length; lp1++) {
//実データを読み込んで送信
          rdata = file.read();
          r_count++;
          snd1byte(rdata);
        }

//CHECK SUMを送信
        rdata = file.read();
        r_count++;
        snd1byte(rdata);

//データ長を送信
        b_length = file.read();
        r_count++;
        snd1byte(b_length);
      }

//データ長が0x00だった時の後処理
//CHECK SUMを送信
      rdata = file.read();
      r_count++;
      snd1byte(rdata);

//ファイルエンドに達していたらFILE CLOSE
      if (f_length == r_count) {
        file.close();
      }
    }
    else{
      file.close();
    }
  }        
}

//BASICプログラムのLOAD処理
void bas_load(void) {
  DBG_PRINTFLN("bas_load()");

  boolean flg = false;
//DOSファイル名取得
  receive_name(m_name);
//ファイル名の指定があるか
  if (m_name[0] != 0x00) {
    addcmt(m_name, f_name);
  
//指定があった場合
//ファイルが存在しなければERROR
    ASCIItoUTF8_str(f_name, f_name_sd);
    if (SD.exists(f_name_sd)) {
//ファイルオープン
      file = SD.open(f_name_sd, FILE_READ);
      if (file.isOpen()) {
//f_length設定、r_count初期化
        f_length = file.size();
        r_count = 0;
//状態コード送信(OK)
        snd1byte(0x00);
        flg = true;
      }
      else {
        snd1byte(0xf0);
        sdinit();
        flg = false;
      }
    }
    else{
      snd1byte(0xf1);
      sdinit();
      flg = false;
    }
  }
  else{
//ファイル名の指定がなかった場合
//ファイルエンドになっていないか
    if (f_length > r_count) {
      snd1byte(0x00);
      flg = true;
    }
    else{
      snd1byte(0xf1);
      flg = false;
    }
  }

//良ければファイルエンドまで読み込みを続行する
  if (flg == true) {
    int rdata = 0;
      
//ヘッダーが出てくるまで読み飛ばし
    while (rdata != 0x3a && rdata != 0xd3) {
      rdata = file.read();
      r_count++;
    }

//ヘッダー送信
    snd1byte(rdata);

//ヘッダーが0xd3なら続行、違えばエラー
    if (rdata == 0xd3) {
      while (rdata == 0xd3) {
        rdata = file.read();
        r_count++;
      }

//実データ送信
      int zcnt = 0;
      int zdata = 1;

      snd1byte(rdata);

//0x00が11個続くまで読み込み、送信
      while (zcnt < 11) {
        rdata = file.read();
        r_count++;
        snd1byte(rdata);
        if (rdata == 0x00) {
          zcnt++;
          if (zdata != 0) {
            zcnt = 0;
          }
        }
        zdata = rdata;
      }

//ファイルエンドに達していたらFILE CLOSE
      if (f_length == r_count) {
        file.close();
      }      
    }
    else{
      file.close();
    }
  }
}

void w_body(void) {
  DBG_PRINTFLN("w_body()");

  byte r_data;
  byte csum;

//ヘッダー 0x3A書き込み
  w_file.write(char(0x3A));

//スタートアドレス取得、書き込み
  s_adrs1 = rcv1byte();
  s_adrs2 = rcv1byte();
  w_file.write(s_adrs2);
  w_file.write(s_adrs1);

//CHECK SUM計算、書き込み
  csum = 0 - (s_adrs1 + s_adrs2);
  w_file.write(csum);

//スタートアドレス算出
  s_adrs = s_adrs1 | (s_adrs2 << 8);

//エンドアドレス取得
  s_adrs1 = rcv1byte();
  s_adrs2 = rcv1byte();

//エンドアドレス算出
  e_adrs = s_adrs1 | (s_adrs2 << 8);

//ファイル長算出、ブロック数算出
  w_length = e_adrs - s_adrs + 1;
  w_len1 = w_length / 255;
  w_len2 = w_length % 255;

//実データ受信、書き込み
//0xFFブロック
  while (w_len1 > 0) {
    w_file.write(char(0x3A));
    w_file.write(char(0xFF));
    csum = 0xff;
    for (unsigned int lp1 = 1; lp1 <= 255; lp1++) {
      r_data = rcv1byte();
      w_file.write(r_data);
      csum += r_data;
    }

//CHECK SUM計算、書き込み
    csum = 0 - csum;
    w_file.write(csum);
    w_len1--;
  }

//端数ブロック処理
  if (w_len2 > 0) {
    w_file.write(char(0x3A));
    w_file.write(w_len2);
    csum = w_len2;
    for (unsigned int lp1 = 1; lp1 <= w_len2; lp1++) {
      r_data = rcv1byte();
      w_file.write(r_data);
      csum += r_data;
    }

//CHECK SUM計算、書き込み
    csum = 0 - csum;
    w_file.write(csum);
  }

  w_file.write(char(0x3A));
  w_file.write(char(0x00));
  w_file.write(char(0x00));
}

// MONITOR Wコマンド .CMT SAVE
void cmt_save(void) {
  DBG_PRINTFLN("cmt_save()");

  byte r_data;
  byte csum;

//DOSファイル名取得
  receive_name(m_name);

//ファイル名の指定が無ければエラー
  if (m_name[0] != 0x00) {
    addcmt(m_name, f_name);
  
    if (w_file.isOpen()) {
      w_file.close();
    }

//ファイルが存在すればdelete
    ASCIItoUTF8_str(f_name, f_name_sd);
    if (SD.exists(f_name_sd)) {
      SD.remove(f_name_sd);
    }

//ファイルオープン
    w_file = SD.open(f_name_sd, FILE_WRITE);
    if (w_file.isOpen()) {
//状態コード送信(OK)
      snd1byte(0x00);
      w_body();
      w_file.close();
    }
    else {
      snd1byte(0xf0);
      sdinit();
    }
  }
  else {
    snd1byte(0xf6);
    sdinit();
  }
}

//BASICプログラムのSAVE処理
void bas_save(void) {
  DBG_PRINTFLN("bas_save()");

  unsigned int lp1;

//DOSファイル名取得
  receive_name(m_name);

//ファイル名の指定が無ければエラー
  if (m_name[0] != 0x00) {
    addcmt(m_name, f_name);
  
    if (w_file.isOpen()) {
      w_file.close();
    }

    ASCIItoUTF8_str(f_name, f_name_sd);

//ファイルが存在すればdelete
    if (SD.exists(f_name_sd)) {
      SD.remove(f_name_sd);
    }

//ファイルオープン
    w_file = SD.open(f_name_sd, FILE_WRITE);
    if (w_file.isOpen()) {
//状態コード送信(OK)
      snd1byte(0x00);

//スタートアドレス取得
      s_adrs1 = rcv1byte();
      s_adrs2 = rcv1byte();

//スタートアドレス算出
      s_adrs = s_adrs1 | (s_adrs2 << 8);

//エンドアドレス取得
      s_adrs1 = rcv1byte();
      s_adrs2 = rcv1byte();

//エンドアドレス算出
      e_adrs = s_adrs1 + (s_adrs2 << 8);

//ヘッダー 0xD3 x 9回書き込み
      for (lp1 = 0; lp1 <= 9; lp1++) {
        w_file.write(char(0xD3));
      }

//DOSファイル名の先頭6文字をファイルネームとして書き込み
      for (lp1 = 0; lp1 <= 5; lp1++) {
        w_file.write(m_name[lp1]);
      }

//実データ (e_adrs - s_adrs +1)を受信、書き込み
      for (lp1 = s_adrs; lp1 <= e_adrs; lp1++) {
        w_file.write(rcv1byte());
      }

//終了 0x00 x 9回書き込み
      for (lp1 = 1; lp1 <= 9; lp1++) {
        w_file.write(char(0x00));
      }

      w_file.close();
    }
    else {
      snd1byte(0xf0);
      sdinit();
    }
  }
  else{
    snd1byte(0xf1);
    sdinit();
  }
}

//BASICプログラムのKILL処理
void bas_kill(void) {
  DBG_PRINTFLN("bas_kill()");

  unsigned int lp1;

//DOSファイル名取得
  receive_name(m_name);

//ファイル名の指定が無ければエラー
  if (m_name[0] != 0x00) {
    addcmt(m_name, f_name);
  
//状態コード送信(OK)
    snd1byte(0x00);

    ASCIItoUTF8_str(f_name, f_name_sd);

//ファイルが存在すればdelete
    if (SD.exists(f_name_sd)) {
      SD.remove(f_name_sd);

//状態コード送信(OK)
      snd1byte(0x00);
    }
    else{
      snd1byte(0xf1);
      sdinit();
    }
  }
  else{
    snd1byte(0xf1);
    sdinit();
  }
}

//5F9EH CMT 1Byte読み込み処理の代替、OPENしているファイルの続きから1Byteを読み込み、送信
void cmt_5f9e(void) {
  DBG_PRINTFLN("cmt_5f9e()");

  int rdata = file.read();
  r_count++;
  snd1byte(rdata);

//ファイルエンドまで達していればFILE CLOSE
  if (f_length == r_count) {
    file.close();
  }      
}

//read file open
void sd_ropen(void) {
  DBG_PRINTFLN("sd_ropen()");

  receive_name(f_name);

  ASCIItoUTF8_str(f_name, f_name_sd);

//ファイルが存在しなければERROR
  if (SD.exists(f_name_sd)) {
//ファイルオープン
    file = SD.open(f_name_sd, FILE_READ);
    if (file.isOpen()) {
//f_length設定、r_count初期化
      f_length = file.size();
      r_count = 0;

//状態コード送信(OK)
      snd1byte(0x00);
    }
    else {
      snd1byte(0xf0);
      sdinit();
    }
  }
  else{
    snd1byte(0xf1);
    sdinit();
  }
}

//write file append open
void sd_waopen(void) {
  DBG_PRINTFLN("sd_waopen()");

  receive_name(w_name);

  ASCIItoUTF8_str(w_name, w_name_sd);

//ファイルオープン
  if (w_file.isOpen()) {
    w_file.close();
  }
  w_file = SD.open(w_name_sd, FILE_WRITE);
  if (w_file.isOpen()) {
//状態コード送信(OK)
    snd1byte(0x00);
  }
  else {
    snd1byte(0xf0);
    sdinit();
  }
}

//write file new open
void sd_wnopen(void) {
  DBG_PRINTFLN("sd_wnopen()");

  receive_name(w_name);
  if (w_file.isOpen()) {
    w_file.close();
  }

  ASCIItoUTF8_str(w_name, w_name_sd);

//ファイルが存在すればdelete
  if (SD.exists(w_name_sd)) {
    SD.remove(w_name_sd);
  }

//ファイルオープン
  w_file = SD.open(w_name_sd, FILE_WRITE);
  if (w_file.isOpen()) {
//状態コード送信(OK)
    snd1byte(0x00);
  }
  else {
    snd1byte(0xf0);
    sdinit();
  }
}

//write 1byte 5F2FH代替
void sd_w1byte(void) {
  DBG_PRINTFLN("sd_w1byte()");

  int rdata = rcv1byte();
  if (w_file.isOpen()) {
    w_file.write(rdata);

//状態コード送信(OK)
    snd1byte(0x00);
  }
  else {
    snd1byte(0xf0);
    sdinit();
  }
}

//5ED9H代替
void sd_wdirect(void) {
  DBG_PRINTFLN("sd_wdirect()");

  if (w_file.isOpen()) {
    w_body();
    w_file.close();

//状態コード送信(OK)
    snd1byte(0x00);
  }
  else{
    snd1byte(0xf0);
    sdinit();
  }
}

//write file close
void sd_wclose(void) {
  DBG_PRINTFLN("sd_wclose()");

  w_file.close();
}

// SD-CARDのFILELIST
void dirlist(void) {
  DBG_PRINTFLN("dirlist()");

//比較文字列取得 32+1文字まで
  for (unsigned int lp1 = 0; lp1 <= 32; lp1++) {
    c_name[lp1] = rcv1byte();
  }

//
  FILE_OBJ file = SD.open("/");
  if (file.isOpen()) {
//状態コード送信(OK)
    snd1byte(0x00);

    FILE_OBJ entry = file.openNextFile();
    int cntl2 = 0;
    unsigned int br_chk =0;
    int page = 1;

//全件出力の場合には16件出力したところで一時停止、キー入力により継続、打ち切りを選択
    while (br_chk == 0) {
      if (entry) {
        entry.getName(f_name_sd, sizeof(f_name_sd) - 1);
        UTF8toASCII_str(f_name_sd, f_name);
        unsigned int lp1 = 0;
//一件送信
//比較文字列でファイルネームを先頭10文字まで比較して一致するものだけを出力
        if (f_match(f_name, c_name)) {
          while (lp1 <= 36 && f_name[lp1] != 0x00) {
            snd1byte(upper(f_name[lp1]));
            lp1++;
          }

          snd1byte(0x0D);
          snd1byte(0x00);
          cntl2++;
        }
      }

      if (!entry || cntl2 > 15) {
//継続・打ち切り選択指示要求
        snd1byte(0xfe);

//選択指示受信(0:継続 B:前ページ 以外:打ち切り)
        br_chk = rcv1byte();

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
            while (cntl2 < page * 16) {
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

//FDLの結果が18件未満なら継続指示要求せずにそのまま終了
      if (!entry && cntl2 < 16 && page == 1) {
        break;
      }
    }

//処理終了指示
    snd1byte(0xFF);
    snd1byte(0x00);
  }
  else{
    snd1byte(0xf1);
  }
}

//f_nameとc_nameをc_nameに0x00が出るまで比較
//FILENAME COMPARE
boolean f_match(char *f_name, char *c_name) {
  DBG_PRINTFLN("f_match()");

  boolean flg1 = true;
  unsigned int lp1 = 0;
  while (lp1 <= 32 && c_name[0] != 0x00 && flg1 == true) {
    if (upper(f_name[lp1]) != c_name[lp1]) {
      flg1 = false;
    }

    lp1++;
    if (c_name[lp1] == 0x00) {
      break;
    }
  }

  return flg1;
}

void loop()
{
  DBG_PRINTFLN("loop()");

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
  DBG_PRINTFLN("COMMAND WAIT");
  byte cmd = rcv1byte();
  DBG_PRINTFLN("CMD=[%02X]", cmd);
  if (eflg == false){
    switch(cmd) {
//70h:PC-8001 CMTファイル SAVE
      case 0x70:
        DBG_PRINTFLN("CMT SAVE START");

//状態コード送信(OK)
        snd1byte(0x00);
        cmt_save();
        break;

//71h:PC-8001 CMTファイル LOAD
      case 0x71:
        DBG_PRINTFLN("CMT LOAD START");

//状態コード送信(OK)
        snd1byte(0x00);
        cmt_load();
        break;

//72h:PC-8001 5F9EH READ ONE BYTE FROM CMT
      case 0x72:
        DBG_PRINTFLN("CMT_5F9E START");

//状態コード送信(OK)
        snd1byte(0x00);
        cmt_5f9e();
        break;

//73h:PC-8001 CMTファイル BASIC LOAD
      case 0x73:
        DBG_PRINTFLN("CMT BASIC LOAD START");

//状態コード送信(OK)
        snd1byte(0x00);
        bas_load();
        break;

//74h:PC-8001 CMTファイル BASIC SAVE
      case 0x74:
        DBG_PRINTFLN("CMT BASIC SAVE START");

//状態コード送信(OK)
        snd1byte(0x00);
        bas_save();
        break;

//75h:PC-8001 CMTファイル KILL
      case 0x75:
        DBG_PRINTFLN("CMT KILL START");

//状態コード送信(OK)
        snd1byte(0x00);
        bas_kill();
        break;

//76h:PC-8001 SD FILE READ OPEN
      case 0x76:
        DBG_PRINTFLN("SD FILE READ OPEN START");

//状態コード送信(OK)
        snd1byte(0x00);
        sd_ropen();
        break;

//77h:PC-8001 SD FILE WRITE APPEND OPEN
      case 0x77:
        DBG_PRINTFLN("SD FILE WRITE APPEND OPEN START");

//状態コード送信(OK)
        snd1byte(0x00);
        sd_waopen();
        break;

//78h:PC-8001 SD FILE WRITE 1Byte
      case 0x78:
        DBG_PRINTFLN("SD FILE WRITE 1Byte START");

//状態コード送信(OK)
        snd1byte(0x00);
        sd_w1byte();
        break;

//79h:PC-8001 SD FILE WRITE NEW OPEN
      case 0x79:
        DBG_PRINTFLN("SD FILE WRITE NEW OPEN START");

//状態コード送信(OK)
        snd1byte(0x00);
        sd_wnopen();
        break;

//7Ah:PC-8001 SD WRITE 5ED9H
      case 0x7A:
        DBG_PRINTFLN("SD WRITE 5ED9H START");

//状態コード送信(OK)
        snd1byte(0x00);
        sd_wdirect();
        break;

//7Bh:PC-8001 SD FILE WRITE CLOSE
      case 0x7B:
        DBG_PRINTFLN("SD FILE WRITE CLOSE START");

//状態コード送信(OK)
        snd1byte(0x00);
        sd_wclose();
        break;

//83hでファイルリスト出力
      case 0x83:
        DBG_PRINTFLN("FILE LIST START");

//状態コード送信(OK)
        snd1byte(0x00);
        sdinit();
        dirlist();
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
