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
| P5-02 | Implement the complete one-shot vertical cycle through existing P3 policy/approval, Shared workspace, mode switch, supervisor, Device UI and WebUI/reconnect owners | Exact one-run approval and full-source review; fail-closed no-effect cases; success/exception/reset/watchdog/power-loss return; exactly-once originating-chat result; no replay; exact cleanup; manual workspace preserved | completed |
| P5-03 | Phase closure and publication | Focused host/build/COM8/browser acceptance, resource/latency/SD ownership, compatibility and documentation checks, exact cleanup, independent review, per-row publication evidence, green CI and reviewed merge only to `develop` | completed |

## P5-02 execution checkpoints

These checkpoints expose independently observable implementation and proof progress inside the
single coherent P5-02 vertical production row. A completed checkpoint is not an independently
completed feature, row publication, permission to omit P5-02f runtime, or permission to commit a
partial protocol. The top-level execution matrix remains authoritative for atomic row status.

| ID | Execution checkpoint | Evidence boundary | Status |
| --- | --- | --- | --- |
| P5-01 | Scope/inventory/design gate | Canonical scope, inherited-owner inventory, bounded design/proof/write set and independent pre-edit review | completed |
| P5-02a | Schema, policy, mandatory exact-byte approval and production-linked host proof | Catalog/plan/policy/pending path, size, SHA-256, full-source and stale/fail-closed host observations | completed |
| P5-02b | CardMind consume-once request/NVS/audit/recovery/attachment/cleanup implementation and code-review checkpoint | Persisted identity, effect ordering, originating-chat exactly-once attachment, no-replay and exact-cleanup code reviewed; runtime remains delegated to P5-02f | completed |
| P5-02c | MicroPython supervisor claim/WDT/bounded result/no-replay implementation and host proof | Claim-before-mount/exec, CardMind preselection, fixed WDT, bounded stdout/stderr/exit and host supervisor proof; installed-image runtime remains delegated to P5-02f | completed |
| P5-02d | Device approval/full-source/return wiring and static proof | Exact approval identity, full-source access and Device return wiring inspected statically; physical behavior remains delegated to P5-02f | completed |
| P5-02e | Web handoff/reconnect wiring, generated asset consistency and UI smoke | Terminal handoff SSE, same-address polling, generated asset consistency and UI smoke; browser behavior remains delegated to P5-02f | completed |
| P5-02f | Exact installed-image delivery plus bounded staged Web proofs for `complete` and `claimed`, no replay, originating-chat return, resources and exact cleanup | Architect accepted both installed runtime boundaries with the claimed marker limitation retained explicitly; no repeat, physical step, recovery chain or retained/broad harness | completed |
| P5-02g | Architect row closure, exact commit/push/remote verification | Reconciled row-owned diff/evidence, Architect closure GO, one exact row commit, immediate push and authenticated remote SHA | completed |
| P5-02h | Reopened cancellation correction at the existing router/staging boundary | Cancellation is latched before staging and at the last reversible pre-run-commit point; request/temp exact-cleaned; no runnable run state, boot change or handoff; activity/result use existing canceled semantics | completed |
| P5-02i | Reopened recovery ownership for ambiguous staging failures | Safely-cleaned failures retain ordinary handling; request/possible-run ambiguity preserves exact claimed pending, blocks continuation/clear/handoff/replay/acknowledgement and remains under startup recovery ownership | completed |
| P5-02j | Reopened proof of Python artifact presence/absence/access | Cleanup and startup classify each exact request/result path as present, proven absent or access failure; access ambiguity preserves exact claimed recovery ownership | completed |
| P5-03 | Phase reconciliation, CI/PR/review/merge to `develop` | Full phase audit, required regressions and documentation, green CI, reviewed PR and merge only to `develop` | completed |

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
   MicroPython partition and reset. A deterministic pre-run failure before `writeRunBlob()` starts
   finishes the audit as failed without a runnable NVS state. If `writeRunBlob()` itself reports
   failure, its set/commit/readback outcome is persistence-ambiguous: CardMind finishes the audit as
   failed but preserves the exact request, possible `run` and claimed pending for startup recovery.
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
| Pending call is `ClaimedApprove`, before request/run persistence | No runnable NVS state exists, so MicroPython cannot execute it | Deterministic pre-run failure returns the existing bounded tool failure; after reboot CardMind attaches/already-detects `approval interrupted before runnable staging; no code executed`, projects any unmatched audit as interrupted and exact-cleans pending/orphan artifacts |
| Request is durable and `writeRunBlob()` reports failure | Installed Preferences has already attempted set plus commit before separate readback, so durable `run` presence is ambiguous | Preserve request, possible `run`, failed audit and exact claimed pending without same-boot erase or cleanup; startup classifies run-present as pending/pre-execution recovery and run-absent as detached request-only recovery; generic acknowledge and replay are forbidden |
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
| Runtime/recovery | One Web-origin exit-0 run for durable `complete`, then one Web-origin WDT loop for durable `claimed`, both through the production startup branch | More than one top-level launch per approval, retry after claimed, manual Web worker/server start during one-shot |
| Exactly-once chat/audit | Host crash-window checks for ClaimedApprove/no-run, full uint64 audit identity, result hash and every VFS/NVS boundary plus device reboot outcomes for complete and claimed state | Duplicate assistant result, duplicate terminal audit, changed-chat overwrite or replay |
| Device/Web UI | Existing Device pending/file viewer ownership plus passed static Device/Web wiring and UI smoke for full-source access, no Allow-for-chat, terminal handoff and same-address polling | Physical key injection, approval repost, lost Web return, second Python Web server, hidden full source or truncated preview treated as full source |
| Compatibility/resources | Passed no-run supervisor proof and unchanged inherited manual workspace owner; one exact pinned build/upload; two-run pre/post heap/largest-block/stack/latency fields and exact-owned cleanup | Manual-workspace cycle in this proof, 64 KiB uplift, package/automation UI, persistent worker, unrelated data deletion or material P4 regression |

No physical power-cycle is an acceptance action. The fixed-WDT run is the sole installed
`claimed` observation. Explicit reset and post-claim power loss are allocated compositionally to
that same durable claimed/no-result/no-replay consumer; they are not additional Device cases and
do not enter the prohibited device recovery-escalation chain.

### P5-02f installed-state proof allocation

This is a proof refinement only, not a product-scope or acceptance reduction. Only two distinct
installed durable states require real-device observation:

1. One Web-origin exit-0 run. Existing authenticated `/api/file/upload` plus
   `/api/project/link` create one collision-checked exact-owned linked fixture; exact readback bytes
   and SHA-256 are required before the model turn. This fixture setup is not a substitute Files
   acceptance and adds no product interface. The only model request is Python-only: it asks to run
   that existing source and must stop at mandatory
   Allow-once. The pending oracle requires typed `python_source`/`mandatory` kind and reason, exact
   path/bytes/SHA-256, `truncated=false`, exact action flags, and a body containing the complete exact
   source plus the public privileged-code/no-sandbox, originating-chat and watchdog trust facts; it
   does not compare the complete body. The proof observes terminal HTTP handoff, same-address
   CardMind return, exactly one semantic originating-chat exit-0 result with unique stdout and stderr
   markers, one exact succeeded terminal Python activity, pending absent, no duplicate after reload,
   approval to attached result under 30 seconds, and the complete resource record below.
2. One Web-origin WDT run creates a new collision-checked exact-owned project/chat/source/marker
   fixture only through existing authenticated upload/link routes; accepted Files model behavior is
   not repeated. The source opens `/sd/assistant/files/<exact marker>` in append mode, writes one
   unique line, closes it, calls `os.sync()` and only then loops forever. One Python-only request then
   receives its mandatory one-run Allow-once under the same typed pending oracle. The proof observes
   JSON-SSE handoff, the fixed 30-second watchdog return, the marker line exactly once, exactly one
   originating-chat attachment containing semantic effects-unknown and no-replay facts, one exact
   failed terminal Python activity with no exit status, pending absent and no duplicate after two
   authenticated reloads.

Uncaught exception shares the observed complete-result persistence path. Explicit reset and
post-claim power loss are composed from the observed WDT claimed/no-replay durable consumer rather
than separately exercised. Pre-claim, detached-request and
already-attached crash windows remain host/static proof. Device pending/file UI and browser
transition remain the passed static UI plus inherited viewer ownership; no physical-user steps,
key injection, selector or second browser framework is added. Manual workspace is not cycled in
this proof; compatibility remains owned by the passed no-run supervisor proof and unchanged
inherited runtime.

Each disposable runtime proof owns one exact project/chat/source and any state-specific marker.
Before mutation it requires literal `pending=false` and unique fixture title/path collisions absent.
Under the standing
disposable-device decision it does not snapshot, digest, compare or restore unrelated project/file
inventories, prior selection, SSH state or prior policy values. It protects only same-address
Wi-Fi/HTTP operation and API-key configured flags, without credential values, and uses the already
authorized canonical policy defaults as the final coherent state.

One collision-checked host-temp ledger exists only for crash-resistant evidence. It may retain the
fixture and marker title/path/ID when known, exact source size/SHA-256, monotonic
`t0`/`t1`/`t2`/`t3`, typed
observations and pre-run API-key configured booleans. It never stores password, cookie, CSRF,
pending ID or handoff token. Every mutation attempt is marked before its POST; no model request or
approval is replayed.

Cleanup independently attempts to clear only the known run pending when safely identifiable,
remove the exact-owned fixture and marker, set canonical policies, verify pending absent, fixture
and marker absence,
unchanged API-key configured flags and authenticated same-address HTTP operation, then close the
Console. Every cleanup error is aggregated and a latency failure cannot bypass evidence collection
or cleanup. No unrelated inventory equality or original-selection restoration is required.

Immediately before each rebooting Python Allow-once POST, the runner moves the current auth value to
one request-local reference and sets its durable cleanup reference to null. A lost handoff frame is
therefore an uncertain approval outcome: cleanup performs one fresh login and never reposts approval.
The Complete chat oracle parses semantic stdout and stderr sections, requires the stdout marker once
only in stdout and the stderr marker once only in stderr, and does not compare the whole message.

`/api/activity` exposes no sequence and at most 16 newest records. For each installed run the oracle
therefore requires `after.length == min(before.length + 1, 16)`, validates every field of the exact
new first typed record, and deep-compares `after.slice(1)` with the visible
`before.slice(0, after.length - 1)`. It neither assumes nor requires an untruncated older catalog.

Each status sample requires numeric `free_heap`, `largest_heap`, `minimum_heap` and `stack_free`, SD
state `ready`, readable/writable true, and internally consistent total/used/free byte counts. The
proof records exact same-image pre/post values and deltas, plus deltas against the retained P4 idle
comparison `122752 / 60404 / 111200 / 7860`; Architect owns the materiality decision rather than a
positive-value-only pass.

The serial bootstrap owns one initial `PING`, one `CONSOLE`, LF parsing with optional CR and the
canonical 20-second Console-start bound. Production readiness must match the complete prefix
`WEB_CONSOLE result=ready address=http://` followed by one non-empty authority and `/`. After that
initial readiness the serial handle performs zero further writes and is intentionally closed before
the rebooting Python approval. Continuous SerialPort survival and post-handoff serial `ready` or
`stopped` are not requirements across the intentional `ESP.restart()` -> MicroPython ->
`machine.reset()` partition cycle. Authenticated same-address HTTP owns handoff receipt, returned
CardMind session/state, originating-chat result and final Console close. Initial PING/Console failure
still ends evidence without retry, reprobe, reset, upload or recovery.

### P5-02f local-only proof preparation evidence

- Architect authorized only local preparation and retained `STOP` on Device/Web execution. The
  combined 27,771-byte runner was subsequently rejected and permanently retired after concrete
  lost-response ownership, child-process completion-order and credential-error sanitization defects;
  no third repair cycle or execution is permitted.
- `tools/python_handoff_e2e.mjs` was restored without checkout/reset to P5-01 blob
  `8408e7aaa244ddfdee3d2f914efb72048dfeab04`; `git hash-object` matched and its path has zero diff.
- Retired `%TEMP%/cardmind_p5_two_state_probe.mjs` and
  `%TEMP%/cardmind_p5_console_holder.ps1` were exact-deleted and verified absent. The earlier standalone
  oracle remains absent. No P5 disposable source file currently exists.
- The replacement is a primary-agent command inventory, not a shared runner or state machine. A
  27-step pure manifest self-test passed with unique single-purpose IDs, no retry, exactly two durable
  observations, independent cleanup ordering and Console-close-before-holder-exit. Its project owner
  oracle accepts a sole exact title after an absent-before recorded create attempt when the response
  ID is unknown, requires an exact ID match when one was received, and rejects duplicate/mismatched
  candidates. Credential-load failure is reduced to `credential_load_failed` without its path.
- The exact inline holder text parsed as PowerShell, is 21 lines/1,042 UTF-8 bytes with SHA-256
  `6016BB78BED80FDB937D32A3B40065A94C4225F44309CB8B921FE2E036B8AFD9`, contains exactly two serial
  writes and accepts only the production ready-address shape. It was not saved or run.
  Retained direct-runner bytes are zero. The pure manifest also passed saturated activity,
  stdout/stderr separation and complete SD/resource typing without network, COM8, model or Device I/O.
  No installed-state acceptance is claimed.

### P5-02f direct command inventory

The primary records every mutation attempt in task evidence before invoking it. Every HTTP/model
command has one hard-coded purpose, performs a fresh in-memory login, catches credential read/parse
failure as the path-free `credential_load_failed`, never logs cookie/CSRF/password, clears references
on exit, and has no retry/rephrase branch. No session or secret is written to disk.

1. Read-only baseline: `safe_baseline` records pending absence, collision absence, operational HTTP
   and API-key configured booleans.
2. Exact setup: `python_policy_set`, `project_create`, `project_identity`, `source_upload_readback`
   and `source_link`.
3. Complete state: `complete_python_request`, `complete_python_approve`, `complete_observe`.
4. Independent final block: `known_pending_gate`, `fixture_remove`, `canonical_policy_set`,
   `absence_1`, `absence_2`, `safe_state_verify` and `console_close`.


One initial serial bootstrap is closed before approval and is never reopened. An uncertain
model/tool/approval response is never replayed; only fresh authenticated reads classify durable
outcome. A sole exact fixture title after collision-absence plus a recorded create attempt is owned
even if the response ID was lost; when an ID was received, title and ID must both match. Cleanup
commands remain independently attempted where their prerequisites are safely known even after
another cleanup command fails.

### P5-02f single direct lifecycle outcome

**Status:** failed evidence; P5-02 remains `in_progress`. Architect authorized exactly one execution
and prohibits a repeat, repair, reconnect, reset, rebuild/upload or recovery run.

- The exact inline holder reached `HOLDER ready` after its sole `PING` and `CONSOLE`. The read-only
  baseline then observed literal pending false, one project and 11 files, exact fixture collisions
  absent, original project/chat selection retained, SD ready/readable/writable, and resources
  `98140` free heap, `32756` largest block, `10032` minimum heap, `2584` stack free,
  `15910567936` total SD bytes, `43261952` used and `15867305984` free.
- Master `fw/py` and fixture-chat `fw/py` ceilings were changed to Allow with all other policy fields
  preserved. One exact project/chat was created and its original chat policy/model/SSH ceiling was
  captured. No project-link repair or manual link ran.
- The first Complete Files-only model request returned an SSE stream for which the direct command's
  combined `error-or-pending` assertion was true. It did not retain which of those two event types
  caused the assertion. No retry/rephrase followed. The cleanup pending gate subsequently observed
  literal false and performed no deny or acknowledge. No Python request, approval, handoff, reboot,
  Complete result or Claimed run occurred.
- Owner classification: this is failed disposable proof/oracle evidence, not a demonstrated P5
  production defect. The command collapsed two distinct terminal classifications and therefore
  cannot assign the response to product, provider or policy behavior. No production or acceptance
  oracle is changed from this invalid observation.
- Safe cleanup restored and verified the fixture chat policy/model/SSH ceiling and exact original
  master policy. It deleted the sole exact title plus recorded project ID after pending false, proved
  the exact source absent after conditional cleanup, proved the never-owned marker absent without
  deleting it, and restored the original project and chat selections.
- During cleanup the frozen 480-second holder deadline expired. Its direct process ended with a
  serial `ReadLine` timeout before Console close, so `HOLDER stopped` is unobservable and the lifecycle
  necessarily fails. No serial action followed. This timeout is proof-lifecycle ownership, not a
  firmware readiness or Python-runtime claim.
- The independently healthy HTTP finalizer completed. Two separate authenticated read-only passes
  returned the original project digest
  `9a9537f5f4958ab5bd35db2ba7fb34cab66f6a8f8ecd1f1493f79b31dabf89fa`, original file digest
  `bc3ffcaeababc2748feb640bbc7f653f7435ea30eab4ca105457dd4e6fbde5a1`, counts 1/11, pending false,
  exact fixture project/source/marker absent, original project/chat selection and master/new-chat
  policies restored. Pass 1 resources were `95968 / 31732 / 10032 / 2584`; pass 2 was
  `95984 / 31732 / 10032 / 2584`. SD total/used/free remained exactly unchanged. Authenticated
  `/api/console/close` returned `ok=true` after the second pass.
- Cleanup verdict is pass for every exact-owned persistent effect and user-state restoration. The
  acceptance lifecycle verdict remains fail because neither required durable state ran and bounded
  holder stop/exit evidence is absent. No credential, session value, user inventory name/content or
  credential-file path was output.

Architect granted execution `GO` for exactly one direct lifecycle with no repeat. `source_link` is
read-only verification that production model `write_file` auto-linked the exact source; it never
calls `/api/project/link` or repairs absence. The pure manifest self-test is preparation only and is
not acceptance evidence.

`pending_gate` owns conditional exact cleanup. Literal `pending=false` permits fixture deletion. An
unexecuted pending may be denied once only when its known ID, target/source, active fixture project
and chat all prove this lifecycle owns it. A returned Claimed/Denied interrupted record may be
acknowledged once only after its terminal/no-replay observation and exact known ID. Pending is then
re-read and must be literal false before deletion. Foreign, unknown, mismatched or ambiguous pending
is preserved and cleanup fails; approval is never replayed.

At the first holder/Device readiness loss the primary proof stops, with only independently healthy
already-available HTTP cleanup eligible. No manual link, extra scenario, retry/rephrase, reset,
reconnect, reupload or recovery is permitted. Final acceptance requires two identical project/file
inventories equal to baseline, original project/chat selection and master/fixture policies restored,
literal pending false, exact project/source/marker absent, path-free/secret-free output, recorded
resource/latency fields, authenticated Console close, `HOLDER stopped` and direct holder process exit.

### P5-02f reviewed material pivot

- Required tool intent selects a capability group but does not filter unrelated schemas already
  resolved as Allow. The failed lifecycle enabled `py=Allow` during its Required Files source step,
  so `python_run` was exposed and source-step isolation was invalid before any Python execution.
  This is proof-isolation ownership and requires no production or acceptance-oracle change.
- The prior no-repeat decision applies to that unchanged failed lifecycle and its retired combined
  terminal oracle. It does not prohibit the materially different reviewed policy-isolated proof.
- The 60-minute material pivot is one direct five-stage lifecycle: master `fw/py=Allow`; fixture
  `fw=Allow,py=Off` for the source step; fixture `fw=Off,py=Allow` for each run step; distinct typed
  SSE terminals; direct exact-owned source overwrite for Claimed; and the already-proven cleanup.
  Its reviewed inline holder is 2,711 LF UTF-8 bytes without a terminal newline, SHA-256
  `326E98A1FC869E14C583304CB7B447CDC83AF2D9652D4DD40EC817ED85C3F9CF`, with one exact `PONG`,
  one Console start and fixed 900-second total deadline. Stage bounds are 90/90/180/180/180 seconds.
- The prior 480-second deadline expired before Console close while no expected serial response was
  outstanding. No serial I/O error was used as evidence; the independently healthy HTTP interface
  then completed two baseline-identical reads and authenticated close. This was proof-lifecycle
  expiry, not Device readiness or transport loss, so one ordinary corrected COM8/Web lifecycle is
  eligible without reset, reconnect, probe or recovery.
- Architect returned execution `GO` for exactly this one corrected lifecycle, not P5-02 closure GO.
  P5-02f remains the sole `in_progress` checkpoint until observed evidence is classified.

### P5-02f disposable login-command classification

- The corrected holder itself received exact `PONG` and opened the Web Console, but both Stage 1
  and its safe-cleanup command stopped on a non-303 login response before any baseline or feature
  mutation. Their retained source read `web_ui.password` without validation, while the canonical
  credential schema and `tools/python_handoff_e2e.mjs` require a validated non-empty
  `web_ui.installation_password`. The nonexistent value was serialized by `URLSearchParams` as
  `undefined`. This exactly owns both `login_http` results; production authentication, sandbox
  routing, host networking and transport are not implicated.
- Command ordering proves no fixture, policy, file, pending call, run state, partition or approval
  mutation occurred. The holder later exhausted its unchanged deadline because authenticated close
  was unreachable through the defective disposable login command; this adds no Device-readiness or
  production evidence.
- Architect authorized exactly one serial-independent HTTP-only correction against the already
  observed running Web Console address. It reuses the reviewed five stages and exact policy/SSE/
  hash/activity/cleanup oracles, replaces only the disposable credential-field read with the
  canonical validated installation-password contract, omits holder stop/exit evidence, and accepts
  authenticated `/api/console/close` `ok=true` as final lifecycle evidence. This is execution `GO`,
  not P5-02 closure `GO`; no second HTTP lifecycle is permitted.

### P5-02f HTTP-only lifecycle outcome

- The canonical validated installation-password login succeeded and Stage 1 reached authenticated
  `/api/pending`, but its disposable oracle read nonexistent `pending` instead of production's
  boolean `present`. Because `undefined !== false`, it reported `foreign_pending` without evidence
  of any pending call and stopped before baseline capture or any policy, project, chat, file, model,
  approval, run-state or boot mutation. Stages 2-4 did not run.
- The safe-cleanup command repeated the same field error by requiring nonexistent `pending` to be a
  boolean, then stopped as `cleanup_snapshot_schema` before either inventory or authenticated
  Console close. This is invalid disposable-oracle evidence, not a real foreign pending or
  production behavior evidence.
- No cleanup mutation, retry, replay, COM8 action, reset, recovery, build, upload or second HTTP
  lifecycle occurred. Acceptance and cleanup both remain failed because the runtime states did not
  run and two inventories plus authenticated close were not obtained. Command ordering proves no
  P5-owned fixture, policy, file, pending or run mutation occurred. P5-02f remains the sole
  `in_progress` checkpoint pending Architect ownership and proof direction.
- Architect authorized one serial-independent authenticated read-only classification using only
  the typed `present` field. One canonical login and one `/api/pending` GET observed exact
  `present=false`; no id, tool, preview, action or content was inspected or exposed, and Console was
  intentionally left running. There is no external pending blocker.

### P5-02f Files evidence classification and Complete fixture pivot

- A later direct Complete attempt reached one exact holder `PONG` and initial Console `ready`, then
  stopped before the Python request because its Files-only model turn did not satisfy the disposable
  `write_file_inventory` oracle. No Python pending approval was submitted, no manifest or boot
  selection was staged, no reset/handoff occurred and no Python bytes executed. The disposable path
  completed its two cleanup passes; a separate bounded audit then observed typed `present=false`,
  authenticated Console close and exact holder `stopped` without a model call or reboot.
- Architect classified that failure to the disposable evidence path because it discarded the model
  terminal classification before checking inventory. It does not demonstrate a production, API,
  policy, cleanup or external-state defect. The Files-only turn and any SSE/inventory variant are
  permanently retired and must not be repeated.
- The Files boundary is accepted compositionally from already observed facts: exact persisted source
  bytes, production's automatic project link, and the exact new typed `write_file` `succeeded`
  activity. Its `output_bytes=55` is the model-visible serialized tool-result length, not the
  62-byte file length.
- Architect approved one materially different Complete proof. A fresh normal one-PING/Console
  lifecycle may use only the existing authenticated upload/link routes for collision-checked
  exact-owned fixture setup, verify exact readback bytes/SHA-256/link, then issue one Python-only
  model request. It must enforce the frozen mandatory pending, one approval, handoff, originating-chat
  result, activity, resources, no-replay and exact-cleanup contract. No Files model call,
  retry/rephrase, build, VFS/app upload, explicit reset, readiness reprobe or recovery is permitted.
  Any failure freezes the sequence after safe exact-owned cleanup through the still-healthy lifecycle.

### P5-02f staged Complete outcome

- The approved staged lifecycle observed exact holder `PONG` and initial Console `ready`, then
  deterministic fixture setup passed: one collision-checked project, a 62-byte upload, exact
  SHA-256/readback, one exact project link and the expected workspace inventory delta. The sole
  Python-only model turn produced the exact mandatory `python_source` pending with the approved
  path/size/SHA-256/full source, `truncated=false`, Allow-once and deny enabled, Allow-chat and
  acknowledge disabled, and the privileged/no-sandbox trust facts. Exactly one approval was posted.
- After approval the holder exited during the intentional partition-reset cycle. Architect assigned
  this to the disposable proof lifecycle: continuous SerialPort survival was an invalid expectation,
  while the exact originating-chat result and a fresh authenticated same-address Console close prove
  CardMind startup, result attachment, Web reopening and HTTP service return. No post-handoff serial
  reopen, readiness probe, explicit reset, upload or recovery occurred.
- The independently available HTTP path subsequently found exactly one expected exit-0 stdout/stderr
  result in the originating fixture chat, then failed the frozen approval-to-result `<30000 ms`
  ceiling. The disposable path retained only `result_latency`, not the exact elapsed value, so the
  observation is limited to at least 30 seconds and cannot distinguish product latency from terminal
  approval-transport measurement overhead. Activity, post-resource and reload/no-replay checks did
  not run and are not claimed.
- The error path completed two exact-owned cleanup/restoration passes; a separate authenticated HTTP
  close succeeded, and the 22,490-byte disposable runner was removed. No Files model call,
  retry/rephrase, build, VFS/app upload, explicit reset or second lifecycle occurred. P5-02f remains
  `in_progress`; the exact latency and remaining Complete observations were not retained.

### P5-02f narrow Complete measurement allocation

- The `<30000 ms` approval-to-originating-chat target remains frozen and is not waived. The prior
  predicate-only failure discarded the exact value, so it is measurement/evidence ownership rather
  than an established firmware regression.
- Architect approved one materially different narrow Complete measurement lifecycle. After one
  successful initial PING/Console bootstrap, serial is intentionally closed before approval and is
  never reopened. The disposable sequence retains monotonic `t0` at approval POST start, `t1` at
  handoff SSE receipt, `t2` at the first authenticated returned CardMind session/state and `t3` when
  the exact originating-chat result is visible, plus every adjacent and total delta.
- The sequence must not short-circuit on latency. It first retains exact activity, post-resource,
  pending-absent, reload/no-replay, exact fixture absence, unchanged API-key configured flags,
  operational same-address HTTP and two-pass cleanup evidence, then evaluates all assertions
  together. If total latency remains at least 30 seconds, closure stops
  with the exact phase values for ownership. No Files model turn, retry/rephrase, production edit,
  build, VFS/app upload, explicit reset, post-approval serial action or recovery is permitted.

### P5-02f narrow Complete measurement outcome

- The single serial bootstrap observed exact `PONG` and initial Console `ready`, then intentionally
  closed COM8 before approval. Deterministic setup again proved one collision-checked 62-byte
  upload, exact source SHA-256/readback/link and a Python-only mandatory pending with full source,
  exact identity, one-run privileged/no-sandbox facts and only the allowed action flags.
- The disposable approval reader received HTTP 2xx and stream completion but stopped on
  `approve_no_handoff` because it searched for a nonexistent SSE `event: handoff` line. Read-only
  source classification proves production emits `data:{"type":"handoff",...}\n\n`; the parser,
  not production approval/handoff, owned the mismatch. The process discarded its monotonic values
  and original policy/selection snapshot before cleanup, so no t0/t1/t2/t3 or latency claim survives.
- No approval was replayed and no post-approval serial action occurred. A same-address authenticated
  read-only classification observed `pending=false`, the exact 62-byte fixture source/hash, exactly
  one originating-chat exit-0 result, and newest activity
  `python_run/python/succeeded/output_bytes=17/exit_status=0`. Returned status was SD
  ready/readable/writable with free heap 98,420 bytes, largest block 34,804 bytes, minimum heap
  90,848 bytes and stack margin 5,272 bytes.
- Exact fixture cleanup and selection restoration subsequently passed twice: pending was absent,
  the exact project/source were absent, inventory returned to one project and 11 files, the retained
  project digest matched, and the sole project's prior active chat was restored. The disposable
  process had irrecoverably lost the original global policy encodings.
- The user explicitly authorized canonical defaults as the coherent recovery state after that
  harness-owned baseline loss; this is not evidence or a claim that the defaults equaled the prior
  values. Authenticated `/api/settings` restored master policy
  `v1;ws=a;wf=a;fr=a;fw=a;sr=a;sm=a;sf=a;py=o` and new-chat policy
  `v1;ws=a;wf=a;fr=q;fw=q;sr=o;sm=o;sf=o;py=o`; two independent canonical readbacks matched both
  exact strings.
- Every non-policy field exposed by the settings endpoint matched the pre-recovery snapshot. Wi-Fi
  remained reachable through the same HTTP session and all API-key configured-state flags were
  unchanged; no credential value was read or exposed. Authenticated `/api/console/close` returned
  success. Recovery used no serial, build, upload, browser or Python-runtime action.

### P5-02f materially different Complete proof outcome

- One initial COM8 bootstrap emitted exactly one `PING` and one `CONSOLE`, observed exact PONG
  and canonical ready-address output inside the 20-second bound, then closed serial before any
  approval. No post-approval serial action occurred.
- Preflight observed literal pending absent and no collision for the unique fixture title/path.
  Authenticated direct upload/link produced an exact 62-byte source with SHA-256
  `aa63f0f1f2a32978f4e6977d625699a3adf743ba4eb61fd9d7ee70f40d547c85`; full readback bytes/hash/link matched. The sole Python-only model request reached one
  typed `python_source`/`mandatory` pending with exact path/size/SHA, `truncated=false`, complete
  source, privileged/no-sandbox/originating-chat/watchdog facts and only one-run Allow enabled.
- The single Allow-once response was parsed from production `data:` JSON by
  `type == "handoff"`. Durable monotonic observations were
  approval-to-handoff 3,007.213 ms, handoff-to-first-authenticated-CardMind-state 16,923.655 ms,
  returned-state-to-originating-result 77.971 ms and approval-to-result 20,008.839 ms, passing the
  frozen `<30000 ms` target.
- The originating chat contained exactly one exit-0 result with stdout `Oa47c1e\n` and stderr
  `E47c1e6b\n` in their respective sections. Two authenticated reloads retained exactly one
  result. Pending was absent and the exact newest terminal activity was
  `python_run/python/succeeded/output_bytes=17/exit_status=0` with unchanged visible predecessor
  ordering.
- Same-image pre/post resource observations were free heap 98,348/99,408 bytes, largest block
  34,804/36,852 bytes, minimum heap 84,564/90,800 bytes and stack margin 5,272/5,272 bytes. SD stayed
  ready/readable/writable and internally consistent; used bytes changed from 43,261,952 to
  43,343,872.
- Independent cleanup passed for known-pending absence, exact fixture project and source deletion,
  canonical policy restoration, two postcondition passes, unchanged API-key configured flags,
  operational same-address Wi-Fi/HTTP and authenticated Console close. The exact host-temp runner
  and crash-resistant ledger were then deleted and verified absent.
- This run used no Files model turn, broad harness, production edit, build/upload, browser, explicit
  reset, readiness reprobe or recovery chain. It closed the installed `complete` observation.
- Architect explicitly accepted this Complete installed-runtime boundary on 2026-09-02. It is proven,
  retained and must never be repeated.

### P5-02f claimed/fixed-WDT proof freeze

**Execution status:** Architect GO was executed exactly once on 2026-09-02. Architect accepted the
bounded claimed boundary with the marker limitation below retained explicitly; no retry is allowed.

- Use a new exact-purpose host-temp runner and crash-resistant safe ledger; do not restore or reuse
  the deleted Complete runner or any retired broad harness. One initial `PING`/`CONSOLE` bootstrap
  closes serial before approval, with no post-approval serial action. Before touching COM8, perform
  one local-only syntax/oracle inspection of that exact runner covering a production JSON-SSE sample,
  marker source/expected bytes, timing arithmetic, single approval/no-replay and failure cleanup
  ordering; this is validation, not another harness framework.
- Preflight requires literal pending absent and unique exact fixture project/source/marker collisions
  absent. Existing authenticated upload/link routes create the project/chat and reviewed source. The
  source appends one unique line to its exact `/sd/assistant/files/<marker>` path, closes the file,
  calls `os.sync()` and then loops forever. No Files model request or file-replacement pending is used.
- The sole model request is Python-only and must produce one mandatory typed `python_source` pending
  with exact path/size/SHA-256, full source, public trust facts and only one-run Allow. One approval
  must end in production JSON-SSE `type == "handoff"`; it is never replayed.
- Durably record monotonic `t0` at approval POST start, `t1` at handoff, `t2` at the first returned
  authenticated CardMind state after the fixed WDT and `t3` when the exact originating-chat recovery
  attachment is visible. Retain and report all raw values and deltas before evaluation. Using the
  accepted Complete `t1` -> `t2` value 16,923.655 ms as the same-image/device/network baseline,
  require `claimed(t1 -> t2) - 16923.655` to be within `[25000, 40000]` ms. The configured exact
  30,000 ms WDT remains production/host evidence; this differential is installed-runtime
  corroboration. Timing failure cannot bypass other evidence or cleanup.
- Required returned observations are marker content equal to the unique line exactly once; exactly
  one originating-chat attachment semantically stating effects unknown and no replay; newest activity
  `python_run/python/failed/duration_ms=0/output_bytes=0/exit_status=null`; pending absent; and two
  authenticated reloads with exactly one attachment and unchanged marker content. Returned status
  must report watchdog-class `reset_reason` numeric 5, 6 or 7 (`ESP_RST_INT_WDT`,
  `ESP_RST_TASK_WDT` or `ESP_RST_WDT`); every other reset reason fails claimed ownership.
- The ledger stores only safe fixture/marker identity, source size/hash, `t0`/`t1`/`t2`/`t3`, typed
  observations and pre-run API-key configured booleans. It never stores credentials, cookie, CSRF,
  pending ID, handoff value or raw Wi-Fi SSID. Wi-Fi comparison remains in memory and retained
  evidence reports only `wifi_operational/unchanged=true` plus same-address HTTP success. Final
  cleanup independently handles only safely identified run pending,
  exact fixture/source/marker deletion, canonical policies, pending/absence checks, unchanged API-key
  flags, operational same-address Wi-Fi/HTTP and authenticated Console close.
- Forbidden: production/build/upload/browser changes, explicit reset, readiness reprobe or recovery,
  a Files model call, broad harness, model/approval replay, post-approval serial I/O, early timing
  failure, unrelated inventory/selection/SSH preservation, or more than this single GO execution.

### P5-02f single claimed/fixed-WDT outcome

- Local-only inspection passed before COM8 for the exact 138-byte marker source, 10-byte expected
  marker, production JSON-SSE parser, differential arithmetic, single approval, two no-replay reads
  and fixed cleanup order. One subsequent bootstrap emitted exactly one `PING` and one `CONSOLE`,
  reached canonical ready and closed serial before approval; serial was never reopened.
- Collision preflight, exact source upload/hash/readback/link, one Python-only model request, mandatory
  full-source pending and one Allow-once all passed. Raw monotonic nanoseconds were
  `t0=152435392980100`, `t1=152438399391500`, `t2=152484829382300` and
  `t3=152484910215900`. Deltas were approval-to-handoff 3,006.411 ms,
  handoff-to-CardMind 46,429.991 ms, CardMind-to-result 80.834 ms and approval-to-result
  49,517.236 ms. The differential against accepted Complete was 29,506.336 ms, inside the frozen
  `[25000, 40000]` ms window.
- Returned `reset_reason=6` (`ESP_RST_TASK_WDT`) passed watchdog ownership. Exactly one originating
  effects-unknown/no-replay attachment, pending absence and newest activity
  `python_run/python/failed/duration_ms=0/output_bytes=0/exit_status=null` passed.
- The marker and combined no-replay oracle did not pass: both attempted to read the `.marker` through
  `/api/file`, which returned HTTP 400 before content observation. Static source classification at
  `firmware/CardputerAssistant/src/web_console.cpp:4716` proves `handleFileRead` rejects a filename
  unless `isWorkspaceTextFile` succeeds, before calling the SD reader. This is a disposable
  observation-endpoint defect, not a production WDT/reset failure. Marker content/count and two
  complete combined no-replay observations therefore remain unproven and no compositional claim is
  made from the other successful signals.
- Resources before/after were free heap 98,596/99,448 bytes, largest block 32,756/34,804 bytes,
  minimum heap 87,508/90,888 bytes and stack margin 5,272/5,256 bytes. SD remained
  ready/readable/writable and internally consistent; used bytes changed from 43,261,952 to
  43,352,064.
- All independent cleanup steps passed: known pending, exact project/source/marker, canonical policy,
  two postconditions, unchanged API-key configured flags, operational unchanged Wi-Fi/HTTP and
  authenticated Console close. The exact runner and ledger were deleted and verified absent. No
  Files model turn, Complete repeat, production/build/upload/browser action, explicit reset,
  recovery chain, post-approval serial action or retry occurred.
- Architect accepted the claimed installed boundary from watchdog `reset_reason=6`, the
  29,506.336 ms differential, claimed-specific effects-unknown/no-replay attachment, exact failed
  null-exit activity, pending absence, one attachment plus one successful reload, retained
  claimed-before-WDT-before-exec host/static ordering and the accepted same-image Complete
  exactly-once attachment boundary. Marker contents/count remain unobserved and no side-effect claim
  is made; the omitted second identical reload adds no independent state transition.
- P5-02f is `completed` and must not be repeated. P5-02g is the sole `in_progress` checkpoint for
  mandatory Architect row closure review before any staging, commit or push.

### P5-02g first closure STOP and bounded correction

- Architect's first personal P5-02 closure review returned `STOP` on 2026-09-02 and supersedes the
  earlier incremental code-review `GO` for closure purposes. Two row-owned defects remained: two
  unused static startup strings retained 64 bytes of global RAM, and a successfully read durable
  `complete` result with invalid hash/schema/identity/base64/UTF-8/output bounds stopped before the
  frozen generic-failure audit/chat/cleanup path.
- The single bounded production correction removes only those unused strings from the existing
  startup consumer. After `decodeCompleteRecovery` successfully reads the bounded result file, all
  subsequent validation failures now return one deterministic bounded effects-unknown/no-replay
  message with failed execution, no exit status and zero accepted output bytes. The existing startup
  path therefore finishes the exact persisted activity as failed, appends or detects that one
  originating-chat message, erases the run state, clears the exact pending call and removes only the
  fixed P5 artifacts. Failure to read the result remains `success=false`, preserves all evidence and
  performs no audit, chat or cleanup effect; missing/unreadable SD state is not reclassified as
  corrupt. No route, persisted format, module, framework, fixture or public interface was added.
- No mock-only host seam was added. The existing host binary does not link `python_mode.cpp`, while
  this boundary depends on Arduino SD, Preferences/NVS and ArduinoJson; exposing an internal helper
  or building a fake storage stack would test a substitute and add more architecture than the fix.
  The proportional proof is the exact production control-flow diff, strict firmware compilation and
  the existing production-linked consumer/runtime evidence. The already accepted installed
  `complete` and `claimed` scenarios are frozen and were not repeated.
- Corrected cheap evidence observed `host_tests: PASS`,
  `MICROPYTHON_SUPERVISOR_TEST result=pass`, `WEB_CONSOLE_UI_TEST result=pass` and focused
  custom-work-tree `diff --check` pass. The one exact pinned compile passed with M5Stack core
  `3.2.1` and FQBN
  `m5stack:esp32:m5stack_cardputer:FlashSize=8M,PartitionScheme=custom`: flash is 3,478,606 bytes
  (20%) and global RAM is 65,732 bytes (20%), leaving 261,948 bytes. Relative to the prior accepted
  corrected build, flash decreased by 84 bytes and global RAM by exactly 64 bytes. Parsed
  `build.options.json` contained one exact FQBN and one unique resolved core directory at `3.2.1`,
  with no other M5Stack core.
- No upload, COM8, HTTP, Browser, reset, recovery, Device/Web runtime, production fixture or secret
  action followed the correction. P5-02g remains the sole `in_progress` checkpoint pending the
  required Architect personal re-review; staging, commit and push remain forbidden until explicit
  closure `GO`.

### P5-02g second closure STOP and NVS ownership correction

- Architect's correction re-review returned a second `STOP` on 2026-09-02 and supersedes the first
  correction package for closure. Inspection of the installed M5Stack ESP32 `3.2.1` Preferences
  source established that `putBytes()` performs `nvs_set_blob` and `nvs_commit` before it returns;
  `writeRunBlob()` can therefore report a later readback failure even though `cardmind_py/run` is
  already durable. The existing write-failure branch removed request artifacts without first
  erasing that possible runnable state, and the generic Device/Web acknowledgement paths could then
  clear its sole pending/chat owner.
- The bounded correction makes `discardPythonRunState()` authoritative: after every erase attempt it
  independently reopens and reads the run state. Verified absence succeeds even if the erase call's
  return was uncertain; read failure or a still-present state fails and preserves ownership. A
  post-`writeRunBlob` failure now invokes that authoritative discard before SD cleanup and returns
  immediately with the claimed pending and request evidence intact when absence is not proven. The
  existing partition-selection failure already uses discard-before-SD-cleanup and inherits the same
  stronger absence proof.
- The two existing generic acknowledgement owners now classify the pending call through the exact
  catalog. Only for `PythonRun`, Device and Web call the existing `loadPythonRunRecovery()` boundary
  before clearing; a failed authoritative read or any present run state refuses acknowledgement.
  Verified absence permits the unchanged generic clear. Foreign pending calls and normal
  acknowledgement behavior are unchanged, and no replacement/status/route/recovery framework was
  added.
- No fake SD/NVS failure harness was added. The relevant commit-before-readback behavior belongs to
  the installed vendor Preferences implementation and `python_mode.cpp` is absent from the host-test
  link; a mock persistence stack would not prove that hardware ordering. The proportional proof is
  the inspected installed vendor call order, the exact production ordering, the existing
  production-linked recovery consumers, cheap regressions and strict pinned compilation.
- Corrected evidence observed `host_tests: PASS`,
  `MICROPYTHON_SUPERVISOR_TEST result=pass`, `WEB_CONSOLE_UI_TEST result=pass` and focused
  custom-work-tree `diff --check` pass. One exact post-correction compile passed with M5Stack core
  `3.2.1` and FQBN
  `m5stack:esp32:m5stack_cardputer:FlashSize=8M,PartitionScheme=custom`: flash is 3,480,082 bytes
  (20%) and global RAM remains 65,732 bytes (20%), leaving 261,948 bytes. Parsed
  `build.options.json` again contained one exact FQBN and one unique resolved `3.2.1` core directory,
  with no other M5Stack version.
- No upload, COM8, HTTP, Browser, reset, Device/Web runtime, model call, fixture, secret or recovery
  action occurred. Both accepted installed scenarios remain frozen. P5-02g remains the sole
  `in_progress` checkpoint pending Architect personal re-review; staging, commit and push remain
  forbidden until explicit closure `GO`.

### P5-02g final NVS durability refinement

- Architect's second correction review returned a third durability `STOP` and then proactively
  narrowed the safe design before finalization. Installed ESP-IDF `nvs.h` guarantees nonvolatile
  set/erase only after successful `nvs_commit()`; closing a handle or observing a fresh same-boot
  read does not upgrade a failed commit into durable absence. This supersedes the preceding
  attempted post-`writeRunBlob` discard path.
- `discardPythonRunState()` now succeeds only when `removeKeyIfPresent()` itself succeeds, a fresh
  authoritative read succeeds and that read reports absent. A failed erase/commit remains failure
  even when the fresh current-boot view is absent. This strong discard remains only where verified
  run presence preceded cleanup, including partition-selection failure and startup orphan cleanup.
- When `writeRunBlob()` itself reports failure, `stagePythonRun()` now returns immediately before
  partition selection and preserves the fixed request, any uncertain durable `run`, exact
  `ClaimedApprove` pending and failed audit. Startup is the sole next-boot classifier: run-present
  enters the existing pending/pre-execution recovery, while run-absent plus the exact request enters
  detached request-only recovery. Neither branch executes or replays the script, and no tombstone or
  additional persistence state was added.
- Generic acknowledgement is never offered or accepted for an exact `PythonRun` in
  `ClaimedApprove`. Device action production returns only `Back`; Web `/api/pending` returns
  `can_acknowledge=false` with a recovery-pending message; both direct handlers independently
  classify the catalog entry and fail closed. Python `Awaiting`/`Denied` and every non-Python pending
  retain their existing acknowledgement behavior. No Web asset, route, status, marker, helper or
  framework changed.
- No fake NVS/SD harness was added: the failure is specifically the installed vendor commit
  durability contract and `python_mode.cpp` is not linked by the host binary. After the discard and
  no-ack changes, cheap evidence again observed `host_tests: PASS`,
  `MICROPYTHON_SUPERVISOR_TEST result=pass`, `WEB_CONSOLE_UI_TEST result=pass` and focused
  `diff --check` pass. The final refinement then changed only the unlinked staging failure branch;
  its affected `diff --check` passed and one final exact pinned compile passed.
- The final build uses the exact FQBN
  `m5stack:esp32:m5stack_cardputer:FlashSize=8M,PartitionScheme=custom` and one unique M5Stack core
  directory at `3.2.1`, with no other core. Flash is 3,479,726 bytes (20%); global RAM remains
  65,732 bytes (20%), leaving 261,948 bytes.
- No upload, COM8, HTTP, Browser, reset, Device/Web runtime, model call, fixture, secret or recovery
  action occurred. Both installed runtime outcomes remain frozen. P5-02g remains the sole
  `in_progress` checkpoint pending Architect personal re-review; staging, commit and push remain
  forbidden until explicit closure `GO`.

### P5-02 Architect closure GO

- On 2026-09-02 Architect personally reviewed the final actual 24-path row diff, complete
  producer/consumer inventory, installed ESP-IDF/Preferences durability semantics, Device/Web
  capability and direct-handler ordering, retained runtime evidence, cleanup, resources and
  residuals, then returned explicit P5-02 closure `GO`. All prior corrections are accepted and no
  further production change is authorized under that verdict.
- Architect independently observed `host_tests: PASS`,
  `MICROPYTHON_SUPERVISOR_TEST result=pass`, `WEB_CONSOLE_UI_TEST result=pass`, clean
  `git diff --check`, an empty index and a final build artifact newer than every corrected source.
  The final app binary is 3,479,920 bytes with SHA-256
  `B38B4B2770A38E04B123A591D8F33FA1E1D13BA0B98AA06833BDD5256BF6E74E`.
  Its build options contain the exact required FQBN and one unique M5Stack `3.2.1` hardware core.
  Compiler resources remain flash 3,479,726 bytes, global RAM 65,732 bytes and 261,948 bytes
  available for local variables.
- Accepted installed evidence remains one exact Complete cycle and one claimed/fixed-WDT cycle.
  Marker content was not observed, so no external-side-effect exactly-once claim is made. Explicit
  reset/power loss compose through the accepted claimed/no-replay state machine, and no maximum
  64-KiB Device latency claim is made. Cleanup preserves API credentials and Wi-Fi configuration;
  all other Cardputer state is disposable by standing user decision.
- P5-02 and its P5-02g closure checkpoint are completed. P5-03 remains pending, leaving zero active
  rows until the exact P5-02 commit is pushed and independently verified at the authenticated remote
  SHA.

### Frozen P5-02 write set

- Policy/dispatch/pending: `firmware/CardputerAssistant/src/tool_catalog.h`,
  `firmware/CardputerAssistant/src/tool_catalog.cpp`,
  `firmware/CardputerAssistant/src/api_client.cpp`,
  `firmware/CardputerAssistant/src/tool_router.h`,
  `firmware/CardputerAssistant/src/tool_router.cpp`,
  `firmware/CardputerAssistant/src/pending_tool_call.h`,
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
  `tests/micropython_supervisor_test.py`, `tests/web_console_ui_test.mjs` and this trace.

`AGENTS.md`, `ROADMAP.md`, `micropython/vfs/boot.py`, the manual Python HTTP/API implementation,
build scripts and all package/automation/USB/QOL surfaces are outside the expected P5-02 diff. Any
new required file or responsibility must be justified by a concrete reviewed blocker before edits.

The inherited `tools/python_handoff_e2e.mjs` manual-workspace harness must be restored byte-for-byte
to its P5-01 baseline and remain unchanged. P5-02f uses separately frozen exact-owned disposable
runs outside the retained repository diff; it adds no test framework or production interface.

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

## P5-01 publication evidence

- Row commit `521830486f0478e7899e187c151660116e91e90e` contains only this trace, passed the
  repository-local row checker, was pushed immediately to `feature/phase-5-python-one-shot`, and
  was returned at that exact SHA by authenticated GitHub user `varlolwut` with the required Author
  and Committer. The three approved Architect-owned `.codex/agents` files remain untracked.
- P5-01 was documentation-only. No production/test behavior, runtime state, resource measurement,
  build, Device, Web, stash or secret changed.

## P5-02 active gate

**Started:** 2026-09-01T21:49:32+03:00.

- Current hypothesis: the reviewed frozen design can be implemented as one coherent marginal diff
  inside the existing owners and exact write set, without a new server, runner, queue, handoff,
  audit or recovery framework.
- The P5-01 inventory, pre-edit reviewer `GO`, Architect closure `GO`, vendor contracts, ceilings,
  crash table and proof matrix are the P5-02 design gate. A newly discovered contradiction fails
  closed and returns to Architect before production changes outside that gate.
- P5-02 is the sole `in_progress` row. P5-03 remains pending. Its task clock excludes waits for
  platform approval, external review and hardware state.
- Architect incremental review stopped the first supervisor ordering because it mounted SD before
  durably preventing replay. The corrected valid-run path now selects CardMind, persists `claimed`,
  starts the fixed 30000 ms WDT, and only then mounts SD and validates/executes. The no-run branch
  preserves the inherited manual order exactly: clear `mode_error`, commit, mount SD, then continue
  the existing update/Wi-Fi/Web flow. Malformed and non-pending run records select CardMind and
  reset without entering manual mode. The retained supervisor test dynamically observed no-run,
  malformed/non-pending, valid dispatch and the exact boot/claim/WDT/mount effect order;
  `MICROPYTHON_SUPERVISOR_TEST result=pass`.
- The first host run exposed an oracle-only catalog-growth failure: hard-coded `0x1000` had become
  the valid bit for `PythonRun`. Production already derives and checks the exact 13-schema mask.
  The test now derives its first unknown bit from `ToolSchemaId::Count` with a compile-time mask-width
  condition; no production policy behavior changed. The corrected cheap suite observed
  `host_tests: PASS`. No firmware build, upload, Device or browser proof was run while the
  incremental Architect STOP remained active.
- After incremental Architect `GO`, the exact pinned build passed with M5Stack core `3.2.1`, FQBN
  `m5stack:esp32:m5stack_cardputer:FlashSize=8M,PartitionScheme=custom`, flash 3,474,502 bytes
  (20%) and global RAM 65,796 bytes (20%), leaving 261,884 bytes. Compile findings were confined
  to omitted new availability arguments, Arduino sketch prototype visibility, one local timestamp
  owner and the pinned mbedTLS SHA-256 symbol names; each was corrected at its existing P5 owner.
  The first upload command stopped before upload or Device testing because `esptool` could not open
  COM8 (`port is busy or doesn't exist`) and wrote zero bytes. Architect classified this as a host
  precondition failure, not loss of normal Device readiness during a test. After an unrelated stale
  host process chain was removed, that material environment change authorized one first actual
  upload attempt using the same already-built and options-verified image, without rebuild, reset,
  separate COM8 probe or recovery action. Runtime, reconnect, resource and cleanup acceptance remain
  unproven until that lifecycle succeeds.
- The required fresh code review then returned `STOP` on seven P5-02 defects: foreign pending-call
  ownership, exact request-only/no-run recovery, contiguous 64 KiB source allocation, unbounded
  `SystemExit`, loss of remaining required groups, incomplete/unserialized trust guidance, and the
  omitted `pending_tool_call.h` write-set owner. Architect confirmed all seven as P5-02 and approved
  one bounded correction batch. The corrected code classifies the canonical tool before mutation;
  preserves foreign/malformed/ambiguous state; accepts only an exact request matching the claimed
  pending file identity; streams source size/UTF-8/SHA-256 in 1 KiB chunks; normalizes exit status
  to signed int32; makes Python legal only after all other required groups; serializes guidance and
  states durable-chat/non-containment trust facts; and records the actual header owner.
- Corrected evidence observed `host_tests: PASS`, `MICROPYTHON_SUPERVISOR_TEST result=pass`, and
  `WEB_CONSOLE_UI_TEST result=pass`. The next exact pinned compile passed at flash 3,478,690 bytes
  (20%) and unchanged global RAM 65,796 bytes (20%), leaving 261,884 bytes. At that evidence point
  no upload, Device, browser or recovery action had followed the host precondition failure.
- The same fresh reviewer performed its single permitted blocker follow-up and returned no findings:
  all seven exact defects are resolved without architecture expansion or a new contract regression.
  Residual gaps are exclusively unobserved hardware behavior: installation of the changed
  MicroPython VFS, success/exception/reset/WDT, no replay, exactly-once chat attachment, exact
  cleanup/manual-workspace preservation/SD ownership, full-source Device/Web approval and
  same-address return, plus runtime heap/largest-block/stack/latency. P5-02 remains `in_progress`
  and is not evidence-ready until the newly authorized first actual upload and one coherent runtime
  lifecycle provide those observations.
- Architect personally inspected the bounded seven-finding correction and returned incremental
  `GO`; this is explicitly not P5-02 closure approval. Final read-only reconciliation against the
  P5-01 commit observed `diff --check` pass and exactly 24 modified tracked files, all inside the
  frozen P5-02 write set. The only untracked paths remain the three pre-existing Architect-owned
  `.codex/agents` definitions. Nothing is staged, committed or pushed.
- After Architect reclassified the earlier zero-byte COM8 failure as a host precondition and an
  unrelated stale process chain was removed, the one authorized first actual upload attempt used
  the existing corrected image without rebuild, reset or separate probe. `esptool` again failed
  while opening COM8 (`port is busy or doesn't exist`); upload and Device testing never began and
  zero bytes were written. That path stopped immediately with no retry, recovery, Device or Web
  action. The external host/transport precondition therefore still blocks all mandatory runtime
  evidence and P5-02 completion.
- Architect read-only Windows enumeration then classified the host precondition as device absence,
  not port contention: Win32 serial/PnP inventory contains no COM8 and no Espressif, Cardputer or
  USB-JTAG device. `SERIALCOMM` exposes only COM5, identified as an unrelated CH340
  (VID_1A86/PID_7523), so COM5 is forbidden as a substitute. P5-02 remains the sole `in_progress`
  row with an exact external USB-enumeration blocker. No Device, Web, build or upload action is
  authorized until the user separately restores a data-capable USB connection and Windows exposes
  the Cardputer again.
- On 2026-09-02 Windows again enumerated the Cardputer as Espressif VID_303A/PID_1001 with USB
  JTAG/serial and COM8, ending that external USB-enumeration blocker interval. Architect authorized
  one upload attempt from the unchanged already-built, options-verified corrected image, followed
  only on success by one coherent smallest production runtime lifecycle. No rebuild, reset,
  separate readiness probe, recovery chain or repeated scenario is authorized.
- That exact upload completed successfully: `esptool` wrote 3,478,880 bytes at `0x00010000`,
  verified the image hash and returned COM8 after its normal RTC-WDT reset. Before the runtime
  lifecycle began, Architect stopped work to expose the P5-02 execution checkpoints above. No
  runtime scenario was started; P5-02f therefore remains the sole `in_progress` checkpoint.
- The first P5-02f holder passed its single initial PING, started the normal Web Console and ran
  the Node child for about 36 seconds, but its disposable evidence-capture layer called `.Trim()`
  on a null empty-stdout value and masked the child's primary stderr/cleanup verdict. The holder
  still observed exact `WEB_CONSOLE result=stopped`; no reset, recovery, probe or repeated scenario
  followed. Because its temporary output files were removed by its finalizer, this run is classified
  as holder-owned evidence loss, not production evidence, and neither runtime success nor exact-owned
  HTTP cleanup is claimed from it.
- Architect authorized one materially corrected holder with direct capture and a pre-mutation HTTP
  inventory. Before touching COM8, local disposable children proved the corrected capture preserves
  empty stdout plus exact `CAPTURE_STDERR` at exit 7 and structured
  `{"result":"capture-pass"}` stdout with empty stderr at exit 0. This validation is holder evidence
  only, not P5 acceptance. The corrected Device lifecycle must stop before mutation if its read-only
  preflight finds any `P5 one-shot ` project, `cardmind_p5_` workspace file or pending request.
- That read-only preflight observed no `P5 one-shot ` project and no `cardmind_p5_` file, but found
  one orphan `awaiting` `write_file` pending whose public preview target is blank. It stopped before
  policy, project, file or prompt mutation, and the holder again observed exact
  `WEB_CONSOLE result=stopped`. Public `/api/pending` omits persisted project/chat/revision/call
  arguments, a blank target proves only preview failure, and acknowledge validates no owner beyond
  the supplied pending id. Architect therefore classified the orphan as not provably P5-owned.
  Deny, acknowledge, deletion and another P5-02f lifecycle are forbidden until explicit ownership
  authorization; this is the current external exact-ownership blocker.
- Review of the disposable harness found that failed pending cleanup could previously continue into
  project/source deletion. The local-only correction now requires literal `present=false` for an
  already-absent pending, literal `present=true` before exact target/action handling, and throws on
  missing/null/malformed presence. After an action it still requires a second literal
  `present=false`; project deletion is gated on that proof and source deletion on both pending
  absence and project removal. Otherwise evidence is preserved and cleanup fails. Unsandboxed
  `node --check` and custom-work-tree `diff --check` both passed; no COM8/HTTP action followed.
- The user then explicitly authorized cleanup of only that single non-resumable orphan pending,
  without execution or prefix-based deletion. One bounded normal Console lifecycle observed exact
  fresh shape `present=true`, `state=awaiting`, `tool=write_file`, empty target,
  `can_acknowledge=true`, `can_deny=false`; posted `acknowledge` with the freshly read id; and then
  observed `present=false`. Read-only postconditions found zero `P5 one-shot ` projects, zero
  `cardmind_p5_` files and zero running `python_run` activities. Serial evidence was one `PONG`,
  exact `WEB_CONSOLE result=ready`, and exact `WEB_CONSOLE result=stopped`. No model call, deny,
  execution, project/file deletion, build, upload, reset or recovery occurred. The ownership blocker
  is cleared, but P5-02f remains frozen pending review of a smaller direct proof; the repeated
  489-line one-shot harness is retired from further runtime use.
## P5-02 publication evidence

- Final row commit `4913e4f1d6ab89f83751a6995691af088ddac4d4` has subject
  `P5-02: add one-shot Python execution`, unchanged tree
  `6ae36331d37e6d49c98f69de267ffdf5ce43cf09` and exactly the Architect-approved 24 paths.
  Author and Committer are both exactly
  `Alexey Bulygin <30726976+varlolwut@users.noreply.github.com>`.
- The repository-local row checker returned
  `ROW_LOCAL_CHECK result=pass row=P5-02 sha=4913e4f1d6ab89f83751a6995691af088ddac4d4 paths=24`.
  The commit was pushed immediately to `feature/phase-5-python-one-shot`; authenticated GitHub MCP
  user `varlolwut` returned both that branch head and the commit at the exact same SHA with all 24
  paths. No production, runtime, Device, Web, fixture or secret state changed during publication.

## P5-03 closure evidence

**Started:** 2026-09-02T15:09:06+03:00.

**Completed:** 2026-09-02T17:16:34+03:00.

- Current hypothesis: Phase 5 can close through canonical scope/evidence reconciliation, proportional
  retained regression and CI/PR/merge gates without new P5 production behavior or re-running the two
  frozen installed scenarios. Any omitted acceptance, foreign defect or new runtime requirement is
  classified before mutation and reported to Architect under the normal ownership rules.
- P5-03 became the sole active row after P5-02h passed its local row-close check, was pushed to the
  phase branch and matched both authenticated exact-SHA and branch-head GitHub lookups. Its
  five-file documentation/trace diff remained P5-03-owned through review; no production or runtime
  work was reopened.
- After successful P5 closure, stop without activating, preparing or modifying Phase 6; the user
  will restart the system before any later phase transition.
- Architect's mandatory P5-03 closure review returned `STOP` on one consolidated documentation
  consistency defect: the user guide and Web Console made ordinary return/resumption absolute, and
  the limitations text omitted the implemented durable-but-invalid complete-result case. The three
  P5-03-owned statements now qualify normal completion/fixed-watchdog return, state that authorized
  privileged code can defeat recovery or alter boot behavior, and require a valid complete result.
  Only diff-check and local Markdown-link sanity are rerun before immediate personal re-review.
- The permitted `git diff --check` and local Markdown-link sanity both passed. Architect then
  personally inspected the actual five-file working-tree diff and returned explicit P5-03 closure
  `GO`; a fresh independent final wording review also returned `GO` with no scope expansion. The
  exact row ownership is `README.md`, `docs/limitations.md`, `docs/user-guide.md`,
  `docs/web-console.md` and this trace. No runtime, production, test, asset, schema, route, persisted
  state, Device/Web state or vendor contract changed.

### P5-03 reconciliation and local evidence

- The complete canonical Phase 5 clauses reconcile to the accepted P5-02 implementation and
  installed evidence: separate-image/manual-workspace reuse, mandatory exact-byte approval,
  CardMind and MicroPython hash checks, bounded consume-once handoff/result, preselected CardMind
  return plus fixed WDT, originating-chat exactly-once attachment/no replay, exact cleanup and
  same-address Web transition. The accepted marker-observation and maximum-64-KiB latency residuals
  remain explicit; no removed package, automation, queue, retry, background or recovery framework
  has re-entered scope.
- A CI-equivalent local closure run passed third-party/license verification, Python tool syntax,
  all MicroPython VFS syntax, the supervisor integration suite, strict host C++ tests, the retained
  Web Console UI suite and `diff --check`. Results included
  `THIRD_PARTY result=pass components=8`, `MICROPYTHON_SUPERVISOR_TEST result=pass`,
  `host_tests: PASS` and `WEB_CONSOLE_UI_TEST result=pass`; its exact-owned WSL temporary directory,
  host binary and bytecode were removed. No Device, COM8, upload, HTTP, Browser, model, reset or
  runtime scenario was repeated.
- The tracked filename-only secret scan found zero secret-like filenames and inspected no secret
  contents. There is no repository pull-request template. The documentation audit found that the
  retained user docs described only the inherited manual Python workspace; P5-03 therefore owns the
  minimal one-shot/trust/reconnect guidance in `README.md`, `docs/limitations.md`,
  `docs/user-guide.md` and `docs/web-console.md`. Together with this trace, these five files are the
  complete P5-03 write set; production, tests, assets, build configuration and vendor inputs remain
  unchanged. At this reconciliation checkpoint, CI, final independent/Architect review, row
  publication and merge remained pending.
- Architect's focused documentation review returned one `STOP`: the first limitations wording
  incorrectly grouped every hash mismatch with pre-handoff denial, while result integrity is also
  checked after execution. The bounded correction now distinguishes no-handoff approval/input
  failures, CardMind's pre-switch exact-source check, MicroPython's post-switch/pre-exec request and
  source check, and a post-exec result-integrity failure whose effects are unknown and which is never
  replayed. No other prose, production behavior, proof, build or runtime action changed.
- The comparable retained P4 Web/API sample is free heap `98,804`, largest block `35,828`, minimum
  heap `74,880` and stack free `5,588`. P5 Complete pre/post samples are
  `98,348 / 99,408`, `34,804 / 36,852`, `84,564 / 90,800` and `5,272 / 5,272`; their
  deltas from P4 are `-456 / +604`, `-1,024 / +1,024`, `+9,684 / +15,920` and
  `-316 / -316`. P5 Claimed pre/post samples are `98,596 / 99,448`,
  `32,756 / 34,804`, `87,508 / 90,888` and `5,272 / 5,256`; their deltas are
  `-208 / +644`, `-3,072 / -1,024`, `+12,628 / +16,008` and `-316 / -332`.
  Across the comparable Web-active observations, free heap remains at least `98,348`, largest block
  at least `32,756` and stack free at least `5,256`, while minimum heap improves; this is no material
  P5 regression against the retained P4 sample and requires no rerun.
- P5-02 initially completed remote publication no later than the observed P5-03 start at
  `2026-09-02T15:09:06+03:00`, after starting at `2026-09-01T21:49:32+03:00`: the wall interval is
  `17h19m34s`. Exact primary-agent active duration cannot be reconstructed because retained task
  history exposes turn-level intervals but not per-item active/wait accounting. The interval includes
  excluded overnight inactivity, Architect and specialist review waits, platform/tool waits, the
  external COM8 absence and user restoration interval, and bounded hardware/HTTP/CI waits. Therefore
  neither the 12-hour target nor compliance with the normal 16-hour active ceiling is claimed; closure
  conservatively classifies the ceiling as exceeded/unproven rather than subtracting invented time.
  The required 60-minute material pivot is retained in the task turn whose observable wall window was
  `2026-09-02T06:25:01+03:00` through `2026-09-02T08:37:22+03:00`; item-level timestamps are not
  exposed, so an exact minute is unavailable. Its materially different approach was the reviewed
  policy-isolated direct five-stage lifecycle already recorded above, replacing the invalid combined
  proof path rather than repeating it. P5-02h has its separate reopen timestamp and clock.
- The authoritative Device-state boundary for closure preserves only API credentials and Wi-Fi
  configuration because losing them would require user intervention. All other settings, projects,
  chats, files, Python/SSH state and selections are disposable; historical restoration observations
  remain facts, but P5-03 requires only those two protected values and a documented coherent final
  baseline and spends no further proof effort restoring disposable state.

## P5-02h cancellation reopen gate

**Reopened:** 2026-09-02T16:05:00.816+03:00.

- Architect's personal Phase 5 closure review found that the existing Python branch in
  `approvePendingProjectToolCall()` accepted an `isCancelled` callback but never observed or forwarded it;
  cancellation remained active only on the non-Python path. Because `stagePythonRun()` can create
  the request, commit runnable `run=pending` state and select MicroPython, this is a real omitted
  ROADMAP cancellation boundary and reopens P5-02 rather than being absorbed into P5-03.
- P5-02h owns only a minimal correction in the existing router/staging/activity/result owners and
  the smallest production-linked proof. Cancellation must latch before Python staging and at the
  last reversible point before runnable run-state commit. Cancellation observed after request
  creation exact-cleans only the request/temp, commits no runnable run state, changes no boot
  selection, emits no handoff, finishes the started activity as Canceled with zero output/no exit,
  and returns the existing canceled tool result while preserving the existing continuation and
  `ClaimedApprove` ownership.
- No new file, module, route, status, queue, framework, broad harness, Device/Web run, build, upload
  or runtime action is authorized before the bounded design and smallest executable proof are
  reviewed. P5-03 remains pending; its existing five-path documentation diff is preserved unstaged.
- Architect returned pre-edit `GO` for exactly `tool_router.cpp`, `python_mode.h`,
  `python_mode.cpp` and the P5-02h trace hunks, and rejected the proposed disposable router/stage
  stub as disproportionate mock evidence. The implemented approval owner now uses one monotonic
  latched callback, polls it before staging, forwards it to `stagePythonRun()`, and maps either
  observed cancellation to the existing canceled tool result plus a Canceled activity with zero
  output and no exit. No caller, pending, continuation, activity format or persisted state changed.
- `stagePythonRun()` now performs its second callback poll after the exact request has been written
  and the local run blob constructed but immediately before the first `writeRunBlob()` call. On
  cancellation it independently checks removal of only request temporary and request paths and
  returns without NVS commit, partition selection or handoff. Cleanup failure remains explicit and
  may retain only a non-runnable detached request for the existing startup owner. A callback that
  remains false follows the previous run-blob, boot-selection and handoff path unchanged; a callback
  changing after the final false observation is intentionally beyond the reversible boundary.
- The approved production static/order check passed over the real functions: pre-stage poll before
  stage call, callback forwarding, request write before the second poll, both request-only cleanup
  attempts before run commit, run commit before boot selection, no broad cleanup or forbidden owner
  in the cancel block, and retained false/false handoff. `git diff --check` passed, all eight current
  working paths are the three P5-02h production owners plus the frozen five-path P5-03 docs/trace
  set, and nothing is staged.
- A fresh independent read-only code review returned `GO` with no findings. It confirmed both cancel
  branches, cleanup-failure behavior, Canceled/zero-output/no-exit activity, standard canceled result,
  unchanged `ClaimedApprove`/continuation ownership, compatible signature/aggregate initialization,
  unchanged false/false handoff and no new persisted format, helper, route or framework. Residual risk
  is the intentionally unobserved late cancel after the irreversible boundary and a possible
  non-runnable request artifact after an explicitly reported SD cleanup failure.
- One exact pinned compile passed with FQBN
  `m5stack:esp32:m5stack_cardputer:FlashSize=8M,PartitionScheme=custom` and one unique M5Stack core
  `3.2.1`. Flash is `3,480,890` bytes, global RAM remains `65,732` bytes and available RAM remains
  `261,948` bytes; versus the accepted P5-02 build this is `+1,164` flash bytes and zero global-RAM
  growth. The app binary is `3,481,072` bytes with SHA-256
  `5D6B1262E79833B97F7C3B9F078495AA8C3588EFFD07812AA1094C64E413164E` and is newer than all three
  changed sources. No second build, upload, COM8, Device/Web, browser, model, reset or runtime action
  occurred, and no retained or disposable test/harness file was added.

### P5-02h Architect closure GO

**Completed:** 2026-09-02T16:41:50.265+03:00.

- Architect personally reviewed the actual three-file production diff, all unchanged callback
  producers and continuation/recovery consumers, the two reversible cancellation windows, exact
  request-only cleanup, activity/result semantics, false/false preservation, static/order evidence,
  fresh independent review, build pins/resources and residuals, then returned explicit P5-02h
  closure `GO`. No further P5-02h production change is authorized.
- P5-02 and P5-02h are completed. P5-02h commit
  `ccfbf602a7e8fe4358d36f9888d1e931c82743e6` passed the local row-close check, was pushed to
  `feature/phase-5-python-one-shot`, and matched both authenticated exact-SHA and branch-head GitHub
  MCP lookups with the exact identity and four row-owned paths. P5-03 has passed its mandatory
  closure review and is completed, leaving zero active rows while its exact atomic commit and remote
  publication are performed.

## P5-02i recovery-owned staging failure reopen gate

**Started:** 2026-09-02T17:40:03+03:00.

**Completed:** 2026-09-02T18:36:00+03:00.

- Architect's full-phase review at exact published head
  `60356ebe73983944485b999f6f1ef6381e32172e` returned `STOP` on one production crash-ownership
  defect. `stagePythonRun()` can report failure after `writeRunBlob()` while retaining the request
  and possibly durable run state, but `approvePendingProjectToolCall()` currently converts every
  staging failure into a successful pending decision with an ordinary invalid-tool result. Device
  and Web consumers can then continue the model and clear the exact `ClaimedApprove` pending,
  contradicting the retained startup-recovery/no-generic-continuation contract.
- P5-02 is reopened only as P5-02i and is the sole `in_progress` owner. P5-03 remains completed;
  exact-head PR/CI/full-phase closure and merge are frozen. Phase 6 remains untouched and cannot be
  activated after P5 closure because the user will restart the system first.
- The correction must distinguish safely-cleaned ordinary staging failure from recovery-owned or
  ambiguous staging failure through one explicit typed signal in existing owners. `writeRunBlob()`
  failure is recovery-owned. Boot-selection failure is ordinary only when both run-state discard and
  artifact cleanup are verified successful; otherwise it is recovery-owned. Recovery-owned failure
  attempts the existing Failed audit, returns a non-success pending decision, preserves exact
  `ClaimedApprove`, and performs no model continuation, pending clear, boot handoff, replay or generic
  acknowledgement. Deterministic pre-run, cancellation and safely-cleaned failure behavior remains
  unchanged.
- No recovery manager, persistence/status store, route, module, framework, error-text parsing, broad
  fault-injection harness, NVS/SD mock, build, CI, Device, Web or runtime action is authorized before
  the producer/consumer inventory, minimal design, executable proof and exact write set receive
  Architect pre-edit `GO`.
- Frozen producer inventory: `stagePythonRun()` is the sole producer of `PythonRunStageResult` and
  `approvePendingProjectToolCall()` is its sole caller. Source verification, request serialization
  bounds and request hashing fail before an SD artifact can be created and remain ordinary.
  `writeRequestFile()` is the sole request writer; after any open/write/readback/rename failure it
  must attempt and retain the exact cleanup results for both temporary and committed request paths.
  Its failure is ordinary only when those exact-owned artifacts are verified absent/removed;
  otherwise it is recovery-owned. After a committed request, cancellation is ordinary only when both
  temporary/request cleanup results verify success; either failure is recovery-owned. A
  `writeRunBlob()` failure remains recovery-owned because the request remains and `run` may be absent,
  partial or durable. After boot selection failure, ordinary ownership requires verified success
  from both `discardPythonRunState()` and `cleanupPythonRunArtifacts()`; either failure is
  recovery-owned. The successful handoff remains unchanged. The router's one local canceled
  initializer is the only other stage-result initializer.
- Frozen consumer inventory: Device `allowPendingToolOnce()` and `allowPendingToolForChat()`, plus Web
  `handlePendingAllowOnce()` and `handlePendingAllowChat()`, all stop before continuation when the
  pending decision has `success == false`. Their existing continuation helpers are the only paths
  that can continue the model and later clear the terminal pending; they require a successful
  decision and therefore need no production edit. Mandatory Python approval cannot save Allow for a
  chat, but both generic Allow-for-chat callers remain mapped and protected by the same failure gate.
- Frozen pre-stage ownership: after durable `ClaimedApprove`, keep
  `revalidateClaimedPendingToolCall()` failure as the existing ordinary bounded result because the
  current approval has not begun staging. Call `preparePythonRunStaging()` only in a separate branch.
  Any preparation failure returns `PendingToolDecisionResult.success == false` with the exact claimed
  pending and error, before activity creation or `stagePythonRun()`. This conservatively preserves an
  unreadable/unknown run, an already-present run, or failed orphan-artifact cleanup for startup; it
  performs no continuation, pending clear, handoff, replay or acknowledgement and creates no audit
  record because no activity started. No preparation type, field, helper or persisted state is added.
- Frozen minimal design: add one explicit `startupRecoveryRequired` Boolean to
  `PythonRunStageResult` and one pure inline `pythonRunStageFailureAllowsContinuation()` predicate in
  the existing host-compatible `python_mode.h`. Every initializer sets the field explicitly. In
  `python_mode.cpp`, one small local typed request-write result carries success, recovery ownership
  and error; one failure helper attempts both exact-owned request cleanups and aggregates their
  outcomes without an output parameter or mode flag. The router attempts the existing Canceled audit
  for observed cancellation and the existing Failed audit for every other staging failure. A
  recovery-owned result then returns `PendingToolDecisionResult.success == false` with the claimed
  pending and aggregated error. An ordinary failure retains the accepted Canceled or
  `invalidToolCall()` continuation. No error-string parsing or new persisted state is introduced.
- Frozen crash mapping: ambiguous request-write cleanup and canceled-request cleanup preserve the
  request residue, the appropriate Failed/Canceled audit attempt and durable `ClaimedApprove` for
  startup ownership. The same applies after ambiguous NVS run write, or after boot-selection failure
  whose run discard/artifact cleanup is not fully verified. No boot handoff, continuation, pending
  clear, replay or acknowledgement occurs. If request cleanup verifies success, or boot selection
  fails and both later cleanup operations verify success, no runnable state remains and the accepted
  ordinary continuation is safe. Audit-finish failure is appended to the returned error but cannot
  convert a recovery-owned result into an ordinary decision.
- A failed pre-stage preparation has no new audit sequence and preserves the claimed pending plus all
  unknown/existing run or artifact state unchanged for the startup owner. It cannot be converted to
  `invalidToolCall()` continuation; only the preceding stale-authority revalidation failure retains
  that ordinary path.
- Frozen startup reuse: after `loadPythonRunRecovery()` authoritatively reports no `run` and the
  catalogued pending is proven to be the exact Python `ClaimedApprove`, retain the existing valid
  detached request/result recovery and validated request-only paths. Any other final or temporary
  result residue remains ambiguous execution evidence and is preserved fail-closed. Only when both
  result paths are absent may no artifact, committed request, temporary request or both request paths
  be treated as the bounded pre-staging/no-code-executed outcome and fall through the existing
  `kInterruptedBeforeStaging` idempotent chat attachment, followed by existing pending-clear-before-
  artifact-cleanup ordering. If authoritative run absence or exact pending ownership is unproven,
  existing fail-closed preservation remains unchanged. No recovery mode, manager, status or route is
  added.
- Frozen exact write set is `firmware/CardputerAssistant/src/python_mode.h`,
  `firmware/CardputerAssistant/src/python_mode.cpp`,
  `firmware/CardputerAssistant/src/tool_router.cpp`,
  `firmware/CardputerAssistant/CardputerAssistant.ino`, `tests/host_tests.cpp` and this trace. Device
  and Web approval handlers, persistence schemas, routes, assets, supervisor, build configuration
  and documentation are excluded.
- Frozen smallest proof: extend the existing strict host binary, which already includes
  `python_mode.h`, with one focused test executing the production continuation predicate for ordinary
  failure, recovery-owned failure and success. A bounded source-order/inventory check maps every
  request-writer cleanup result, all stage-result initializers, NVS/boot-cleanup classifications, the
  cancellation-specific Canceled-audit/non-success ordering, the non-cancel Failed-audit branch, and
  the exact startup no-run/claimed-residue path through idempotent attachment and pending-before-
  artifact cleanup. It also proves revalidation stays before a separate preparation call, every
  preparation failure returns non-success before activity creation/staging, and only revalidation
  failure reaches the ordinary pre-stage continuation; it is supporting evidence, not the sole
  oracle. Then run the existing strict
  host suite, one exact pinned firmware compile for production/caller compatibility, `diff --check`,
  and resource-size comparison. The startup mapping must separately prove that result-bearing
  ambiguity is preserved while request-only residue reaches the no-code attach/cleanup path. No
  NVS/SD mock, retained fault injector, upload, COM8, Device, Web, Browser, CI or runtime rerun is in
  the proof.
- The first independent reviewer `GO` is superseded: Architect personally confirmed that it missed
  discarded request-cleanup failures, recovery-owned canceled cleanup and the exact startup
  no-run/claimed residue branch. The revised consolidated gate requires a fresh independent review
  and final Architect pre-edit `GO`; production and tests remain frozen.
- The fresh reviewer returned `STOP` on one startup distinction: an invalid final/temporary result
  can be execution evidence and must not be replaced by the no-code message or destroyed. The
  corrected gate preserves every unvalidated result-bearing combination fail-closed and limits the
  no-code fallback to no-result request/request-temporary residue after exact run absence and pending
  ownership are proven. The same reviewer may verify this one correction once; production and tests
  remain frozen.
- The same fresh reviewer performed the permitted one-time verification of that exact correction and
  returned `GO`; no additional blocker or scope expansion was found. Architect final pre-edit review
  remains mandatory before any production or test change.
- Architect's corrected-gate review then returned `STOP` on the earlier
  `preparePythonRunStaging()` branch, which also follows durable claim but precedes the stage-result
  signal. The gate now separates ordinary stale revalidation from non-success preparation failure as
  specified above. Architect directed no third reviewer cycle; production and tests remain frozen
  pending immediate personal re-review.
- Architect personally rechecked the corrected trace and producer/consumer code and returned final
  pre-edit `GO` for exactly the frozen six-file design and proof. Implementation now uses the one
  reviewed request-write cleanup result, stage recovery signal/continuation predicate, separate
  non-success preparation branch, Canceled/Failed audit split, conservative boot cleanup ownership,
  result-bearing startup preservation and no-result request-residue attach path. No excluded owner,
  persistence format, route, module, framework or runtime surface is added. Verification remains
  pending and no closure status is claimed.
- The bounded source/order inventory passed across the exact six-file write set: all four request-
  writer failures use the exact cleanup result, request-write and cancellation ambiguity set recovery
  ownership, NVS-write and boot-cleanup ownership are explicit, stale revalidation precedes the
  separate non-success preparation branch and activity creation, both Canceled and Failed audit
  paths consult the production continuation predicate, result-bearing startup residue remains
  fail-closed, and no-result request residue reaches idempotent attachment followed by pending-before-
  artifact cleanup. Result: `P5_02I_SOURCE_ORDER result=pass paths=6 request_failures=4
  predicate_cases=3`.
- The pinned dependency check passed. The existing strict WSL host suite compiled with
  `-std=c++17 -Wall -Wextra -Werror`, executed the retained production-predicate cases and all existing
  host coverage, returned `host_tests: PASS`, and removed its exact-owned temporary ELF. The six-file
  `git diff --check` also passed.
- One exact pinned firmware compile passed with FQBN occurrence count `1` and unique resolved M5Stack
  core `3.2.1`. Flash is `3,481,774` bytes, `+884` from the accepted P5-02h build; global RAM remains
  `65,732` bytes with `261,948` available, a delta of `0`. The app binary is `3,481,968` bytes,
  SHA-256 `10D78E446BBAE861280688EB6F735165D9089BF05EC3121C4A3755E570316D76`, and is newer than every
  changed production source. No upload, COM8, Device, Web, Browser, CI or accepted-runtime scenario
  ran.
- A fresh independent code reviewer inspected the actual six-path implementation and returned `GO`
  with no blocker. Architect then personally reviewed the complete diff line by line, crash contract,
  producers/consumers, NVS/SD ambiguity, audit ordering, proof, resources, cleanup and residuals and
  returned mandatory P5-02i closure `GO`. P5-02 and P5-02i are completed with zero active rows;
  staging, one exact atomic commit and remote publication are now authorized. P5-03 remains completed,
  exact-head phase closure stays frozen until publication, and Phase 6 remains untouched.

## P5-02j Python artifact access/absence reopen gate

**Started:** 2026-09-02T18:56:15+03:00.

**Completed:** 2026-09-02T19:46:40+03:00.

- P5-02i commit `993eb202e18c2346a4bcef83cf1d983cc2807be2` passed its local row-close
  checker, was pushed, and matched authenticated exact-SHA and branch-head lookups with the exact six
  paths and identity. Exact-head build-and-release run `33649846696`, job `100313783003`, passed and
  PR #3 was clean, but these do not override the following full-phase recovery finding.
- Fresh final Red Team and Architect returned mandatory full-phase `STOP`: pinned M5Stack ESP32
  `3.2.1` `FS/src/vfs_api.cpp` implements `VFSImpl::exists()` with Boolean false both for proven
  absence and mount/open/filesystem failure. Raw checks of the same four fixed artifacts occur in
  `removeOwnedFile()`, `writeRequestFile()` before final rename,
  `loadDetachedPythonRunRecovery()`, both the pre-read and anti-change post-read snapshots in
  `validateDetachedPythonRunRequest()`, and every startup presence branch in
  `consumePythonRunAtStartup()`. Any one of those checks can infer absence from access failure and
  release or overwrite ownership while request or result evidence remains ambiguous.
- P5-02 is reopened only as P5-02j and is the sole `in_progress` owner. P5-02i and P5-03 remain
  completed. PR/merge and all Phase 6 work remain frozen; no production edit, test, build, runtime or
  diagnostic may begin before a bounded vendor-backed design, exact inventory/proof and Architect
  pre-edit `GO`.
- Architect personally returned P5-02j pre-edit `GO` after verifying the corrected complete raw-check
  inventory, typed classifier/aggregate contract, exact temporary-versus-final request ownership,
  existing startup result shape, bounded four-file write set, proportional proof and exclusions.
  Production implementation and only the frozen proof are authorized; staging, commit and push
  remain forbidden until a separate mandatory Architect closure `GO`.
- Required typed boundary: `python_mode` defines one three-way artifact state (`Present`,
  `ProvenAbsent`, `AccessError`), one private exact-path classifier, and one no-argument aggregate for
  request, request-temporary, result and result-temporary. The four path constants remain private;
  no arbitrary-path public API is added. Aggregate fields default to `AccessError`, inspection stops
  at the first error, and no consumer may use a presence value unless the complete aggregate is
  successful.
- The classifier reuses `requireSdCleanupAccess()`, whose existing owner checks card presence,
  readable root, expected NVS identity, the on-card identity marker and replacement/restart state and
  permits both `Ready` and `Full`. It checks that access immediately before the target probe, clears
  `errno`, opens the target with `SD.open(..., FILE_READ)`, captures validity and `errno`, closes any
  valid handle, and checks the same expected-card access again. Either access failure is
  `AccessError`; a valid handle after both checks is `Present`; an invalid handle is
  `ProvenAbsent` only when the captured error is `ENOENT` and both access checks succeeded. An
  invalid handle with zero or any other error is `AccessError`. This uses the pinned vendor API and
  does not hard-code the VFS mountpoint or create another storage owner.
- `removeOwnedFile()` uses that classifier before and after removal. Pre-remove `ProvenAbsent` is
  success, `AccessError` is failure, and only `Present` permits `SD.remove()`. After removal, only a
  new `ProvenAbsent` classification succeeds; still-present and access-error outcomes fail. No raw
  `SD.exists()` remains in that owner.
- `writeRequestFile()` classifies the final request immediately before rename and may rename only on
  `ProvenAbsent`; `Present` and `AccessError` fail without touching the final request. The fixed
  request-temporary file becomes exact current-call-owned only after this invocation creates it, so
  a local failure may exact-clean only that temporary file through the same checked removal owner.
  A pre-existing or access-ambiguous final request is never current-call-owned and must not be
  deleted or overwritten to obtain a successful rename.
- `loadDetachedPythonRunRecovery()` and both artifact snapshots in
  `validateDetachedPythonRunRequest()` use the complete aggregate. Either snapshot rejects
  `AccessError`; validation may compare pre/post states only after both complete successfully, so
  the second anti-change snapshot cannot treat an access failure as stable absence.
- `consumePythonRunAtStartup()` loads persisted recovery and pending state without mutation, then
  obtains one successful aggregate before any no-pending, foreign-pending, wrong-state or exact
  `ClaimedApprove` branch uses artifact presence. `AccessError` returns the existing
  `PythonRunStartupResult` with `success=false` and leaves the exact pending and all durable state
  untouched. `PythonRunStartupResult` gains no field or status. Only a fully successful aggregate
  may permit cleanup, continuation, pending clear, handoff, replay or acknowledgement decisions.
  If access later becomes unknown during already-authorized cleanup, checked removal fails and
  durable recovery ownership remains; success is never inferred.
- Unmounted, removed or replaced-card access must not appear absent. No new module, framework,
  route, persisted format, status field, Device/Web behavior, recovery manager or `sd_storage`
  responsibility is allowed.
- Preferred unopened write set is this trace, `firmware/CardputerAssistant/src/python_mode.h`,
  `firmware/CardputerAssistant/src/python_mode.cpp` and
  `firmware/CardputerAssistant/CardputerAssistant.ino`. Any growth requires evidence and Architect
  approval before it occurs. Required proof is pinned vendor-source mapping; no remaining raw
  `SD.exists()` for request, request-temporary, result or result-temporary in `python_mode.cpp` or
  `CardputerAssistant.ino`; positive absence/access verification for request rename and every
  removal success; successful complete pre/post detached-validation snapshots; and an access-error
  guard before every startup presence branch or mutation. Complete with bounded source/order
  inventory, the unchanged strict host suite, `diff --check`, one pinned compile/resource delta and
  fresh focused code review. No upload, COM8, Device/Web, Browser or fault-injection runtime is
  permitted.

### P5-02j implementation evidence awaiting closure

- The implementation stayed inside the frozen four-file boundary. `python_mode` now provides one
  typed `Present`/`ProvenAbsent`/`AccessError` classifier and aggregate; checked removal, final
  request pre-rename ownership, detached recovery, both detached-validation snapshots and every
  startup presence branch use that boundary. Failed request creation/commit cleans only the exact
  current-call temporary path and never removes the pre-existing or ambiguous final request.
- Bounded static proof passed: `diff --check` reported no error; no raw `SD.exists()` remains for
  request, request-temporary, result or result-temporary in `python_mode.cpp` or
  `CardputerAssistant.ino`; and source/order assertions confirmed expected-card checks around the
  target probe, pre/post removal classification, final-request proof before rename, complete
  detached recovery and validation snapshots, aggregate success before startup presence branches,
  and no startup-result status expansion.
- The unchanged strict host command from `.github/workflows/firmware.yml` passed with
  `host_tests: PASS`; its exact-owned WSL binary was deleted after success.
- The single pinned compile passed with M5Stack ESP32 core `3.2.1` and exact FQBN
  `m5stack:esp32:m5stack_cardputer:FlashSize=8M,PartitionScheme=custom`. Program storage is
  `3,484,350` bytes, `+2,576` bytes from P5-02i; global RAM is `65,732` bytes, unchanged. The binary
  is `3,484,544` bytes (`+2,576`) with SHA-256
  `5B850D7BE0867858CC8C41B167BF69DD61D2310D3F0B2A5F8EC8D7E770ECB5EC`.
- The tracked row diff is exactly this trace, `python_mode.h`, `python_mode.cpp` and
  `CardputerAssistant.ino`. Generated build output and the Architect-owned untracked `.codex/`
  files remain untouched and excluded. No test, `sd_storage`, schema, route, module, framework, UI
  or persisted-format file changed.
- One fresh focused read-only code reviewer was started but produced no verdict within the Architect
  timebox and was closed without replacement. Per Architect instruction, the completed diff and
  evidence return directly for the mandatory independent closure review. This row remains
  `in_progress`; no staging, commit or push is authorized before explicit closure `GO`.
- No upload, COM8, Device/Web, Browser, runtime fault injection or additional proof was performed.
- Architect personally reviewed the final actual four-file diff, pinned ESP32 `3.2.1` semantics,
  every request/result producer and consumer, mutation ordering, recovery ownership, evidence,
  resources, cleanup and exclusions and returned mandatory closure `GO`. The fresh independent code
  review also returned `GO`. P5-02 and P5-02j are completed with zero active rows; only their exact
  row commit, immediate push and authenticated remote-SHA verification are now authorized.
