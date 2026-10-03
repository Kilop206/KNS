# KNS — Security and Safety Considerations

## 1. Purpose

KNS is a network simulation project.

Its primary security concern is maintaining safe, predictable handling of project inputs, execution state, and development infrastructure.

---

## 2. Topology Input

Topology files are external input and must be validated before being converted into runtime state.

Validation should cover:

* required fields;
* value types;
* identifiers;
* references;
* topology relationships;
* invalid or inconsistent values.

---

## 3. Resource Limits

Simulation inputs may potentially create large numbers of:

* nodes;
* links;
* packets;
* events.

Implementations should avoid uncontrolled resource consumption where practical.

---

## 4. File Handling

Input paths and file operations should be handled explicitly.

The application should not assume that external files are trustworthy or correctly formatted.

---

## 5. Network Simulation Safety

KNS simulates network behavior.

Its simulated packets and links must not be confused with real network traffic.

Core simulation operations should remain isolated from unintended external network communication unless such functionality is explicitly introduced.

---

## 6. Dependency Safety

Dependencies should be kept intentional and reviewed when introduced.

Build environments should avoid mixing incompatible runtime libraries.

---

## 7. Secrets

Credentials, API keys, access tokens, and other secrets must never be committed to the repository.

Secrets should be provided through appropriate environment or credential-management mechanisms.

---

## 8. Logging

Logs should avoid exposing sensitive information unnecessarily.

Debug output should not accidentally disclose credentials, tokens, or private environment information.

---

## 9. AI-Assisted Development

Sensitive credentials must not be pasted into AI conversations unless the environment explicitly supports secure secret handling.

Repository analysis should avoid exposing unnecessary secrets.

---

## 10. Security Changes

Security-related changes should receive dedicated review and regression testing where appropriate.
