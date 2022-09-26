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
- After you chose an endpoint and confirmed, put the API key and secret provided by Bybit into the first two boxes and click on check.
- The second section lets you choose between mailbox mode and webhook mode. For mailbox mode, you'll need to type in the IMAP url (you can search for "your email provider name + IMAP server address"), username and password. For webhook mode select the port to listen on. If you're using TLS, put the certificate and key on cert/cert.crt and cert/cert.key.
- On the third section you can define the rules. See the examples below.
- The fourth section gives you a simple candlestick graph.

## Examples of rules

## Setting up TradingView
