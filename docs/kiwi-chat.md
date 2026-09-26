# KiWi topology chat

## Contract and acceptance criteria

- The Intelligence panel offers Analysis and KiWi Chat tabs. Chat sends the
  current deterministic topology analysis, score, and conversation to Sentient
  KNS (`POST /api/v1/intelligence/chat`), which forwards to KiWi (`POST /api/v1/chat`).
- Responses answer the user's question in natural language using a local LLM.
  Chat never substitutes a deterministic report for an unavailable LLM.
- Each request has `requestId`, `topologyRevision`, `analysisMode`, `context`,
  `score`, and alternating user/assistant `messages`, ending in a user message.
  Responses echo both identifiers and contain `message`.
- The UI stays responsive, permits one pending turn, preserves failed questions
  for retry, and supports a new conversation. Changing topology or clearing chat
  discards late responses. History is local to the application session.
- At most ten completed turns accompany a question. Questions are limited to
  4,096 UTF-8 bytes in the desktop UI. The server bounds messages and model input;
  oversized requests fail explicitly instead of silently dropping topology facts.
- Topology context is a deterministic snapshot, not live simulation telemetry.
  Historical messages and embedded topology labels are untrusted text. The LLM
  must distinguish evidence from suggestions and acknowledge missing facts.

## Run

Start KiWi with its existing `KIWI_LLM_ENABLED=true` and `KIWI_LLM_MODEL`
configuration (install its `.[llm]` dependencies). Start Sentient KNS on port 8080
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
