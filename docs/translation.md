# GUI translation

KNS integrates with the text translation API documented by the
[xnx3/translate](https://github.com/xnx3/translate) project. The JavaScript
library itself is browser-only because it scans an HTML DOM; KNS calls the
service API from its native C++ desktop process instead.

## Behavior

- English is local and never makes a translation request.
- Selecting Portuguese, Spanish, French, German, Japanese, or Simplified
  Chinese queues visible static labels from every KNS panel, including network,
  topology, TCP, metrics, event-log, node-details, and settings views.
- Requests run on a worker thread and are batched to avoid blocking Dear ImGui.
- Successful translations are cached in memory for the selected language.
- Network errors leave the original English label visible. Failed batches retry
  automatically with exponential backoff up to 30 seconds; the UI also exposes
  an immediate retry action.
- Changing language invalidates translations from an older in-flight request.
- Japanese and Simplified Chinese glyphs are merged from common system fonts
  when one is installed. Windows uses the standard Yu Gothic/MS Gothic and
  Microsoft YaHei families; macOS and Linux probe their usual CJK font paths.

Only strings explicitly passed to `TranslationService::translate()` are sent.
The current integration passes static interface labels; topology contents,
filenames, packet data, and simulation results are not translated or uploaded.

## Service endpoint

The default service is:

```text
http://api.translate.zvo.cn/translate.json
```

The request is an `application/x-www-form-urlencoded` POST containing `to` and
`text`. The latter is a JSON array, which lets KNS translate labels in batches.

Set `KNS_TRANSLATION_API_BASE_URL` before starting KNS to use a compatible
private deployment. Supply the base URL without `/translate.json`; a trailing
slash is accepted and removed.

PowerShell example:

```powershell
$env:KNS_TRANSLATION_API_BASE_URL = "http://127.0.0.1:8080"
.\build\app\Debug\KNS.exe
```

The public endpoint documented by the provider uses unencrypted HTTP. KNS sends
only the static labels described above. Use a trusted private deployment when
transport privacy or service availability is required.

## Extending coverage

Pass the application-owned `TranslationService` explicitly to UI components and
use `translate()` for visible static text. Use `label()` for windows and widgets
so their Dear ImGui IDs remain stable across languages. Avoid sending dynamic or
user-controlled content unless that behavior is explicit in the feature and its
privacy contract.
