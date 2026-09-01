# CardMind Phase 5 traceability

This is the only active Phase 5 matrix. `ROADMAP.md` defines product scope; this file
records the minimum atomic plan and only observed evidence. Phase 5 is one user-visible
foreground cycle, so its execution, handoff, result return and required interfaces are one
coherent production row rather than independently publishable partial features.

## Phase lock

- Phase source is authenticated remote `develop` at
  `89163c43a997ba4779446c63146c8cf2f539a9bf`, the reviewed merge of Phase 4 source head
  `9e9cf474b8d6f5edf0d2acb694aa7546244570c9` through PR #2. Remote `main` remains
  `681cc8ffa9b6d26897d4847001d5d57f17b5d340`.
- Work branch is `feature/phase-5-python-one-shot`, created from that exact `develop` commit.
  The sole retained stash `6073fd15eb2836351ef2ae4323926565339a495b` and the three
  approved Architect-owned untracked `.codex/agents` definitions are outside Phase 5 ownership.
- Target elapsed implementation time is 12 active hours. Sixteen active hours is the normal
  ceiling, not planned contingency. Work above it requires a concrete hardware blocker,
  ownership classification and a materially different pivot.
- The existing Shared workspace, manual CardMind/MicroPython mode switch, MicroPython
  supervisor, browser same-address reconnect and Phase 3 policy/pending/audit/cancel boundaries
  are inherited owners. P5 may extend them only at the one-shot integration boundary.

## Locked product contract

One cycle is:

`model writes script -> user approves exact bytes -> CardMind stages one request and boots
MicroPython -> one foreground script runs -> one bounded result returns to the originating
chat -> CardMind resumes`

- Model-written code always requires one explicit one-run approval, including under policy
  `Allow`. Approval binds one normalized project-linked `.py` path under the existing Shared
  workspace, exact byte size and SHA-256. The complete source must be available for review;
  a truncated preview alone is not approval evidence.
- Changed bytes, path or hash, a new run, or a stale/rebooted pre-approval requires a new
  approval. `Off`, `Deny`, malformed, stale, changed and unapproved input creates no runnable
  request, changes no boot selection and executes no script.
- After approval, those exact bytes are user-authorized privileged MicroPython code with full
  device access. Phase 5 makes no sandbox claim.
- CardMind rechecks path, size and SHA-256 before staging. MicroPython rechecks them immediately
  before `exec`. Any mismatch fails closed and returns to CardMind without execution.
- The handoff is one bounded consume-once request/result. Before `exec`, MicroPython selects
  CardMind as the next boot target and enables one fixed watchdog. It runs exactly one foreground
  script and writes bounded stdout, stderr and exit state.
- CardMind attaches a durable result to the originating chat exactly once, distinguishes an
  already-attached result after interruption, prohibits replay and removes only exact-owned
  request/result artifacts. The existing browser address and polling/reconnect path are reused.

## Explicit non-goals

- No package/module catalog or installer UI, source-limit uplift, `.mpy` tooling, templates,
  named/manual/scheduled automation, autorun/safe-mode UI, queue, job, background execution,
  replay, automatic retry or general recovery framework.
- No second Python workspace root, Web server, permission/profile/storage framework, handoff
  framework, USB preparation or Phase 6/7 UI/QOL/transport work.
- The existing 64 KiB script ceiling and one-script concurrency remain unchanged unless a
  separately measured user requirement changes `ROADMAP.md` before implementation.

Status values are `pending`, `in_progress`, and `completed`.

## Execution matrix

| ID | Atomic boundary | Required observation | Status |
| --- | --- | --- | --- |
| P5-01 | Phase kickoff: reconcile canonical scope and P4 inheritance; inventory every existing producer, consumer, persisted representation and vendor contract; freeze the one-shot design, proof matrix, non-goals and exact write set | Exact source/branch/clean-state evidence, complete inventory, proportional proof, bounded crash/cleanup contract and independent pre-edit GO before production | completed |
| P5-02 | Implement the complete one-shot vertical cycle through existing P3 policy/approval, Shared workspace, mode switch, supervisor, Device UI and WebUI/reconnect owners | Exact one-run approval and full-source review; fail-closed no-effect cases; success/exception/reset/watchdog/power-loss return; exactly-once originating-chat result; no replay; exact cleanup; manual workspace preserved | pending |
| P5-03 | Phase closure and publication | Focused host/build/COM8/browser acceptance, resource/latency/SD ownership, compatibility and documentation checks, exact cleanup, independent review, per-row publication evidence, green CI and reviewed merge only to `develop` | pending |

## P5-01 active gate

**Started:** 2026-09-01T20:33:23+03:00.

**Completed:** 2026-09-01T21:45:57+03:00.

- Current hypothesis: the complete P5 contract is a marginal integration over already verified
  `python_mode`, Phase 3 pending confirmation/continuation, the installed MicroPython supervisor
  and browser reconnect. No new framework, server or execution owner is needed.
- Expected information from inventory: the exact existing path/hash/policy producers; boot-marker
  and partition-switch ordering; supervisor request/result and watchdog seams; originating-chat
  continuation owner; Device/Web full-source and transition surfaces; and the smallest retained
  test boundary that executes production rather than duplicating it.
- P5-01 changes no production behavior. The canonical phase transition is already present in the
  inherited source commit, so its retained write set is only this trace. Production files enter
  only the later reviewed P5-02 write set.
- P5-01 completes only after one bounded independent reviewer confirms requirement coverage,
  existing-boundary reuse, crash/no-replay ordering, proportional proof and absence of unnecessary
  responsibility. Architect closure GO, one row-owned commit and immediate remote verification
  remain mandatory before P5-02 starts.

### Locked inventory and reuse

- `tool_catalog`, `api_client`, `tool_policy` and `tool_router` already own stable schema identity,
  layered Off/Ask/Allow resolution, per-message intent, availability and exact dispatch. P5 adds
  only the exact `python_run({name})` schema under `PythonWriteRun`; stored defaults remain Off and
  both Ask and Allow resolve to mandatory one-run confirmation.
- `pending_tool_call` already persists the canonical call, project/chat identity and revision,
  original message count, plus exact file path/existence/size/SHA-256. Its same-boot resumability,
  claim-before-effect and stale/reboot refusal remain the approval owner. Python staging performs
  a final independent path/size/hash read after claim and before any P5 artifact or boot effect.
- The Shared workspace remains `/assistant/files`; existing canonical path validation, project
  links, atomic file writes and 64 KiB source ceiling remain authoritative. No Python workspace,
  package store or source path is added.
- `python_mode` already owns the `cardmind_py` NVS namespace, MicroPython image/layout readiness,
  partition selection and CardMind/MicroPython switch. P5 adds one consume-once state blob and two
  fixed internal SD records there; it does not add a second handoff or recovery owner.
- `micropython/vfs/boot.py` already provides fail-back startup and remains unchanged.
  `cardmind_supervisor.py` keeps its manual authenticated Web workspace and volatile single runner;
  one-shot execution is a synchronous startup branch before Wi-Fi/server startup and never enters
  the manual worker or `/api/run` path.
- `project_chat_storage` already owns durable chat append. The pending record's original message
  count plus an exact deterministic result message distinguishes not-attached from already-attached
  after reset; no model/API continuation is persisted or replayed.
- Device pending screens and Web pending dialog already own confirmation; Device and Web file
  viewers already expose full source in bounded pages. P5 exposes the exact source path from the
  Python confirmation and reuses those viewers. The Web approval response reuses the existing SSE
  terminal stream and same-address session polling; it never opens or duplicates the Python Web
  workspace.
- `tool_activity` remains the audit owner. P5 adds the Python target and one narrow idempotent
  terminal append for an exact persisted running sequence after reboot; it does not add an audit,
  job or recovery journal.
- Existing `host_tests.cpp`, `micropython_supervisor_test.py`, `web_console_ui_test.mjs` and
  `python_handoff_e2e.mjs` are the retained proof owners. P5 extends only their changed boundaries.

### Frozen one-shot design

1. Admission requires an available installed Python image, readable/writable Shared workspace, an
   exact normalized project-linked case-sensitive `.py` path, size at most 65,536 bytes, complete
   valid UTF-8 source that the existing text viewers can render, and the catalogued
   `python_run` call with no extra arguments. Off, Deny, unavailable, malformed and non-UTF-8 calls
   stop before pending creation or any P5 side effect.
2. Pending preview records path, size and SHA-256 and identifies the full-source viewer. Approval is
   always `Mandatory`, offers Allow once/Deny only, and is valid only in the creating boot. The
   approval path claims the pending call, reopens the exact file and recomputes path, size,
   SHA-256 and whole-file UTF-8 validity before any P5 side effect.
3. After durable `ClaimedApprove`, CardMind revalidates source and exact-cleans stale fixed P5
   artifacts, starts the existing audit and obtains its authoritative 64-bit sequence, then
   serializes one strict compact request to `/assistant/v2/python_run_request.json` through its
   fixed `.tmp`. The request binds that sequence and the approval surface (`device` or `web`).
   CardMind hashes the exact serialized bytes, then persists one fixed NVS blob under
   `cardmind_py/run`. The 91-byte blob is version, state, return surface, 16-hex pending/run id,
   request SHA-256, result SHA-256 (all zero until complete) and full 64-bit audit sequence. The
   individually durable `run=pending` write is last. Only then may CardMind select the installed
   MicroPython partition and reset. Same-boot staging failure finishes that audit as failed and
   removes exact request/temp without creating runnable NVS state.
4. `cardmind_supervisor.start()` checks for one-shot `run` immediately after app validation and NVS
   namespace acquisition, before the existing `mode_error` erase/commit and all manual Wi-Fi/Web
   startup. A valid `pending` blob selects CardMind as next boot, persists the same blob as
   `claimed`, then starts `machine.WDT(0, timeout=30000)`. Under that claimed/no-replay state it
   reads the bounded request, verifies its exact hash and strict fields including matching surface
   and 64-bit sequence, validates the Shared path, reads at most 65,536 exact script bytes,
   verifies size/SHA-256 and decodes those same bytes as valid UTF-8. It then performs exactly one
   foreground `exec` with `__name__=__main__` and `__file__` set. There is no worker, queue, retry
   or second source read. Validation failure writes a bounded failure result when possible; if that
   persistence fails, `claimed` returns the conservative effects-unknown/no-replay outcome.
5. A bounded collector keeps stdout and stderr separate under one 16,384-byte decoded UTF-8 budget,
   records per-stream truncation, and emits compact base64 fields so JSON escaping cannot amplify
   SD/RAM use. Normal completion, `SystemExit`, validation failure and uncaught exception produce
   one strict result at `/assistant/v2/python_run_result.json` through its fixed `.tmp`. The exact
   durability sequence is: serialize and hash at most 24,576 bytes in RAM; open the absent fixed
   temp for binary write; require full write count; flush and close; `os.sync()`; bounded reopen and
   length/SHA-256 readback of temp; require final path absent; same-VFS `os.rename(temp, result)`;
   `os.sync()`; bounded reopen and length/SHA-256 readback of final. Only then does MicroPython
   persist `run=complete` with that result SHA-256. Any write/flush/close/sync/readback/rename
   exception, mismatch or power loss before complete leaves `claimed`; CardMind never executes it
   again and exact-cleans any temp/final residue after attaching the conservative outcome.
6. CardMind boot resolves `pending` as interrupted before launch, `claimed` as interrupted with
   effects unknown, and `complete` as a validated stdout/stderr/exit result. It finishes the exact
   persisted 64-bit audit sequence idempotently, then appends one deterministic bounded assistant message to the
   pending record's originating chat. If the original message count is already plus one and the
   last message is byte-identical, it treats attachment as complete rather than appending again.
   Any other history mismatch fails closed and preserves evidence without replay.
7. After attached/already-attached evidence, cleanup first makes execution impossible, then clears
   only the exact pending call and removes only the four fixed request/result/tmp paths. NVS writes
   are individually durable, not atomic across keys. For a Web-origin run CardMind must first
   persist existing `open_web=1`; only after that succeeds may it erase `run`. For Device it erases
   `run` directly. Only after successful run erasure does either path clear the exact pending
   record; only after pending clear succeeds may it remove request/result/temp files. If marker
   set, run erase or pending clear fails, all still-needed request/result evidence remains. A
   crash with both keys repeats attachment/cleanup idempotently; a crash after `run` erase retains
   `open_web` and cannot replay. Erase-before-set is forbidden. Device-origin cleanup never writes
   `open_web` and erases only `run`. A boot with no owned
   pending record never executes an orphan request; it removes only orphan fixed paths/state and
   reports the failure. Manual Python startup remains the unchanged fallback when no P5 run blob is
   present.
8. Device approval shows the exact path/size/hash and the existing full-source route, then a bounded
   running/return message before reset. Web approval sends one terminal `handoff` SSE event, closes
   normally, resets through the existing deferred restart flag and polls the same `/api/session`
   address for at most 60 seconds. CardMind reads `open_web` without erasing it, opens the existing
   Web Console, then acknowledges by erasing the marker only after the port-80 listener has begun.
   Crash before acknowledgement reopens Web on the next CardMind boot; acknowledgement failure
   leaves the marker for retry. This same late-ack path preserves manual-Python return behavior.
   The browser reloads the original chat and never reposts approval. Device-origin runs do not set
   that marker and return to the ordinary Device chat surface.

### Resource, latency and trust ceilings

| Boundary | Ceiling |
| --- | --- |
| Source | Existing normalized Shared path at most 512 bytes; exact viewer-renderable valid UTF-8 `.py` bytes 0..65,536; no uplift or `.mpy` |
| Concurrency | One fixed run state and one foreground top-level `exec`; no queue, worker, retry or replay |
| Request/result SD | Request at most 1,024 bytes; result at most 24,576 bytes; one temp-plus-rename write each; four fixed exact-owned paths |
| Output/chat | Stdout plus stderr at most 16,384 decoded UTF-8 bytes; deterministic attached message at most 18,432 bytes; explicit truncation |
| NVS/flash wear | One 91-byte blob. ESP-IDF 5.5.1 set/erase calls are individually durable and `nvs_commit` is a no-op. Device: four mutations (pending, claimed, complete, erase run). Web: six (the same four, set open_web before run erase, late erase open_web). One-shot dispatch precedes the existing mode_error mutation, so it is not executed or counted |
| Timeout/reboot | Fixed 30,000 ms WDT after durable `claimed` and CardMind preselection, before bounded validation and `exec`; exactly one boot into MicroPython and one return reset in the normal cycle |
| Browser/latency | Same-address polling at most 60 seconds; trivial real-device success target at most 30 seconds from approval to attached chat result |
| RAM/stack | No unbounded collector or recursive parser; source/output/request/result allocations remain within the caps above; real-device free heap, largest block and stack margin are measured against the unchanged P4 baseline |
| Secrets/trust | Request/state contain identity and hashes only. Output is user-authorized chat content. Approval warns that code is privileged, output is stored in chat, and no sandbox or adversarial timeout/boot guarantee exists |

Approved code may deliberately feed the watchdog, change the boot partition, spawn work or perform
non-idempotent device effects. Therefore P5 guarantees at-most-one CardMind launch/no replay and
exactly-once result attachment, not exactly-once external Python effects or containment of approved
privileged code.

### Crash and cleanup contract

| Durable point | Return behavior | Replay/cleanup |
| --- | --- | --- |
| Pending call is `ClaimedApprove`, before audit/request/run state | No runnable NVS state exists, so MicroPython cannot execute it | Same-boot validation/staging failure returns the existing bounded tool failure and continuation; after reboot CardMind attaches/already-detects `approval interrupted before runnable staging; no code executed`, projects any unmatched audit as interrupted and exact-cleans pending/orphan artifacts |
| Audit start, before `pending` NVS commit | No runnable handoff exists and boot selection is unchanged; the existing journal projects an unmatched running record as interrupted | Finish it in the same boot on ordinary staging failure; after crash remove only orphan exact request/temp and do not invent a recovered terminal sequence |
| `pending` committed, before MicroPython claim | CardMind reports interrupted before execution if it regains control | Never run from CardMind; attach once, then exact cleanup |
| `claimed`, during bounded validation or before/during `exec` | Normal reset, validation persistence failure, user reset or WDT returns to CardMind | Report effects unknown when no valid complete result; no replay |
| Temp write/close/first sync/readback failure | NVS remains `claimed`; final is absent | Ignore/delete exact temp after conservative attachment; no replay |
| Rename/second sync/final readback failure | NVS remains `claimed`; temp or final may exist | Ignore/delete both exact paths after conservative attachment; no replay |
| Final readback succeeds, before `complete` | NVS remains `claimed` even if valid final exists | Discard uncommitted result after conservative attachment; no replay |
| `complete`, before chat append | Result bytes must match NVS result SHA-256 and strict schema | Retry attachment only, never execution; mismatch attaches bounded failure and exact-cleans |
| Chat append, before cleanup | Exact last message and original count plus one prove attachment | Do not append again; resume exact cleanup |
| Web `open_web` set, before `run` erase | Both keys are durable; complete/claimed processing and attachment are idempotent | Never erase open_web; retry run cleanup, then continue to Web |
| Web `run` erased, before console start/late marker acknowledgement | No runnable state remains; open_web is durable | Open existing console; erase open_web only after listener begin; crash retries Web return |
| After run erase, pending clear fails; or pending clears before file removal | On pending-clear failure the exact pending plus request/result bytes remain for byte-identical already-attached proof. After pending clear, no chat attachment owner remains and only fixed orphan files may survive | Retry pending proof/clear before deleting files. If pending is absent, orphan cleanup deletes only fixed files and never attaches or replays |
| SD missing/replaced | NVS state forbids replay; no other card/request is trusted | Preserve chat/pending evidence until the owned card is available or report bounded failure; never erase/initialize a card |

### Frozen proportional proof

| Requirement | Smallest proof | Forbidden effects |
| --- | --- | --- |
| Catalog/policy/approval | Existing host policy/catalog/pending tests extended for exact schema, availability, mandatory Ask/Allow, linked valid-UTF-8 `.py` identity and stale/hash mismatch | Off/Deny/non-UTF-8/error creates no request, audit start, NVS state, partition change or executor call |
| Persisted request/result | Host checks for strict bounded codecs plus MicroPython helper tests for state/path/hash/output bounds and ordering | Extra fields, oversize, wrong id/hash/path or orphan SD record cannot reach `exec` |
| Runtime/recovery | Focused real-device success, uncaught exception, explicit reset and one WDT loop through the production startup branch | More than one top-level launch, retry after claimed, manual Web worker/server start during one-shot |
| Exactly-once chat/audit | Host crash-window checks for ClaimedApprove/no-run, full uint64 audit identity, result hash and every VFS/NVS boundary plus device reboot outcomes for complete and claimed state | Duplicate assistant result, duplicate terminal audit, changed-chat overwrite or replay |
| Device/Web UI | Existing Device pending/file viewer observation and Web static harness for source access, no Allow-for-chat, durable return surface, terminal handoff and same-address polling | Approval repost, lost Web return, Web opening after Device approval, second Python Web server, hidden full source or truncated preview treated as full source |
| Compatibility/resources | Existing manual `python_handoff_e2e.mjs`, one exact pinned build/upload, before/after heap/largest-block/stack/latency and exact-owned artifact check | 64 KiB uplift, package/automation UI, persistent worker, unrelated data deletion or material P4 regression |

No physical power-cycle is an acceptance action. The claimed-state explicit reset and WDT cases
exercise the same no-result/no-replay recovery boundary without entering the prohibited device
recovery-escalation chain.

### Frozen P5-02 write set

- Policy/dispatch/pending: `firmware/CardputerAssistant/src/tool_catalog.h`,
  `firmware/CardputerAssistant/src/tool_catalog.cpp`,
  `firmware/CardputerAssistant/src/api_client.cpp`,
  `firmware/CardputerAssistant/src/tool_router.h`,
  `firmware/CardputerAssistant/src/tool_router.cpp`,
  `firmware/CardputerAssistant/src/pending_tool_call.cpp`,
  `firmware/CardputerAssistant/src/pending_tool_preview.h` and
  `firmware/CardputerAssistant/src/pending_tool_preview.cpp`.
- Persistence/runtime/audit: `firmware/CardputerAssistant/src/python_mode.h`,
  `firmware/CardputerAssistant/src/python_mode.cpp`,
  `firmware/CardputerAssistant/src/tool_activity.h`,
  `firmware/CardputerAssistant/src/tool_activity.cpp` and
  `micropython/vfs/cardmind_supervisor.py`.
- Integration/UI call sites: `firmware/CardputerAssistant/CardputerAssistant.ino`,
  `firmware/CardputerAssistant/KeyboardNavigation.ino`,
  `firmware/CardputerAssistant/SerialDiagnostics.ino`,
  `firmware/CardputerAssistant/VoiceAndSpeech.ino`,
  `firmware/CardputerAssistant/src/web_console.cpp`,
  `firmware/CardputerAssistant/assets/web_console.html` and generated
  `firmware/CardputerAssistant/src/web_console_asset.h`.
- Retained proof/evidence: `tests/host_tests.cpp`,
  `tests/micropython_supervisor_test.py`, `tests/web_console_ui_test.mjs`,
  `tools/python_handoff_e2e.mjs` and this trace.

`AGENTS.md`, `ROADMAP.md`, `micropython/vfs/boot.py`, the manual Python HTTP/API implementation,
build scripts and all package/automation/USB/QOL surfaces are outside the expected P5-02 diff. Any
new required file or responsibility must be justified by a concrete reviewed blocker before edits.

`SerialDiagnostics.ino` is retained only because its existing request-plan call sites must pass the
new explicit Python availability input while preserving all existing selectors as Python-disabled.
P5 adds no serial selector, fixture, runner or diagnostic subsystem there.

### Adopted vendor semantics

- The exact pinned MicroPython 1.28 image in this checkout is SHA-256
  `931F47DE3076F51F386DB204C2CC710D0AC42480DED50B1298CFB860E1BC1BFA` and embeds ESP-IDF
  5.5.1. Its NVS `set`/`erase` operations persist individually; `nvs_commit` is a no-op and cannot
  provide a cross-key transaction. P5 therefore uses the explicit set-open_web-before-erase-run
  sequence above:
  https://github.com/espressif/esp-idf/blob/v5.5.1/components/nvs_flash/src/nvs_api.cpp#L519-L530
- In MicroPython 1.28, `os.sync()` issues `CTRL_SYNC` to every mounted FAT VFS and `os.rename()`
  dispatches only within one resolved VFS. P5 adds bounded readback/hash checks around both syncs
  before persisting complete:
  https://github.com/micropython/micropython/blob/v1.28.0/extmod/modos.c and
  https://github.com/micropython/micropython/blob/v1.28.0/extmod/vfs.c
- MicroPython 1.28 `machine.WDT` cannot be stopped or reconfigured after start; P5 constructs one
  instance only after durable claimed state and before validation/user `exec`:
  https://docs.micropython.org/en/v1.28.0/library/machine.WDT.html
- MicroPython `Partition.set_boot()` and ESP-IDF 5.5.1 `esp_ota_set_boot_partition()` persist the app
  selected for the next reset. P5 treats successful selection as mandatory before user code:
  https://docs.micropython.org/en/v1.28.0/library/esp32.html#esp32.Partition.set_boot and
  https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32/api-reference/system/ota.html

### Independent pre-edit review

- Fresh read-only reviewer `Boyle` (`01a05e27-000c-71e1-90a1-44e0552b1b1a`) returned `STOP` on
  three concrete gaps: non-UTF-8 bytes could be approved without full text review; the audit crash
  window lost the authoritative 64-bit sequence before NVS; and Web/Device return ownership was
  RAM-only. No sandbox/adversarial WDT blocker was found.
- The frozen corrections above make whole-file UTF-8 validity a pre-pending and pre-stage invariant,
  preserve the full 64-bit sequence while explicitly assigning the pre-NVS unmatched-running case
  to the existing interrupted journal projection, and bind one return-surface byte to request/NVS
  with exact transfer to the existing `open_web` marker. The write set and non-goals are unchanged.
- The same reviewer performed the one permitted correction check and returned `GO`: all three
  blockers are resolved without a new owner or expanded write set. P5-01 remains `in_progress`
  until Architect returns the required closure GO.
- Architect's personal closure review then returned `STOP` on five persistence details: the
  unsupported cross-key NVS atomicity claim and wrong ESP-IDF runtime, omitted
  `ClaimedApprove`-before-run recovery, inconsistent claimed/WDT/validation order, an incomplete
  result VFS durability contract, and the uncounted manual `mode_error` NVS mutation.
- The consolidated correction uses individually durable set-open_web-before-erase-run plus
  non-destructive marker read/late acknowledgement; gives ClaimedApprove/no-run to the existing
  continuation or startup exact-cleanup owner; fixes order to select CardMind, persist claimed,
  start WDT, validate, then exec; binds final result hash only after two sync/readback checks; and
  dispatches one-shot before the existing mode_error write. The frozen write set is unchanged and
  no new reviewer cycle or production owner is introduced.
- Architect personally rechecked the final clauses, owners, exact pinned vendor semantics, all
  pre-run/result/cleanup crash windows, ceilings, Web/Device return, proof and write-set minimality.
  The final pending-before-files cleanup order closed the last ambiguity. Architect returned
  explicit `GO`; P5-01 is completed with no production or test change.
