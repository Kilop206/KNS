# Find and focus devices

Use **Find devices** in the Network toolbar, or press **Ctrl+F** while the
Network window has focus. Search results can locate devices outside the current
viewport. Select a row with the mouse, or use Up/Down and Enter. Escape closes
the finder.

The search matches labels, device type names, addresses, MAC addresses and
external discovery IDs. Whitespace-separated terms are combined with AND;
different terms may match different fields of the same device.

| Query | Meaning |
| --- | --- |
| `gateway` | Any searchable field contains gateway |
| `192.0.2.20` | Address or another field contains that substring |
| `type:router` | Exact device type filter |
| `#17` | Exact active node ID 17 |
| `type:server 192.0.2` | A server with a matching address/identity/label |

Matching ignores ASCII letter case. Other UTF-8 characters are matched as
entered. Use the stable type names from `DeviceType.hpp`, such as `access_point`
and `network_segment`, for type filters. A malformed #ID or unknown type has no
matches. Bare numbers are ordinary substring queries; use #ID for exact lookup.

The popup shows at most 100 results in node-ID order and displays the total
match count. Refine the query to reach matches beyond that limit. It refreshes
from live metadata every frame, excludes removed devices and never treats a
truncated list as complete. The table shows the first address; hover over it
to see additional addresses. The query persists when reopening the finder.

Selecting a result centers the device at a readable zoom and selects it for the
existing device/service panels. It ends any pending placement, cable or TCP
selection. No traffic is started and device positions remain unchanged.
**Focus selected**, the **F** key, and the device context menu's **Focus device**
action recenter an already selected node. F is ignored during text editing.
Ctrl+F is scoped to the Network window and does not replace another open popup.

The existing **Fit** action still frames the whole topology. Search, focus and
camera changes do not alter routing, scheduler state, saved positions or
simulation time. Cached route overlays are also scoped to the engine instance,
so switching engines with equal routing revision counters refreshes the path.
