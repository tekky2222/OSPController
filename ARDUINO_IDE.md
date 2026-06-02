# OSP Controller — Arduino IDE

Este projeto pode ser compilado e gravado pela [Arduino IDE](https://www.arduino.cc/en/software) (recomendado: versão 2.x).

## Requisitos

1. **Placa ESP32** no Board Manager  
   - *Ferramentas → Placa → Gerenciador de Placas*  
   - Instale **esp32** by Espressif Systems (mesma família usada no PlatformIO).

2. **Bibliotecas** (*Ferramentas → Gerenciar Bibliotecas*):

   | Biblioteca | Autor / notas |
   |------------|----------------|
   | **PubSubClient** | Nick O'Leary |
   | **ModbusMaster** | Doc Walker |
   | **EspSoftwareSerial** | Peter Lerup (`plerup`) — necessária para UART software no ESP32 |

3. **Placa alvo** (menu *Ferramentas*):

   | Opção | Valor |
   |-------|--------|
   | Placa | **NodeMCU-32S** ou **ESP32 Dev Module** |
   | Upload Speed | 115200 (ou o valor que você já usa) |
   | Monitor Serial | 115200 |

## Abrir e compilar

1. Clone este repositório.
2. Abra o sketch: **`OSPController/OSPController.ino`**  
   (Arquivo → Abrir — selecione a pasta `OSPController` que contém o `.ino`).
3. A biblioteca **MPPTLib** já está em `OSPController/libraries/MPPTLib` e é detectada automaticamente.
4. Verifique se as três bibliotecas externas da tabela acima estão instaladas.
5. Compile (*Verificar*) e grave (*Carregar*).

## Versão do firmware

A string de versão (`GIT_VERSION`) fica em `OSPController/version.cpp`.

Para atualizá-la a partir do Git (com o repositório clonado):

```bash
python utils.py
```

Isso reescreve `version.cpp` com `git describe` e a data do último commit. Você também pode editar `version.cpp` manualmente.

## PlatformIO (opcional)

O repositório ainda inclui `platformio.ini` para CI e quem preferir PlatformIO. O código-fonte é o mesmo sketch em `OSPController/`.

```bash
pio run -t upload
```

## Problemas comuns

- **ModbusMaster / PubSubClient não encontrados** — instale pelo Gerenciador de Bibliotecas; reinicie a IDE se necessário.
- **SoftwareSerial** — use **EspSoftwareSerial**, não a biblioteca SoftwareSerial genérica do AVR.
- **Erro de compilação com `dynamic_cast`** — nas placas ESP32 o RTTI vem habilitado por padrão; não desative flags de compilação que removam RTTI.
