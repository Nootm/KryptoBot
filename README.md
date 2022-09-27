# KryptoBot

English | [正體中文](https://github.com/Nootm/KryptoBot/blob/main/README_ZHT.md)

![madewithc++](https://img.shields.io/badge/made%20with-c%2B%2B-informational?style=for-the-badge)
![workinprogress](https://img.shields.io/badge/work-in%20progress-critical?style=for-the-badge)
![kindaworks](https://img.shields.io/badge/kinda-works-success?style=for-the-badge)

![Interface](https://raw.githubusercontent.com/Nootm/KryptoBot/master/gui.jpg)

> Because I need somewhere to run my shitty strategy.

Trading bot to be used with TradingView and Bybit, like [WunderTrading](https://wundertrading.com/en).

> Disclaimer: I am not responsible for any of your losses using this program. This project is still in early stages, so do test it with [Bybit Testnet](https://testnet.bybit.com/en-US/) for at least a week before using it for production.

## Features
- Fully self-hosted. Auditable binaries produced by GitHub Actions.
- Easy to set up with either an existing email account (webhook mode) or a public IP address (mailbox mode).
- Don't rely on 3rd party services, only connections with Bybit and the specified SMTP server (if you use mailbox mode) would be made.
- GUI mode and headless mode provided.
- I don't have a Windows rig to test, so please run it on VPS or [a virtual machine with Linux](https://itsfoss.com/install-linux-in-virtualbox/) if you use Windows. It will also protect your strategy from potential malwares. I use BuyVM, and recommand it as long as you use crypto to pay. However, if someone is willing to build and test it for Windows with GitHub Action, you can open an issue and I will put the link here.

## Tested on
- Alpine Linux 3.16.2, x86_64, headless
- Arch Linux Latest, x86_64, Sway

## Using GUI mode
- After you chose an endpoint and confirmed, put the API key and secret provided by Bybit into the first two boxes and click on check.
- The second section lets you choose between mailbox mode and webhook mode. For mailbox mode, you'll need to type in the IMAP url (you can search for "your email provider name + IMAP server address"), username and password. For webhook mode select the port to listen on. If you're using TLS, put the certificate and key on cert/cert.crt and cert/cert.key.
- On the third section you can define the rules. See the examples below.
- The fourth section gives you a simple candlestick graph.

## Examples of rules and TradingView setups

Signals are parts from the message sent by TradingView. It should only contain alphanumberic charactors. KryptoBot retrives message from TradingView in the format of "KRYPTOBOT_DoSomething1_Action2_XDDD", and it would execute the corresponding action triggered by signal "DoSomething1", "Action2" and "XDDD". KryptoBot will always wait for the previous order to be completely filled before executing the next. It's recommanded to always send one message at a time, and put all the signals inside this message.

For setting up TradingView, you may check this article from WunderTrading first: https://help.wundertrading.com/en/articles/5173846-tradingview-strategy-alert-automation. The main difference is, for mailbox mode you need to select "Send email-to-SMS" in "more options" instead of Webhook URL, and for webhook mode you would fill in the url box with your own server address.

Here's an example in headless mode using webhook mode:
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

You can use the following types:
```plain
"Swing buy using same quantity", "Swing sell using same quantity"
"Buy using specified quantity", "Sell using specified quantity" (arg: quantity, in trading coin)
"Close all positions"
"Buy using specified quantity, reduce only", "Sell using specified quantity, reduce only" (arg: quantity, in trading coin)
"Buy using specified leverage times total equity", "Sell using specified leverage times total equity" (arg: leverage, please set allowed leverage at the Bybit trading panel a bit higher than the value put here or order may fail)
```

For mailbox mode, replace these lines:

```json
    "mode": "webhook",
    "webhook_port": 443,
    "webhook_use_ssl": true,
```

with:

```json
    "smtp_url": "imap.example.com",
    "smtp_login": "impostor@among.us",
    "smtp_password": "Pa55w0rt",
```

In TradingView:

```python3
if condition_long
    strategy.entry("KRYPTOBOT_CA_CM_CR_BA_BM_BR", strategy.long)
else if condition_short
    strategy.entry("KRYPTOBOT_CA_CM_CR_SA_SM_SR", strategy.short)
```

This will result in 3x leverage evenly distributed in APEUSDT, MATICUSDT and RUNEUSDT.
