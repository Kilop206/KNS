# Design Guidelines

## Por que este arquivo existe

Este documento define a direcao visual e interativa do projeto antes que telas, componentes e estilos sejam implementados.

Ele evita que a IA invente uma linguagem visual nova a cada tarefa. Use este arquivo para registrar identidade, tokens, componentes, padroes de layout, acessibilidade e exemplos de uso.

Quando o projeto tiver UI, site, app, dashboard, landing page, design system, marca ou experiencia visual, este documento deve ser preenchido antes da implementacao relevante.

## Natureza de Template

Este arquivo e um template. No primeiro chat de escopo, revise se o projeto tera UI; ajuste overview, principios, tokens, layout, componentes, estados, acessibilidade e do's/don'ts, ou solicite aprovacao humana para remover.

O baseline abaixo existe para dar consistencia minima quando ainda nao ha marca definida. Se o projeto tiver marca, design system, referencia visual ou identidade propria, substitua os tokens e regras pelo sistema real.

Se o projeto tiver UI e o humano nao fornecer preferencias visuais, referencias de marca, exemplos ou restricoes de design, a IA deve perguntar antes de implementar telas, componentes ou estilos. O baseline pode ser usado como proposta inicial, mas nao deve substituir a validacao humana sobre direcao visual.

## Como Usar

1. Defina a direcao visual do projeto em linguagem clara.
2. Ajuste os tokens base de cor, tipografia, espacamento, raios, sombras e grid.
3. Registre componentes esperados, estados e variacoes.
4. Explique o que a IA deve e nao deve fazer ao criar UI.
5. Atualize este documento quando uma decisao visual virar padrao recorrente.

## Perguntas de Kickoff Visual

Se o projeto tiver UI e ainda nao houver direcao visual definida, a IA deve perguntar ao humano:

1. O produto deve parecer mais utilitario, editorial, premium, divertido, tecnico, institucional ou comercial?
2. Existe marca, logo, paleta, fonte, site de referencia ou design system a seguir?
3. Existe algum exemplo visual que voce gosta ou quer evitar?
4. A interface sera mais densa e operacional ou mais simples e orientada a marketing?
5. Qual e o publico principal e em que contexto ele usa a interface?
6. Ha requisitos de acessibilidade, responsividade ou dispositivos prioritarios?
7. Existem cores, estilos, imagens, ilustracoes ou animacoes proibidas?

A IA deve registrar as respostas neste documento antes de implementar UI relevante.

## Overview

Descreva a direcao de design em 2 a 5 paragrafos.

O projeto e um `[tipo de produto]` para `[publico]`. A interface deve transmitir `[atributos principais: confianca, velocidade, clareza, sobriedade, energia, sofisticacao, etc.]`, priorizando `[tarefas principais do usuario]`.

A experiencia visual deve favorecer consistencia, leitura rapida e hierarquia clara. A marca se expressa principalmente por `[cor, tipografia, composicao, fotografia, ilustracao, movimento, etc.]`. Evite decoracao que nao ajude o usuario a entender, decidir ou agir.

Se nao houver marca definida, use o baseline SpecFirst: canvas claro, texto forte, uma cor primaria de acao, superficies neutras, componentes com raio moderado e tipografia sans-serif legivel.

**Key Characteristics:**

- Uma cor primaria para acoes e estados ativos; nao criar varias cores de destaque sem necessidade.
- Superficies neutras com hierarquia por espacamento, borda, sombra leve e contraste.
- Tipografia sans-serif unica, com pesos 400/500/600/700 e line-height confortavel.
- Cards com raio moderado; botoes e inputs consistentes em altura, padding e estados.
- Layout responsivo com container maximo, gutters previsiveis e breakpoints documentados.
- Componentes devem ter estados default, hover, focus, active, disabled, loading e error quando aplicavel.

## Principios de Design

- **Clareza:** cada tela deve deixar evidente o proximo passo do usuario.
- **Consistencia:** tokens, componentes e padroes devem se repetir antes de novas variacoes serem criadas.
- **Hierarquia:** tamanho, peso, espacamento e contraste devem indicar prioridade.
- **Restricao visual:** uma interface boa nao precisa de muitas cores, sombras ou estilos concorrentes.
- **Acessibilidade:** contraste, foco, teclado, labels e tamanhos de toque devem ser tratados como requisitos.
- **Responsividade:** layouts devem funcionar bem em mobile, tablet e desktop sem sobreposicao ou quebra de texto.

## Colors

Defina roles, nao apenas hexadecimais soltos. Substitua o baseline abaixo por tokens da marca quando existirem.

### Brand & Accent

- **Primary** (`{colors.primary}` - `#2563eb`): cor principal de acao, links importantes, estados ativos e destaque controlado.
- **Primary Hover** (`{colors.primary-hover}` - `#1d4ed8`): hover/pressed de acoes primarias.
- **Primary Soft** (`{colors.primary-soft}` - `#dbeafe`): superficies suaves relacionadas a selecao, informacao ou destaque leve.
- **Accent** (`{colors.accent}` - `#14b8a6`): cor opcional para indicadores secundarios. Use apenas se houver necessidade real.

### Surface

- **Canvas** (`{colors.canvas}` - `#ffffff`): fundo principal.
- **Canvas Soft** (`{colors.canvas-soft}` - `#f8fafc`): faixas, regioes secundarias e fundos alternados.
- **Surface** (`{colors.surface}` - `#ffffff`): cards, paineis, modais e popovers.
- **Surface Muted** (`{colors.surface-muted}` - `#f1f5f9`): inputs, chips e areas de baixa enfase.
- **Border** (`{colors.border}` - `#e2e8f0`): divisorias e contornos sutis.
- **Border Strong** (`{colors.border-strong}` - `#cbd5e1`): foco estrutural, separadores importantes e bordas ativas.

### Text

- **Ink** (`{colors.ink}` - `#0f172a`): texto principal e headings.
- **Body** (`{colors.body}` - `#334155`): texto padrao.
- **Muted** (`{colors.muted}` - `#64748b`): legendas, metadados, placeholders e texto secundario.
- **On Primary** (`{colors.on-primary}` - `#ffffff`): texto sobre `primary`.
- **On Dark** (`{colors.on-dark}` - `#ffffff`): texto sobre superficies escuras.

### Semantic

- **Success** (`{colors.success}` - `#16a34a`): feedback positivo e conclusao.
- **Warning** (`{colors.warning}` - `#d97706`): alerta, atencao e estado intermediario.
- **Danger** (`{colors.danger}` - `#dc2626`): erro e acoes destrutivas.
- **Info** (`{colors.info}` - `#0284c7`): informacao contextual.

### Color Rules

- Use `{colors.primary}` para a acao principal da tela.
- Evite mais de uma cor saturada competindo no mesmo viewport.
- Nao use cor como unica forma de comunicar estado.
- Estados semanticos devem ser reservados para feedback real, nao decoracao.
- Se a marca tiver paleta propria, substitua estes tokens e registre a decisao em `docs/decision-log.md`.

## Typography

### Font Family

Baseline recomendado:

- **Interface Sans:** Inter, Geist, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif.
- **Monospace:** "JetBrains Mono", "SFMono-Regular", Consolas, monospace.

Use uma familia principal para display, body, botoes e captions, salvo quando a marca exigir outro par tipografico.

### Hierarchy

| Token | Size | Weight | Line Height | Letter Spacing | Use |
| --- | --- | --- | --- | --- | --- |
| `{typography.display-xxl}` | 56px | 700 | 64px | 0 | Hero ou titulo principal de landing. |
| `{typography.display-xl}` | 44px | 700 | 52px | 0 | Titulo principal de pagina. |
| `{typography.display-lg}` | 36px | 700 | 44px | 0 | Titulo de secao importante. |
| `{typography.heading-xl}` | 30px | 700 | 38px | 0 | Heading de pagina interna. |
| `{typography.heading-lg}` | 24px | 600 | 32px | 0 | Card ou painel de destaque. |
| `{typography.heading-md}` | 20px | 600 | 28px | 0 | Subsecao, modal, card title. |
| `{typography.body-lg}` | 18px | 400 | 28px | 0 | Lead paragraph. |
| `{typography.body-md}` | 16px | 400 | 24px | 0 | Texto padrao. |
| `{typography.body-md-strong}` | 16px | 600 | 24px | 0 | Enfase, labels importantes. |
| `{typography.body-sm}` | 14px | 400 | 20px | 0 | Captions, labels, metadados. |
| `{typography.body-sm-strong}` | 14px | 600 | 20px | 0 | Chips, table headers, badges. |
| `{typography.caption}` | 12px | 400 | 16px | 0 | Fine print, timestamps, hints. |
| `{typography.button-md}` | 14px | 600 | 20px | 0 | Botoes padrao. |
| `{typography.button-lg}` | 16px | 600 | 24px | 0 | Botoes grandes. |
| `{typography.code}` | 13px | 400 | 20px | 0 | Codigo, keys, valores tecnicos. |

### Typography Rules

- Use sentence-case por padrao.
- Reserve uppercase para badges, labels curtos ou convencao de marca.
- Nao aplique letter-spacing negativo.
- Nao escale fonte com viewport width.
- Display sizes devem ser usados apenas para headings reais.
- Texto dentro de botoes, chips e cards deve caber sem quebrar layout.

## Layout

### Spacing System

Baseline com unidade de 4px:

| Token | Value | Use |
| --- | --- | --- |
| `{spacing.0}` | 0px | Sem espacamento. |
| `{spacing.xs}` | 4px | Micro gaps, icon/text tight gap. |
| `{spacing.sm}` | 8px | Gaps pequenos. |
| `{spacing.md}` | 12px | Gaps internos compactos. |
| `{spacing.lg}` | 16px | Padding padrao de componentes. |
| `{spacing.xl}` | 24px | Cards, grupos, secoes compactas. |
| `{spacing.2xl}` | 32px | Secoes e grids. |
| `{spacing.3xl}` | 48px | Bandas maiores. |
| `{spacing.section}` | 72px | Espacamento vertical desktop entre secoes. |

### Grid & Container

- **Container maximo:** 1200px.
- **Gutters desktop:** 32px.
- **Gutters tablet:** 24px.
- **Gutters mobile:** 16px.
- **Grid default:** 12 colunas no desktop, 6 no tablet, 4 no mobile.
- **Card gap:** 24px desktop, 16px mobile.

### Breakpoints

| Name | Width | Key Changes |
| --- | --- | --- |
| Mobile | < 640px | Layout 1 coluna, nav simplificada, cards empilham, tabelas viram cards ou scroll horizontal. |
| Tablet | 640-1023px | Grid 2 colunas quando fizer sentido, sidebar pode colapsar. |
| Desktop | 1024-1279px | Layout completo, grids 3-4 colunas, sidebar fixa quando aplicavel. |
| Desktop Large | >= 1280px | Container centralizado e largura maxima aplicada. |

### Responsive Rules

- Componentes devem ter largura fluida dentro do container.
- Cards empilham antes de comprimirem texto.
- Tabelas densas devem ter alternativa mobile ou scroll horizontal claro.
- Navegacao deve manter acesso aos fluxos principais em mobile.
- Imagens devem declarar aspect-ratio e nao causar layout shift.

## Elevation & Depth

| Level | Treatment | Use |
| --- | --- | --- |
| 0 - Flat | Sem sombra, borda opcional. | Layout base, superficies amplas. |
| 1 - Hairline | `1px solid {colors.border}`. | Cards simples, tabelas, inputs. |
| 2 - Soft Lift | `0 1px 3px rgba(15, 23, 42, 0.08), 0 1px 2px rgba(15, 23, 42, 0.06)`. | Cards interativos, popovers leves. |
| 3 - Overlay | `0 10px 24px rgba(15, 23, 42, 0.16)`. | Modais, drawers, menus flutuantes. |

Depth rules:

- Nao use sombra pesada como decoracao.
- Prefira borda + contraste de superficie para produtos operacionais.
- Use overlay apenas para elementos temporarios ou acima da hierarquia normal.

## Shapes

### Border Radius Scale

| Token | Value | Use |
| --- | --- | --- |
| `{rounded.none}` | 0px | Divisorias, layouts full-bleed, tabelas densas. |
| `{rounded.sm}` | 4px | Badges pequenos, controls compactos. |
| `{rounded.md}` | 8px | Inputs, botoes, cards pequenos. |
| `{rounded.lg}` | 12px | Cards padrao, panels. |
| `{rounded.xl}` | 16px | Cards de destaque, modais, containers grandes. |
| `{rounded.pill}` | 999px | Pills, chips, segmented controls. |
| `{rounded.full}` | 9999px | Avatares circulares, icon buttons circulares. |

Shape rules:

- Botoes e inputs devem compartilhar raio, altura e padding quando estiverem no mesmo formulario.
- Cards nao devem ser excessivamente arredondados sem razao de marca.
- Use pill apenas quando o formato comunicar filtro, chip ou acao curta.

## Components

Documente componentes como contratos. Cada componente deve ter uso, visual, estados e variacoes.

### Buttons

**`button-primary`**

- Uso: acao principal da tela ou fluxo.
- Visual: background `{colors.primary}`, text `{colors.on-primary}`, type `{typography.button-md}`, padding `{spacing.md} {spacing.xl}`, min-height 44px, rounded `{rounded.md}`.
- Estados: hover `{colors.primary-hover}`, focus com outline visivel, disabled com baixa opacidade, loading com spinner ou label persistente.

**`button-secondary`**

- Uso: acao secundaria.
- Visual: background `{colors.surface}`, text `{colors.ink}`, border `{colors.border-strong}`, same height/padding do primary.
- Estados: hover em `{colors.surface-muted}`, focus visivel, disabled claro.

**`button-ghost`**

- Uso: acoes terciarias, toolbar, menus.
- Visual: background transparente, text `{colors.body}`, hover `{colors.surface-muted}`, rounded `{rounded.md}`.

**`button-danger`**

- Uso: acoes destrutivas confirmadas.
- Visual: background `{colors.danger}`, text branco, mesma geometria do primary.
- Regra: nunca usar para navegacao comum.

### Cards & Containers

**`card-default`**

- Uso: agrupar conteudo relacionado.
- Visual: background `{colors.surface}`, border `{colors.border}`, rounded `{rounded.lg}`, padding `{spacing.xl}`.

**`card-elevated`**

- Uso: item interativo, destaque ou agrupamento acima da pagina.
- Visual: `card-default` + shadow Level 2.

**`panel-muted`**

- Uso: regioes secundarias, filtros, informacoes de suporte.
- Visual: background `{colors.canvas-soft}`, border opcional, rounded `{rounded.lg}`, padding `{spacing.xl}`.

### Inputs & Forms

**`text-input`**

- Uso: entrada textual.
- Visual: background `{colors.surface}`, text `{colors.ink}`, border `{colors.border-strong}`, rounded `{rounded.md}`, height 44px, padding `{spacing.md} {spacing.lg}`.
- Estados: focus com outline ou border `{colors.primary}`, error com `{colors.danger}`, disabled com `{colors.surface-muted}`.

**`select-input`**

- Uso: escolhas em lista curta ou media.
- Visual: igual a `text-input`, com indicador de dropdown.

**`checkbox` / `radio`**

- Uso: selecao binaria ou exclusiva.
- Visual: estado checked usa `{colors.primary}`; focus visivel.

### Navigation

**`nav-bar`**

- Uso: navegacao principal.
- Visual: background `{colors.canvas}`, border-bottom `{colors.border}`, height 56-72px conforme densidade.
- Responsivo: colapsar itens secundarios em menu quando largura nao comportar.

**`side-nav`**

- Uso: apps, dashboards, areas administrativas.
- Visual: background `{colors.surface}`, active item com `{colors.primary-soft}` e text `{colors.primary}`.

**`tabs`**

- Uso: alternar views relacionadas.
- Visual: active tab com border/underline `{colors.primary}`; nao usar como navegacao principal de produto.

### Feedback

**`alert-info` / `alert-success` / `alert-warning` / `alert-danger`**

- Uso: feedback contextual.
- Visual: background suave, border e icone conforme token semantico.
- Regra: alertas devem ter texto claro e acao quando necessario.

**`toast`**

- Uso: feedback temporario.
- Visual: surface elevada, shadow Level 3, texto curto.
- Regra: nao esconder erros criticos apenas em toast.

### Data Display

**`table-default`**

- Uso: dados comparaveis e densos.
- Visual: header `{colors.canvas-soft}`, borders `{colors.border}`, body `{typography.body-sm}` ou `{typography.body-md}` conforme densidade.
- Responsivo: scroll horizontal ou cards mobile.

**`badge`**

- Uso: status curto.
- Visual: rounded `{rounded.pill}`, padding `{spacing.xs} {spacing.sm}`, type `{typography.body-sm-strong}`.

## Conteudo Visual

- **Fotografia:** use imagens reais e relevantes quando produto, pessoa, lugar ou estado precisam ser reconhecidos.
- **Ilustracao:** use apenas quando reforcar conceito ou marca; evite decoracao generica.
- **Icones:** preferir biblioteca consistente; todos devem ter mesmo stroke/weight.
- **Graficos:** usar paleta limitada, labels legiveis e contraste suficiente.
- **Midia responsiva:** declarar aspect-ratio, crop e fallback.

## Interacao e Estados

- **Hover:** feedback sutil em background, border ou text color.
- **Focus:** sempre visivel para teclado; nao remover outline sem substituto.
- **Pressed/Active:** estado persistente deve ser diferente de hover.
- **Loading:** preserve dimensao do componente; evite layout shift.
- **Empty states:** explicar situacao, proximo passo e acao disponivel.
- **Errors:** mensagem proxima ao campo, clara e acionavel.
- **Success:** feedback proporcional a importancia da acao.

## Acessibilidade

- Contraste minimo: WCAG AA para texto normal e interface.
- Tamanho minimo de toque: 44x44px.
- Navegacao por teclado deve cobrir todos os controles.
- Inputs devem ter label persistente ou associacao acessivel.
- Conteudo nao deve depender apenas de cor.
- Movimento deve respeitar reducao de movimento quando aplicavel.
- Texto nao deve sobrepor outro conteudo nem estourar containers em mobile.

## Do's and Don'ts

### Do

- Use tokens antes de criar valores soltos.
- Mantenha uma acao primaria clara por tela ou bloco.
- Reutilize componentes antes de criar variacoes.
- Teste estados de loading, empty, error, disabled e focus.
- Verifique layout em mobile e desktop.
- Atualize este documento quando um padrao visual virar contrato.

### Don't

- Nao criar nova paleta por tela.
- Nao usar gradientes, sombras ou ilustracoes decorativas sem motivo.
- Nao usar cards dentro de cards sem necessidade clara.
- Nao introduzir fonte nova sem decisao registrada.
- Nao esconder informacao importante apenas por cor.
- Nao implementar UI relevante se este documento estiver vazio ou contraditorio.

## Diretrizes para IA

Ao implementar UI, a IA deve:

1. ler este documento antes de criar componentes, paginas ou estilos;
2. usar os tokens definidos aqui como fonte de verdade;
3. perguntar preferencias visuais ao humano quando o projeto tiver UI e este documento ainda nao estiver preenchido para o projeto real;
4. propor ajustes ao humano quando a marca exigir mudanca em cor, tipografia, layout ou componentes;
5. nao introduzir nova paleta, fonte, grid ou linguagem visual sem aprovacao humana;
6. registrar decisao em `docs/decision-log.md` quando mudar o sistema visual;
7. atualizar este documento quando um padrao visual virar contrato do projeto;
8. validar responsividade, acessibilidade e estados interativos antes de concluir.

Se este documento ainda estiver incompleto para o projeto real, a IA deve propor o preenchimento antes de implementar UI relevante.
