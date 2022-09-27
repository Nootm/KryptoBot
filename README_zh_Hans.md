# KryptoBot

[English](https://github.com/Nootm/KryptoBot/blob/main/README.md) | [正體中文](https://github.com/Nootm/KryptoBot/blob/main/README_ZHT.md) | 简体中文

下面都是[朋友寫的](https://github.com/hiDandelion)，和我沒有關聯。~~目前沒有加拿大置產的打算~~

![Interface](https://raw.githubusercontent.com/Nootm/KryptoBot/master/gui.jpg)

> 或许是因为我想在加拿大盖豪宅？

KryptoBot 是与 TradingView 和 Bybit 配合工作的交易机器人，与项目 [WunderTrading](https://wundertrading.com/en) 类似。

> 声明：对于此程序导致的任何损失，请自行承担，作者概不负责。此项目仍处于早期开发阶段，在投入生产环境使用前，请使用 [Bybit Testnet](https://testnet.bybit.com/en-US/) 进行至少一周的测试。

## 特性
- 可完全由用户进行自我托管。二进制文件由 GitHub Actions 生成。
- 简易配置。仅需一个邮箱地址（对于 webhook 模式）或一个公共网络 IP 地址（对于 mailbox 模式）。
- 无第三方服务。仅与 Bybit 以及指定的 SMTP 服务器（对于 mailbox 模式）进行通讯。
- 提供 GUI 模式以及 headless 模式。
- Windows 平台未经测试，请使用 Linux 服务器或 [Linux 虚拟机](https://itsfoss.com/install-linux-in-virtualbox/)来使用本程序。更多关于 Windows 平台的信息请阅读我们的[英文文档](https://github.com/Nootm/KryptoBot/blob/main/README.md)

## 测试平台、架构及模式
- Alpine Linux 3.16.2, x86_64, headless
- Arch Linux Latest, x86_64, Sway

## 使用 GUI 模式
- 第一部分：选择接入点并确认，随后填写 Bybit 的 API key 以及 secret，点击"检查"按钮。
- 第二部分：选择 mailbox 模式或 webhook 模式。对于 mailbox 模式，您需要填写 IMAP 地址（您可以通过搜索引擎搜索您邮件提供商的 IMAP 地址，或通过邮件提供商的设置页面进行获取）、用户名和密码。对于 webhook 模式，您需要填写监听端口以及选择是否启用 TLS，如果您想要启用 TLS，请将证书命名为 cert.crt 以及 cert.key 并放置于本程序根目录的 cert 文件夹下。
- 第三部分：定义规则，请见下方示例。
- 第四部分：提供一个简单的 K 线图。

## 规则示例以及 TradingView 配置

KryptoBot 利用 TradingView 发送的消息的部分作为交易信号。这些消息必须仅由字母和数字组成。KryptoBot 以格式 "KRYPTOBOT_DoSomething1_DoSomething2_DoSomething3" 从 TradingView 接收消息，然后由交易信号 "DoSomething1"、"DoSomething2"、"DoSomething3"执行指定的操作。KryptoBot 会等待之前的交易委托完成后才会进行下一个交易委托。当您需要执行多个操作时，我们建议一次仅向 KryptoBot 发送一条信息，并在此消息中包含所有的交易信号。

您可以通过阅读来自 WunderTrading 的文章：https://help.wundertrading.com/en/articles/5173846-tradingview-strategy-alert-automation 来设置 TradingView。不同之处是：对于 mailbox 模式，您需要在"more options"中选择"Send email-to-SMS"选项；对于 webhook 模式，您需要填写您自己的服务器地址。

以下为使用 headless 模式及 webhook 模式时的一个示例：
```json
{
    "http_url": "https://api.bybit.com",
    "ws_url": "wss://stream.bybit.com",
    "key": "xxxxx",
    "secret": "xxxxxxxxxx",
    "mode": "webhook",
    "webhook_port": 443,
    "webhook_use_ssl": true,
    "rules": [
        {
            "signal": "CA",
            "pair": "APEUSDT",
            "type": "Close all positions"
        },
        {
            "signal": "CM",
            "pair": "MATICUSDT",
            "type": "Close all positions"
        },
        {
            "signal": "CR",
            "pair": "RUNEUSDT",
            "type": "Close all positions"
        },
        {
            "signal": "BA",
            "pair": "APEUSDT",
            "type": "Buy using specified leverage times total equity",
            "arg": 1.0
        },
        {
            "signal": "BM",
            "pair": "MATICUSDT",
            "type": "Buy using specified leverage times total equity",
            "arg": 1.0
        },
        {
            "signal": "BR",
            "pair": "RUNEUSDT",
            "type": "Buy using specified leverage times total equity",
            "arg": 1.0
        },
        {
            "signal": "SA",
            "pair": "APEUSDT",
            "type": "Sell using specified leverage times total equity",
            "arg": 1.0
        },
        {
            "signal": "SM",
            "pair": "MATICUSDT",
            "type": "Sell using specified leverage times total equity",
            "arg": 1.0
        },
        {
            "signal": "SR",
            "pair": "RUNEUSDT",
            "type": "Sell using specified leverage times total equity",
            "arg": 1.0
        }
    ]
}
```

对于 mailbox 模式，将以下内容

```
    "mode": "webhook",
    "webhook_port": 443,
    "webhook_use_ssl": true,
```

替换为：

```
    "smtp_url": "imap.example.com",
    "smtp_login": "impostor@among.us",
    "smtp_password": "Pa55w0rt",
```

在 TradingView 中的设置为：

```
if condition_long
    strategy.entry("KRYPTOBOT_CA_CM_CR_BA_BM_BR", strategy.long)
else if condition_short
    strategy.entry("KRYPTOBOT_CA_CM_CR_SA_SM_SR", strategy.short)
```

以上设置将实现在交易对 APEUSDT、MATICUSDT、RUNEUSDT 间平均分配的 3x 杠杆策略。
