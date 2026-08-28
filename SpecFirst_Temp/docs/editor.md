# Editor

## Por que este arquivo existe

Use este documento quando o projeto tiver conteudo editavel por usuarios nao tecnicos, CMS, rich text, formularios administrativos ou serializacao de conteudo.

Ele define o que pode ser editado e o que deve continuar em codigo.

## Natureza de Template

Este arquivo e um template. No primeiro chat de escopo, revise se o projeto tera conteudo editavel, CMS ou editor; ajuste decisao padrao, regras, blocos e evolucao, ou solicite aprovacao humana para remover.

## Decisao Padrao

Descreva a ferramenta ou estrategia editorial.

- Editor/CMS: `[ferramenta]`
- Conteudo permitido: `[tipos]`
- Conteudo proibido: `[tipos]`

## Regras

- Conteudo editavel deve representar informacao real, nao detalhe tecnico de layout.
- Campos devem ter labels e descricoes claras.
- Conteudo rico deve ser sanitizado ou validado.
- Componentes de apresentacao continuam no codigo quando fizer sentido.

## Blocos ou Campos Aceitos

- `[campo ou bloco]`

## Evolucao

Novos campos ou blocos podem ser adicionados quando:

- houver necessidade recorrente;
- o contrato for claro;
- houver validacao ou teste quando aplicavel.
