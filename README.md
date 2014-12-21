
sushi
=====

## 概要

sushiはスネークゲームライクな寿司🍣ゲームです。画面上に表れる魚🐟とご飯🍚を食べることで
寿司🍣が成長します。

![ScreenShot](https://raw.githubusercontent.com/aasoukai128/sushi/master/screenshot.jpg)

## 環境

ncursesに依存しています。Mac OS X 10.9上のOS X付属のターミナルで動作確認を行いました。
それ以外の環境では、ターミナルのフォントで絵文字が表示でき、UTF-8が扱えるターミナルであれば、動作するかもしれません。

## インストール

    ./configure
    make
    make install

##オプション

	-s  : 寿司のスピードを設定、デフォルトは10
	-l  : 寿司のスタート時の長さを設定、デフォルトは10
	-f  : 魚とご飯の現れる量を設定、デフォルトは0.1

