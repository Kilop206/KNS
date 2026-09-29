# KiWi topology chat

## Contract and acceptance criteria

- The Intelligence panel offers Analysis and KiWi Chat tabs. Chat sends the
  current deterministic topology analysis, score, and conversation to Sentient
  KNS (`POST /api/v1/intelligence/chat`), which forwards to KiWi (`POST /api/v1/chat`).
- Responses answer the user's question in natural language using a local LLM.
  Chat never substitutes a deterministic report for an unavailable LLM.
- Each request has `requestId`, `topologyRevision`, `analysisMode`, `context`,
  `score`, and alternating user/assistant `messages`, ending in a user message.
  Responses echo both identifiers and contain `message` and `historyTurnsOmitted`.
  The latter counts complete oldest turns omitted by KiWi to fit the model's
  token budget; older servers may omit the field (treated as zero).
- The UI stays responsive, permits one pending turn, preserves failed questions
  for retry, and supports a new conversation. Changing topology or clearing chat
  discards late responses. History is local to the application session.
- At most ten completed turns accompany a question. Questions are limited to
  4,096 UTF-8 bytes in the desktop UI. The server bounds messages and model input;
  KiWi drops complete oldest conversation turns as needed to fit the token budget,
  preserving the entire snapshot and current question. If those alone cannot fit,
  the request fails explicitly. The UI annotates each reply with the total number
  of turns omitted by client and server; the visible transcript remains intact.
- Topology context is a deterministic snapshot, not live simulation telemetry.
  Historical messages and embedded topology labels are untrusted text. The LLM
  must distinguish evidence from suggestions and acknowledge missing facts.

## Using the desktop chat

Load a topology and open **KNS Intelligence → KiWi Chat**. In a new conversation,
the suggested questions fill the composer for editing; they never send a request
automatically or replace an existing draft. **Enter** sends; **Shift+Enter** or
**Ctrl+Enter** inserts a new line.

After a reply, **Explicar melhor**, **Próximos passos**, and **Mostrar evidências**
prepare follow-up questions for editing. Suggestions never overwrite a draft.
The composer remains editable while waiting, so you can prepare the next question;
sending waits until the pending request finishes. A byte counter shows the input
limit and a timer shows how long the current request has been waiting.

Reading older messages preserves the scroll position when a reply arrives. Use
**Nova resposta - ir ao fim** to jump to it. Sending a question follows the latest
message. Failed questions can be retried, edited when the composer is empty, or
discarded without losing the next draft.

Each message has a **Copiar** button. **Copiar conversa** copies the entire visible
conversation as plain UTF-8 text, including the topology revision and notices
about history omitted from model input. It includes pending/failed questions as
unanswered and does not copy internal transport errors. Copy before starting a
new conversation, changing topology, or closing KNS to keep the transcript.

## Run

Start KiWi with its existing `KIWI_LLM_ENABLED=true` and `KIWI_LLM_MODEL`
configuration (install its `.[llm]` dependencies). Start Sentient KNS on port 8081
and KNS as usual. The chat gateway uses `KIWI_BASE_URL` (default
`http://localhost:8000`) and `KIWI_CHAT_TIMEOUT_SECONDS` (default 120).

KNS uses `KNS_INTELLIGENCE_BASE_URL` and the existing optional bearer token.
`KNS_INTELLIGENCE_CHAT_ENDPOINT` defaults to `/api/v1/intelligence/chat`;
`KNS_INTELLIGENCE_CHAT_TIMEOUT` defaults to 130 seconds. For direct local testing,
set the base URL to `http://localhost:8000` and chat endpoint to `/api/v1/chat`.
The separate analysis endpoint retains its existing contract.

KiWi applies its existing conservative grounding rules to the reply. These rules
do not prove arbitrary natural-language statements correct, particularly outside
English; users should verify suggestions against KNS facts. No chat action edits
or runs a topology. Automated tests use fake inference and local HTTP servers;
model quality requires separate evaluation with the configured weights.
