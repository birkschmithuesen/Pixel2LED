# Pixel2LED Controller — Manual (voller Wortlaut)

> Wörtliche Transkription aus dem Original-PDF „Handhabung_Controller" (2019).
> Bedienungsanleitung für den Pixel2LED / BlackLED Controller: Strommanagement
> der Pixel-Stripes, Output-/Framerate-Konfigurationen, ArtNet-/Universen-Setup
> und das Web-Interface. Ergänzt die technische [README](../README.md)
> (Firmware/Build) um die Betriebs-/Konfigurationsseite.

---

## Pixel Stripe Strommanagement

**allgemeine Hinweise:**

- da die LED's mit nur 5V Spannung versorgt werden, fließen relativ hohe Ströme.
- Aufgrund des geringen Querschnitts der Leiterbahnen sinkt die Spannung über die
  Länge des Stripes schnell, sodass zunächst die blaue und dann die grüne LED
  nicht mehr voll leuchten - eine weiße Linie verfärbt sich nach hinten ins rot
- das oben beschriebene Voltage Drop Problem taucht am stärksten auf, wenn alle
  LED's auf 100 % Leistung, also 100% weiß laufen.

**Aufbau:**

- Man kann 10m – 2x5m Stripes mit nur einer Zuleitung (Daten/Strom) betreiben,
  solange der Content nicht 100% weiß ist. Bei weißem Stroben sollte also die
  Gesamthelligkeit auf ca 50% gedimmed werden
- Ab 15m – 3x5m muss unbedingt eine extra Stromeinspeisung erfolgen. Dazu wird
  ein T-Stück zwischen dem 2. und 3. Stripe, oder am Ende des 3. Stripe
  angeschlossen und über die 2-pol-Buchse mit zusätzlichem Strom versorgt.
- Sollte die extra Stromeinspeisung von einem anderen Netzteil als die erste
  Stromeinspeisung kommen, dürfen die beiden +-Pole der Netzteile nicht über den
  Stripe verbunden sein. Dafür gibt es LED-Stripes, bei denen der +-Pol am Ende
  herausgebohrt ist. Diese sind am Ende mit schwarzem Gaffa Tape markiert.

---

## Controller

### Outputs

Die Controller haben jeweils 6 Outputs, welche wie folgt genutzt werden können:

**jeweils 1 Stripe / 5m pro Output**

- 1800 px @ max 25 fps: 6 out á 300 pixel / 6 x 5m Stripes
- 1500 px @ max 30 fps: 5 out á 300 pixel / 5 x 5m Stripes
- 1200 px @ max 40 fps: 4 out á 300 pixel / 4 x 5m Stripes
- 900   px @ max 44 fps: 3 out á 300 pixel / 3 x 5m Stripes
- 600   px @ max 60 fps: 2 out á 300 pixel / 2 x 5m Stripes
- 300   px @ max 60 fps: 1 out á 300 pixel / 1 x 5m Stripes

**jeweils 2 Stripes / 10m pro Output**

- 1800 px @ max 25 fps: 3 out á 600 pixel / 3 x 10m Stripes
- 1200 px @ max 40 fps: 2 out á 600 pixel / 2 x 10m Stripes
- 600   px @ max 60 fps: 1 out á 600 pixel / 1 x 10m Stripes

**jeweils 3 Stripes / 15m pro Output**

- 1800 px @ max 25 fps: 2 out á 900 pixel / 2 x 15m Stripes
- 900   px @ max 44 fps: 1 out á 900 pixel / 1 x 15m Stripes

**jeweils 4 Stripes / 20m pro Output**

- 1200 px @ max 40 fps: 1 out á 1200 pixel / 1 x 20m Stripe

**jeweils 5 Stripes / 25m pro Output**

- 1500 px @ max 30 fps: 1 out á 1500 pixel / 1 x 25m Stripe

**jeweils 6 Stripes / 30m pro Output** !!!! eventuell Datenprobleme beim letzten Stripe

- 1800 px @ max 30 fps: 1 out á 1500 pixel / 1 x 25m Stripe

---

## ArtNet

Die Controller haben als Werkseinstellung die IP 2.2.2.x.
X entspricht der Nummer, welche an der Seite am Controller aufgelabelt ist.
Das Lichtprogramm/Lichtconsole muss auf die IP 2.x.x.x mit der Subnetmaske
255.0.0.0 eingestellt sein. Die Controller unterstützen keinen DHCP Server – die
IP's müssen also statisch vergeben werden.
Aufgrund der viele Universen, sollten die ArtNet Daten per Unicast an die
Controller gesendet werden.

Die einzelnen LED's werden als RGBW-Lampe angesprochen. Ein Universum kann daher
512/4 => 128 LED's ansteuern. Jeder Output beginnt immer mit der DMX Adresse 1
eines neuen Universums. Es gibt folgende Konfigurationen:

- 5m Stripe / 300 LED's: 3 Universen
- 2 x 5m Stripes / 600 LED's: 5 Universen
- 3 x 5m Stripes / 900 LED's: 8 Universen
- 4 x 5m Stripes / 1200 LED's: 10 Universen

---

## Web Interface

Das Webinterface zur Konfiguration des Controllers ist mit dem Webbrowser über
die Adresse `http://2.2.2.x/setup` zu erreichen. Hier können IP-Adresse, Start
Universum und Anzahl der Outputs gewählt werden. Wenn der „restart" Button
gedrückt wird, startet der Controller neu und übernimmt die gesetzten
Einstellungen.

**!!!!!!!!!!ACHTUNG!!!!!!!!!**
Wenn die IP-Adresse geändert wird, ist der Controller nur noch über die neue
Adresse erreichbar. Aktuell gibt es keine Möglichkeit, die Werkseinstellung
wieder zu erstellen. Der Controller ist also nur noch über die NEU EINGESTELLTE
IP ADRESSE erreichbar.
