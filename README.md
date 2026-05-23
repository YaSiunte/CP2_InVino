# InVino

## Sobre o Projeto

O InVino é um sistema inteligente de monitoramento para vinherias desenvolvido utilizando Arduino e sensores ambientais. O objetivo do projeto é auxiliar no controle das condições do ambiente onde os vinhos são armazenados, garantindo uma melhor conservação dos produtos.

O sistema realiza o monitoramento de temperatura, umidade e luminosidade em tempo real, exibindo os dados diretamente em um display LCD I2C. A proposta do projeto é demonstrar como a automação e os sistemas embarcados podem ser utilizados em soluções voltadas ao controle ambiental.

---

## Objetivo

Desenvolver um sistema capaz de monitorar fatores importantes para o armazenamento adequado de vinhos, permitindo um acompanhamento contínuo das condições do ambiente.

---

## Componentes Utilizados

* Arduino Uno
* Sensor DHT11
* Sensor LDR
* Display LCD I2C
* Protoboard
* Jumpers

---

## Funcionalidades

* Monitoramento de temperatura
* Monitoramento de umidade
* Monitoramento de luminosidade
* Atualização automática das leituras
* Exibição dos dados em tempo real no display LCD
* Sistema de monitoramento contínuo

---

## Funcionamento do Sistema

O sistema realiza leituras periódicas dos sensores a cada 10 segundos. O sensor DHT11 coleta informações de temperatura e umidade do ambiente, enquanto o sensor LDR mede o nível de luminosidade.

Após a coleta dos dados, as informações são processadas pelo Arduino e exibidas no display LCD I2C em tempo real.

O objetivo é manter o ambiente monitorado constantemente, permitindo identificar alterações que possam prejudicar a conservação dos vinhos.

---

## Diferenciais do Projeto

* Aplicação prática de automação em monitoramento ambiental
* Atualização periódica dos dados em tempo real
* Integração entre múltiplos sensores
* Interface simples e funcional utilizando display LCD
* Projeto voltado para uma situação real de armazenamento

---

## Dificuldades Encontradas

Durante o desenvolvimento do projeto, algumas dificuldades foram enfrentadas, principalmente na configuração do display LCD I2C e na comunicação entre os sensores e o Arduino.

Também houve desafios relacionados ao controle do tempo de atualização das leituras, evitando atrasos ou travamentos no sistema.

Esses problemas foram resolvidos através da revisão das conexões dos componentes, reorganização da lógica do código e ajustes nas bibliotecas utilizadas.

