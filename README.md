# Imperador — Bot do Discord (Tibério)

Bot privado do Discord escrito em TypeScript, inspirado no personagem Tibério, com personalidade imperial, memória persistente, análise contextual e um runtime próprio de IA/ML com capacidade progressiva de autonomia.

> **Estado atual — 2 de setembro de 2026**
>
> O projeto já passou pela construção do núcleo conversacional e avançou para uma arquitetura própria de inteligência, memória semântica, aprendizado incremental, ferramentas autônomas, observação, planejamento, segurança operacional e auditoria. O build e a suíte de testes atuais estão funcionando.

## Visão geral

O Imperador começou como um bot de respostas e triggers para Discord. Atualmente, sua arquitetura é dividida em camadas que permitem que o sistema:

- compreenda características básicas das mensagens;
- mantenha memória persistente;
- recupere contexto relevante de conversas anteriores;
- mantenha estado emocional e personalidade;
- classifique intenções com um modelo próprio;
- faça recuperação semântica baseada em TF-IDF e embeddings próprios;
- mantenha memória neural semântica;
- aprenda incrementalmente a partir de feedback controlado;
- gere candidatos de aprendizado ativo;
- avalie e refine modelos sem mistura indevida entre treino, validação e teste;
- execute ferramentas internas com políticas de risco;
- observe execuções e eventos do sistema;
- planeje e execute ações em ciclos autônomos;
- mantenha limites de frequência e orçamento;
- possua kill switch e controle administrativo;
- registre alterações do runtime em um audit trail persistente.

A autonomia continua sendo tratada como uma camada controlada. O agente não recebe acesso arbitrário ao sistema operacional ou à internet: suas ações dependem das ferramentas registradas, do Planning Engine e do Safety Permission Engine.

---

## Principais funcionalidades

### Conversação e personalidade

- ✅ Frases espontâneas categorizadas
- ✅ Respostas por palavra-chave
- ✅ Respostas por contexto
- ✅ Respostas dependentes de frequência
- ✅ Sistema de raridade
- ✅ Modos especiais de comportamento
- ✅ Triggers automáticos
- ✅ Detecção de agressividade
- ✅ Detecção de sarcasmo
- ✅ Validação de respostas para reduzir contradições
- ✅ Sistema de elogios
- ✅ Controle de modos por comandos

### Memória

- ✅ Memória persistente em SQLite
- ✅ Memória automática de interações
- ✅ Memória contextual
- ✅ Tópicos dinâmicos de memória
- ✅ Memórias influenciando respostas
- ✅ Eventos e histórico de memória
- ✅ Recuperação semântica de memórias relevantes

### IA / ML / DL próprios

O projeto não depende de uma API externa de IA para seu pipeline principal de inteligência.

- ✅ Análise textual
- ✅ Multinomial Naive Bayes próprio para intenção
- ✅ Dataset e avaliação de intenção
- ✅ Aprendizado incremental persistente
- ✅ Active Learning para candidatos de intenção
- ✅ Feedback supervisionado para classificação
- ✅ TF-IDF próprio
- ✅ Similaridade semântica
- ✅ Word Embedding Model próprio baseado em Skip-gram / Negative Sampling
- ✅ Semantic Sentence Model próprio
- ✅ Dataset de pares semânticos
- ✅ Split de dados para treinamento, validação e teste
- ✅ Error analysis
- ✅ Hard negative mining
- ✅ Data augmentation
- ✅ Semantic fine-tuning
- ✅ Registry de modelos semânticos
- ✅ Neural Semantic Memory
- ✅ Hybrid Retrieval
- ✅ Semantic Context
- ✅ Model Manager
- ✅ Persistência dos modelos
- ✅ Integração do runtime de IA
- ✅ Feedback semântico
- ✅ Active Learning semântico
- ✅ Fine-tuning semântico controlado
- ✅ SemanticSafeFineTuningService

### Runtime autônomo

A arquitetura atual possui um agente autônomo experimental e controlado, formado por componentes independentes:

- ✅ `ToolRegistry`
- ✅ `SafetyPermissionEngine`
- ✅ `ObservationEngine`
- ✅ `PlanningEngine`
- ✅ `AutonomousToolCatalog`
- ✅ `AutonomousAgentOrchestrator`
- ✅ `AutonomousRuntimeControlService`
- ✅ `AutonomousRuntimeAuditService`
- ✅ `runtime_audit` tool

O agente trabalha em ciclos e pode criar ou executar planos de forma limitada. Ferramentas de diagnóstico atuais são somente de leitura e de baixo risco.

Ferramentas autônomas disponíveis no catálogo padrão:

| Ferramenta | Risco | Finalidade |
|---|---|---|
| `system_health` | low | Diagnóstico básico do estado do sistema |
| `active_goals` | low | Consulta objetivos ativos |
| `recent_observations` | low | Consulta observações recentes |
| `active_plans` | low | Consulta planos ativos |
| `model_status` | low | Consulta estado dos modelos |
| `runtime_audit` | low | Consulta o histórico de auditoria do runtime |

Ferramentas existentes são preservadas quando o catálogo padrão é registrado, permitindo extensibilidade sem sobrescrever registros customizados.

---

## Segurança da autonomia

O runtime autônomo foi projetado para falhar de forma conservadora.

### Safety Permission Engine

O `SafetyPermissionEngine` controla, entre outros aspectos:

- estado de habilitação do runtime;
- kill switch;
- nível máximo de risco permitido;
- exigência de aprovação para níveis de risco elevados;
- limite de execuções por janela;
- janela de frequência;
- orçamento de execução;
- fontes permitidas e negadas;
- autorização de ferramentas;
- auditoria de segurança.

### Kill switch

O kill switch possui prioridade sobre a ativação do agente.

```text
!autonomia kill
    ↓
Kill switch ATIVO
    ↓
Agente desabilitado

!autonomia on
    ↓
BLOQUEADO enquanto o kill switch estiver ativo

!autonomia unkill
    ↓
Kill switch DESATIVADO
    ↓
Agente continua desligado

!autonomia on
    ↓
Agente ATIVADO

Essa separação é intencional: remover o kill switch não liga automaticamente o agente.

Comandos administrativos de autonomia

Os comandos abaixo exigem a permissão Administrator no Discord.

!autonomia status
!autonomia on
!autonomia off
!autonomia kill
!autonomia unkill
!autonomia status

Exibe informações como:

estado do agente;
estado do kill switch;
estado do orquestrador;
quantidade de ciclos na janela;
objetivos ativos;
planos ativos;
execuções de ferramentas;
orçamento utilizado;
quantidade de registros de auditoria de segurança;
quantidade de registros de auditoria do runtime;
última decisão do agente.
!autonomia on

Habilita explicitamente o runtime autônomo.

O comando não pode contornar um kill switch ativo.

!autonomia off

Desabilita o orquestrador sem remover outras configurações de segurança.

!autonomia kill

Ativa o kill switch e desabilita imediatamente o agente.

!autonomia unkill

Remove o kill switch, mas mantém o agente desligado até um !autonomia on explícito.

Auditoria do runtime

O AutonomousRuntimeAuditService mantém um histórico persistente em:

data/autonomous-runtime-audit.json

Eventos registrados atualmente:

runtime_started
runtime_shutdown
runtime_enabled
runtime_disabled
kill_switch_enabled
kill_switch_disabled
runtime_reset

Cada evento pode armazenar:

identificador único;
timestamp;
origem;
ator responsável;
detalhes sanitizados.

O armazenamento possui limite de entradas e escrita por arquivo temporário exclusivo para reduzir problemas de concorrência e substituir o arquivo de forma segura.

Consulta pelo agente

A ferramenta runtime_audit permite recuperar os registros recentes e filtrar por tipo, mantendo a consulta somente de leitura e classificada como baixo risco.

Ciclo do agente autônomo

O runtime é executado em ciclos de aproximadamente 15 segundos quando habilitado.

O loop pode permanecer ativo mesmo com o agente desligado. Nesse estado, ele apenas verifica a configuração e não executa um ciclo autônomo.

Isso permite ligar e desligar a autonomia em runtime sem reiniciar o bot:

Bot online
   ↓
Runtime autônomo OFF
   ↓
!autonomia on
   ↓
Runtime autônomo ON
   ↓
Ciclos periódicos

Ao desligar o agente:

!autonomia off
   ↓
Orchestrator OFF
   ↓
Próximos ciclos ignorados
Arquitetura de alto nível
Discord
   │
   ▼
Message Handler
   │
   ├── TriggerManager
   ├── TextAnalyzer
   ├── AutoMemoryService
   ├── EmotionEngine
   └── ReplyService
           │
           ▼
   Response Engine
           │
           ├── Memória contextual
           ├── Personalidade
           ├── Recuperação semântica
           └── Runtime de IA

Runtime autônomo
   │
   ▼
AutonomousAgentOrchestrator
   │
   ├── Goal Engine
   ├── Planning Engine
   ├── Tool Registry
   ├── Observation Engine
   └── Safety Permission Engine
           │
           ├── limites de risco
           ├── frequência
           ├── orçamento
           ├── kill switch
           └── auditoria

AutonomousRuntimeAuditService
   │
   ▼
 data/autonomous-runtime-audit.json
Estrutura do projeto
Imperador/
├── src/
│   ├── config/
│   │   └── config.ts
│   ├── intelligence/
│   │   ├── aiRuntimeService.ts
│   │   ├── emotionEngine.ts
│   │   ├── modelManager.ts
│   │   ├── observationEngine.ts
│   │   ├── planningEngine.ts
│   │   ├── safetyPermissionEngine.ts
│   │   ├── toolRegistry.ts
│   │   ├── autonomousAgentOrchestrator.ts
│   │   ├── autonomousToolCatalog.ts
│   │   ├── autonomousRuntimeControlService.ts
│   │   ├── autonomousRuntimeAuditService.ts
│   │   ├── autonomousRuntimeAuditTool.ts
│   │   └── ...
│   ├── services/
│   │   ├── scheduler.ts
│   │   ├── reply.ts
│   │   ├── triggerManager.ts
│   │   ├── modeManager.ts
│   │   ├── autoMemoryService.ts
│   │   ├── textAnalyzer.ts
│   │   ├── responseEngine.ts
│   │   ├── memoryContext.ts
│   │   ├── semanticMessageActiveLearningService.ts
│   │   └── ...
│   ├── state/
│   │   └── emotionState.ts
│   └── index.ts
├── tests/
│   ├── autonomousAgentOrchestrator.test.ts
│   ├── autonomousAgentToolExecution.test.ts
│   ├── autonomousToolCatalog.test.ts
│   ├── autonomousRuntimeControlService.test.ts
│   ├── autonomousRuntimeAuditService.test.ts
│   ├── autonomousRuntimeAuditTool.test.ts
│   └── ...
├── data/
│   └── ...
├── build/
├── package.json
├── tsconfig.json
└── README.md
Tecnologias
Node.js
TypeScript
CommonJS
discord.js 14
dotenv
node:sqlite
node:test
tsx

Configuração de compilação atual:

{
  "target": "ES2020",
  "module": "commonjs",
  "rootDir": "./src",
  "outDir": "./build",
  "strict": true
}
Requisitos
Node.js compatível com as APIs utilizadas pelo projeto
npm
Aplicação e bot criados no Discord Developer Portal
Token do bot
IDs dos canais permitidos, conforme a configuração atual do projeto
Instalação local

Clone o projeto:

git clone https://github.com/Kilop206/Imperador.git
cd Imperador

Instale as dependências:

npm install

Configure as variáveis de ambiente necessárias para o bot em seu arquivo local de ambiente.

Nunca versione tokens, senhas ou outros segredos.

Desenvolvimento

Compile o TypeScript:

npm run build

Execute a versão compilada:

npm start

Execute todos os testes:

npm test
Validação atual

A suíte de testes foi executada com sucesso durante o desenvolvimento atual e o build TypeScript também foi concluído com sucesso.

Configuração do runtime autônomo

O agente começa desligado por padrão.

Para manter esse comportamento seguro em produção:

AUTONOMOUS_AGENT_ENABLED=false

Para iniciar o agente já habilitado pelo ambiente:

AUTONOMOUS_AGENT_ENABLED=true

Mesmo quando iniciado habilitado, o runtime continua sujeito ao SafetyPermissionEngine e ao kill switch.

Para uma primeira implantação, recomenda-se manter:

AUTONOMOUS_AGENT_ENABLED=false

e habilitar manualmente após verificar o ambiente com:

!autonomia status
!autonomia on
Deploy

O projeto está sendo preparado para execução em produção na Discloud.

O package.json utiliza:

{
  "main": "build/index.js",
  "scripts": {
    "build": "tsc",
    "start": "node build/index.js",
    "test": "tsx --test"
  }
}

O fluxo de produção esperado é:

npm test
   ↓
npm run build
   ↓
Deploy
   ↓
Bot online com autonomia OFF
   ↓
!autonomia status
   ↓
Validação em produção
   ↓
!autonomia on

Segredos devem ser configurados diretamente no ambiente de hospedagem e nunca incluídos no repositório.

Como funciona o sistema de respostas

A camada conversacional usa múltiplas fontes de informação antes de responder.

Entre elas:

contexto da mensagem;
agressividade;
elogios;
modo atual;
palavras-chave;
histórico/frequência;
memória contextual;
recuperação semântica;
personalidade e estado emocional.

O sistema possui mecanismos de validação para evitar respostas incoerentes com o conteúdo recebido.

Modos especiais

O projeto possui os seguintes modos especiais de personalidade:

Bêbado
Ameaça
Humor
Sério
Nostálgico
Filosófico
Romano

A seleção de modo pode ocorrer por comando ou automaticamente através dos triggers configurados.

Comandos tradicionais do Tibério incluem:

!tiberio_caotico
!tiberio_bebado
!tiberio_normal
!tiberio_ameaca
!tiberio_humor
!tiberio_serio
!tiberio_nostalgico
!tiberio_filosofico
!tiberio_romano
!tiberio_status
!tiberio_raro
!tiberio_triggers
Triggers automáticos

O sistema monitora padrões de conversa e pode ativar modos específicos.

Exemplos de categorias monitoradas:

humor e risadas;
bebida e comemoração;
temas graves;
nostalgia e memória;
questões filosóficas;
temas relacionados ao Império Romano;
agressividade.

Os triggers possuem contadores e janelas temporais para impedir que uma única mensagem cause mudanças permanentes de comportamento.

Filosofia de desenvolvimento

O projeto segue uma evolução incremental:

Bot de respostas
      ↓
Contexto
      ↓
Memória
      ↓
Estado emocional
      ↓
Personalidade
      ↓
ML de intenção
      ↓
Semântica
      ↓
Aprendizado incremental
      ↓
Memória neural
      ↓
Ferramentas
      ↓
Observação
      ↓
Planejamento
      ↓
Autonomia controlada

A ideia é aumentar a capacidade do agente sem transformar o sistema em uma caixa-preta sem limites operacionais.

Segurança e boas práticas

⚠️ Nunca compartilhe ou versione:

token do Discord;
arquivos .env com segredos;
chaves privadas;
credenciais de serviços;
arquivos de produção contendo dados sensíveis.

O runtime autônomo deve permanecer sob observação durante a fase inicial de produção.

Recomenda-se validar primeiro:

status → autonomia OFF → validação → autonomia ON → observação
Troubleshooting
Bot não conecta

Verifique:

token do Discord;
intents habilitadas no Discord Developer Portal;
permissões do bot no servidor;
configuração do ambiente de execução.
Bot conecta mas não responde

Verifique:

canais permitidos;
permissões de leitura e envio;
conteúdo das mensagens;
logs da aplicação.
Runtime autônomo não executa ciclos

Execute:

!autonomia status

Confirme:

Agente: ATIVO;
Kill switch: INATIVO;
Orquestrador: habilitado.

Se o kill switch estiver ativo:

!autonomia unkill
!autonomia on
Agente foi desligado por segurança

O kill switch deve ser removido explicitamente antes de uma nova ativação:

!autonomia kill
!autonomia unkill
!autonomia on
Status do projeto
Núcleo do bot
✅ Discord runtime
✅ Respostas automáticas
✅ Modos de personalidade
✅ Triggers
✅ Memória persistente
✅ Estado emocional
Inteligência
✅ Intent classification
✅ Aprendizado incremental
✅ Active Learning
✅ TF-IDF
✅ Embeddings próprios
✅ Sentence semantics
✅ Semantic retrieval
✅ Neural semantic memory
✅ Hybrid retrieval
✅ Semantic context
✅ Safe semantic fine-tuning
Autonomia
✅ Tool Registry
✅ Safety Permission Engine
✅ Observation Engine
✅ Planning Engine
✅ Autonomous Tool Catalog
✅ Autonomous Agent Orchestrator
✅ Runtime Control
✅ Runtime Audit Trail
✅ Runtime Audit Tool
✅ Controle administrativo via Discord
✅ Kill switch
✅ Limites operacionais
Produção
✅ npm test
✅ npm run build
🚧 Preparação final para deploy na Discloud
🚧 Validação do runtime autônomo em produção
Licença

MIT