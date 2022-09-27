# KryptoBot
[English](https://github.com/Nootm/KryptoBot) | 正體中文

![Interface](https://raw.githubusercontent.com/Nootm/KryptoBot/master/gui.jpg)

> 因爲我需要找個地方運行我的爛交易策略。

KryptoBot 是用於 TradingView 和 Bybit 的交易機器人，就像 [WunderTrading](https://wundertrading.com/zh)

> 聲明：我不對任何因使用此程序造成的損失負責。項目仍在早期開發中，請先使用 [Bybit 測試網](https://testnet.bybit.com/zh-TW/) 測試至少一週後再使用。

## 功能:
- 可完全自己搭建。
- 配置簡單，你只需要一個電子信箱或公網 IP。
- 不依賴第三方服務，只會連接 Bybit 和用戶提供的 SMTP 伺服器（如果使用信箱模式）。
- 提供 GUI 模式和進程模式。
- 我沒有運行 Windows 的機器進行測試，所以如果你使用 Windows，請在伺服器上運行或使用 [Linux 虛擬機](https://forum.gamer.com.tw/C.php?bsn=8897&snA=90925)。

## 在這些系統下通過測試：
- Alpine Linux 3.16.2, x86_64, headless
- Arch Linux Latest, x86_64, Sway

## 使用 GUI 模式
- 第一個窗口依次爲：主網接口 1，主網接口 2，測試網接口。
- 確認後第二個窗口，第一部分：Bybit API 的 key 以及 secret（從 bybit 獲取），填好後點下面按鈕檢查。
- 第二部分：依次爲郵箱模式和 webhook 模式。郵箱模式：依次爲 IMAP url（請搜索你的郵件服務 + IMAP，如 mailbox.org 即爲 imap.mailbox.org), 郵箱帳號，郵箱密碼。webhook 模式：綁定端口，是否啓用 TLS（若啓用，TLS 證書放在 cert 目錄下，命名爲 cert.crt 和 cert.key）。
- 第三部分：定義規則。依次填入：TradingView 發出訊號（僅支持大小寫字母和數字），操作內容，交易對，參數（某些操作內容需要）。請參考下方例子。寫好一條之後點 “Add rule”
- 第四部分：簡單的 K 線圖。依次爲選擇交易對（可多選），切換時間間隔，是否使用 Helkin-Ashi K 線。
