# KryptoBot

English | [正體中文](https://github.com/Nootm/KryptoBot/blob/main/README_ZHT.md)

![Interface](https://raw.githubusercontent.com/Nootm/KryptoBot/master/gui.jpg)

> Because I need somewhere to run my shitty strategy.

Trading bot to be used with TradingView and Bybit, like [WunderTrading](https://wundertrading.com/en).

> Disclaimer: I am not responsible for any of your losses using this program. This project is still in early stages, so do test it using [Bybit Testnet](https://testnet.bybit.com/en-US/) for at least a week before using it.

## Features:
- Fully self-hosted.
- Easy to set up with either an existing email account (webhook mode) or a public IP address (mailbox mode).
- Don't rely on 3rd party services, only connections with Bybit and the specified SMTP server (if you use mailbox mode) would be made.
- GUI mode and headless mode provided.
- I don't have a Windows rig to test, so run it on VPS or [a virtual machine with Linux](https://itsfoss.com/install-linux-in-virtualbox/) if you use Windows.

## Tested on:
- Alpine Linux 3.16.2, x86_64, headless
- Arch Linux Latest, AArch64, Sway
- Arch Linux Latest, x86_64, Sway
- macOS 12.6, AArch64

## Using GUI mode
- 第一個窗口依次爲：主網接口 1，主網接口 2，測試網接口。
- 確認後第二個窗口，第一部分：Bybit API 的 key 以及 secret（從 bybit 獲取），填好後點下面按鈕檢查。
- 第二部分：依次爲郵箱模式和 webhook 模式。郵箱模式：依次爲 IMAP url（請搜索你的郵件服務 + IMAP，如 mailbox.org 即爲 imap.mailbox.org), 郵箱帳號，郵箱密碼。webhook 模式：綁定端口，是否啓用 TLS（若啓用，TLS 證書放在 cert 目錄下，命名爲 cert.crt 和 cert.key）。
- 第三部分：定義規則。依次填入：TradingView 發出訊號（僅支持大小寫字母和數字），操作內容，交易對，參數（某些操作內容需要）。請參考下方例子。寫好一條之後點 “Add rule”
- 第四部分：簡單的 K 線圖。依次爲選擇交易對（可多選），切換時間間隔，是否使用 Helkin-Ashi K 線。
