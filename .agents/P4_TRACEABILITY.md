# CardMind Phase 4 traceability

This is the only active Phase 4 matrix. Each row is one independently observable
roadmap, interface, recovery, security, or closure boundary. Evidence is retained only
after that row's own acceptance observations pass.

## Scope lock

- Implement only `ROADMAP.md` Phase 4 SSH and remote-workspace behavior.
- The primary P4 task agent may use subagents at its own discretion for read-only review,
  bounded research, inventory, test-result analysis, and an outside opinion. The primary
  agent personally writes and integrates all repository code, tests, and harnesses; subagents
  do not edit the repository unless the user explicitly requests that delegation.
- SSH profiles have a product maximum of five. P4-01 uses a bounded store and must not add an
  SD-paginated profile index or scalable profile/storage framework.
- Extend the Phase 3 tool-policy, catalog, preview, confirmation, audit, and cancellation
  boundaries; do not create a parallel permission, profile, job, or execution framework.
- Extend existing profile CRUD/UI, the single Web PTY/ANSI terminal, manual SFTP/transfer
  streaming and host-key verification/store. Retain existing Device SD scrollback/recall;
  do not replace these boundaries with parallel subsystems.
- Long commands and terminals remain foreground. Background queues, durable jobs, reboot
  resume, running-job reconnect, automatic retry/result handles, generic execution budgets,
  and configurable audit retention remain unphased future work.
- Python, USB, API profiles/PWA/encrypted whole-backup, recovery/HTTPS, and the general
  security audit remain Phases 5 through 9.
- Secret bytes and paths, private-key bytes, passphrases, and SSH authority internals never
  enter model context, tool schemas/results, Web/API read routes, workspace tools, serial,
  logs, diagnostics, exports, or Git.
- Read-only/mutating classification applies only to model-issued `ssh_command` through the
  existing Phase 3 permission/runtime path. It does not change the authority semantics of an
  explicit manual Device or Web terminal session.
- Keep the existing single Web terminal and bounded Device terminal history. Phase 4 adds no
  terminal tabs, terminal-session UI state, configurable history rotation, or new history
  subsystem.
- `main` remains unchanged. Phase 4 may merge only to `develop` after closure passes.

### User scope reduction (2026-08-30)

The user explicitly removed, without implementation: Web terminal tabs, the 8 KiB direct-command
increase, configurable Device history rotation/new viewer, known_hosts pagination/rotation,
separate profile diagnostics, separate P4-18/P4-19 journey subsystems, and broad P4-20
re-certification of unchanged NVS/SD. Existing single Web terminal, 1,024-byte command cap,
terminal.log/old.log and bounded known_hosts remain. Host-key mismatch blocking remains mandatory.
P4-16/P4-17 own the required consolidated Device/Web controls. Safe Actions remain only as a
small fixed built-in reviewed set without presets, editor, macros, persistence or framework.

## Execution matrix

| ID | Atomic boundary | Required observation | Status |
| --- | --- | --- | --- |
| P4-01 | Existing indexed NVS SSH profile store capped at five, stable opaque non-secret uint64 IDs, public summaries, selected-profile JIT secret load, and ID-only legacy compatibility | Five profiles and IDs persist and a sixth is rejected before any write; IDs survive edit/select/delete/reboot and move with profiles; summaries read and return no w/k; selected runtime reads only exact selected w/k; legacy count zero through five adds only missing i0 through i4 and count above five stays unchanged | completed |
| P4-02 | Opaque private-key records and explicit profile-to-key ID binding | No shared mutable key slot; key material remains write-only and unreachable through model/file/read APIs | completed |
| P4-03 | Conservative model SSH classification in the existing Phase 3 policy/runtime path | Every arbitrary shell `ssh_command` remains `SshMutate`; `SshRead` is available only to exact fixed reviewed safe actions or strict non-shell templates; no regex/prefix shell classifier exists; manual Device/Web terminal authority is unchanged | completed |
| P4-04 | Configurable model SSH command timeout/output policy while retaining the existing 1,024-byte direct command cap | Timeout/output policy is explicit; the user-removed 8 KiB increase is not implemented | in_progress |
| P4-05 | Single-pass streamed SD command log, bounded model summary/reference, cancellation, and downloadable output | Output is written once while streaming to SD; the model receives only a bounded summary/reference; a long foreground command cancels and its log downloads without background execution | pending |
| P4-06 | Paged model SFTP list/read/write/move through existing `SftpReadWrite` and Phase 3 permission/confirmation boundaries | Model listing is bounded/paged; `Ask` confirms every model SFTP call; `Allow` still confirms overwrite/delete/move onto an existing target; overwrite defaults deny and no prior remote target is truncated/deleted before confirmed safe replacement; manual Device/Web SFTP remains direct-user authority | pending |
| P4-07 | Existing streaming CardMind workspace transfer to/from selected remote host | Both directions preserve workspace policy, total foreground deadline and cooperative cancel; overwrite defaults deny and replacement never destroys the prior target before confirmation | pending |
| P4-08 | Small fixed built-in Safe Actions set for logs, service state, containers, disk and processes | Reviewed fixed actions obey existing Off/Ask/ceiling/host-key/timeout/audit boundaries; no presets, editor, macros, persistence schema or action framework | pending |
| P4-09 | Removed by user: Web terminal tabs; existing single terminal remains | Scope closed by explicit user decision; not implemented | removed_by_user |
| P4-10 | Removed by user: configurable Device terminal-history rotation and new viewer; existing terminal.log/old.log remains | Scope closed by explicit user decision; not implemented | removed_by_user |
| P4-11 | Existing bounded known_hosts store with unconditional host-key-change block | Every mismatch blocks connection; user-removed pagination/rotation is not implemented | pending |
| P4-12 | Removed by user: separate SSH profile diagnostics feature | Scope closed by explicit user decision; not implemented | removed_by_user |
| P4-13 | Encrypted-at-rest evaluation and physical-access threat-model documentation only | No encrypted vault is implemented in P4; documentation states measured limitations and makes no unsupported encryption claim | pending |
| P4-14 | Project/chat ceilings bound to immutable opaque profile IDs | Authenticated config/project metadata may carry the ID; project/chat selection only narrows hosts and cannot exceed global authority or redirect stale authority | pending |
| P4-15 | Credential/private-key/profile-ID non-addressability across model, file tools, API, logs, serial, and diagnostics | Model SSH never returns credential/private-key bytes, private-key path, or internal profile ID; authenticated config APIs expose only allowed non-secret ID/summary data | pending |
| P4-16 | Consolidated Device Phase 4 journey for profile/security plus required command/SFTP/transfer controls using existing terminal/history | Required Device controls and acceptance are observable without a separate journey subsystem | pending |
| P4-17 | Consolidated Web Phase 4 journey for profile/security plus required command/SFTP/transfer controls using the existing single terminal | Required Web controls and acceptance are observable without terminal tabs or a separate journey subsystem; first real Cardputer private-key auth E2E runs here, with at most one authenticated CSRF-protected forget action for the exact selected profile whose host/port are resolved server-side | pending |
| P4-18 | Removed by user as a separate subsystem: Device command/SFTP/transfer journey | Required controls moved to P4-16; no separate implementation | removed_by_user |
| P4-19 | Removed by user as a separate subsystem: Web command/SFTP/transfer journey | Required controls moved to P4-17; no separate implementation | removed_by_user |
| P4-20 | Changed-boundary-only recovery and security acceptance | Verify only Phase 4 changed boundaries, including mismatch block, deletion of only the exact test-host known_hosts entry and byte-for-byte preservation of unrelated entries; broad re-certification of unchanged NVS/SD was removed by user | pending |
| P4-21 | Phase closure: documentation, focused/full regression, performance/resources, cleanup, independent review, CI/PR/merge | Docs/threat model/licenses/secret scans and exact Device/Web regressions pass without repeating P4-17/P4-20-specific scenarios; idle/general mode retains the 70 KiB floor; active SSH is compared with the existing active-SSH heap/largest-block/stack/latency baseline and must not regress or reset/freeze; SD ownership and exact cleanup are evidenced; final review has no blockers; green CI and reviewed PR merge only to `develop` with `main` and stash unchanged | pending |

## P4-01 design gate

**Status:** completed
**Pivot baseline:** feature/phase-4-ssh-remote-workspace at
99f6de9e0e5d558ba0162c714a14044111d9aab2. The discarded overengineered attempt is retained
locally only at trash/p4-01-overengineered-20260830
(aca0539a29a337e61160d86134d65ef5098a12e1).

### Locked roadmap clauses and acceptance

- Cap saved SSH profiles at five.
- Keep the existing indexed cardmind_ssh NVS representation. Public n/h/p/u/a fields remain
  separate from password/passphrase w/k fields.
- Persist one collision-checked non-zero opaque uint64_t ID for each logical profile index.
  IDs survive edit, selection and reboot, and move with their profile when indexed deletion shifts
  records.
- Expose a bounded public-summary loader that reads no w/k values.
- The selected runtime loader reads only the exact selected profile's w/k values just in time.
- Legacy logical counts zero through five gain only missing ID keys. Existing profile fields,
  selection and secrets are not rewritten by ID assignment. A stored indexed count above five
  fails closed before any ID write.
- A sixth create is rejected after read-only count inspection and before ID assignment or any
  profile NVS write.

### Inventory and ownership

- Existing indexed producers/consumers remain saveSshProfileAt(), saveSshProfile(),
  selectSshProfile(), deleteSshProfile(), Device SshTools.ino, Web Console profile CRUD and
  the bounded compatibility loadSshProfiles() API.
- Selected execution consumers are ssh_tool.cpp, pending_tool_call.cpp, Device terminal and
  Web terminal through loadSshProfile().
- loadSshProfiles() remains the existing compatibility owner for Device/Web editing in P4-01.
  It may materialize full records; P4-16/P4-17 own migration of those UI consumers to public
  summaries. P4-01 claims JIT behavior only for loadSshProfile() and secret-free behavior only
  for loadSshProfileSummaries().
- The older single-profile public keys and password/key_pass remain readable without format
  migration. ID key zero identifies that logical profile until an explicit existing CRUD mutation
  converts it through the existing indexed writer.
- NVS write/crash behavior remains the existing indexed-store behavior. This row adds no new
  recovery owner. SD profile metadata and the legacy SD private-key file are untouched.

### Minimal design

- Add SshProfileSummary, physically containing only opaque ID, name, host, port, username and
  auth mode.
- Store IDs as fixed indexed NVS keys i0 through i4. Zero means missing and is never a legal
  persisted ID.
- Validate all public records, stored selection and all non-zero IDs before assigning a missing
  ID. Generate from ESP32 randomness with a bounded collision check against at most five records,
  persist one missing ID, and read it back.
- loadSshProfileSummaries() rejects stored indexed count above five before writes, reads only
  public keys, assigns only missing IDs for valid zero-to-five legacy state, and returns at most
  five summaries.
- loadSshProfile() first resolves the selected public summary, then opens NVS read-only and reads
  only selected w/k or the one legacy password/key_pass pair.
- Existing CRUD keeps index semantics. Create appends one new ID; edit and selection preserve IDs;
  delete erases the matching ID alongside the profile and writes shifted profile/ID pairs together
  through the existing writer.
- initializeSshStorage() invokes the summary loader after existing known-host recovery so legacy
  IDs are assigned at boot without touching profile fields or secrets.

### Expected write set

- firmware/CardputerAssistant/src/ssh_client.h
- firmware/CardputerAssistant/src/ssh_client.cpp
- .agents/P4_TRACEABILITY.md
- firmware/CardputerAssistant/SshTools.ino only if the existing focused diagnostic cannot prove
  the frozen observations without a smaller direct manual check.
- No pending_tool_call, tool_router, Device/Web UI/assets, policy, project/chat, key storage or
  unrelated production files.

### Frozen minimal proof

| Observation | Required result |
| --- | --- |
| Five plus sixth and reboot | Five profiles, IDs and selection persist; sixth create returns failure with profile fields, secrets, IDs and selection byte-for-byte unchanged |
| ID stability | Edit and select preserve IDs; deleting an index moves every surviving ID with its matching public fields and secret; reboot is unchanged |
| Public summaries | At most five records; no password/passphrase members, reads or returned bytes |
| Selected JIT | Runtime reads only exact selected indexed w/k or exact legacy password/key_pass; switching selection changes the sole secret pair read |
| Legacy zero through five | Only missing i0...i4 keys are added; existing public fields, secrets and selection remain unchanged |
| Legacy above five | All P4-01 loads and mutations fail closed and no ID/profile/secret/selection key changes |
| Focused resources | After review GO, exact-core compile and focused Device run preserve bounded heap/stack/latency and no reset/freeze |

### Forbidden effects

- No public profile blob, binary/string codec, COW slot, binding, generation, orphan GC, marker,
  transaction/recovery layer or new storage framework.
- No pending approval identity, pending_tool_call, tool_router, UI/Web asset, project/chat,
  policy, SFTP, command, known-host or private-key change.
- No profile/secret rewrite during legacy ID assignment and no write on stored indexed count above
  five or sixth create.
- No claim that compatibility Device/Web list consumers are already selected-only JIT.
- No test harness before a read-only code-review GO.

### Gate status

- Independent read-only review returned GO for both the minimal production diff and the retained diagnostic after concrete false-success, bounds, selected-JIT, shifted-identity and restoration defects were corrected.
- The pinned build, COM8 upload, focused real-device diagnostic, reboot persistence observation, resource checks, exact-owned cleanup/restoration and final independent closure review all passed.
- P4-01 is complete. No P4-02 or later production scope was implemented.
### Observed P4-01 evidence after review GO

- Cheap boundary checks passed: only ssh_client.h, ssh_client.cpp and the existing SshTools.ino
  diagnostic changed; diff whitespace and placeholder scans passed.
- Exact pinned build passed with FQBN m5stack:esp32:m5stack_cardputer:FlashSize=8M,PartitionScheme=custom
  and resolved M5Stack ESP32 core 3.2.1. Flash use was 3,300,002 bytes and global RAM 65,604 bytes.
- Built image: 3,300,192 bytes; SHA-256
  EA16B47B3EE2F6C00921F115FA087260A185F8B285DA00608253D0B700DF03B6.
- COM8 upload completed with verified flash hash and no NVS or microSD erase.
- Focused real-device SSHPROFILETEST passed with exact-owned fixture cleanup and restoration.
  STATUS before/after remained responsive with reset reason 1, free heap 122,644 bytes,
  minimum heap 110,824 bytes and stack margin 7,868 bytes. Largest block was 60,404 before and
  58,356 after, matching the retained P3 largest-block baseline after the changed-boundary run.
- The focused diagnostic directly exercised fill-to-five, sixth-create rejection with unchanged profiles/summaries/selection, ID-preserving edit/select/delete-shift, exact selected-profile JIT load, exact-owned cleanup, restoration and repeated cleanup; it returned `SSHPROFILETEST result=pass`.
- Independent source review verified that public summaries have no password/passphrase members or reads, the selected loader reads only the selected indexed `w/k` pair, valid legacy counts add only missing IDs and stored counts above five fail closed before writes. The reviewer accepted static evidence for the unsafe-to-fixture count-above-five path, so user NVS was not deliberately corrupted.
- A whole-NVS hash comparison changed across reboot because unrelated NVS owners also write there; that result was rejected as a test-method defect and was not used as P4-01 evidence or repeated under the same hypothesis.
- A purpose-built memory-only parser then compared only written `cardmind_ssh/i0...i4` uint64 entries across a hardware reboot. Exact equality passed for one persisted profile; no ID value, raw partition, hash or secret was logged or written to disk.
- Post-reboot focused execution remained responsive with reset reason 11, free heap 120,528 bytes, largest block 56,308 bytes, minimum heap 113,336 bytes and stack margin 7,868 bytes; no freeze or unexpected reset occurred.
- The independent closure reviewer returned `GO` after the exact reboot-ID observation. P4-01 is complete on 2026-08-30.

## P4-02 design gate

**Status:** completed

- Independent pre-edit review returned `GO` for the fixed-five record design, commit ordering, bounded cleanup, selected-key JIT and the migration-only zero-profile owner.
- That design reviewer was closed after this verdict and will receive no P4-02 follow-up or later-row work; final P4-02 code review is reserved for one fresh read-only reviewer after the diff is stable.
**Started:** 2026-08-30 after P4-01 closure and visible-plan transition.

### Locked roadmap clauses and non-goals

- Replace the single shared private-key slot with opaque key records and bind each profile to an
  explicit key ID. Installed key bytes remain write-only and inaccessible to workspace/file tools,
  model context/results, authenticated read APIs, serial, logs, diagnostics and exports.
- Preserve the existing five-profile cap, password/passphrase NVS owner, 16 KiB validation ceiling,
  selected-profile runtime, manual Device/Web authority semantics and one live SSH session.
- Remove the installed legacy SD private-key file only after a complete NVS record and every
  required profile binding have been read back. A failed migration leaves the legacy key and old
  authority intact.
- Do not implement encryption-at-rest, a vault, credential/key manager, repository, codec framework,
  general transaction/recovery engine, profile UI redesign, model policy, project/chat ceilings,
  SFTP changes or P4-03 and later scope.

### Producer, consumer and persisted-state inventory

- Current installed-key authority is the single legacy SD private-key file; libssh2 authenticates
  it through its mounted internal path. The existing exact-owned Web key-upload staging file and
  Device import read an explicitly selected `.pem` or
  `.key` workspace source, then removes that source only after installation succeeds.
- Producers are `installSshPrivateKey()`, Device `installSshKeyFromWorkspace()` and authenticated
  Web `POST /api/ssh/key`. Consumers are `sshProfileIsComplete()`, Device terminal/SFTP,
  Web terminal/SFTP and model `ssh_command` through `loadSshProfile()` and
  `SshClient::authenticateControlled()`.
- Existing profile public fields are indexed `n/h/p/u/a`; password/passphrase secrets remain
  indexed `w/k`; P4-01 profile IDs remain `i0...i4`. P4-02 adds only indexed non-secret profile
  key references, five authoritative key-record slots and one fixed transient copy-on-write slot
  in the same existing SSH NVS owner.
- Device/Web profile CRUD currently materializes full profiles and must preserve the key reference
  during edit and indexed delete/shift. Web state returns only selected-profile installed/not-installed;
  it must not serialize a key ID or key bytes. Model/tool schemas remain unchanged.
- Boot owner is `initializeSshStorage()`. Cleanup runs only at boot and before key/profile mutation,
  scans exactly five profile references and five record slots, reads no unselected key bytes and
  removes only exact unreferenced/incomplete slot entries. There is no SD recovery file or growing
  scan. SD removal during legacy migration/upload fails explicitly and leaves committed authority
  unchanged; upload abort/session close retains existing exact-owned staging cleanup.
- Installed Arduino ESP32 `Preferences` 3.2.1 delegates blobs to `nvs_set_blob`/`nvs_get_blob` and
  commits each write. Espressif documents one data entry per 32 blob bytes plus two overhead entries,
  explicit not-enough-space/value-too-long outcomes and a maximum of the lesser of 508000 bytes or
  97.6% of partition size minus 4000 bytes. The project NVS partition is 0x5000, so P4 must report
  capacity failure and cannot promise five maximum-size keys. Installed libssh2 1.11.1_DEV provides
  `libssh2_userauth_publickey_frommemory()` and removes the need for an installed SD key file.

### Minimal design and state transitions

- Add one internal opaque non-zero `uint64_t` key ID to `SshProfile`; it is never added to
  `SshProfileSummary`, model/tool JSON or Web read state. Persist the per-profile reference as fixed
  indexed keys alongside the existing profile and move it with the profile on delete/shift.
- Store at most five authoritative records plus one fixed transient copy-on-write slot. Each slot has one small ID authority entry and one NVS
  blob containing the same ID followed by 64...16384 PEM bytes. A loader scans only the five small
  profile references and six fixed record IDs, then reads and validates exactly the selected record blob. Authentication zeroizes that one
  heap buffer immediately after libssh2 finishes or fails.
- Install snapshots the stable P4-01 profile ID, validates the staged PEM, finds one free fixed slot,
  writes/readbacks the new blob, writes/readbacks its opaque ID, then writes/readbacks only that
  profile's key reference. Only after the new binding is authoritative may the old unreferenced
  record be removed. NVS capacity failure occurs before binding and preserves the old record/ref.
- The sixth fixed slot exists only so replacement still uses immutable copy-on-write when five
  profiles already own five records; it is not another profile/key capacity or a dynamic registry.
  Crash before record ID commit leaves a fixed-slot blob orphan; crash after ID commit but before
  profile binding leaves a complete unreferenced record; crash after binding leaves the new authority
  and at most the old unreferenced record. The bounded boot/pre-mutation scan handles only these five
  profile references and six record slots. It does not read key values, choose authority, repair a referenced mismatch or expose IDs.
- Legacy migration reads the existing SD key once, creates/readbacks one record, binds existing
  profiles to that exact record without rewriting public fields, passwords or passphrases, verifies
  every binding, and only then removes the legacy SD private-key file. A partial reboot resumes from matching committed
  refs while the legacy file remains. If no profile exists, one migration-only non-secret pointer
  owns the imported record until the first created profile claims it; this is not a general marker
  or transaction API. Conflicting refs, malformed records, missing SD or storage exhaustion fail
  closed and retain the old file.
- Existing Web/Device upload controls are minimally rebound to the selected stable profile ID. The
  existing Web staging and cleanup lifecycle remains; no assets, routes, terminal backend or new UI
  surface are added in this row.

### Expected write set

- `firmware/CardputerAssistant/src/ssh_client.h`
- `firmware/CardputerAssistant/src/ssh_client.cpp`
- `firmware/CardputerAssistant/SshTools.ino`
- `firmware/CardputerAssistant/src/web_console.cpp`
- `.agents/P4_TRACEABILITY.md`
- No `pending_tool_call`, `tool_router`, policy, project/chat, Web asset/state/route, workspace/file,
  SFTP, known-host, partition-table or dependency change.

### Frozen minimal proof

| Observation | Required result |
| --- | --- |
| Record and binding | Two exact-owned profiles can hold distinct opaque records; selected-profile installed state and the reviewed JIT authentication call path resolve only the bound record and zeroize it after use; IDs/bytes are never emitted. First real Cardputer private-key auth E2E is owned by P4-17, not this row |
| Replace/full failure | Replacement switches only the target profile after verified record commit; a deliberately unstoreable bounded key returns an actionable NVS-capacity error and leaves the old ref/record usable |
| Legacy migration/reboot | The existing shared legacy SD private-key file becomes one explicitly bound record, the SD installed-key file disappears only after readback, saved authentication still works and reboot preserves refs |
| Crash/cleanup boundaries | Fixed-slot orphan states are removed at boot/pre-mutation; referenced missing/mismatched records fail closed; cleanup reads no key bytes and never scans beyond five profile references plus six fixed record slots |
| CRUD identity | Edit/select preserve key ref; indexed delete moves refs with profiles and removes only records no remaining profile owns |
| Exposure | Model/file/API/state/serial/log/diagnostic checks contain no key bytes, internal key ID or installed key path; only authenticated selected-profile installed boolean is readable |
| Resources | Exact-core build and focused COM8/Web checks measure NVS capacity, idle heap/stack and active-key SSH against retained baselines with no unexpected reset/freeze |

### First code-review verdict and active-row correction

- The fresh read-only P4-02 code reviewer returned `STOP`; all six findings are classified as
  active-row defects: two stale zero-argument installed-key call sites, ambiguous `cnt` authority
  during cleanup, missing stable-profile recheck at binding commit, an untyped unknown binding
  outcome, no replacement path when five records are authoritative, and installed-state validation
  that did not inspect record identity/PEM.
- The single correction patch must: validate `cnt` type/presence before any deletion; recheck the
  exact stable profile ID immediately before and after `qN` commit; classify binding as committed,
  unchanged or unknown; retain immutable replacement through the one bounded transient slot; and
  make selected installed-state use the same exact record validator as authentication. No build or
  runtime evidence may start until the same reviewer performs its one allowed blocker follow-up.
- The reviewer used its single blocker follow-up and returned `GO`: all six findings are resolved,
  the four-file production scope remains exact, and no key bytes or internal key IDs entered a
  schema, state, log or diagnostic. The reviewer was then closed and will not be reused.

### Evidence log

- Exact-core compilation passed with M5Stack ESP32 `3.2.1` and exact FQBN; the uploaded image is
  3,315,440 bytes with SHA-256 `C613E028604F950FA51AFC5D27F6F0D6D1CC6AED62509D740200322C4AFA52A7`.
- Post-upload idle status passed on COM8 with reset reason 1, free heap 122,600 bytes, largest block
  60,404 bytes, minimum heap 113,128 bytes and stack margin 7,876 bytes.
- The first focused saved-host SSH E2E stopped before authentication with `TCP connection to SSH
  host failed`; Web Console then emitted `result=stopped`. This is classified as an external
  remote-host/connectivity failure, not an active-row firmware defect, and the unchanged experiment
  will not be repeated. P4-02 authentication evidence must use a materially different exact-owned
  reachable local SSH target.
- The exact-owned alternate target used AsyncSSH 2.24.0 with two one-off RSA client keys and passed
  a host loopback public-key smoke. Through the authenticated CardMind WebUI, two collision-checked
  temporary profiles each reported an installed private key, held distinct non-secret references,
  and replacing the first profile's key did not change the second profile's installed state. The
  first profile was then restored to its original one-off key before cleanup. No key bytes, IDs,
  profile values or NVS payloads were printed or retained in evidence.
- A memory-only COM8 inspection used official `esptool` 4.8.1 and Espressif ESP-IDF v5.4.2
  `nvs_parser.py` (SHA-256
  `621BDBF0AC60E34AE190F63BE10F0F1A4DD4D18C25EBAB0CF56FF53A6F2B6C2C`). It wrote no raw flash
  file and returned only the aggregate observation: three profiles, two distinct fixture refs,
  both refs backed by bounded records, and unchanged metadata across reboot.
- Web cleanup selected each fixture only after an exact name check, deleted B then A through the
  existing confirmation, restored the original selection at index zero, and reloaded to observe
  exactly one original profile with both fixture names absent. A post-delete boot/memory inspection
  then observed one profile, no deleted fixture refs, zero orphan records and stable metadata across
  reboot. The first post-cleanup disposable check emitted only an over-redacted generic failure;
  after changing only that harness to expose a fixed assertion code, the materially informative run
  passed. No production correction followed the harness-only failure.
- The final attempt to obtain real device private-key authentication stopped before creating a new
  fixture because WebUI reported the microSD absent. Focused COM8 `status` independently confirmed
  `microsd=unavailable`, `microsd_state=missing`, Wi-Fi connected, reset reason 11, free heap 134,140
  bytes, largest block 67,572 bytes, minimum heap 117,736 bytes and stack margin 9,440 bytes. This is
  classified as an external hardware/lifecycle blocker; P4-02 remains `in_progress`, production is
  unchanged, and authentication will resume only after the card is physically available.
- Exact-owned cleanup stopped the local SSH server and removed both temporary keys, the temporary
  Python environment, official parser copy, stop/ready markers and all disposable server/Web/NVS
  scripts. One key initially resisted deletion because its smoke-test ACL granted read-only access;
  the owning user restored FullControl only on that exact file via DACL, and the final enumerated
  cleanup check reported zero remaining items. Elapsed for this resumed evidence/cleanup pass was
  approximately 45 minutes; the root-cause pivot was from unavailable saved-host transport to a
  local target, followed by an external SD-state stop rather than further firmware edits.
- On resumption, a fresh independent COM8 `status` again reported `microsd=unavailable`,
  `microsd_state=missing` and `micro_sd_required` while Wi-Fi and the rest of the runtime remained
  responsive. Source inspection found the existing `sd-mount` diagnostic performs six `SD.begin`
  attempts, but its success path also invokes `deleteRecordingIfPresent()`. The platform therefore
  rejected that action as potentially destructive; the command did not start and no storage state
  changed. The exact remote observation is persistent card mount/detection failure, not a stale
  Web state or replacement-confirmation state. Physical power-off, microSD reseat and power-on are
  required to distinguish absence/contact failure from a failed card before P4-02 authentication
  evidence can resume; production remains unchanged.
- After the user fully powered off, reseated the same card and powered on, the first read-only COM8
  `status` passed with `microsd=ready`, `microsd_state=ready`, no storage error, chats/files/crash
  journal available, two retained chats and two retained history entries. Reset reason was 1; free
  heap was 122,764 bytes, largest block 60,404 bytes, minimum heap 111,392 bytes and stack margin
  7,876 bytes. No software remount, format or replacement confirmation ran, so recovery used only
  the normal power-on path and preserved the prior card contents.
- The resumed auth-test inventory confirmed that Web unknown-host acceptance persistently calls
  `trustSshHost()`, Web has no exact-host forget route, and profile deletion does not remove a
  known-host entry. The existing automatic primitive `forgetTrustedSshHost(host, port)` is exposed
  only by Device UI and a separate fixed demo diagnostic. Under the user's new rule, a test must
  restore device state automatically after success or failure and may not use a path that can leave
  SD unmounted or touch non-exact-owned data. Therefore no new profile, key or trust entry was
  created while Architect classifies whether the auth E2E belongs to later Web/security integration
  or whether a minimal exact-host cleanup control belongs to a specific current P4 row.
- Architect selected the no-expansion ownership boundary: P4-02 does not add a forget route/control
  or modify known-host behavior. This row owns opaque records, exact profile-to-key binding,
  selected-record JIT load/zeroization in the reviewed authentication call path, immutable
  replacement with preservation on capacity failure, reboot persistence, bounded orphan cleanup
  and non-addressability. The first real Cardputer private-key authentication E2E is explicitly not
  yet proven and belongs to P4-17. P4-17 may add only one authenticated CSRF-protected forget action
  for the exact selected profile, resolving host/port server-side through the existing
  `forgetTrustedSshHost()`; it may not accept a free host, list/clear trust state or create a manager.
  P4-20 owns mismatch blocking and proof that cleanup removes only the exact test-host entry while
  unrelated known-host entries remain unchanged. P4-21 repeats only final regression; a later E2E
  key/JIT defect reopens P4-02 by ownership.
- The resumed capacity scenario used the existing authenticated local Web/API path with one
  collision-checked exact-owned profile. A 1,675-byte RSA PEM first installed successfully. A
  16,384-byte PEM replacement then returned the explicit NVS-capacity rejection while the same
  fixture remained selected, the two-profile public inventory remained unchanged and
  `ssh_key_installed` remained true. The authenticated state exposed neither a private-key record
  ID nor key bytes. This directly proves preservation of the previous binding on capacity failure;
  it does not claim real Cardputer private-key authentication, which remains owned by P4-17.
- Cleanup revalidated the exact fixture name, host, port, user and auth mode before deletion, then
  ran the same absence/restoration check twice. It restored the original one-profile inventory and
  selected index, zeroized both local key buffers, removed only the exact temporary directory, and
  observed SD `ready`, readable and writable with no storage error. The retained two chats/history
  entries were present before the scenario. The same serial holder then received `EXIT` and
  reported `WEB_CONSOLE result=stopped`; no manual recovery or SD lifecycle mutation was needed.
- The functional capacity proof exposed an active-row resource blocker. During the active Web
  scenario free heap was 97,916 bytes, largest block 32,756 bytes, stack margin 5,604 bytes and
  reset reason 1, but minimum heap fell to 57,460 bytes, below the required 70-KiB floor. Source
  ownership is P4-02: `createSshPrivateKeyRecord()` retains the full input record while allocating
  a same-sized readback vector. P4-02 therefore remains `in_progress`; no unchanged experiment will
  be repeated and no later row absorbs this defect.

### Active resource correction gate

- A fresh read-only resource/design reviewer returned `GO` with no blockers. The pinned
  Arduino-ESP32 `Preferences` byte-count contract permits readback into the existing buffer, and
  the installed mbedTLS 3 API provides `mbedtls_sha256()`. Hashing after ID encoding, then checking
  exact length/read count, embedded ID and SHA-256 while wiping digest temporaries preserves the
  existing blob-to-ID-to-binding ordering. The reviewer was closed and will not be reused.
- Frozen minimal change: keep the existing fixed-slot/COW/binding contracts, compute a
  collision-resistant digest of the prepared record, reuse that same secret buffer for NVS
  readback, compare exact length/identity/digest, and preserve all current failure ordering and
  zeroization. Do not change routes, schemas, record layout, slot count, cleanup or authentication.
- Frozen proof after review and one coherent patch: cheap compile/static checks, one exact pinned
  build/upload, then the same single exact-owned small-key plus 16,384-byte capacity-rejection
  scenario with automatic cleanup. It must preserve the old binding, expose no IDs/bytes, keep SD
  ready, avoid reset/freeze and retain at least 70 KiB minimum heap.
- The primary implementation changed only `ssh_client.cpp`: the prepared record is SHA-256 hashed,
  zeroized and reused for complete NVS readback; exact byte count, embedded ID and digest must match.
  `git diff --check` passed. The exact pinned 3.2.1 build passed at 3,315,454 sketch bytes and
  65,612 global bytes; the uploaded image is 3,315,648 bytes with SHA-256
  `BA5853480EE66002ECA10F0945AC024390269528C8E77742C7332F9401ED3240`, and flash verification passed.
- The first post-upload COM8 status reported SD ready with the retained two chats and two history
  entries, reset reason 1, free heap 122,792 bytes, largest block 60,404 bytes, minimum heap 110,980
  bytes and stack margin 7,876 bytes. The focused automatic Web/API rerun then observed explicit
  16,384-byte NVS-capacity rejection, unchanged profile inventory, the previous binding still
  installed and no internal key ID in authenticated state. During that changed boundary free heap
  was 98,804 bytes, largest block 35,828 bytes, minimum heap 74,880 bytes and stack margin 5,588
  bytes; reset reason stayed 1 and SD stayed ready with no error. This raises the measured minimum
  by 17,420 bytes and passes the 70-KiB floor.
- The rerun's `finally` path revalidated exact fixture fields, restored the original one-profile
  inventory and selection, repeated the cleanup as an idempotent no-op, zeroized local key and
  multipart buffers, and left no temporary file. The same serial holder then reported
  `WEB_CONSOLE result=stopped`. Resource-correction elapsed time was about 15 minutes, from the
  20:20 failure classification through the 20:35 completed device cleanup. Final P4-02 closure is
  waiting only for one fresh read-only code review of the stable diff.
- The fresh final code reviewer returned `STOP` with two concrete findings outside the accepted
  SHA-256 buffer-reuse correction. First, indexed profile deletion can reset between separately
  committed public-field, `qN` and `iN` shifts and leave a valid-looking profile paired with the
  previous index's key. Second, indexed removal ignores `Preferences::remove()` failures and can
  report success while an inactive `qN` survives and blocks later initialization. Both findings are
  owned by P4-02 because the unsafe state is the newly added key-reference authority; P4-01 remains
  complete. No production edit or repeated runtime test followed this verdict yet. The reviewer is
  retained only for its one allowed combined follow-up after both blockers are corrected.

### 60-minute closing pivot

**Started:** 2026-08-30 20:52:08 +03:00. **Timebox:** 30 minutes overall, with at most
10 minutes to prove that the existing fields permit fail-closed ordering.

The bounded ordering is possible without reopening P4-01 or adding storage state:

| Reset point in existing indexed delete | Durable observation while old `cnt` remains authoritative |
| --- | --- |
| Before the first write | Original profile, `iN` and `qN` bindings remain valid |
| After destination `iN` takes source `iN`, before/during shifted public and `qN` writes | The source index still owns the same non-zero ID, so duplicate-ID validation fails closed |
| During checked tail removal | The duplicate remains until the tail's required public field is absent; after the tail ID is removed, that missing public field keeps old-`cnt` loading fail closed |
| Last-index deletion with no shift | Remove the tail's required public field first, so old-`cnt` loading fails closed before `qN` or `iN` can become stale |
| After selected-index write, before count write | Old `cnt` still observes the duplicate ID or incomplete tail and fails closed |
| After verified count write | Only the new active prefix is authoritative; each profile, stable ID and key reference is the intended shifted tuple |

- Frozen correction: specialize only the existing indexed-delete write order, check every indexed
  removal, and require every inactive `qN` and `iN` to be absent before reporting success.
- Expected production write set: `firmware/CardputerAssistant/src/ssh_client.cpp` only.
- Forbidden: new schema, public blob, slot indirection, transaction/COW/recovery layer, P4-01
  redesign, adjacent cleanup, helper hunting or another broad inventory pass.
- Frozen proof: cheap static/host checks, the retained reviewer's one combined follow-up, then one
  exact pinned build/upload and the smallest existing delete/shift/cleanup Device boundary. Reuse
  unchanged capacity, resource and reboot evidence; do not rerun the 16-KiB capacity scenario.
- `git diff --check` passed, and the correction added no production file outside
  `ssh_client.cpp`. The retained reviewer used its one combined follow-up and returned `GO`:
  destination-ID duplication remains until required tail metadata is absent, name-first checked
  removal makes last-index deletion fail closed, selection precedes the final count authority
  switch, and final verification rejects any inactive `iN` or `qN`. The reviewer was then closed
  and will not be reused.
- The single pinned closing build passed with exact FQBN
  `m5stack:esp32:m5stack_cardputer:FlashSize=8M,PartitionScheme=custom`, the unique resolved M5Stack
  core `3.2.1`, 3,318,750 sketch bytes and 65,612 global bytes. The uploaded image is 3,318,944
  bytes with SHA-256 `1D26C08CBDC16889DE116091C66E0F02FFF745CC75B7EB1CEAD97FF0F13FF627`;
  COM8 upload verified the flash hash.
- The existing exact-owned `SSHPROFILETEST` delete/shift/cleanup boundary returned
  `result=pass`. Because each successful delete now includes final inactive-identity verification,
  that runtime pass covers shifted profile/key binding, checked removal and absence of inactive
  `iN/qN`; its retained cleanup/restoration path completed. Post-test status remained responsive
  with SD ready, two retained chats and two history entries, reset reason 1, free heap 122,632
  bytes, largest block 58,356 bytes, minimum heap 111,264 bytes and stack margin 7,812 bytes.
- The disposable serial holder then exited non-zero only because it expected a nonexistent
  `STATUS result=...` suffix after already receiving the complete valid `STATUS` line. This is a
  harness matcher defect, not a firmware failure; no unchanged Device scenario was repeated.
- Unchanged capacity-failure preservation, bounded cleanup, reboot persistence, selected-record
  JIT/zeroization, non-addressability and resource evidence above is reused. The 16-KiB capacity
  scenario was not repeated. Real Cardputer private-key authentication remains explicitly unproven
  here and owned by P4-17. P4-02 is complete; P4-03 is now the sole active row.

### Forbidden effects

- No key record list/editor, user-defined key labels, arbitrary record count, dynamic key registry,
  general cleanup engine, background executor, retry framework or encrypted-at-rest claim.
- No password/passphrase migration, no key bytes on an installed workspace path, no key-ID schema for
  the model, no read/download/export route and no fallback to the legacy SD key after migration commits.
- No deletion of the old key/file before the new record and exact profile bindings are authoritative;
  no ordinary failure result after a binding was confirmed committed.

## P4-03 design gate

**Status:** completed
**Started:** 2026-08-30 21:12:22 +03:00.

### Locked clause and non-goals

- ROADMAP lines 473-475 require every arbitrary model-issued `ssh_command` to remain mutating;
  read authority belongs only to exact fixed reviewed Safe Actions, never inferred from shell text,
  a prefix or a regular expression.
- P4-03 does not implement Safe Actions, SFTP, timeout/output changes, a shell classifier, a command
  parser, another schema, or any Device/Web manual-terminal authority change. P4-08 owns the fixed
  read-authorized actions.

### Existing contract inventory

- `ToolCapability` already persists separate `SshRead` (`sr`) and `SshMutate` (`sm`) policy slots.
  This row changes no policy format, NVS/SD state, migration, reboot, crash or cleanup owner.
- The sole arbitrary shell schema is the canonical `SshCommand`/`ssh_command` catalog row, bound
  directly to `SshMutate`. `buildToolRequestPlan()` takes eligibility only from that catalog
  capability; `resolveChatToolPermissions()` marks `SshRead` unavailable and exposes
  `SshMutate` only when the selected SSH authority is available.
- API schema construction exposes exactly one UTF-8 `command` field. Pending-call normalization
  checks only exact shape, UTF-8 and the retained 1,024-byte bound; it performs no safety parsing.
  Every `SshCommand` is mandatory-confirmation even when policy resolves to Allow, and pending
  execution rechecks selected SSH authority identity.
- `tool_router` dispatches the canonical schema through the existing audited/cancelable
  `executeSshTool()` path. Device `runSshTerminal()` and the existing Web terminal are direct user
  sessions and do not enter model request-plan classification.
- Existing host coverage asserts the catalog maps `SshCommand` to `SshMutate`, mutate eligibility
  exposes the schema, and Allow on `SshRead` with unavailable `SshMutate` exposes no arbitrary SSH
  schema. No vendor API or external standard participates in this application policy decision.

### Minimal design and frozen proof

- Expected repository write set is this traceability entry only. If the existing checks pass, no
  production or test edit is permitted: retaining the exact current mapping is the minimal design.
- After independent design GO, run the existing strict host test once. Required observations are:
  catalog identity remains `SshCommand -> SshMutate`; read-only SSH authority leaves the arbitrary
  schema absent; arbitrary command preview/canonicalization preserves bytes without classifying
  text; mandatory confirmation and audited/cancelable dispatch remain intact.
- A fresh independent read-only closure reviewer then checks the stable no-code boundary and host
  evidence. Device build/upload/Web tests are not useful for an unchanged pure policy/catalog row.
- Independent read-only design review returned `GO` with no blockers. It confirmed that the sole
  catalog schema is bound to `SshMutate`, `SshRead` is unavailable to model requests, argument
  handling performs no safety inference, and mandatory confirmation, authority identity, audit and
  cancellation remain intact. The reviewer accepted the frozen existing host proof and was closed.
- The first WSL host-test invocation was rejected before compilation because the disposable runner
  passed an empty output filename through shell quoting. The changed runner used a fixed exact-owned
  `/tmp` ELF with automatic `trap` cleanup; the strict `-std=c++17 -Wall -Wextra -Werror` suite then
  returned `host_tests: PASS`. This is harness-only evidence and caused no repository change.
- P4-03 has no production or test diff. The passing suite directly covers catalog
  `SshCommand -> SshMutate`, mutate-only schema eligibility, read-only exclusion, mandatory
  confirmation and byte-preserving arbitrary-command preview. A fresh independent read-only
  closure verdict remains before the row can complete.
- A fresh independent read-only closure reviewer returned `GO` with no blockers and was closed.
  P4-03 completed at 2026-08-30 21:24:34 +03:00 after about 12 minutes. No production, test,
  persistence, Device or Web change was made; P4-04 is now the sole active row.

### Forbidden effects

- No regex, prefix, token, AST or shell-content safety inference.
- No assignment of `ssh_command` to `SshRead`, no bypass of Off/Ask/global/project/chat ceilings,
  confirmation, preview, authority identity, audit or cancel.
- No model policy applied to explicit manual Device/Web terminals and no P4-08 implementation.
