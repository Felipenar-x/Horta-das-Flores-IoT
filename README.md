# Horta das Flores IoT

O projeto acompanha a luz que chega à Horta das Flores. A cada 10 segundos, o ESP32 lê um sensor de luz (LDR) e publica via MQTT uma mensagem JSON com o horário UTC, a média do ADC, a luz relativa em porcentagem e a iluminância estimada em lux.

## Componentes e montagem

A montagem usa um ESP32, um LDR de 5 mm, um resistor fixo e fios de ligação. O firmware pressupõe um resistor de 10 kΩ. Ligue uma perna do LDR ao pino de 3,3 V e a outra ao GPIO34. Ligue o resistor entre esse mesmo GPIO34 e o GND. Assim, o ESP32 lê a tensão no ponto de encontro do LDR com o resistor.

## Conversão para lux

O firmware calcula a média de 20 leituras de tensão, com intervalo de 10 ms entre amostras. A partir da tensão média `V`, em volts, estima a resistência do LDR em ohms e a iluminância em lux.

```text
R_LDR = 10000 × (3,3 / V − 1)
lux   = 10 × (15000 / R_LDR)^(1 / 0,7)
```

Os valores de 15 kΩ a 10 lux e de expoente 0,7 são hipóteses para um LDR GL5528. A estimativa depende do resistor instalado e da calibração do sensor. Se a tensão média ficar abaixo de 150 mV ou atingir 3100 mV, ou se a média do ADC atingir 4095, o firmware publica `null` no campo de lux. A porcentagem de luz é calculada separadamente, com 100 e 3800 como referências de escuro e claro na escala do ADC.

## Como usar

1. Na pasta `Horta_LDR_Lux`, copie `secrets.example.h` para `secrets.h`.
2. Preencha no arquivo local o nome e a senha da rede Wi-Fi e o usuário e a senha do broker MQTT.
3. Abra `Horta_LDR_Lux/Horta_LDR_Lux.ino` na IDE Arduino, com o suporte a placas ESP32 e a biblioteca PubSubClient instalados. Selecione a placa e a porta e envie o sketch. O monitor serial exibe as mensagens a 115200 baud.

## Grupo Os Felipes

Felipe Yuji, Leonardo Moreno e Gabriel da Silva  
Disciplina de Desenvolvimento de Sistemas de IoT — Mackenzie
