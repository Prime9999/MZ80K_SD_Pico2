# MZ80K_SD、PC-8001_SDのArduino Pro MiniをRaspberry Pi Pico 2に置き換える試み

![MZ80K_SD_Pico2](images/board05.jpg)

## 免責事項
こちらは個人的な試みの成果を公開するものであります。<br>
この成果物を使用することによりMZ80K_SDやPC-8001_SDに不具合が生じた場合も、当方は責任を負わないものとします。

## 概要
[MZ80K_SD](https://github.com/yanataka60/MZ80K_SD)と[PC-8001_SD](https://github.com/yanataka60/PC-8001_SD)で使用されているArduino Pro MiniをRaspberry Pi Pico 2に置き換え、ファームウェアの改造を容易にする試みです。

両ボードのフラッシュメモリとRAMの容量は次の通りであり、Pico 2に置き換えることにより実装の追加が容易となります:
||Arduino Pro Mini|Raspberry Pi Pico 2|
|----|----:|----:|
|Flash|32KB|4MB|
|RAM|2KB|520KB|

ここで公開しているファームウェアは、オリジナルに対して次の機能を追加しています:
* SD側のファイル名をUTF-8で処理可能とする(つまり、半角英数字以外も取り扱い可能にする)
* UTF-8対応による、PCとSD間の相互変換可能文字が増加(半角カナおよび一部の記号と漢字)

## 組み立て
Gerber/*.ZIPで出力した基板を入手していることを前提として解説します。
![](images/board01.jpg)

0. 必要物
   * Raspberry Pi Pico 2(Picoは使用不可)
   * ピンヘッダ(20ピン×2、12ピン×2、2ピン)
1. Arduino側のピンヘッダ(9ピン×2と2ピン)をはんだ付けします。
   ![上面](images/board02.jpg)![下面](images/board03.jpg)
2. Raspberry　Pi Pico 2側に、ピンヘッダ(20ピン×2)はんだ付け済みのRaspberry　Pi Pico 2を取り付けます。
   ![上面](images/board04.jpg)![下面](images/board05.jpg)
3. Raspberry　Pi Pico 2側のピンヘッダ(20ピン×2)をはんだ付けします。
   ![下面](images/board07.jpg)
4. PC-8001_SDに取り付ける場合はRaspberry　Pi Pico 2側のピンヘッダがICと干渉するので、短く切断します。
   ![下面](images/board08.jpg)
5. Arduino IDE経由で、Arduinoのスケッチを書き込みます。
   * MZ80K_SD用: Arduino/MZ-80K_SD_Pico2
   * PC-8001_SD用: Arduino/PC-8001_SD_Pico2
6. 組み立てたものを、Arduino Pro Miniと入れ替えます。

なおブレッドボードを用いて結線する場合は、KiCad以下にあるプロジェクトをKiCadで開き、回路図エディターを参考に行ってください。