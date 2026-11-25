# Design

Superfície de controle do InputLeap em estilo macOS claro (linguagem visual Apple).

## Mundo

Janela cinza-claro #f5f5f7 com cartões brancos e hairlines #d2d2d7, como Ajustes do macOS.
Acento azul Apple #0071e3 para o vínculo ativo e ações primárias. Fonte do sistema
(San Francisco no macOS, Segoe UI no Windows). UI em pt-BR direto na fonte.

## Superfície

- Barra superior: marca + chip de estado (verde conectado / âmbar conectando / cinza parado)
- Cartões brancos Servidor | Cliente (borda azul no papel ativo)
- Faixa da mesa: chip esquerdo ↔ chip direito
- Cartão da pasta compartilhada: caminho, enviar, abrir
- Rodapé: Recarregar (contorno) + Iniciar/Parar (pílula preenchida; vermelho ao parar)

## Tokens

- janela `#f5f5f7` · cartão `#ffffff` · hairline `#d2d2d7` / `#e5e5ea`
- texto `#1d1d1f` · secundário `#6e6e73` / `#515154`
- primário `#0071e3` (hover `#0077ed`) · parar `#ff3b30`
- estados: verde `#d8f5e0`/`#1d7a3a` · âmbar `#ffe9c8`/`#96590a` · neutro `#e8e8ed`/`#6e6e73`
- raios: cartões 12px · controles 8px · pílulas 17px
- mono: SF Mono / Menlo / Cascadia Mono
