# Vegemetre

Le vegemetre permet de mesurer un indice tel que le NDVI pour observer l'expression photosynthétique des plantes. Il mesure 18 longueurs d'ondes allant de 350nm à 950nm. 
Toutes les données sont stockées sur une carte SD.

Ce repository contient

    le code Arduino
    un tutoriel d'utilisation du boitier

# Liste du matériel 
- Un [Arduino Uno Rev 3](https://www.amazon.fr/Arduino-A000066-M%C3%A9moire-flash-32/dp/B008GRTSV6/ref=sr_1_4?__mk_fr_FR=%C3%85M%C3%85%C5%BD%C3%95%C3%91&crid=36TTIZ164O4AA&keywords=arduino+uno&qid=1645093550&s=computers&sprefix=arduino+uno%2Ccomputers%2C263&sr=1-4)
- Un [cable USB type B](https://www.amazon.fr/MYAMIA-Alimentation-Transmission-Donn%C3%A9es-Arduino/dp/B07H23N214/ref=sr_1_3?keywords=cable+arduino&qid=1645093493&s=computers&sr=1-3) parfois inclu avec l'arduino
- Un shield LCD comme le [VMA203](https://www.velleman.eu/products/view/?country=be&lang=fr&id=435510)
- Un capteur [AS7265x](https://www.sparkfun.com/products/15050)
- La connectique pour connecter le capteur : 
    - Un [cable qwiic](https://www.sparkfun.com/products/14427)
    - Un [shield qwiic](https://www.sparkfun.com/products/14352)
- Un lecteur de carte SD du type [AZDelivery](https://www.amazon.fr/azdelivery-Reader-M%C3%A9moire-Memory-Arduino/dp/B077MCR2RC/ref=sr_1_8?__mk_fr_FR=%C3%85M%C3%85%C5%BD%C3%95%C3%91&crid=7VNAI4RSWJFP&keywords=SD+shield+arduino&qid=1645094459&s=computers&sprefix=sd+shield+arduino%2Ccomputers%2C127&sr=1-8)
- Une [carte micro SD](https://www.amazon.fr/QUMOX-Micro-Carte-M%C3%A9moire-Classe/dp/B07F81QTPP/ref=sr_1_4?__mk_fr_FR=%C3%85M%C3%85%C5%BD%C3%95%C3%91&crid=SF91NXPG3IOB&keywords=micro+sd+8go&qid=1645094511&s=computers&sprefix=microsd+8go%2Ccomputers%2C267&sr=1-4) classique (pas besoin d'une grosse carte de nouvelle génération. Les cartes de 64Go ne sont pas supportées)

# Montage du système
Le plus délicat est le soudage du support de carte SD sur le shield qwiic.

# Installation du code

Un guide d'utilisation de ce capteur est donné par [Sparkfun](https://learn.sparkfun.com/tutorials/spectral-triad-as7265x-hookup-guide/all)

Il est nécessaire de télécharger [cette librairie](https://github.com/sparkfun/SparkFun_AS7265x_Arduino_Library/archive/main.zip) pour faire fonctionner le capteur. Telecharger le zip, puis dans le logiciel Arduino, cliquez sur *Croquis > Inclure une bibliothèque > Ajouter la bibliothèque ZIP*, puis selectionner le .zip téléchargé
![image](https://user-images.githubusercontent.com/24956276/154458640-b881a772-1e48-459a-a46e-6444c5e14176.png)

Le code est disponible sur ce github. Il faut le flasher sur la carte : après avoir branché l'arduino, il faut aller dans *Outils > ports >* Choisir l'arduino
puis cliquer sur le bouton *téléverser* :
![image](https://user-images.githubusercontent.com/24956276/154459886-bd04d811-d328-48ef-a890-12dc35910290.png)

