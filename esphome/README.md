# OSP MPPT DPS — ESPHome

Controlador MPPT para **fonte DPS** (Modbus RTU), com a mesma lógica do [OSPController](../README.md):

- Controle proporcional em `Vin` vs `setpoint`
- Varredura (sweep) para encontrar o ponto de máxima potência
- Detecção e recuperação de colapso do painel
- Estados: `off`, `mppt`, `sweeping`, `full_cv`, `capped`, `collapsemode`, `error`

## Arquivos

| Arquivo | Função |
|---------|--------|
| `osp_mppt_dps.yaml` | Configuração ESPHome (Modbus, entidades, intervalos) |
| `mppt_dps_logic.h` | Algoritmo MPPT (port de `solar.cpp`, só DPS) |
| `secrets.yaml.example` | Modelo WiFi |

## Hardware

- **ESP32** (ex.: NodeMCU-32S)
- **DPS5005 / DPS5020** (ou compatível Modbus)
- Ligação UART: ESP TX → RX da DPS, ESP RX → TX da DPS, GND comum
- Endereço Modbus padrão: **1**, baud **19200**

Ajuste em `osp_mppt_dps.yaml` → `substitutions`:

```yaml
uart_tx_pin: GPIO17
uart_rx_pin: GPIO16
modbus_address: "1"
dps5020: "false"   # true para DPS5020, false para DPSDPS5005
```

## Instalação

1. [Instalar ESPHome](https://esphome.io/guides/getting_started_command_line.html) (CLI ou Dashboard).
2. Copiar `secrets.yaml.example` → `secrets.yaml` e preencher WiFi.
3. Na pasta `esphome/`:

```bash
esphome compile osp_mppt_dps.yaml
esphome upload osp_mppt_dps.yaml
esphome logs osp_mppt_dps.yaml
```

## Primeira utilização

1. Definir **Battery Voltage Set** (`dps_set_volt`) para a tensão da bateria.
2. Ligar **DPS Output** e **MPPT Enable**.
3. Definir **MPPT Vin Setpoint** (0 = só após primeiro sweep; ou valor estimado do MPP).
4. Opcional: botão **MPPT Sweep** para varredura manual.
5. Ajustar **MPPT P Gain**, **Ramp Limit**, **Current Cap** conforme o sistema.

Integração Home Assistant: as entidades aparecem automaticamente via API nativa do ESPHome.

## Diferenças em relação ao OSPController Arduino

| Recurso | OSPController | Este ESPHome |
|---------|---------------|--------------|
| Fonte DROK | Sim | Não |
| Fonte DPS | Sim | Sim |
| MQTT custom / Publishable | Sim | API ESPHome / HA |
| ADC Vin externo | Sim | Não (usa registo 5 da DPS) |
| Proteção LV (GPIO) | Sim | Não (pode acrescentar com `switch` GPIO) |
| OTA HTTP custom | Sim | OTA ESPHome |

## Registos Modbus DPS

| Registo | Função |
|---------|--------|
| 0 | Limite tensão (×100) |
| 1 | Limite corrente (×1000 ou ×100) |
| 2 | Tensão saída |
| 3 | Corrente saída |
| 5 | Tensão entrada (painel) |
| 8 | Flag CC |
| 9 | Saída ON/OFF |
| 11 | Modelo (deteção 5020) |
