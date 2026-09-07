# ThinkStation D30 hwmon 1.5 — швидкий старт

## Встановлення або оновлення з 1.1–1.4

Розпакуйте архів на ThinkStation D30, перейдіть у каталог драйвера та виконайте
від root:

```bash
chmod +x install.sh uninstall.sh
./install.sh
```

Інсталятор:

- встановить DKMS, `lm-sensors`, заголовки поточного ядра та
  `proxmox-default-headers`;
- вивантажить старий модуль і прибере DKMS-версії 1.1–1.4;
- збере та встановить версію 1.5;
- увімкне автоматичне завантаження модуля після перезапуску.

## Перевірка

```bash
uname -r
modinfo d30_hwmon | grep -E '^(version|description):'
lsmod | grep d30_hwmon
journalctl -k -b --no-pager | grep d30_hwmon
sensors d30_hwmon-virtual-0
dkms status -m d30-hwmon
```

У журналі очікуються повідомлення про Lenovo ThinkStation D30, NCT6681 з ID
`0xB2xx`, базу EC `0x0A00` і реєстрацію датчиків.

## Діагностичні байти температур і датчик кришки

```bash
HWMON="$(dirname "$(grep -l '^d30_hwmon$' /sys/class/hwmon/hwmon*/name)")"
grep -H . "$HWMON"/temp*_raw_low
cat "$HWMON/intrusion0_alarm"
```

`intrusion0_alarm` дорівнює `1`, коли BIOS-зафіксований сигнал відкриття
корпусу активний, і `0`, коли він не активний. Драйвер лише читає цей біт і
ніколи його не скидає.

`tempN_raw_low` — сирі молодші байти регістрів. Версія 1.5 також використовує
їх у стандартному 16-бітному перетворенні NCT668x і тому показує крок 0,5°C.

## Видалення

```bash
./uninstall.sh
```
