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
| P4-04 | Configurable model SSH command timeout/output policy while retaining the existing 1,024-byte direct command cap | Timeout/output policy is explicit; the user-removed 8 KiB increase is not implemented | completed |
| P4-05 | Single-pass streamed SD command log, bounded model summary/reference, cancellation, and downloadable output | Output is written once while streaming to SD; the model receives only a bounded summary/reference; a long foreground command cancels and its log downloads without background execution | completed |
| P4-06 | Paged model SFTP list/read/write/move through existing `SftpReadWrite` and Phase 3 permission/confirmation boundaries | Model listing is bounded/paged; `Ask` confirms every model SFTP call; `Allow` still confirms overwrite/delete/move onto an existing target; overwrite defaults deny and no prior remote target is truncated/deleted before confirmed safe replacement; manual Device/Web SFTP remains direct-user authority | completed |
| P4-07 | Existing streaming CardMind workspace transfer to/from selected remote host | Both directions preserve workspace policy, total foreground deadline and cooperative cancel; overwrite defaults deny and replacement never destroys the prior target before confirmation | completed |
| P4-08 | Small fixed built-in Safe Actions set for logs, service state, containers, disk and processes | Reviewed fixed actions obey existing Off/Ask/ceiling/host-key/timeout/audit boundaries; no presets, editor, macros, persistence schema or action framework | completed |
| P4-09 | Removed by user: Web terminal tabs; existing single terminal remains | Scope closed by explicit user decision; not implemented | removed_by_user |
| P4-10 | Removed by user: configurable Device terminal-history rotation and new viewer; existing terminal.log/old.log remains | Scope closed by explicit user decision; not implemented | removed_by_user |
| P4-11 | Existing bounded known_hosts store with unconditional host-key-change block | Every mismatch blocks connection; user-removed pagination/rotation is not implemented | completed |
| P4-12 | Removed by user: separate SSH profile diagnostics feature | Scope closed by explicit user decision; not implemented | removed_by_user |
| P4-13 | Encrypted-at-rest evaluation and physical-access threat-model documentation only | No encrypted vault is implemented in P4; documentation states measured limitations and makes no unsupported encryption claim | completed |
| P4-14 | Project/chat ceilings bound to immutable opaque profile IDs | Authenticated config/project metadata may carry the ID; project/chat selection only narrows hosts and cannot exceed global authority or redirect stale authority | completed |
| P4-15 | Credential/private-key/profile-ID non-addressability across model, file tools, API, logs, serial, and diagnostics | Model SSH never returns credential/private-key bytes, private-key path, or internal profile ID; authenticated config APIs expose only allowed non-secret ID/summary data | completed |
| P4-16 | Consolidated Device Phase 4 journey for profile/security plus required command/SFTP/transfer controls using existing terminal/history | Required Device controls and acceptance are observable without a separate journey subsystem | completed |
| P4-17 | Consolidated Web Phase 4 journey for profile/security plus required command/SFTP/transfer controls using the existing single terminal | Required Web controls and authenticated public state are observable without terminal tabs, SSH connection/trust mutation or a separate journey subsystem; the real project/chat ceiling controls emit the P4-14 encoded header and display their saved/effective state; the mismatch-only forget control is hidden in ordinary state and resolves host/port server-side | completed |
| P4-18 | Removed by user as a separate subsystem: Device command/SFTP/transfer journey | Required controls moved to P4-16; no separate implementation | removed_by_user |
| P4-19 | Removed by user as a separate subsystem: Web command/SFTP/transfer journey | Required controls moved to P4-17; no separate implementation | removed_by_user |
| P4-20 | Changed-boundary-only recovery and security acceptance | Compose the observed real reachable-profile Cardputer private-key connection with completed P4-02/P4-11/P4-17 key, mismatch-before-auth, canonical exact-host rewrite and authenticated selected-host forget evidence; retain the exact-owned failed-endpoint lifecycle/cleanup evidence and explicit Phase 9 ownership of the unreachable rotatable-endpoint runtime; broad re-certification of unchanged NVS/SD was removed by user | completed |
| P4-21 | Phase closure: documentation, focused/full regression, performance/resources, cleanup, independent review, CI/PR/merge | Docs/threat model/licenses/secret scans and exact Device/Web regressions pass without repeating P4-17/P4-20-specific scenarios; idle/general mode retains the 70 KiB floor; final-image SSH passes the fragmented functional sequence without reset/freeze, while the platform-unobservable post-fix numeric active-session sample is removed from acceptance and retained as a user-accepted residual with no inferred performance claim; SD ownership and exact cleanup are evidenced; final review has no blockers; green CI and reviewed PR merge only to `develop` with `main` and stash unchanged | completed |

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
| Record and binding | Two exact-owned profiles can hold distinct opaque records; selected-profile installed state and the reviewed JIT authentication call path resolve only the bound record and zeroize it after use; IDs/bytes are never emitted. First real Cardputer private-key auth E2E is owned by P4-20 with its exact known_hosts cleanup, not this row |
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
  yet proven and belongs to P4-20. P4-17 may add only one authenticated CSRF-protected forget action
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
  it does not claim real Cardputer private-key authentication, which remains owned by P4-20.
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
  here and owned by P4-20. P4-02 is complete; P4-03 is now the sole active row.

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

## P4-04 design gate

**Status:** completed after Architect personal closure GO
**Started:** 2026-08-30 21:24:34 +03:00.

### Second approved reopen gate (2026-08-31)

- P4-08 A/B/C proved canonical project/chat load and pending save/load/clear both
  before and after direct SSH execution. Only `loadPendingToolPreview` failed:
  `readCanonicalStringArgument` still limits canonical field names to 16 bytes,
  while P4-04 introduced the 23-byte `max_inline_output_bytes` key.
- Frozen correction: in `pending_tool_call.cpp`, replace only that stale literal
  with one narrowly named local `constexpr` equal to
  `sizeof("max_inline_output_bytes") - 1`. Do not change value limits, the generic
  JSON reader, the 64-byte SD metadata limit, schemas, serializers,
  project/chat storage, policy, executor, preview content or architecture.
- Focused host/static proof: canonical P4-04 command/action objects containing
  `timeout_ms` and `max_inline_output_bytes` extract `command`/`action`, retain
  current value bounds, and reject a key above the exact 23-byte ceiling.
- Focused device proof: on one exact-owned canonical project/chat, Ask must pass
  save/load and preview, reach `ask_execute`, execute once, produce the expected
  audit record, clear exact pending/output, restore selection/inventories, and
  leave the device responsive. One exact-core build/upload is permitted.
- P4-08 is paused while this owner is active. Its production and disposable A/B/C
  diagnostic hunks remain frozen and must not enter the P4-04 correction commit.
- The first post-upload Device attempt stopped before fixture creation or the changed
  boundary because its disposable runner used one initial `PING` instead of the
  project's bounded three-attempt serial synchronization. No ledger, project, chat,
  pending call or output was created, and both disposable files were removed. This is
  a harness-readiness failure; the material correction is limited to reusing the
  existing bounded serial handshake with no rebuild or upload.
- The corrected runner then reused the established CRLF and three-attempt `PING`
  handshake but COM8 still produced no `PONG`. It again stopped before Web Console,
  fixture creation or the changed boundary; no ledger or exact-owned/user state was
  created and the disposable files were removed. This is an external device-readiness
  blocker. Further unchanged serial attempts are stopped pending Architect direction;
  production, build output and the frozen P4-08 diff remain unchanged.
- Architect authorized one non-flashing recovery boundary. The project-local
  `m5stack/tools/esptool_py/4.9.dev3` executable completed `--no-stub chip_id`
  against ESP32-S3 and issued the vendor `hard_reset` without writing or erasing
  flash. COM8 then reappeared in the port-name inventory, but the immediate passive
  capture could not open it (`Access to the path 'COM8' is denied`). No boot, panic or
  dispatcher observation and no fixture mutation followed. The exact external boundary
  was reported and no reset/probe/serial retry was made.
- Fresh independent read-only code review returned `GO` with no correction blockers:
  the exact 23-byte local ceiling is used by both production preview fields, the new
  host cases cover command/action and reject 24 bytes, existing value-bound evidence
  remains intact, and P4-08 was explicitly excluded. The reviewer was closed. Its sole
  residual risk is the still-blocked real-device Ask preview observation.
- After Architect independently proved that the re-enumeration lock had cleared, the
  one authorized focused proof opened COM8 successfully without another reset, build
  or upload. The established three-attempt readiness handshake nevertheless received
  no `PONG`. The run stopped before Web Console and before ledger/fixture creation; no
  project, chat, pending call, output or user state changed, and the disposable files
  were removed. The new exact external boundary is an open serial port with no
  application-dispatcher response; no repeat or production mutation followed.
- The final authorized automatic recovery used the project-local esptool with
  `--after soft_reset --no-stub chip_id`; ROM communication and soft reset completed
  without flash write/erase. The subsequent open/capture/handshake process exceeded
  its calculated bounded window and emitted no BOOT/FATAL/panic marker or `PONG`; it
  was interrupted without a repeat. Because re-enumeration/open backoff, passive
  capture and read waits were bounded, the exact residual transport boundary is an OS
  SerialPort open-or-write call that did not return; the disposable probe had no
  sub-stage marker capable of distinguishing those two calls. No Web Console,
  credential, ledger, fixture, pending call or output was created.
- The user then completed the required physical power-cycle. After restoring the
  canonical visible plan, the single authorized read-only check opened COM8 and sent
  the established three bounded `PING` attempts with a one-second write timeout, but
  received no `PONG`; `STATUS` was therefore not sent. No reset, build, upload, Web
  Console, fixture or data mutation occurred. Normal application-dispatcher readiness
  remains externally blocked after the physical reboot, so the focused Ask proof and
  correction commit remain gated.
- The user directly observed a live normal display with the main cards after that boot.
  This proves that setup completed and the main UI loop is running, rejects the prior
  early-startup/FATAL hypothesis, and narrows the missing `PONG` to the application
  HWCDC/USB-serial transport boundary. The earlier hard-reset, soft-reset and physical
  power-cycle escalation was not required for acceptance and violated the user's Device
  test-safety boundary. It must not be repeated or extended; P4-04 remains frozen for a
  proof redesign that performs no further Device recovery action.
- The Architect's proposed runtime-preview waiver was explicitly superseded before
  commit or closure. Ordinary independent Device/Web/HTTP/Browser actions and the
  existing authorized credential path remain allowed; only a recovery-escalation chain
  after readiness loss is prohibited. P4-04 therefore remains `in_progress` for one
  minimal authenticated Web Console proof through existing boundaries: exact-owned
  project/chat, pending preview, one approval/execution/audit observation, exact cleanup
  and restoration. That proof must not use reset, rebuild/reupload, power/card action or
  another serial-readiness probe. P4-08 production and diagnostic hunks remain frozen.
- The single direct in-app Browser navigation to the established local Web Console
  address returned `ERR_CONNECTION_REFUSED` before authentication, fixture creation or
  any CardMind state change. The HTTP handler is therefore not active. Starting the
  existing console now requires either the terminated serial test path or manual Device
  UI participation; there is no existing permitted automated Web entry point. Per the
  superseding decision, the Web proof stopped at this exact boundary without retry,
  recovery action or automatic waiver. P4-04 remains `in_progress` and P4-08 remains
  frozen.
- Architect closure review returned `STOP`: the retained host cases duplicated the
  23-byte field-name ceiling and called generic `json_reader::readObjectStringField`
  directly, while the host link did not include `pending_tool_call.cpp`. They therefore
  could remain green if the production preview boundary regressed to 16 bytes.
- Architect-reviewed minimal correction: move the existing bounded canonical reader and
  exact 23-byte `readCanonicalStringArgument` into the already host-linked
  `pending_tool_preview.h/.cpp` as one exported pure helper. The three P4-04-owned
  production preview extraction sites (read name, write content and SSH command) and the
  focused host cases use that same helper. Command/action canonical objects with
  `max_inline_output_bytes` must parse and a 24-byte field name must fail. The frozen
  P4-08 Safe Action worktree consumer already resolves through the helper, but it is not
  P4-04 evidence or commit ownership. No schema, storage, policy, router, SSH behavior or
  P4-08 change is permitted.
- Correction evidence: `git diff --check` and strict static ownership checks passed with
  one exported definition, three P4-04-owned production preview callsites, three focused
  host calls, no local reader in `pending_tool_call.cpp`, and no duplicated ceiling in
  the tests. The fourth current worktree callsite is the excluded frozen P4-08 Safe Action
  consumer and is not counted as P4-04 evidence.
  The CI-equivalent WSL C++17 `-Wall -Wextra -Werror` host link, including
  `pending_tool_preview.cpp`, passed and its exact-owned temporary ELF was removed.
- One exact pinned compile passed with the required Cardputer FQBN and the only resolved
  M5Stack ESP32 core at `3.2.1`: sketch 3,436,814 bytes and global RAM 65,628 bytes.
  The 3,437,008-byte binary has SHA-256
  `7A3D04E8AB8A5555A1D537D1830D5C2DAA02EA3430A99769786919026DE9144E`.
  No upload, COM8, Web/HTTP/Browser, fixture or Device action occurred. P4-04 remains
  under closure review; P4-08 remains frozen.
- Architect personally reviewed the actual row-owned correction, all three committed
  preview consumers, generic-reader semantics, host linkage/test invocation, trace
  ownership, resources and cleanup, and returned explicit closure `GO`. The accepted
  basis is the single exact 23-byte helper in `pending_tool_preview`, three P4-04
  consumers using it, retained tests invoking that production helper and rejecting a
  field above 23 bytes, strict host/static success, exact pinned compile success and
  unchanged 65,628-byte global RAM. No schema, storage, policy, executor or Device
  behavior was added.
- Explicit residual risk: post-fix `loadPendingToolPreview` was not observed on the
  Cardputer because the separately owned HWCDC path lost readiness and the Web handler
  was inactive. Pre-fix Device A/B/C localized the exact helper boundary; the direct
  production-helper host test plus firmware compile is accepted as proportional closure
  evidence. No fixture existed and the exact-owned temporary host ELF was removed.
- Before isolated staging, the frozen P4-08-only tracked patch relative to
  `f439ff09b1c6dfa41c22fb9d137e047a5ae69eaf` had SHA-256
  `76B83FBEB2D2694057617AE8ABCC7C3BBE3678B2B651546C53494C40AC369227`
  over 60,842 normalized bytes. It remains outside P4-04 ownership.

**Completed:** 2026-08-31 10:49:14 +03:00.

### Approved reopen gate

- Architect approved reopening only because installed ESP32 core `3.2.1`
  `String::indexOf('\0')` resolves through `strchr` and therefore matches the terminating NUL for
  every non-empty `String`; the observed literal `ls -lR /pub` was rejected before channel open.
- The correction is limited to length-aware embedded-NUL validation on ArduinoJson `7.2.1`
  `JsonString` before constructing `String`, removal of the impossible lower-layer NUL predicate,
  and focused normal-command/embedded-NUL evidence. Timeout, output policy, direct command cap and
  all P4-05 behavior remain unchanged.
- Frozen proof: an ordinary non-empty command parses and reaches execution; a JSON command
  containing `\u0000` fails before connection; existing option defaults/bounds/unknown-field and
  overflow-clear observations still pass. Forbidden effects are any timeout/output-policy change,
  P4-05 implementation change, persistence/UI change or broader validation rewrite.

### Locked clauses and observable contract

- Model-issued `ssh_command` gains only optional `timeout_ms` (`1,000..60,000`, default `60,000`)
  and `max_inline_output_bytes` (`1..16,384`, default `16,384`). The command remains bounded to
  1,024 bytes. Unknown fields, wrong JSON types and out-of-range values fail before connection.
- `timeout_ms` is one total deadline starting immediately before connection and covering connect,
  host-key lookup, authentication and command completion. User cancellation remains a distinct
  canceled outcome; deadline expiry is an explicit failed outcome.
- `max_inline_output_bytes` is the combined stdout/stderr cap returned to the model. P4-04 keeps one
  bounded inline buffer; overflow fails explicitly and returns no partial output. P4-05 alone owns
  one-pass SD logging and bounded summary/reference behavior.
- Values are per call. There is no Settings/storage/migration owner and no Device/Web control.
  Manual Device/Web terminal behavior is unchanged.

### Producer, consumer and failure inventory

- `api_client.cpp` builds the model schema. `pending_tool_call.cpp` validates exact allowed fields,
  normalizes omitted values to explicit defaults and binds the full canonical object to mandatory
  confirmation. Preview continues to show the exact command bytes.
- `ssh_tool.cpp` performs the independent runtime parse, loads only the selected profile, starts the
  total clock immediately before `connectControlled()`, and reuses one latched callback across
  connect/authenticate/execute. One first-observed terminal state is latched as `None`,
  `UserCancelled` or `TimedOut`; user cancellation wins if both are first observed in the same poll.
  The state is polled after every controlled stage returns regardless of that stage's result and
  immediately after host-key lookup before interpreting its result. Only `UserCancelled` maps to a
  canceled outcome, `TimedOut` maps to explicit failure, and a lower-stage error is preserved only
  while the terminal state remains `None`.
- `ssh_client.cpp` already combines stdout and stderr into one buffer and receives explicit timeout
  and output limits. The only lower-boundary correction is to use the row-owned combined-size check,
  clear the partial buffer before returning overflow, and report the actual configured cap.
- There is no persistence, reboot recovery, SD owner, crash replay, cleanup fixture or vendor API
  change. libssh2 and WiFiClient continue through the existing controlled operations.

### Minimal design and expected write set

- Add one small `ssh_command_options.h` contract with constants and pure validation/deadline/output-
  fit helpers. This is not a general execution-budget or output framework.
- Update only `api_client.cpp`, `pending_tool_call.cpp`, `ssh_tool.h/.cpp`, `ssh_client.cpp`, focused
  `tests/host_tests.cpp`, and, only for a small no-network parser/default/bounds Device observation,
  the existing `CardputerAssistant.ino`, `SshTools.ino` and `SerialDiagnostics.ino` diagnostic path.
- The exact write set for the second preview-reader correction is
  `pending_tool_preview.h/.cpp`, `pending_tool_call.cpp`, focused `tests/host_tests.cpp`,
  and this trace. P4-08 hunks in shared files remain frozen and excluded.
- The row-owned commit also includes this trace update. It includes no P4-05 behavior or generated
  build/test artifacts.

### Frozen minimal proof

| Observation | Required result |
| --- | --- |
| Defaults and exact shape | Command-only input normalizes to 60,000/16,384; optional fields may appear independently; unknown/mistyped fields fail |
| Bounds | 1,000 and 60,000 ms plus 1 and 16,384 bytes pass; adjacent out-of-range values fail before any connection |
| Total deadline | One wrap-safe elapsed clock is shared across all model SSH stages; deadline expiry is failure, user cancellation remains canceled |
| Inline output | Combined stdout/stderr at the cap succeeds; the first byte beyond it clears partial output and fails with the configured limit |
| Interfaces/non-goals | Model schema documents both fields/defaults/bounds; command preview/mandatory confirmation bind normalized arguments; no Settings/UI/manual-terminal/P4-05 change |

- After design GO: strict host tests first, then one fresh read-only code review. After code-review GO,
  run one exact pinned build/upload and the smallest existing serial diagnostic exercising the same
  parser/default/bounds/deadline/output-fit helpers without network, profile or SD mutation; record
  idle resource/status evidence. No remote fixture or broad SSH regression is required for this
  non-persistent parameter-boundary row.

### Forbidden effects

- No persisted maximum, Device/Web control, generic budget abstraction, dynamic inline allocation
  above 16,384 bytes, truncation-success mode, SD log/reference, background work or retry.
- No change to manual terminal/SFTP authority, timeout or transport behavior; no command-length
  increase; no return of partial stdout/stderr after overflow.

### Design-review blocker and bounded correction

- The independent reviewer returned `STOP` on the initial callback-only wording because blocking
  TCP connect, channel/read timeout or host-key lookup can return after the total deadline without
  polling the callback, allowing a lower-stage error to mask the required timeout outcome.
- The corrected first-observed terminal-state contract above closes that gap without changing lower
  SSH APIs or adding a budget framework. The same reviewer used its one focused follow-up and
  returned `GO`: cancellation/timeout precedence is deterministic, and the terminal state is
  latched during and after every controlled stage and after host-key lookup. The reviewer was then
  closed and will not be reused.

### Implementation and code-review evidence

- The coherent P4-04 diff adds only per-call schema/canonical/runtime options, one SSH-specific
  deadline/output helper, direct combined-output clearing, and a no-network serial diagnostic.
  It adds no persistence, Settings, Device/Web controls, manual-terminal change or P4-05 logging.
- `git diff --check` passed. The strict WSL host suite passed with the exact-owned ELF removed from
  `/tmp`; its first invocation did not compile because the shell expanded an empty output variable,
  and the corrected literal-path invocation passed without a production-code change.
- A fresh read-only code reviewer initially found a missing owning `<cstring>` include and proof
  that exercised only pure helpers. One correction added that include, made the tested append/clear
  primitive the exact `SshClient` path, and added `SSHOPTIONSTEST` over the production parser with
  no profile, network, storage or fixture effects. The strict host suite passed again.
- The reviewer used its single combined follow-up and returned `GO`: both blockers are closed and
  no mandatory defect or scope expansion was introduced. The reviewer was then closed and will
  not be reused. Exact pinned build/upload and Device evidence remain before closure.
- The exact pinned build passed with FQBN
  `m5stack:esp32:m5stack_cardputer:FlashSize=8M,PartitionScheme=custom` and the unique resolved
  M5Stack core `3.2.1`: 3,324,886 sketch bytes and 65,612 global bytes. The uploaded image is
  3,325,072 bytes with SHA-256
  `2AEA99E4683200B10894F331D32A3C4A7BF968B4EC0E6C1F500A234268CC3A66`; every flash segment
  reported hash verification and COM8 returned after reset.
- On the uploaded firmware, `SSHOPTIONSTEST` passed the production parser defaults, exact limits,
  malformed/unknown/type rejection and production combined-output clear path without network,
  profile, NVS or SD mutation. It reported free heap 124,040 bytes, largest block 61,428 bytes and
  stack margin 7,860 bytes.
- Immediate idle `STATUS` reported microSD ready, the preserved two chats/two history entries,
  reset reason 1, free heap 124,304 bytes, largest block 61,428 bytes, minimum heap 113,320 bytes
  and stack margin 7,860 bytes. Against the retained P3 idle baseline, free heap is +1,544 bytes,
  largest block +3,072 bytes and stack margin -8 bytes; the 70-KiB idle floor is preserved with no
  freeze or unexpected reset. No Device fixture existed, and the exact-owned WSL ELF was removed.
- P4-04 completed at 2026-08-30 22:18:57 +03:00 after approximately 54 minutes. P4-05 is now the
  only active row.

### Approved reopen correction evidence

- Static invariants and `git diff --check` passed: exactly one length-aware `JsonString` NUL check
  exists at the parser boundary, the broken lower-layer `String::indexOf('\0')` predicate is absent,
  and the existing diagnostic includes the decoded `\u0000` rejection case.
- A fresh independent read-only code reviewer returned GO with no correctness, security or scope
  blockers and was closed without follow-up or reuse.
- The exact pinned build passed with FQBN
  `m5stack:esp32:m5stack_cardputer:FlashSize=8M,PartitionScheme=custom`, resolved M5Stack ESP32 core
  `3.2.1`, 3,340,590 sketch bytes and 65,628 global bytes. The uploaded image is 3,340,784 bytes
  with SHA-256 `8082E6D923BB7D8C48C977A73655F802FCEF27AF3F739EEFA26C83AA0B24690F`; every flash segment
  reported hash verification and COM8 returned after reset.
- On that image, `SSHOPTIONSTEST` passed the ordinary command, decoded embedded-NUL rejection,
  unchanged option limits and overflow-clear path: free heap 122,488 bytes, largest block 60,404
  bytes and stack margin 7,860 bytes. Immediate `STATUS` remained responsive with microSD ready,
  two preserved chats/two history entries, reset reason 1, free heap 122,752 bytes, minimum heap
  111,200 bytes and the same largest block/stack margin. No fixture or persisted state was created.
- The approved correction completed at 2026-08-30 23:34:49 +03:00. Standalone commit
  `47d4aaeb3596f9db74741c910de1ad66879bffce` was pushed to the Phase 4 feature branch and GitHub MCP
  verified that exact remote SHA. P4-05 then resumed without a production change.

## P4-05 design gate

### Focused correction after independent design STOP

This correction is authoritative wherever an earlier P4-05 draft is less precise.

- Combined command-output order is the existing SSH callback-delivery order: each stdout chunk,
  then each stderr chunk, exactly as delivered by the single foreground channel loop. Before every
  append, reject a total beyond `UINT32_MAX`; the first spill space preflight covers the complete
  retained inline prefix plus the triggering chunk. While the file is open, the internal persisted
  count means only bytes accepted by exact `File::write` calls. After `flush()` and `close()`, reopen
  the exact file and verify its size. Only that verified size may be reported as `output_bytes` with
  a complete log. A short write, reopen failure, or size mismatch retains the collision-owned
  filename when one was created, but reports storage failure and never claims a verified byte count
  or completeness.
- The output callback returns `OperationResult`; its failure stops channel reads immediately. The
  existing first-observed terminal latch is authoritative, including cancellation precedence when
  cancellation and deadline are first observed together. After reads stop, observe/latch terminal
  state, close channel/session, then perform SD promotion/finalization exactly once. The first
  latched cancellation or timeout remains the tool outcome ahead of later transport, sink, or
  finalization failures, while the error includes relevant storage detail. Cancellation names a log
  only if exact creation succeeded; the API continues to discard canceled tool output, so no
  canceled bytes reach model continuation. This remains one foreground execution with no continuing
  reader after return.
- Promotion is required not only on normal overflow. If cancellation, timeout, connector failure,
  or invalid inline UTF-8 occurs after any below-cap bytes were received, those bytes are promoted
  once before finalization when the original SD remains writable. If promotion cannot start, return
  the primary terminal outcome plus explicit storage failure and no filename. A fixed model summary
  contains metadata only and never samples command bytes.

Frozen focused proof additions:

- Deterministically observe exact-cap inline success and cap-plus-one single-pass spill.
- Observe cancellation below cap and timeout/connector error below cap promoting retained bytes;
  cancellation must not reach model continuation and its error names the log only after creation.
- Spill raw bytes containing NUL/non-UTF-8 and verify exact downloaded bytes without placing those
  bytes in the model result, audit, serial output, or summary.
- Exercise short-write or final-size-mismatch failure at the narrow capture boundary and verify that
  no verified `output_bytes`/completeness claim is emitted.
- Exercise missing/full/removed/replaced SD before creation and an identity/space failure after
  creation; no overwrite, retry, or continued SSH read is allowed.
- Cleanup uses only the returned collision-owned filename, first confirms the original SD volume is
  still active, removes only that file after success or failure, verifies absence, and repeats
  idempotently. Existing non-owned workspace files remain unchanged.

**Focused review status:** `GO` from independent read-only reviewer
`01a05426-cb94-7c51-9a54-1163e18b9a86`; all three concrete blockers were resolved in one focused
follow-up. The reviewer is closed and production evidence may begin.

**Code-review correction status:** `GO` from fresh independent read-only reviewer
`01a05439-74c9-7ae2-b791-1e409205de5e` after its single focused follow-up. The reviewer found four
bounded blockers. The corrected remote diagnostic cancels only after an actual SD spill and accepts
only the exact cancellation outcome; the existing authenticated hardware-Web runner downloads the
exact returned filename and owns two-pass Device cleanup from both its normal path and `finally`;
failed/unverified storage reports a retained filename without claiming it is currently downloadable.
The proportional test-only write set therefore also includes
`tools/hardware_web_e2e.{ps1,mjs}`; it adds one P4-05 suite and no production route or framework.
Static pre-review evidence: `git diff --check`, PowerShell parse and `node --check` all passed.
The reviewer is closed; focused build/device/Web evidence may run.

**Status:** code_review_GO

**Build failure classification:** active-row diagnostic integration defect. The first pinned build
reached Arduino compilation and failed because the preprocessor did not synthesize declarations for
the three new P4-05 `.ino` diagnostic functions before `SerialDiagnostics.ino` consumed them.
Production capture behavior was not implicated. The minimal correction is three explicit declarations
in the existing sketch declaration owner; no contract, route, persistence or test scope changes.

**Device failure classification (2026-08-30 23:14:39 +03:00; elapsed about 45 minutes):**
test/harness defect, not a production failure. The retained diagnostic tried to induce a final-size
mismatch by appending through a second FAT file handle while the capture handle remained open; the
filesystem did not expose that concurrent write to the authoritative reopen, so the synthetic
expectation was invalid. The exact write-count/final-reopen comparison remains independently
code-reviewed, while real missing/full/removed/replaced failures before and after creation stay in
the Device matrix. Remove only the unreliable concurrent-handle injection; do not alter production
capture or repeat the experiment with the same hypothesis. The diagnostic's exact-owned finalizer
ran before the suite returned failure.

**Remote E2E failure classification:** the unchanged existing `SSHDEMOTEST` then passed connect,
authentication, SFTP list/download, PTY and exact host-key cleanup on `test.rebex.net` with
heap 122,092 bytes and stack margin 7,876 bytes. Therefore Wi-Fi, the host, trust and authentication
are not the owner. The new exec/cancel diagnostic returned before any captured chunk, but replaced
its first error with a generic assertion. This is a P4-05 harness-observability defect. One
instrumentation-only correction preserves the underlying bounded libssh2 error in the diagnostic;
production streaming/capture behavior remains unchanged. Do not repeat the remote case until that
new observation is available.

**Foreign regression ownership:** the instrumented remote run observed
`SSH command must contain 1 to 1024 bytes without NUL characters` before channel open for the
literal command `ls -lR /pub`. The P4-04 validator uses Arduino
`String::indexOf('\0')`, which includes the terminating NUL and therefore rejects every non-empty
command. P4-05 capture received no bytes because it was never entered. P4-05 is paused without a
design/code change; P4-04 is explicitly reopened for the single validator correction, focused
command-boundary evidence and its own follow-up commit. After P4-04 closes, resume this unchanged
P4-05 proof from the remote E2E step.

**Focused E2E failure ownership and 60-minute pivot:** after P4-04 closed, `SSHOUTPUTTEST` passed,
but the remote diagnostic retained 157 verified bytes and reported that `ls -lR /pub` completed
before the next cancellation poll. The initial test-harness hypothesis added a bounded post-output
sleep; a materially different second run still returned success with 193 verified bytes. Both
runner finalizers removed their exact-owned files, and two explicit cleanup retries after each run
observed the file already absent. Code inventory then proved an active-row ordering defect:
`executeCommandStreamingControlled` invokes `onOutput`, then checks channel EOF before its next
loop-head cancellation observation. A callback-triggered cancel can therefore lose to same-iteration
EOF. The frozen minimal correction checks the existing callback immediately after stream callbacks
and before EOF success, with the original finite `ls` restored as the regression scenario. No SSH
state, API, timeout, output-storage or channel-management contract changes.
**Started:** 2026-08-30 22:30:00 +03:00.

### Locked clauses and non-goals

- A model-issued foreground `ssh_command` that exceeds its P4-04
  `max_inline_output_bytes` must stop returning partial inline output and instead retain the
  complete combined stdout/stderr stream in one downloadable microSD-backed command log.
- Output through the inline cap remains the existing bounded JSON result. The first byte beyond
  that cap promotes the already collected prefix to the log exactly once, clears the inline
  buffer, and writes every later chunk directly to the same log. The inline buffer never grows
  beyond the requested cap.
- User cancellation remains cooperative and foreground. If any command bytes exist when
  cancellation, timeout or connector failure is observed, those bytes are promoted/finalized as
  a downloadable partial log; cancellation remains `ToolExecutionOutcome::Cancelled`.
- The model receives only a bounded summary, byte count and opaque log filename/download
  reference. It never receives the full spilled bytes, an absolute SD path, credentials or SSH
  authority internals.
- Non-goals: background jobs/queues, durable job manifests, resume/reconnect, automatic retry,
  result handles, retention/rotation settings, a general streaming/storage framework, a larger
  inline buffer, Device/Web controls, manual-terminal changes, SFTP/transfer and P5 execution.

### Existing producer/consumer and persistence inventory

- `executeSshTool()` is the only model-command producer and currently calls the sole live
  `executeCommandControlled()` consumer. Manual Device/Web terminals use separate shell APIs and
  remain unchanged.
- `SshClient` reads libssh2 stdout then stderr in bounded 1,024-byte blocks on the existing
  foreground channel. P4-04 already supplies one total deadline/cancel callback and the combined
  inline cap; P4-05 changes only the output sink after that cap.
- `ToolExecutionResult` and the API continuation accept bounded UTF-8 JSON. Finished failures can
  carry a bounded log reference to the model. Canceled results short-circuit before tool output is
  appended, so the actionable error string must also name the retained log for Device/Web users.
- Existing tool activity records only tool, selected-SSH target, status, duration, bounded result
  bytes and exit status. It never stores command output; P4-05 does not change that journal.
- `createWorkspaceFile()` collision-checks a safe relative name under `/assistant/files`.
  An unlinked generated `.log` is visible/downloadable to the authenticated user through the
  existing `GET /api/file/download?name=...` stream, while project file tools cannot read it
  without an explicit project link. P4-05 creates no link.
- `requireSdWriteAccess()`, `checkSdOperationSpace()` and the retained 1-MiB operational floor
  own missing/full/removed/replaced-card rejection. No write starts before the current card
  identity and capacity permit it.
- ESP32 FS from the pinned M5Stack core `3.2.1` defines `FILE_WRITE` as `w` and
  `FILE_APPEND` as `a`; `File::write` reports accepted bytes, while `flush()` and
  `close()` return no status. Therefore each write count and final file size must be checked.
- A direct append-only `.log` is authoritative user output, not an atomic metadata document.
  Reset while streaming leaves a valid downloadable partial file. No boot scan, transaction
  marker, retry or running-command recovery owns it; ordinary authenticated file deletion owns
  later user cleanup.

### Minimal design and state transitions

- Add one SSH-specific SD output-capture connector with states `inline`, `logged` and
  `finalized`. It owns at most the requested inline bytes, one `File`, a 32-bit persisted byte
  count and one generated name `ssh-command-<16 lowercase hex>.log`.
- Name generation is bounded and collision checked; an existing file is never opened or
  overwritten. Creation reuses `createWorkspaceFile()`, then opens the exact new file with
  `FILE_APPEND`.
- `inline -> logged` occurs on first overflow or explicit promotion after terminal
  cancellation/error. It preflights SD space, writes the prefix once, clears it, then accepts each
  subsequent stdout/stderr chunk only after a per-chunk space check and exact write-count check.
- `finish` flushes, verifies exact file size and closes. Once a filename exists, later
  connector/SD failure retains the honest partial log and returns its reference; failure before
  creation clears partial inline output and leaves no artifact.
- Extend `SshClient` by one SSH-output callback method and implement the existing bounded method
  as a thin callback wrapper, keeping one libssh2 channel loop and unchanged manual callers.
- Successful spill returns bounded JSON with `exit_status`, `output_bytes`, summary and
  `output_log{name,download_path}`. Timeout/failure returns the same bounded reference; cancel
  additionally includes the filename in `error` because the API deliberately discards canceled
  tool output.

### Crash/SD/cleanup table

| Window | Durable state and required behavior |
|---|---|
| Before promotion | No command-log file; inline bytes remain bounded RAM only |
| After collision-checked create, before/during prefix write | Exact new log exists and may be empty/partial; report and retain only that file |
| During later chunk append | Exact log contains the verified persisted prefix; stop on cancel/deadline/short write/SD state change and retain it |
| After flush/size verification | Closed downloadable log with exact persisted byte count |
| Reset in any logged window | No executor resumes; the ordinary workspace file remains an honest partial/final log |
| Test success/failure | Delete only the returned collision-checked fixture, verify absence and repeat cleanup idempotently; preserve all prior files/inventory |

### Frozen proof and forbidden effects

- Cheap gate: strict host regression, callback/output-cap static checks, no new route/settings/assets,
  bounded JSON/reference and no absolute/private-storage path in schemas/results.
- Device gate: one exact-core build/upload; a no-network capture diagnostic proves inline boundary,
  first-byte spill, exact combined bytes, promotion/finalization, SD write/size checks and automatic
  exact-owned cleanup.
- Integration gate: one controlled read-only SSH command produces a spill, cancellation is observed
  after output begins, the authenticated existing Web download route returns the exact logged
  bytes, and a `finally` cleanup deletes only that returned filename and proves absence. Keep Web
  Console active through download/cleanup, then send `EXIT`.
- Record idle and active-SSH free heap, largest block, stack and reset/freeze state against their
  respective retained baselines. The 70-KiB floor applies to idle/general mode; active SSH must not
  regress from the existing active-SSH baseline.
- Forbidden: overwrite/collision, a second output pass, output bytes in audit/serial/model result,
  project linking, model file-tool access to the log, silent truncation, auto retry, background
  execution, log rotation/retention, Web/Device UI work or any P4-06+ implementation.

### Expected write set

- Production: `src/ssh_client.{h,cpp}`, `src/ssh_tool.cpp`, and one narrow
  `src/ssh_command_output.{h,cpp}` SD connector.
- Retained proportional diagnostics only if needed:
  `CardputerAssistant.ino`, `SshTools.ino`, `SerialDiagnostics.ino`, and
  `tests/host_tests.cpp`.
- Evidence only: this traceability row. No `file_workspace`, `sd_storage`, `tool_router`,
  `api_client`, Web route/assets, Settings, profile/key or permission-policy changes.

### Observed P4-05 evidence and closure

- The stabilized design and production/diagnostic diff each received independent read-only GO.
  After the real same-iteration output/EOF race was observed, one fresh reviewer returned GO for
  the single cancel-before-EOF correction and was closed without follow-up or reuse.
- Cheap checks passed: strict host tests, PowerShell parser, Node syntax, `git diff --check`, bounded
  callback/output/reference invariants and the generated Web asset consistency remained unchanged.
  No new route, Settings field, permission framework, background executor or UI asset exists.
- Final exact pinned build passed with FQBN
  `m5stack:esp32:m5stack_cardputer:FlashSize=8M,PartitionScheme=custom`, resolved M5Stack ESP32 core
  `3.2.1`, 3,340,602 sketch bytes and 65,628 global bytes. The uploaded image is 3,340,784 bytes
  with SHA-256 `34EA26ED51B85DF7DBBCBDF5314448FDBD2AFF7CBC156A1E523813EB5562D589`; every flash segment
  reported hash verification and COM8 returned after reset.
- `SSHOUTPUTTEST` passed inline/spill/promotion/final-size/SD-failure/cleanup boundaries with free
  heap 122,500 bytes, largest block 60,404 bytes and stack margin 7,876 bytes. Against the retained
  P3 baseline this is -260 free-heap bytes, +2,048 largest-block bytes and +8 stack bytes.
- The final real foreground `SSHOUTPUTE2E` passed on the same device: the finite read-only command
  produced and retained exactly 157 bytes in collision-owned
  `ssh-command-42e5ea1879cfc4d4.log`, callback-triggered cancellation beat same-iteration EOF, and no
  background execution or retry occurred. The existing authenticated Web route downloaded exactly
  157 bytes with the required filename pattern while Web Console remained active; `EXIT` then
  returned `WEB_CONSOLE result=stopped`.
- Runner cleanup deleted only the returned command-log fixture. Two later cleanup calls both passed
  with `already_absent=yes`; the two failed hypotheses received the same failure-finalizer cleanup.
  Five exact-owned local log/sidecar files were removed from nine collision-checked candidate paths.
- Final `STATUS` remained responsive with microSD ready, two preserved chats/two history entries,
  reset reason 1, free heap 104,060 bytes, largest block 37,876 bytes, minimum heap 48,948 bytes and
  stack margin 5,972 bytes. Free heap is 12 bytes below the retained 104,072-byte active-Web/SSH
  comparison point, with bounded RAM, the 70-KiB general floor preserved and no reset or freeze.
- P4-05 completed at 2026-08-30 23:59:26 +03:00 after approximately 89 minutes. The mandatory
  material pivots were the independently closed P4-04 validator regression and then the proven
  same-iteration cancellation-order correction; no third patch/review loop was entered.

## P4-06 design gate

**Status:** completed
**Started:** 2026-08-31 00:15:29 +03:00.

### Locked clauses, adjacent ownership and non-goals

- Add exactly four model-facing tools: `sftp_list`, `sftp_read`, `sftp_write` and `sftp_move`.
  They use the existing single `ToolCapability::SftpReadWrite`, Phase 3 policy resolution,
  confirmation, pending preview, audit and cancellation boundaries.
- `Ask` requires confirmation for every model SFTP call. Under `Allow`, `sftp_write` and
  `sftp_move` still require mandatory confirmation when their explicit `overwrite` argument is
  true. No remote lookup, stat or mutation occurs before confirmation. The pending preview shows
  the exact destination and whether overwrite was requested.
- `overwrite` defaults to false. A write never opens the destination with `TRUNC` and a move never
  unlinks the destination. Replacement uses a collision-owned same-directory temporary file plus
  negotiated POSIX rename only after confirmation; a no-overwrite call uses the same completed-temp
  path and ordinary rename with zero flags.
- Listing is paged and bounded to at most 16 returned entries. Write content and each returned read
  chunk are bounded to 12,288 valid UTF-8 bytes, matching the existing model workspace chunk
  ceiling. Read offsets are raw byte offsets and `max_bytes` is 4..12,288 so a complete Unicode
  code point can always make progress. Remote paths are absolute, length-aware, NUL/CR/LF-free and
  at most 511 bytes.
- Every operation remains one foreground connection with a fixed 60-second total deadline from
  connection start through SFTP completion and cooperative cancellation. Existing trusted-host
  verification remains mandatory and mismatch or missing trust fails closed before authentication
  or SFTP mutation.
- Manual Device/Web SFTP retains direct-user authority and its existing methods. P4-07 owns
  workspace transfer, P4-11 owns standalone mismatch acceptance, P4-14 owns project/chat ceilings,
  and P4-16/P4-17 own consolidated UI journeys.
- Non-goals: a new capability or permission hierarchy, delete tool, shell classifier, remote
  action editor, background executor, retries, resumable transfer, persistent settings, remote
  transaction/recovery framework, UI assets, host-key-store changes or general SFTP abstraction.

### Existing producer, consumer, persistence and vendor inventory

- `tool_catalog` currently has seven schemas and an 8-bit mask. Four appended stable schema IDs
  require an 11-bit `uint16_t` mask; the plan itself is not persisted. The persisted policy codec
  already owns `SftpReadWrite` as `sf`, so no Settings or codec migration is required.
- `resolveChatToolPermissions()` currently leaves `SftpReadWrite` unavailable. The existing
  project route delegates all non-file tools to the generic audited route; enabling the existing
  capability and adding four dispatch cases is sufficient.
- Pending tool-call format v2 already stores canonical arguments and a generic SSH target. P4-06
  reuses that target: its authority hash is rebuilt from the selected public summary's stable
  opaque profile ID, public connection fields and trusted fingerprint without loading any profile
  secret; SFTP targets additionally store the canonical remote destination in `target.name`.
  Existing v2 `ssh_command` targets remain readable; a pre-change authority hash may become stale
  and fail closed, but is not treated as corrupt or migrated.
- Existing Device and Web confirmation renderers already display generic `targetName` and preview
  body. No Device/Web asset change is needed to show destination, source and overwrite intent.
- Existing manual SFTP list allocates and sorts the whole directory; model listing therefore needs
  a separate bounded page method. Existing manual upload opens the destination with `TRUNC` and is
  not reused for model writes. Existing manual download/transfer remains P4-07 ownership.
- The installed CardMind package identifies itself as version 1.11.1 in `library.properties`, while
  the compiled official-derived header is exact `LIBSSH2_VERSION` `1.11.1_DEV`. Its `CREAT|EXCL`
  open fails when a name exists. For SFTP v3, ordinary `rename_ex` does not transmit rename flags;
  therefore no-overwrite uses ordinary rename with zero flags, while confirmed overwrite uses only
  the negotiated `posix-rename@openssh.com` extension. If that extension is unsupported, overwrite
  fails explicitly with source and destination unchanged. There is no unlink/TRUNC fallback and no
  stronger portability claim than the server extension supplies.
- The completed read-only inventory agent was closed after one report and will not be reused for
  design, code, evidence or another roadmap row.

### Minimal contracts and state transitions

- `sftp_list`: exact `path`, `offset` and `max_entries`; `max_entries` is 1..16. It scans the
  foreground directory stream to the requested accepted-entry offset under the total deadline,
  retains at most one page plus one lookahead, and returns entries, `next_offset` and `eof`.
- `sftp_read`: exact `path`, raw byte `offset` and `max_bytes`; `max_bytes` is 4..12,288. A starting
  offset on a UTF-8 continuation byte fails explicitly. The reader returns the largest valid prefix
  not exceeding `max_bytes`; if the bound splits a multibyte code point, `next_offset` remains at
  that code point so the next call returns it once, without skip or duplication. NUL/binary/invalid
  UTF-8 fails with no partial model result.
- `sftp_write`: exact `path`, `content` and optional boolean `overwrite` normalized to false. It
  creates a bounded collision-owned same-directory temporary path with `CREAT|EXCL`, writes and
  closes the full content, then renames once to the destination. False uses ordinary rename with
  zero flags; true uses only `posix-rename@openssh.com` after mandatory confirmation and fails
  unchanged if the extension is unsupported.
- `sftp_move`: exact `source_path`, `destination_path` and optional boolean `overwrite` normalized
  to false. It performs one rename; false uses ordinary rename with zero flags, while true uses only
  `posix-rename@openssh.com` after mandatory confirmation and fails unchanged if unsupported.
- Pending build and claim bind the canonical call, project/chat revision, stable selected profile
  ID, host, port, username, auth mode, trusted fingerprint, private-key ID for private-key auth, and
  displayed destination. One narrow public-only authority lookup reads the selected non-secret ID
  and binding without password, passphrase or private-key bytes. Claim performs no remote operation.
  SFTP write/move preview body states overwrite yes/no; move also states the exact source.
- One outer start time and terminal latch governs connect, trust check, authentication, SFTP open,
  data I/O and rename. Every stage receives only the remaining part of the fixed 60-second budget;
  stages cannot accumulate fresh 60-second windows. Existing bounded channel/session close and an
  exact temporary cleanup attempt occur synchronously after the terminal outcome and outside that
  mutation deadline; no remote operation continues after return.
- Before the first rename call, cancellation/timeout leaves source and destination unchanged and
  attempts exact temporary cleanup. Once any rename attempt begins, cancellation, timeout, reset or
  connector loss before a final status has an explicitly unknown remote outcome and is never
  retried. A success status is committed remote state. A reset before rename may leave only the
  collision-owned temporary path; there is no boot scan/recovery owner. Cleanup failure reports that
  exact path without touching a destination or unrelated path.
- Pending state remains the existing atomic SD v2 owner. A rebooted pending call remains
  non-resumable under the existing boot boundary; no SFTP operation starts during recovery.

### Frozen minimal proof and forbidden effects

| Observation | Required result |
| --- | --- |
| Catalog/policy | Four appended schemas, 11-bit mask, existing `sf` codec unchanged, SSH group and Off/Deny/Unavailable fail closed |
| Confirmation | `Ask` persists all four calls; `Allow` executes list/read and no-overwrite write/move, but persists overwrite write/move; destination and overwrite are visible |
| Pending identity | Canonical exact fields round-trip; selected profile ID/public authority/private-key binding or destination change makes approval stale; no password/passphrase/private-key read or persisted value |
| Bounded read/list | Two list pages contain at most 16 entries with correct continuation; reads return at most 12,288 valid UTF-8 bytes, reject binary/NUL/mid-code-point offsets, and split one multibyte code point across the bound without skip/duplication |
| Safe write | Existing destination is unchanged before approval, on deny, on no-overwrite failure, temp-write failure and cancellation before the first rename call; a started rename without final status is unknown; confirmed replacement writes exact bytes and leaves no temp |
| Safe move | Existing destination blocks no-overwrite; confirmed overwrite uses negotiated POSIX rename or fails unchanged, with no fallback unlink; source/destination and pre-send/sent-unknown/success states are explicit |
| Trust/audit/cancel | Missing or changed host trust blocks before SFTP; every started call uses existing audit/cancel owner and no work continues after foreground return |
| Total deadline | A focused boundary proves connect/trust/auth/open/data/rename consume one shared 60-second budget while bounded teardown/temp cleanup is reported separately |
| Device/resources | One exact-core build/upload and smallest real-host exact-owned list/read/write/move scenario pass; active SSH heap/largest block/stack/latency do not regress or reset/freeze |
| Cleanup | Collision-checked remote fixtures and pending state are removed after success/failure, repeated cleanup is idempotent, and prior profile/selection/remote inventory is restored |

- Forbidden: destination `TRUNC` or pre-delete, remote preflight before confirmation, full-directory
  materialization, partial model output after a read failure, secret/key/path bytes in schema,
  result, pending JSON, preview, audit, serial or diagnostics, and any manual SFTP authority change.
- Forbidden: P4-07 transfer, P4-08 Safe Actions, P4-11 host-store UI, P4-14 ceilings, P4-16/P4-17
  UI integration, background work, retry/reconnect, temp-file boot scan or generic SFTP framework.

### Expected row-owned write set

- Production: `src/tool_catalog.{h,cpp}`, `src/tool_policy.cpp`, `src/api_client.cpp`,
  `src/pending_tool_call.cpp`, `src/tool_router.cpp`, `src/ssh_client.{h,cpp}` and one narrow
  `src/sftp_tool.{h,cpp}` execution owner.
- Proportional proof: `tests/host_tests.cpp` plus the existing serial diagnostic owner only if a
  direct real-host observation cannot be made through an existing command. No Web asset, Settings,
  policy codec, project/chat schema, known-host storage or manual SFTP UI file may change.

### Device-proof owner

- Read-only inventory after the first successful upload confirmed that existing `SFTPTEST` and
  `SSHDEMOTEST` exercise only the pre-P4 manual list/download methods; no existing serial command
  reaches the four model-SFTP executors. The optional existing serial owner is therefore activated
  for one proportional `MODELSFTPTEST` only in `SshTools.ino`, its forward declaration/header include
  in `CardputerAssistant.ino`, and command/result wiring in `SerialDiagnostics.ino`.
- The diagnostic uses the unchanged selected profile/trust, two random collision-checked exact-owned
  `/tmp` names and no SD/pending/profile mutation. It proves two one-entry list pages, UTF-8 raw-offset
  continuation, no-overwrite unchanged state, safe move/write overwrite, then removes only checked
  final paths it attempted and exact temporary paths reported by the connector, verifying every
  candidate absent twice. Any failure runs the same bounded cleanup.

### Diagnostic cleanup correction lock

- Architect health-check and the fresh read-only test review confirmed a P4-06 diagnostic defect,
  not a production-contract or adjacent-row defect: an initial write whose rename outcome is unknown
  can leave its final or reported collision-owned temporary path even though the model executor
  returns an ordinary failure, while the diagnostic previously marked a final path owned only after
  success and skipped cleanup when neither success flag was set. The serial result also derived
  `cleanup=yes` from overall operation success instead of observing cleanup independently.
- Before any mutation, the diagnostic must use the selected trusted profile to collision-check both
  random final paths. Immediately before each attempted write it marks that already-absent final path
  as an exact-owned cleanup candidate. A failed write contributes only a reported path matching the
  exact `/tmp/.cardmind-<16 lowercase hex>.tmp` production pattern; malformed or excess candidates
  fail the diagnostic rather than broadening cleanup. Success, failure and sent-unknown outcomes run
  one bounded cleanup over only these candidates and verify every candidate absent twice.
- After a successful overwrite move, the diagnostic must directly verify that the source path is
  absent before cleanup, so cleanup cannot hide a copy-like or partial move result. The serial result
  reports cleanup completion through a separate output value on both pass and failure paths.
- Frozen correction write set: only this evidence section plus the existing `MODELSFTPTEST`
  declaration, implementation and serial result wiring. No production SFTP contract/API, schema,
  permission, persistence, retry/recovery owner or general fixture framework may change.

### Design review outcome

- Fresh independent reviewer `01a05489-4289-7990-949a-5c03827deb62` returned five concrete STOP
  items: SFTP-v3 overwrite semantics, rename sent/unknown states, private-key binding in pending
  authority, one shared deadline and UTF-8 chunk boundaries. One combined follow-up confirmed that
  all substantive blockers were resolved and identified only two stale trace phrases; those exact
  phrases were corrected above. The reviewer is closed and will not review implementation,
  evidence, closure or another roadmap row.

### Code review correction gate

- Fresh read-only code review returned `STOP` for six concrete P4-06 defects: one duplicate local
  contract declaration; overwrite confirmation recomputed from the wrong canonical field; an
  indeterminate temporary-file open without explicit outcome/cleanup reporting; wrapper loss of
  exact cleanup detail; pre-send versus sent-unknown rename state collapse; and worst-case JSON
  expansion beyond existing pending/result bounds.
- Ownership is active-row P4-06. No completed or adjacent row is reopened. The correction is locked
  to those six findings, uses the existing pending/result limits and SFTP connector, and adds no
  schema, persistence owner, retry, recovery framework, UI or manual-SFTP change.
- Canonical `overwrite`, not remote target state, drives claim-time confirmation. A local
  first-call flag distinguishes cancellation/deadline before rename from a sent request whose
  final status is unavailable. An in-flight temporary create is resolved only through the same
  bounded libssh2 call; success is closed/unlinked, while unresolved status reports the exact
  collision-owned path and unknown outcome without retry.
- Model SFTP content is explicitly text: NUL and C0/DEL controls other than tab, CR and LF are
  rejected while valid UTF-8 up to 12,288 bytes remains accepted. Thus JSON escaping is at most
  twofold and both canonical pending arguments and read results fit the existing 32-KiB envelope;
  no larger buffer, storage side channel or new codec is introduced.
- After one coherent primary-agent correction and cheap checks, the same reviewer may receive its
  single combined follow-up for only these findings, then it is closed.
- The corrected diff passed `git diff --check` and the strict WSL host suite. The same reviewer used
  its single combined follow-up, returned `GO` for all six corrections and was then closed; it will
  not review evidence, closure or another row.
- The first exact firmware compile then failed before link on one P4-06-owned type mismatch while
  prefixing an SFTP move preview (`Arduino String` plus `std::string`). The locked correction is one
  `std::string` expression; no design, interface or adjacent-row behavior changes. This is recorded
  as one late compile escape and justifies one repeated exact build.

### Observed P4-06 evidence and closure

- Strict host regression, `git diff --check` and the focused catalog/policy/pending/parser checks
  passed. They proved four appended schemas with the existing `SftpReadWrite` capability, fail-closed
  Off/Deny/Unavailable behavior, `Ask` confirmation for every call, `Allow` confirmation for explicit
  overwrite, canonical destination/overwrite preview and stale approval after authority, key binding
  or destination change. No Settings codec, Web asset, new capability or manual-SFTP authority changed.
- The independent production code reviewer returned `GO` after its six concrete blockers were
  corrected and was closed. Architect health-check plus the fresh diagnostic reviewer independently
  confirmed the later fixture-cleanup and source-absence defects; the primary correction changed only
  the existing diagnostic declaration/implementation/result wiring and the trace lock above.
- Final exact pinned build passed with FQBN
  `m5stack:esp32:m5stack_cardputer:FlashSize=8M,PartitionScheme=custom`, the unique resolved M5Stack
  ESP32 core `3.2.1`, 3,396,822 sketch bytes and 65,628 global bytes. The uploaded image is 3,397,008
  bytes with SHA-256 `BDEC90F28414C02EC1A4965A7123B1D38CA52AD57B53F18A8BD9420B41F158AE`;
  every flashed segment reported hash verification and COM8 returned after reset.
- Real-device `MODELSFTPTEST` passed in 57,728 ms through the four model executors. It observed two
  bounded one-entry list pages with advancing continuation, UTF-8 byte-offset split and continuation
  rejection, exact reads, no-overwrite preservation, confirmed overwrite move/write, source absence
  after the successful move and final content. The same selected trusted profile and existing manual
  SFTP owner were used; no profile, selection, pending state, SD file or user remote object changed.
- Both random `/tmp/cardmind-p4-06-<nonce>-*` paths were collision-checked before mutation. Every
  attempted final path and any exact connector-reported `/tmp/.cardmind-<16hex>.tmp` candidate used
  the bounded success/failure cleanup path, was verified absent twice, and serial independently
  returned `cleanup=yes`. No retry, boot scan or general fixture/recovery framework was added.
- Post-scenario `STATUS` remained responsive with microSD ready, two preserved chats and two history
  entries, reset reason 1, free heap 117,540 bytes, largest block 53,236 bytes, minimum heap 48,552
  bytes and stack margin 7,860 bytes. The diagnostic result itself reported free heap 117,076 bytes
  and the same minimum/largest/stack bounds. This remains above the retained active-SSH comparison
  points for free heap and largest block, with only a 396-byte lower global minimum than P4-05, and
  no reset, freeze or persistent session.
- P4-06 completed at 2026-08-31 01:37:01 +03:00. P4-07 and later behavior remain pending.

## P4-07 design gate

**Status:** completed
**Started:** 2026-08-31 01:45:15 +03:00.

### Scope lock

- ROADMAP clause: add file transfer between the CardMind workspace and the selected remote host.
- Architect decision (2026-08-31): `LLM-accessible` covers only the four P4-06 SFTP tools. P4-07 is a reusable backend for an explicit direct-user transfer in either direction through the selected trusted SSH profile/session.
- P4-07 owns workspace/path-policy enforcement, bounded streaming, one foreground deadline covering the complete transfer, cooperative cancellation, overwrite-default-deny, safe replacement, and explicit failure/unknown cleanup outcomes.
- P4-16 and P4-17 own Device/Web controls, user-visible destination/overwrite confirmation, stable IDs, and their end-to-end journeys. Until those rows, the new backend may be exercised directly by a proportional diagnostic but is not claimed as integrated UI behavior.
- P4-06 remains the owner of model SFTP list/read/write/move and its existing `SftpReadWrite` permission path.

### Explicit non-goals

- No model schema/catalog entry, pending call, multi-capability policy, model audit/confirmation path, or model-visible transfer result.
- No new transfer framework, job/background executor, retry queue, durable manifest, reconnect/resume, or generic recovery engine.
- No Device/Web asset or route changes in this row, and no change to manual terminal authority.
- No pre-delete, `TRUNC`, or destructive fallback against the destination; no retry after an outcome-unknown rename.
- Do not rewrite the already accepted P4-06 SFTP operations or re-prove generic P2 SD atomic-storage behavior.

### Existing boundary inventory

- Device SFTP upload/download and authenticated Web routes currently call `SshClient::uploadSftpFile` / `downloadSftpFile`; the SSH demo and serial diagnostics also consume the existing download path. These consumers remain unchanged until P4-16/P4-17.
- The existing upload streams a workspace file directly to a remote final path with create/truncate semantics and therefore cannot satisfy the P4-07 safe-replacement contract.
- The existing download streams to the workspace atomic `.tmp` sidecar and commits through `commitWorkspaceBinaryTemporary`, but refreshes its timeout after progress and has no cooperative-cancel contract.
- Reusable P4-06 primitives already provide a monotonic remaining-deadline calculation, collision-owned same-directory remote temporary names, exclusive temporary creation, no-overwrite rename, POSIX replacement rename, and typed mutation outcomes including outcome unknown.
- Existing SD primitives own workspace path validation/parent creation, capacity checks, deterministic temporary/backup recovery, target-to-backup switching, failed-commit restoration, and exact backup cleanup.
- Installed libssh2 is the pinned 1.11.1 source already evidenced by P4-06. Ordinary SFTP rename is the no-overwrite operation; overwrite uses the negotiated POSIX rename extension. There is no unlink/truncate fallback.
- No background, reboot, project/chat, model, or audit consumer exists for direct-user transfer. Cleanup is owned synchronously by the transfer call; deterministic local sidecar recovery remains owned by the existing workspace initialization/recovery path.

### Minimal reviewed design candidate

- Add exactly two explicit controlled `SshClient` operations, upload and download, with `overwrite`, total `timeout_ms`, and cooperative `is_cancelled` inputs. Reuse the existing typed SFTP mutation outcome rather than adding a transfer framework.
- Both operations require the caller's already selected, connected, host-key-verified SSH session. They do not select a profile, acquire broader authority, or expose credentials/authority internals.
- A single absolute deadline is created on entry and is never refreshed after progress. Every open/read/write/close/rename/cleanup wait consumes only its remaining budget.
- Upload validates the workspace source and remote destination, streams through the existing bounded buffer into an exclusively created collision-owned same-directory remote temporary file, closes it, then performs exactly one no-overwrite rename or confirmed POSIX replacement rename. The final path is never opened, truncated, unlinked, or pre-deleted.
- Download validates the remote source and workspace destination, first runs the existing bounded recovery for that destination, then denies the recovered existing target unless overwrite is explicit. It streams into the exact-owned SD temporary sidecar and verifies/flushes it. With overwrite denied it checks the final target again immediately before commit and aborts with exact-temp cleanup if a target appeared; only then may it call the existing safe workspace replacement primitive. Before commit, the prior target is unchanged.
- Cancellation or known failure before rename/commit closes handles and removes only the exact-owned temporary object. Cleanup failure is explicit and cannot be converted to success.
- Exclusive remote-temp creation is also an outcome-bearing mutation. If cancellation arrives while its nonblocking request is pending, the same request may be resolved and an owned handle closed/removed only while the original total deadline still has budget. If the deadline expires or ownership cannot be resolved, return `outcomeUnknown`; do not extend the deadline or delete a path whose ownership is ambiguous.
- Keep the existing `SftpMutationResult`: `outcomeUnknown` carries authority uncertainty, while the error reports the exact cleanup disposition as `cleanup=removed`, `cleanup=failed`, or `cleanup=not_attempted` for every interrupted/unknown remote-temp path. It does not expose credentials or the random temporary path.
- If a remote rename request may have been accepted but its final status is unavailable, return outcome unknown, do not retry, and do not claim which remote object is authoritative. Cleanup is reported `not_attempted`: a successful rename releases ownership of the temporary pathname, so probing or unlinking it afterward could delete an unrelated file created in that race.
- Existing direct-user wrappers and UI consumers are not silently redirected in this row; P4-16/P4-17 will invoke the controlled operations only after explicit destination/overwrite interaction.

### Side-effect and recovery table

| Window | Durable state before | Permitted effect | Required outcome / owner |
|---|---|---|---|
| Upload before exclusive temp create | Remote final unchanged | None | Explicit failure/cancel; no cleanup |
| Upload temp open/partial/closed | Remote final unchanged | Exact random same-directory temp only | Close and remove exact temp on known failure/cancel; cleanup failure explicit |
| Upload temp create request pending at cancel/deadline | Exact random pathname may or may not have been created | Resolve only within remaining original deadline | Confirmed owned handle may be closed/unlinked; otherwise outcome unknown with explicit cleanup disposition; no deadline extension |
| Upload rename sent, final status unavailable | Final may be old or new; temp may or may not remain | One rename request only | Outcome unknown; no retry or destructive fallback; report `cleanup=not_attempted` |
| Upload rename confirmed | New final authoritative | Atomic server rename | Success; exact temp absent |
| CardMind reset after remote temp create and before confirmed rename | Remote final unchanged; exact random temp may persist | No replay or automatic scan | No safe automatic owner exists without forbidden durable metadata. Replay and wildcard deletion are forbidden; the remote-host operator owns manual inspection/removal. CardMind reports this bounded residual risk and does not claim crash cleanup |
| Download before/while SD temp write | Prior workspace target unchanged | Existing atomic `.tmp` sidecar only | Remove exact temp on known failure/cancel; SD removal/cleanup failure explicit |
| Download atomic commit interrupted | Existing target/`.tmp`/`.bak` state only | Existing target-to-backup and temp-to-target sequence | Existing bounded workspace recovery/restoration owns next access/reboot |
| Download commit confirmed | New workspace target authoritative | Existing safe replacement | Success; exact temp/backup cleanup required or cleanup failure explicit |

### Frozen minimal proof matrix

- Static/host: the two controlled operations use one non-refreshing deadline and cooperative cancel checks; upload never opens the final path with `TRUNC`, unlinks it, or pre-deletes it; only an exclusive same-directory temp is streamed; download commits only through the existing workspace atomic primitive.
- Static/host: no model schema/catalog/pending/policy/audit path and no Device/Web asset/route changes; existing callers remain unchanged for P4-16/P4-17.
- Exact-core build/upload after review GO.
- Focused real-device diagnostic through one selected trusted profile: stream payloads larger than one buffer in both directions and compare exact bytes; no-overwrite preserves existing remote and local targets; explicit overwrite safely replaces them; cooperative cancellation leaves prior targets unchanged.
- One proportional deadline-expiry scenario must return within the configured total deadline tolerance, preserve both prior targets, issue no retry, and expose the exact remote-temp cleanup disposition. The test-level fixture owner performs any separately required exact cleanup without changing the production deadline contract.
- The diagnostic collision-checks every fixture, owns only its random remote paths and exact workspace paths/sidecars, cleans them after success and failure, repeats cleanup to prove idempotency, and restores prior profile selection/workspace inventory.
- Observe elapsed time, free heap, largest block, stack margin, reset reason, SD readiness/ownership, and compare active-SSH resources with the retained active-SSH baseline rather than the idle 70-KiB floor.

### Forbidden effects

- No model authority or LLM-addressable transfer path; no credential/private-key/password/passphrase bytes or storage paths in results, diagnostics, serial, logs, Web/API reads, or Git.
- No destination mutation before explicit overwrite is supplied; no partial local/remote final file; no retry of unknown rename; no deletion outside exact-owned temporary/fixture paths.
- No reset/freeze, persistent worker, unbounded RAM growth, changed user profile/selection, changed unrelated workspace/remote data, or retained diagnostic artifacts.

### Expected write set and review gate

- Production: `firmware/CardputerAssistant/src/ssh_client.h`, `firmware/CardputerAssistant/src/ssh_client.cpp` only.
- Proportional retained diagnostic only if required by the frozen proof: existing `firmware/CardputerAssistant/CardputerAssistant.ino`, `firmware/CardputerAssistant/SshTools.ino`, and `firmware/CardputerAssistant/SerialDiagnostics.ino`; no new harness/framework file.
- Evidence/status only: `.agents/P4_TRACEABILITY.md`.
- Independent pre-edit design review: initial consolidated verdict STOP on local no-overwrite ordering, pending-create ambiguity, post-reset remote-temp ownership, and missing timeout proof. All four were resolved above without a new schema/framework; the reviewer's single combined follow-up returned GO on 2026-08-31. Reviewer lifecycle closed; production may proceed only with the locked write set and proof.

### Independent code-review STOP and failure ownership

- Fresh read-only review of the stable five-file diff returned STOP before build/device evidence. All findings are active-row defects; no completed row is reopened.
- Backend: interrupted libssh2 open/stat/read/write/close/rename state could remain pending and bind a later operation to stale state. Correction owner: P4-07 must resolve within the original deadline or invalidate and close the entire SSH session before return.
- Backend: relative helper deadlines rebased a remaining-budget snapshot. Correction owner: P4-07 needs one absolute-deadline wait helper used only by the new transfer methods.
- Backend: local cleanup could claim removal without rechecking expected-card access, and known-size remote growth was not checked per block against the SD floor. Correction owner: P4-07 cleanup/stream boundary.
- Diagnostic: remote-directory ownership was asserted before confirmed `mkdir`, creating a race that could delete another actor's directory. Correction owner: P4-07 diagnostic; ownership begins only after confirmed create.
- Diagnostic: broad failure checks could false-pass cancel/timeout/no-overwrite and did not exercise controlled-download cancellation. Correction owner: P4-07 diagnostic assertions and exact cleanup observation.
- Build/device evidence remains blocked. Apply one coherent correction batch, rerun cheap static checks, then give this code reviewer its single combined blocker follow-up.

### Code-review correction outcome and cheap evidence

- One coherent correction batch added an absolute-deadline wait used only by the two P4-07 methods, invalidated the full SSH session after unresolved libssh2 state, revalidated expected-card cleanup access, rejected post-stat growth before SD write, checked the operational floor per block, moved fixture ownership after confirmed `mkdir`, and made cancel/timeout/no-overwrite assertions specific.
- `git diff --check`: pass. Working diff remains exactly the five locked P4-07 paths; Architect-owned `.codex/` remains untracked and untouched.
- `P4_07_BACKEND_STATIC`: pass; both controlled methods have one absolute deadline, contain no relative controlled wait and no final-path `TRUNC`, and include session invalidation plus expected-card cleanup/growth guards.
- `P4_07_DIAGNOSTIC_STATIC`: pass; remote ownership is post-create, controlled-download cancellation is present, and cancel/timeout accept only confirmed removal or explicit not-attempted with a closed session.
- The same fresh code reviewer used its single blocker follow-up and returned GO with no remaining mandatory findings. Reviewer lifecycle closed. Exact build/device evidence may start.
- First exact compile attempt stopped before image generation because Arduino did not synthesize a cross-INO declaration for `runSftpTransferRemoteTest`. Classified as a P4-07 diagnostic integration defect; production transfer code was not implicated. Minimal correction: one explicit internal forward declaration in the existing serial diagnostic file, then one justified rebuild.

### Verified build, Device, resources, and cleanup evidence

- Exact pinned compile after the diagnostic declaration correction: pass. M5Stack core `3.2.1`, exact FQBN `m5stack:esp32:m5stack_cardputer:FlashSize=8M,PartitionScheme=custom`, sketch `3,424,714` bytes, global RAM `65,628` bytes.
- Parsed `build.options.json`: pass; exact FQBN present, one unique resolved M5Stack hardware path at `3.2.1`, no other core version. One COM8 upload completed with every flash hash verified and normal hard reset; NVS and microSD were not erased.
- Firmware image `CardputerAssistant.ino.bin`: `3,424,896` bytes, SHA-256 `34A8064380970D5A342E770306FDFF9204FDF1BE8602A15DF3300763454DB59C`.
- Focused real-device `SFTPTRANSFERTEST`: pass in `86,749 ms`. Through the selected trusted profile it transferred an exact `65,536`-byte bounded-stream payload in both directions, denied and preserved existing remote/local targets, performed explicit safe overwrite, exercised upload and download cancellation, enforced the bounded total-deadline assertion with no retry, and verified subsequent same-session use or explicit reconnect after invalidation.
- Exact-owned cleanup: `cleanup=yes`. The diagnostic collision-checked one random remote directory and four workspace filenames, asserted ownership only after confirmed remote-directory creation, removed only that directory's contents and exact local final/`.tmp`/`.bak` paths, and repeated absence checks in two cleanup passes. Profile selection/inventory and unrelated remote/workspace data were not mutated by the test path.
- Device result resources: free heap `121,412`, minimum heap `42,672`, largest block `56,308`, stack margin `7,876` bytes. Post-test `STATUS`: free heap `121,676`, largest block `56,308`, minimum heap `42,672`, stack margin `7,876`, `microsd_state=ready`, chats `2`, history `2`, reset reason `1`.
- Resource comparison: post-test idle heap remains above the 70-KiB general floor; free heap/largest block/stack are above the retained P4-06 post-test values (`117,540` / `53,236` / `7,860`). The minimum remains above the existing active-SSH free-heap baseline (`39,664`) despite the broader multi-transfer diagnostic. No reset, freeze, SD ownership change, or Device/Web latency path change occurred; P4-07 adds no active UI consumer.
- One-off serial wrapper reported a failure only after both valid evidence lines because it matched obsolete field names `sd`/`reset` instead of observed `microsd`/`reset_reason`. Classified as a harness-only assertion defect; the passing diagnostic, confirmed cleanup, ready expected card, and normal reset reason were already observed, so the transfer experiment was not repeated.
- Forbidden effects: no model schema/catalog/pending/policy/audit changes, no model transfer authority, no Device/Web control changes, no final-path truncate/pre-delete, no unknown rename retry, and no credential/key/password/passphrase or private storage-path output.
- Residual risk retained honestly: a device reset after remote-temp creation can leave an inert random same-directory temp that CardMind cannot safely rediscover without forbidden durable metadata. Automatic replay/wildcard deletion is forbidden; remote-host operator inspection owns that rare crash residual. P4-16/P4-17 own direct-user controls and explicit overwrite interaction.

**Completed:** 2026-08-31 02:50:53 +03:00. P4-08 and later behavior remain pending.

## P4-08 design gate

**Status:** completed after Architect personal closure GO
**Started:** 2026-08-31 02:53:27 +03:00.
**Completed:** 2026-08-31 11:48:03 +03:00.

**Architect personal closure GO:** the Architect personally reviewed the final
actual nine-file diff, exact contract and non-goals, every schema/prompt/catalog/
policy/router/pending/preview/executor/audit producer and consumer, fixed vendor
command semantics, restored cancel-before-parse behavior, diagnostic removal,
retained host tests, compositional Device evidence, pinned build resources,
cleanup and the explicit post-fix preview residual. The two final trace evidence
ownership statements are accurate and no unresolved blocker remains.

**Independent pre-edit review:** initial STOP found missing explicit audit-path
proof. The frozen matrix was corrected to cover direct Allow, approved Ask and
Succeeded/Failed/Canceled activity records; the reviewer's single focused
follow-up returned GO. The reviewer was then closed.

**Independent code review:** the fresh code reviewer returned GO after one
focused correction, but the later Architect personal closure review superseded
that verdict with two exact blockers: the refactor had moved cancellation after
argument parsing, and the retained firmware diagnostic duplicated lifecycle
ownership while retaining an exact-cleanup hole. The correction restores the
pre-P4-08 cancel-before-parse boundary in both public executors and removes the
firmware diagnostic entirely. No new reviewer loop is opened; the corrected
row-owned diff returns to Architect for personal re-review.

### Scope lock

- ROADMAP Phase 4: add a small fixed built-in Safe Actions set for logs,
  service state, containers, disk and processes.
- Arbitrary model-issued `ssh_command` remains `SshMutate`. `SshRead` exposes
  only exact fixed reviewed actions; no command-text, prefix or regex
  classifier is permitted.
- Every action remains under the existing Off/Ask/Allow hierarchy, selected
  trusted SSH authority, total command deadline, bounded/SD-backed output,
  cooperative cancel, pending confirmation and tool-activity audit paths.
- Manual Device/Web terminal authority is unchanged. Final Device/Web controls
  belong to P4-16/P4-17.

### Inventory and ownership

- Producer: `api_client.cpp::addToolSchema` emits model schemas from the stable
  append-only `tool_catalog`; API tool-call decoding resolves names back
  through that catalog.
- Policy: `ToolCapability::SshRead` and its persisted P3 policy codec already
  exist. `tool_router.cpp` currently keeps it unavailable; `ssh_command`
  remains mapped only to `SshMutate`.
- Runtime: `tool_router.cpp` owns authorization/confirmation dispatch and
  `executeAuditedToolCall`; `ssh_tool.cpp` already owns selected-profile JIT
  secret load/zeroization, one total deadline, cancel, host-key mismatch block,
  authentication, one-pass streamed output and P4-05 SD-log/reference results.
- Pending confirmation: `pending_tool_call.cpp` canonicalizes exact arguments,
  stores selected profile/key/trusted-fingerprint authority SHA-256, and rejects
  stale authority before approval. The Safe Action must reuse that SSH target
  identity while remaining `PolicyAsk`, not mandatory under `Allow`.
- Persistence/reboot/SD: no new settings, action records, migration or recovery
  owner. Existing pending-call persistence and P4-05 output-log ownership are
  reused unchanged. Missing remote utilities return their normal non-zero exit
  status; there is no fallback command.
- Consumers: model schema/prompt, catalog/request plan, router, pending preview,
  audit/cancel executor and host policy tests. P4-16/P4-17 later add matching
  direct-user controls.
- Primary references reviewed: systemd `journalctl`/`systemctl` upstream manual
  sources, Docker CLI `container ls`, GNU Coreutils 9.11 `df`, and procps-ng
  `ps`. The fixed commands are read-only introspection commands:
  `journalctl --no-pager --lines=100 --output=short-iso`,
  `systemctl list-units --type=service --state=running,failed --no-pager --plain`,
  `docker ps --no-trunc`, `df -hP`, and
  `ps -eo pid,ppid,user,stat,etime,comm`.

### Minimal design

- Append one stable `ssh_safe_action` schema mapped to `SshRead`; preserve all
  existing schema IDs and the `ssh_command -> SshMutate` mapping.
- Accept exactly one required enum `action` (`logs`, `service_state`,
  `containers`, `disk`, `processes`) plus the existing optional `timeout_ms`
  and `max_inline_output_bytes`. No free command, path, service/container name,
  filter, shell fragment or other interpolation enters the fixed command.
- Keep the five-ID/five-command table in the existing catalog source so API,
  canonicalization, runtime and host proof share one reviewed authority. This is
  a fixed table, not an action registry/editor/framework.
- Refactor only the existing SSH execution body into one internal function
  called by `ssh_command` and the parsed fixed action. Host-key verification,
  JIT credentials, deadline, cancel, output capture and zeroization remain one
  implementation.
- Treat the Safe Action as an SSH authority target in pending-call save/load and
  stale-identity validation. Its Ask preview shows the exact fixed action; Allow
  runs without mandatory confirmation. Arbitrary `ssh_command` stays mandatory.

### Frozen proof matrix

- Host/static: catalog has one appended `ssh_safe_action -> SshRead`; with only
  `SshRead=Allow/Ask`, the request plan exposes that schema and never
  `ssh_command`/SFTP. With `SshRead=Off/Unavailable`, it exposes none.
- Host/static: the catalog contains exactly the five reviewed ID/command pairs;
  unknown IDs, extra fields, free command text and out-of-range timeout/output
  options fail closed.
- Pending/static: Ask persists selected SSH authority identity, preview names
  the exact fixed action, and stale profile/key/trusted-host identity cannot be
  approved. Allow is not promoted to mandatory; arbitrary SSH remains mandatory.
- Audit/static: direct `Allow` dispatch in `routeToolCall` and approved `Ask`
  dispatch through `approvePendingProjectToolCall ->
  executeConfirmedProjectToolCall` both enter the same
  `executeAuditedToolCall`; the appended catalog row resolves its target as
  `SelectedSsh`. There is no unaudited Safe Action dispatch.
- Compositional runtime evidence: the already observed pre-fix image parsed all
  five fixed mappings, passed canonical pending save/load/clear before and after
  one direct-Allow production execution, and reached the valid pre-cancel,
  invalid audited and Ask paths. Exact project/chat, pending and output-log
  cleanup and restoration passed. The disposable firmware diagnostic is not
  retained and no new Device/Web run is part of this correction.
- Pending/audit ownership: the only observed Ask failure was the shared
  16-byte canonical field-name reader in `loadPendingToolPreview`. P4-04 commit
  `439f630203c92b59b7e38c7223d35b0bbe64de85` replaced it with the exact
  production 23-byte helper and directly host-tested that boundary. Approved
  Ask then continues through the unchanged `executeAuditedToolCall` path proven
  by static ownership and predecessor evidence.
- Cleanup: the five commands create no remote mutation fixture. The pre-fix
  compositional run removed every exact-owned project/chat, pending call and
  output log, verified repeated absence, and restored original inventories and
  selection. No retained diagnostic or fixture remains in final ownership.

### Forbidden effects

- No shell classifier, regex/prefix inference, user-defined preset, editor,
  macro, persistence schema, action framework or background executor.
- No Device/Web terminal policy change, Device/Web controls, P4-14 ceiling
  implementation, host-key management change, retry or recovery framework.
- No credentials/private-key bytes or their paths in model schemas/results,
  pending records, previews, audit, serial, logs or Git.
- No action can accept model-controlled command bytes or mutate remote state;
  `ssh_command` must not become reachable through `SshRead`.

### Expected write set

- `.agents/P4_TRACEABILITY.md`.
- `firmware/CardputerAssistant/src/tool_catalog.h` and `.cpp`.
- `firmware/CardputerAssistant/src/api_client.cpp`.
- `firmware/CardputerAssistant/src/tool_router.cpp`.
- `firmware/CardputerAssistant/src/ssh_tool.h` and `.cpp`.
- `firmware/CardputerAssistant/src/pending_tool_call.cpp`.
- `tests/host_tests.cpp`.

No Web asset, policy codec, Settings, storage, SSH client, SFTP/transfer,
workspace, profile/key or known-host file is in the P4-08 write set.
`CardputerAssistant.ino`, `SerialDiagnostics.ino`, and `SshTools.ino` are also
excluded: the disposable `SSHSAFEACTIONTEST` declaration, selector and helper
block are removed before closure.

### Retained compositional runtime evidence

The following observations came from the now-removed disposable diagnostic.
They remain evidence about the shared production boundaries it called, not a
retained firmware test or a claim of post-correction Device acceptance.

- The initial retained diagnostic attempted five direct actions plus one Ask and
  produced no completion line within the 420-second host timeout. The host then
  re-opened COM8 and immediately observed PONG, so this was classified as
  over-broad diagnostic latency, not reset/freeze or a proven production
  deadlock. No production behavior changed.
- Per the proportional-test and no-repeat rules, the retained diagnostic was
  narrowed to parse/verify all five mappings plus one representative direct
  Allow and one representative approved Ask. This preserves every distinct
  runtime boundary while removing four duplicate SSH connection cycles.
- The narrowed run then reached its 240-second host limit and again returned to
  PONG without reset. The limit exactly overlapped the serial wrapper's blocking
  network readiness wait plus two 60-second SSH deadlines, while final-only
  output hid the active stage. The material diagnostic-lifecycle pivot removes
  that extra readiness wait for this selector and emits only non-secret bounded
  stage names; SSH production deadlines and behavior remain unchanged.
- The approved final diagnostic-only pivot compiled to a 3,434,800-byte image
  with SHA-256 `DEF0BF2537433F3FFA4E898EA9C1C31AAB0A75C45BD4565B738E693F9ED1E8A6`.
  `build.options.json` resolved the exact Cardputer FQBN and only the pinned
  M5Stack ESP32 `3.2.1` hardware path; COM8 upload wrote all 3,434,800 bytes and
  verified the flash hash.
- The single staged selector observed `direct_allow`, then `ask_prepare`, and
  failed before `ask_execute` with `Failed to read JSON field name: JSON string
  field exceeds its byte limit`. Static call ordering places the error in the
  existing active project/chat metadata load inside `savePendingToolCall ->
  buildPendingToolCall`, before `storePendingToolCall`; no pending-call mutation
  occurred. The representative direct Allow and its exact output-log cleanup had
  already completed, and no remote mutation fixture exists.
- Post-failure `STATUS` proved `microsd=ready`, `chats=ready`, `files=ready`,
  `reset_reason=1`, free heap 122,216 bytes, minimum heap 53,116 bytes, largest
  block 55,284 bytes and stack margin 5,748 bytes. The device remained responsive
  and user state was not reset or replaced.
- **External ownership block started 2026-08-31:** the approved one-build,
  one-upload, one-selector budget is exhausted. The unresolved observation is
  whether the existing user project/chat metadata read is a diagnostic-fixture
  dependency or a foreign storage/parser regression. No further build, selector
  or production change is permitted until Architect assigns that ownership.
- Architect classified that observation as a diagnostic dependency and required
  an exact-owned canonical project/chat fixture. Authenticated setup passed with
  one baseline project, two baseline chats and eleven baseline workspace files;
  exact project/chat deletion, repeated absence, original selection and all
  three inventories were restored successfully after the attempt.
- The attempted selector produced no stage while Web Console was active;
  `STATUS` was likewise ignored, while `EXIT` was accepted and stopped the
  console. This proves a harness lifecycle error: the active console serial loop
  did not dispatch ordinary diagnostic commands, so no P4-08 production path was
  exercised. The fixture and non-secret ledger were removed. A corrected
  no-build lifecycle (HTTP setup, stop Console, selector, restart Console,
  cleanup/restore) is awaiting Architect authorization before one retry.
- Architect authorized one corrected-lifecycle retry without rebuild/upload.
  With a fresh canonical exact-owned project/chat selected and Web Console
  stopped, the existing selector reached `direct_allow`, then `ask_prepare`, and
  failed identically after 5,321 ms: `Failed to read JSON field name: JSON string
  field exceeds its byte limit`. This disproves the arbitrary-user-metadata
  hypothesis and is concrete evidence that the existing canonical project/chat
  storage reader blocks `savePendingToolCall -> buildPendingToolCall` before Ask
  persistence. P4-08 production code remains frozen; ownership requires a
  separately approved completed-row reopen.
- Corrected-run cleanup passed: only the exact-owned project/chat was deleted,
  repeated absence was verified, original project/chat selection and
  project/chat/workspace inventories were restored, the non-secret ledger/helper
  were removed, and Web Console stopped. Final PONG/STATUS observed
  `microsd=ready`, `chats=ready` with two original chats, `files=ready`,
  `reset_reason=1`, free heap 100,404 bytes, minimum heap 27,512 bytes, largest
  block 32,756 bytes and stack margin 5,172 bytes. These values are retained as
  failed-run observations, not P4-08 closure evidence.
- Architect did not yet assign a completed-row reopen because Web selection had
  loaded the same canonical fixture and P3-06 previously proved pending save on
  device. One final diagnostic-only A/B/C pivot is authorized: A separately
  marks canonical project load, chat load and exact Ask pending save/load/clear
  before any Safe Action; B runs one existing direct-Allow action; C immediately
  repeats those exact marked operations. Production reader/schema, pending,
  policy and executor code remain frozen. A failure assigns persistence/pending
  ownership; A pass with C failure assigns a P4 sequencing/resource regression;
  both passing resumes the existing P4-08 acceptance within the same run.
- The one A/B/C image used 3,436,786 flash bytes and 65,628 global-RAM bytes;
  the 3,436,976-byte binary had SHA-256
  `EBE684CE29CBE39280AE0A775C86E4EA72A07C7F2833E73CEA1381FD09D4466F`,
  resolved only the pinned M5Stack ESP32 `3.2.1`, and uploaded with verified
  flash hash. A passed project load, chat load and pending save/load/clear; B
  passed the direct-Allow production route; C immediately repeated every A
  operation and passed. The existing acceptance then failed after `ask_prepare`
  and before `ask_execute`.
- Exact static localization assigns that failure to P4-04 preview handling:
  `readCanonicalStringArgument` passes a 16-byte field-name bound, while the
  canonical P4-04 object contains the 23-byte field
  `max_inline_output_bytes`. A/C do not invoke preview, whereas
  `loadPendingToolPreview` does, matching the observed boundary exactly. No
  production correction is made until Architect explicitly reopens P4-04.
- A/B/C cleanup passed: exact fixture project/chat and pending/output were
  absent, repeated absence and original project/chat/workspace inventories and
  selection were restored, and the helper/ledger were removed. Final status was
  `microsd=ready`, two original chats, files ready, `reset_reason=1`, free heap
  100,428 bytes, minimum heap 32,156 bytes, largest block 32,756 bytes and stack
  margin 4,452 bytes. These remain diagnostic observations, not closure values.

### Architect correction and closure proof

- Restore the pre-P4-08 cancellation order in both `executeSshTool` and
  `executeSshSafeActionTool`: validate the exact tool name, return the existing
  pre-connection canceled result when already canceled, then parse arguments.
  The shared `executeSshCommand` check remains the guard between parse and
  connection. No other executor behavior changes.
- Remove the full `SSHSAFEACTIONTEST` declaration, serial selector/result block
  and helper/test block. They are disposable evidence and have no final commit
  ownership.
- Closure proof is limited to `git diff --check`; the existing strict
  host/static suite covering the exact five mappings, SshRead-only exposure,
  full Safe Action/SSH confirmation matrix, pending authority/preview/audit
  ownership and cancellation order; and one exact pinned M5Stack ESP32 3.2.1
  compile for final flash, global RAM and binary hash.
- No COM8, upload, Browser, HTTP, fixture, reset, power-cycle, card action,
  serial probe or new diagnostic is authorized. No new Device latency is
  measured or claimed.
- Residual risk accepted at P4-04 closure remains explicit: post-fix
  `loadPendingToolPreview` was not observed on Device because the separate
  HWCDC path lost readiness and the Web handler was inactive. The exact shared
  helper defect was localized by pre-fix A/B/C, corrected and directly tested
  in production code by P4-04; P4-08 adds no separate preview implementation.

### Corrected closure evidence

- `git diff --check` passed. Static ownership checks proved that the three
  `SSHSAFEACTIONTEST` firmware files have no remaining diff, both public
  executors contain exactly one name-check -> cancel -> parse sequence, the
  shared executor retains its between-parse-and-connection cancel guard, and
  the existing pending authority/preview and audited-router boundaries remain.
- The CI-equivalent C++17 `-Wall -Wextra -Werror` host suite passed. It exercises
  the exact five catalog ID/command pairs, unknown catalog ID and noncanonical
  schema names, SshRead-only exposure, the Safe Action
  Allow=None/Ask=PolicyAsk confirmation matrix, unchanged mandatory
  `ssh_command`, and catalog/request-plan bounds. Extra-field, free-command and
  out-of-range Safe Action parser cases belong only to the earlier removed
  Device diagnostic's compositional evidence, which reached Ask after those
  checks. The Linux ELF and Windows launcher were exact-owned temporary files
  and were removed.
- One compile-only build used the exact FQBN
  `m5stack:esp32:m5stack_cardputer:FlashSize=8M,PartitionScheme=custom` and
  resolved only M5Stack ESP32 `3.2.1`. Sketch use is 3,427,766 bytes; globals
  are 65,628 bytes; the 3,427,952-byte app binary has SHA-256
  `A51AFDE91A1CA378651B4C898555E6E010092DA0E95F7E2EA08750FCADF2E6C9`.
- Final unstaged ownership is exactly nine files: this trace, `api_client.cpp`,
  `pending_tool_call.cpp`, `ssh_tool.cpp`, `ssh_tool.h`, `tool_catalog.cpp`,
  `tool_catalog.h`, `tool_router.cpp`, and `tests/host_tests.cpp`. The index is
  clean. No COM8, upload, Web/HTTP/Browser, fixture, serial probe, reset,
  recovery or Device latency measurement occurred during the correction.
- Architect personal closure review returned explicit GO for this exact diff
  and evidence. P4-08 is completed; P4-11 remains pending until this row's
  commit is published and its exact remote SHA is verified.

## P4-11 design gate

**Status:** completed after Architect personal closure GO
**Started:** 2026-08-31 12:12:37 +03:00.
**Completed:** 2026-08-31 13:04:48 +03:00.
**Pre-edit review:** Fresh bounded read-only review returned `GO`; the reviewer
confirmed the direct-call/race guard, checked staged cleanup, 16-KiB boundary,
existing atomic crash semantics, exact write set and proportional proof ownership.

### Scope lock

- ROADMAP Phase 4 requires explicit host-key change handling and acceptance that
  a host-key mismatch always blocks connection.
- The limit decision keeps the existing 16-KiB known_hosts store. The user
  removed pagination/rotation; no list/clear manager or new storage format is
  permitted.
- P4-17 owns the later authenticated exact-selected-host Web forget control and
  visible consolidated Web journey. P4-20 owns real changed-boundary mismatch
  E2E, exact test-host deletion and byte-for-byte preservation of unrelated
  entries. P4-21 only repeats final regression.

### Inventory and ownership

- Authority is /assistant/ssh/known_hosts, bounded by
  kMaximumKnownHostsBytes = 16384; one tab-delimited line stores host, port and
  a validated SHA-256 fingerprint. No credential or private key is stored
  there.
- initializeSshStorage() and every mutation reuse recoverAtomicSdFile(). The
  existing .tmp/.bak primitive makes the target authoritative when present,
  restores .bak when the target is absent, and removes stale .tmp.
- loadTrustedSshFingerprint() rejects a directory, an authority larger than
  16 KiB and an invalid selected fingerprint. checkTrustedSshHost() returns
  found/matches without mutating state.
- trustSshHost() currently rewrites a matching line even when its fingerprint
  differs and can append a staged file beyond 16 KiB before commit. Those are
  the two active-row defects.
- Model ssh_command and model SFTP already close and fail before authentication
  when trust is absent or mismatched. Pending confirmation hashes the current
  stored trusted fingerprint into selected SSH authority identity.
- Manual Device ensureSshConnection() and the Web SSH worker currently route
  both unknown and mismatched hosts into the same trust prompt. Device profile
  settings already expose exact-host forget; Web forget remains P4-17.
- The installed M5Stack ESP32 3.2.1 File API exposes flush() and current size().
  The existing staged-file primitive commits only target + ".tmp" through
  target -> .bak, staged -> target, then checked backup cleanup.

### Minimal design and transitions

- Keep the existing file, line format, public APIs and SshTrustResult semantics.
- In trustSshHost(), while copying the current authority, validate any exact
  host/port fingerprint. A different stored fingerprint fails closed, removes
  only the staged .tmp, preserves the old target and requires explicit
  exact-host forget before a later reconnect can trust the replacement.
- After flushing the staged file, read its vendor File::size() before commit.
  Size above 16 KiB fails explicitly, removes only .tmp, and leaves the old
  authority unchanged. Exactly 16 KiB remains valid.
- Device and Web connection owners treat found && !matches as an immediate
  explicit mismatch error, close the current connection and never enter the
  trust/authentication continuation. Only !found retains first-host trust.
- No automatic replacement, retry, wildcard cleanup or profile-delete coupling.

### Crash and cleanup ownership

- Before .tmp creation or on mismatch/overflow: target remains authoritative;
  checked cleanup removes only .tmp. Cleanup failure returns an error and boot
  or the next mutation retries the existing bounded recovery.
- Crash while .tmp is written: target remains authoritative and boot removes
  .tmp.
- Crash after target -> .bak but before staged -> target: boot restores .bak
  and removes .tmp.
- Crash after staged -> target: new complete authority is authoritative; boot
  removes stale .bak. No history scan or recovery framework is added.
- SD absence/corruption/oversize is an explicit lookup or mutation failure and
  therefore blocks connection.

### Frozen proof matrix

- Static/source: every model, SFTP, Device and Web consumer closes/fails before
  authentication on mismatch; unknown-host trust is unchanged.
- Static/source: direct trustSshHost() cannot replace a different existing
  fingerprint; mismatch and staged-size overflow preserve target authority and
  own checked .tmp cleanup; exactly 16 KiB may commit.
- Static/source: no pagination/rotation, list/read route, free-host forget API,
  profile-delete coupling, schema/policy or credential change.
- One exact pinned M5Stack ESP32 3.2.1 compile proves firmware compatibility and
  records flash/global RAM/binary hash. No upload or Device/serial recovery
  chain is needed; P4-20 owns the real mismatch and exact-forget runtime
  acceptance.
- Independent code/security review is required before closure because this is
  authoritative persisted security state.

### Forbidden effects and expected write set

- No new framework, storage schema, manager, pagination, rotation, background
  task, retry, recovery engine, trust-on-mismatch path or automatic forget.
- No Web asset/UI control, API read route, diagnostics, fixtures, tests that
  mutate user trust, or exposure of fingerprints to the model.
- Expected files only:
  .agents/P4_TRACEABILITY.md,
  firmware/CardputerAssistant/src/ssh_client.cpp,
  firmware/CardputerAssistant/SshTools.ino, and
  firmware/CardputerAssistant/src/web_console.cpp.

**Independent pre-edit review:** one bounded reviewer returned `GO` on this
frozen design, proof matrix, non-goals and exact write set before production
edits began.

**Code-review correction:** The fresh read-only reviewer found one active-row
TOCTOU path: a host-key change between the Web worker check and the trust POST
was rejected by the central guard, but the handler retained its pre-auth
connection and `AwaitingTrust` state. The bounded correction closes and clears
that connection on every failed trust mutation, publishes `Failed`, and derives
the changed-key status from the still-authoritative store. The stale trace-only
pre-edit-review wording was corrected; no other blocker was reported.

### Implemented boundary and evidence

- The row-owned write set is exactly this trace plus `ssh_client.cpp`,
  `SshTools.ino` and `web_console.cpp`; generated artifacts and unrelated files
  are excluded. Numeric diff snapshots and self-referential patch fingerprints
  are intentionally not retained as row evidence.
- `trustSshHost()` now validates every exact host/port entry while streaming.
  A different or invalid selected fingerprint closes both files, invokes the
  existing checked bounded recovery to remove only the staged `.tmp`, preserves
  the authoritative target and returns an explicit blocked/forget-required
  error. Duplicate exact-prefix lines are all scanned before commit.
- The staged file is flushed and measured before atomic commit. Values above
  16,384 bytes fail with checked staged cleanup; exactly 16,384 bytes remains
  legal. No file format, public API, trust-result semantics or recovery owner
  changed.
- Device changed-key state closes and fails before the first-host trust prompt
  or authentication. Web changed-key state records mismatch, clears
  `AwaitingTrust`, closes and clears the pre-auth connection/credentials and
  publishes `Failed`; only an unknown host reaches first-host trust. A trust-POST
  TOCTOU failure performs the same fail/clear transition after re-reading the
  still-authoritative exact trust state. Existing model SSH and model SFTP
  mismatch paths remain fail-closed before authentication.
- `git diff --check`, the exact four-path write-set check and strict static
  ordering/invariant checks passed across direct trust, Device, Web, model SSH
  and model SFTP. The focused TOCTOU check proved trust failure -> authoritative
  recheck -> mismatch status -> clear -> `Failed` -> HTTP response.
- After the concrete reviewer correction, one exact compile-only build used
  FQBN `m5stack:esp32:m5stack_cardputer:FlashSize=8M,PartitionScheme=custom`
  and the sole resolved M5Stack ESP32 core `3.2.1`. Sketch use is 3,428,962
  bytes; globals remain 65,628 bytes; the 3,429,152-byte binary has SHA-256
  `6AF3DE3E211B7E657CF77BF37FA4AD762159AF3920B1D40BF89A01F0EC273E3F`.
- The fresh independent code reviewer returned initial STOP only for the Web
  trust-POST TOCTOU state and stale trace wording. Its single bounded follow-up
  returned `GO` after both corrections; the reviewer was then closed.
- No upload, COM8, Device/Web/HTTP action, fixture or persisted mutation occurred,
  so exact-owned cleanup is not applicable and user SD/NVS/selection state is
  unchanged. Globals are unchanged from the prior build; no runtime latency or
  heap result is claimed.
- Residual proof is explicit: real mismatch blocking, exact selected-host forget
  and byte-for-byte unrelated `known_hosts` preservation remain P4-20 runtime
  ownership; P4-17 owns the authenticated Web forget control and final visible
  profile/security journey. P4-11 adds no such UI or runtime fixture.

**Architect personal closure GO:** The Architect personally reviewed the exact
current four-path `+213/-6` diff, corrected trace inventory, requirement/non-goal
mapping, every trust producer/consumer, installed M5Stack ESP32 3.2.1
`File::flush()`/`File::size()` semantics, atomic target/`.tmp`/`.bak` ownership,
Device/Web/model/SFTP fail-before-auth paths, the Web trust-POST TOCTOU
correction, compile/resources and the absence of a cleanup obligation. The
accepted residual remains exclusively P4-17/P4-20 ownership; no runtime claim
is added by this row.

**Publication:** local row checker passed for the exact four allowed paths.
Commit `9a90edeef6d76ae122ed82b94154a5dafc60d864` has exact required
Author/Committer and authenticated GitHub MCP resolves
`feature/phase-4-ssh-remote-workspace` to that exact SHA. The publication
report was sent to Architect before P4-13 activation.

## P4-13 design gate

**Status:** completed after Architect personal closure GO
**Started:** 2026-08-31 13:10:06 +03:00.
**Completed:** 2026-08-31 13:50:07 +03:00.

### Scope lock and adjacent ownership

- ROADMAP requires documentation of current plaintext and physical-access
  limitations without claiming encryption. Encryption design, provisioning and
  any ship decision belong to the Phase 9 physical-access audit.
- This row changes documentation only. P4-15 owns executable non-addressability
  acceptance, P4-17 owns final Web SSH integration, P4-20 owns changed-boundary
  security E2E, and P4-21 owns full documentation/regression/license/secret-scan
  closure.
- No encrypted vault, wrapper, key derivation, partition/configuration change,
  secure boot, flash/NVS encryption enablement, export/backup feature, migration,
  UI/API route or production behavior is permitted.

### Inventory and primary-source evaluation

- Completed P4-01/P4-02 evidence establishes that SSH passwords/passphrases and
  opaque private-key records are held in the existing ESP32 NVS owner; selected
  secret/key bytes are loaded just in time and kept outside model/file/read API
  surfaces. Those logical access controls are not encryption at rest.
- `docs/security.md` is the linked user-facing security model. It currently says
  credentials are in NVS and microSD is unencrypted, but does not state that NVS
  is also outside any CardMind encryption claim or describe offline flash,
  modified-firmware and secure-erasure limits. `docs/README.md` already links it;
  no navigation change is needed.
- The removable microSD holds chats, workspace documents, command/terminal logs,
  temporary audio and public SSH trust data. Command output can itself be
  sensitive even though `known_hosts` fingerprints are not credentials.
- The exact project partition table has an ordinary 0x5000 `data,nvs` partition,
  no `nvs_keys` partition and no `encrypted` flag. The exact M5Stack ESP32 3.2.1
  / ESP-IDF 5.4 build config records `CONFIG_NVS_ENCRYPTION`,
  `CONFIG_SECURE_FLASH_ENC_ENABLED` and `CONFIG_SECURE_BOOT` as not set. Installed
  Arduino `Preferences` opens ordinary NVS handles (and only initializes an
  explicitly labeled partition through the ordinary partition API); it contains
  no secure-initialization path.
- Espressif ESP-IDF 5.4 primary documentation states that NVS encryption requires
  the NVS encryption configuration and key-protection scheme/key partition, that
  the partition `encrypted` flag only applies when flash encryption is enabled,
  and that external/removable storage is outside native flash encryption. This
  release neither enables nor verifies those platform protections, so it cannot
  claim them for an installed device.

### Minimal documentation design

- Expand only `docs/security.md` with a concise data-at-rest table covering NVS,
  removable microSD and transient RAM, explicitly separating logical
  non-addressability/write-only controls from physical-at-rest protection.
- State that supported CardMind builds do not enable or verify NVS encryption,
  flash encryption or secure boot; NVS secrets and removable-card data must be
  treated as plaintext to a physical attacker. Do not infer or publish any
  device eFuse value that was not measured.
- State physical consequences without overclaim: offline card reads expose SD
  content; physical flash/debug/reflash access can expose NVS secrets or run
  modified firmware; ordinary deletion/format is not certified secure erase.
- Give proportional user actions: control physical access, keep/remove the card
  separately when appropriate, revoke/rotate credentials after loss or
  untrusted access, and avoid placing secrets in workspace/remote command logs.
- Preserve existing local-HTTP and logical-boundary guidance. Explain that
  removing microSD protects only the separately retained card, not NVS, and that
  future encryption/recovery decisions remain Phase 9 rather than an implicit
  promise.

### Frozen proof and forbidden effects

- Static claim matrix maps every at-rest statement to the exact partition CSV,
  compiled sdkconfig, installed Preferences source, completed P4 persistence
  evidence and Espressif 5.4 primary documentation.
- Documentation scan must contain explicit `not encrypted/not enabled/not
  verified` limits and must not contain claims that secrets, NVS, flash, backups
  or microSD are encrypted, securely erased or protected from physical access.
- Link and Markdown checks cover the existing `docs/README.md` consumer; no
  firmware build, Device/Web action, fixture, persisted mutation, resource or
  latency claim is needed for a documentation-only row.
- Expected write set is exactly `.agents/P4_TRACEABILITY.md` and
  `docs/security.md`. `docs/ssh-sftp.md`, `docs/limitations.md`, production code,
  partition/configuration and generated assets remain unchanged; broader stale
  product-document reconciliation remains P4-21.
- One independent security/design review is required before the documentation
  edit and a fresh read-only document/security review is required before
  closure. No test may inspect or print device secrets or raw NVS contents.

**Independent pre-edit review:** the fresh read-only reviewer returned `STOP`
only because the first inventory wording incorrectly attributed global
`nvs_flash_init` to `Preferences`. The trace was narrowed to the actual ordinary
NVS handle/partition calls; the reviewer's single follow-up returned `GO`, and
the reviewer was closed before documentation edits began.

### Implemented documentation and evidence

- `docs/security.md` now separates logical access controls from encryption at
  rest and documents NVS, removable microSD and volatile-runtime exposure. It
  states that the supported build does not enable or verify NVS encryption,
  flash encryption or secure boot and makes no claim about an unmeasured device
  eFuse state.
- The physical-access section records offline SD read/modify risk, NVS/modified-
  firmware exposure, non-integrity-protected `known_hosts`, lack of a certified
  secure-delete guarantee, the limits of removing only microSD, and proportional
  revoke/rotate/reprovision guidance. It does not expose a credential, key,
  internal ID or private storage path.
- The deferred-decision section leaves encryption provisioning, key ownership,
  update, backup, migration and recovery trade-offs entirely to Phase 9. No
  encrypted vault or product behavior was added or implied.
- Final `git diff --check`, exact two-path write-set, Markdown link and static
  claim-matrix checks passed. The claim matrix rechecked the exact partition CSV,
  compiled ESP-IDF 5.4 sdkconfig, installed Preferences source and required/
  forbidden document phrases; no unsupported encrypted-at-rest, tamper-resistance
  or secure-erasure statement remains.
- A fresh independent final document/security reviewer returned `GO` on the
  actual two-file diff and was closed. No firmware build, Device/Web action,
  fixture, persisted mutation, cleanup, resource or latency measurement applies
  to this documentation-only row; user data and runtime state are unchanged.
- Residual ownership is explicit: P4-15 still proves executable secret/key/ID
  non-addressability, P4-17/P4-20 own their Web/security runtime acceptance, P4-21
  owns broad documentation reconciliation, and Phase 9 owns any encryption ship
  decision. This row does not claim per-device physical protection.

**Architect personal closure GO:** The Architect personally reviewed the exact
two-path documentation diff, ROADMAP/P9 ownership and non-goals, partition and
sdkconfig facts, installed ordinary NVS APIs, completed P4-01/P4-02 logical
controls, documentation links and every physical-access/encryption/secure-erasure
claim. The review confirmed no secret/private path/internal ID disclosure and no
production, resource or cleanup obligation.

**Publication:** local row checker passed for the exact two allowed paths.
Commit `5002d316b8ae1c0a54723d99d7abeb6622fcfbb9` has exact required
Author/Committer and authenticated GitHub MCP resolves the phase branch to that
exact SHA. The publication report was sent to Architect before P4-14 activation.

## P4-14 design gate

**Status:** completed
**Started:** 2026-08-31 13:52:41 +03:00.
**Completed:** 2026-08-31 18:28:11 +03:00.

### Scope lock

- ROADMAP Phase 4 requires per-project and per-chat SSH profile ceilings so model
  permission can use only an explicitly bounded remote profile without increasing global
  authority.
- The ceiling identity is the P4-01 non-zero opaque `uint64_t` profile ID. Its canonical
  persisted/authenticated representation is exactly 16 lowercase hexadecimal characters;
  profile name and NVS index are never authority.
- An empty project or chat ceiling inherits its parent/global selection. A non-empty
  ceiling is conjunctive: every non-empty project/chat value must parse exactly and equal
  the one globally selected, currently available profile ID. A mismatch, malformed value,
  deleted profile, recreated profile at the same index, or unavailable selected profile
  makes `SshRead`, `SshMutate`, and `SftpReadWrite` unavailable.
- A ceiling never selects or reconnects a profile. It only narrows the current global
  selected-profile authority and remains subordinate to the existing global -> project ->
  chat -> message permission hierarchy, mandatory confirmations, trust checks, pending
  authority identity, audit, timeout, output and cancellation boundaries.
- The rule applies only to model-issued `ssh_command`, fixed Safe Actions and model SFTP.
  Existing direct-user Device/Web terminal, manual SFTP and transfer authority is unchanged.

### Explicit non-goals

- No profile set/list ceiling, wildcard, name/index matching, profile auto-selection,
  fallback, role system, new capability, policy framework or permission hierarchy.
- No secret/key/password/passphrase persistence or exposure change, no SSH profile storage
  rewrite, no transaction/recovery layer and no background execution.
- No Device or WebUI controls/assets in this row; P4-16/P4-17 own those journeys. P4-14
  provides only the persisted/runtime contract and existing authenticated API fields needed
  by those later controls.
- No manual terminal restriction and no change to arbitrary-command/Safe-Action/SFTP
  classification or confirmation semantics.
- No profile ID in model schemas, prompts, chat context, tool arguments/results, audit,
  serial output or Shared-workspace project bundles.

### Inventory and ownership

- `ProjectDocument::sshProfile` already persists as project JSON `ssh_profile`, is copied by
  duplicate-project, and is currently treated by `resolveChatToolPermissions` as a blanket
  SSH disable whenever non-empty. It is a forward-compatible string field, not yet a stable
  ID contract and has no current UI writer.
- `ChatDocument` has no SSH ceiling. Project-chat metadata version 1 is written atomically by
  `writeAtomicJsonSdFile`; its reader already permits optional typed fields. The narrow
  compatible extension is an optional `ssh_profile` string that defaults empty for existing
  metadata and is emitted on the next metadata save; no format-version migration is needed.
- `saveProject` and `saveProjectChatMetadata` increment authoritative project/chat revisions.
  Their existing atomic SD replacement/recovery owns crash, SD-removal, write failure and
  retry behavior. P4-14 adds no cross-store transaction: a profile selection or metadata
  change is independently authoritative, and any intermediate mismatch denies model SSH.
- `resolveChatToolPermissions` is the single availability producer for all three SSH
  capabilities before `buildToolRequestPlan`; its consumers are Device prompt/permission/
  pending paths, voice, Web prompt/retry/pending/state paths and retained serial policy
  diagnostics. Every caller currently supplies only a Boolean `sshToolIsAvailable()` result.
- The Device capability view is not allowed to read SD/NVS while rendering. It currently
  builds a synthetic active `ChatDocument` from cached `activeChatToolPolicy` only, so the
  new chat ceiling must be cached beside the existing active-chat model/instructions/policy
  fields. `activateChat`, `createAndActivateChat`, project/chat reload and the existing
  exact-owned diagnostic create/restore assignments are the complete replacement writers;
  project switching clears the cached value. Unrelated title/model/instructions/policy/draft
  saves preserve it. Every resolver view must pass both the cached policy and cached ceiling.
- Device SSH availability is currently a cached Boolean. Its complete refresh owners are
  startup, the four provisioning-portal returns in `KeyboardNavigation.ino`, the direct Device
  SSH Tools return, and Web Console return. `runSshTool()` can create/select/edit/delete profiles
  and install a private key, so its caller must refresh after both identity and completeness
  mutations. Web Console already calls `initializeChats()` to reload active project/chat state,
  but does not refresh SSH availability after its profile CRUD handlers. P4-14 replaces the
  Boolean with the selected available profile ID, refreshes it at all seven owners, and never
  performs an SD/NVS read from a render function.
- `sshToolIsAvailable()` loads only the selected runtime profile and checks its existing
  completeness. `loadSshProfile` already obtains and revalidates the selected public summary
  before JIT-reading that selected profile's secrets; the minimal extension returns that same
  summary ID with the loaded profile so availability and identity cannot race or be paired
  across two independent loads.
- Pending SSH/SFTP records already bind project/chat revisions and a SHA-256 identity over the
  selected opaque profile ID, host, port, username, auth mode, private-key ID and trusted host
  fingerprint. Approval reloads the current project/chat, rebuilds the current request plan,
  requires the schema to remain included and revalidates the authority identity; P4-14 does
  not alter the pending format.
- The authenticated Web chat state already carries project/chat policy metadata. Existing
  project and chat settings routes are authenticated/CSRF-protected persistence owners; they
  can carry optional canonical ceiling IDs without a new route. P4-16/P4-17 later add controls.
- `handleProjectSettingsRawComplete()` currently saves from `activeProject` and refreshes only the
  project summary list. Because `saveProject()` derives but does not write back the incremented
  revision, a second save in the same Web session can reuse a stale revision. P4-14 owns one
  handler-local canonical project reload/assignment after a successful save; `saveProject()` and
  its other callers remain unchanged. The chat settings handler already reloads active chat after
  save and needs no corresponding rewrite.
- Project bundles live in Shared workspace and can be read by workspace tools. They must not
  export local opaque authority IDs. Existing project bundle `ssh_profile` remains present as
  an empty compatibility field; import clears it, and chat ceilings are not exported/imported.
  Imported chat SSH policies remain forced Off by the existing import boundary.
- Profile deletion does not rewrite project/chat metadata. The immutable stale ID remains
  visibly configured but unavailable, so deletion/recreation at the same index cannot redirect
  authority. Metadata cleanup remains owned by explicit project/chat edit or deletion.
- Device prompt execution constructs the request plan and synchronously invokes the model/tool
  callback on the main loop stack; its cancellation callback only calls `M5Cardputer.update()`
  and reads Escape, and does not dispatch keyboard menus or profile mutation. Web prompt/retry
  execution and direct tool dispatch remain inside one synchronous `server.handleClient()`
  callback; no nested `handleClient()` exists, and all profile save/select/delete handlers run
  only as separate callbacks. The Web SSH worker does not mutate profiles. Therefore no profile
  selection/edit/delete can interleave between plan construction and direct-Allow JIT load.
  Ask remains the only cross-turn path and retains its existing revision/authority revalidation.

### Minimal design

1. Add exact pure profile-ID parse/format helpers to the already existing, host-linked
   `tool_policy.h/.cpp`, shared by persistence settings, authenticated state, runtime policy and
   retained host tests. The parser accepts exactly 16 lowercase hexadecimal characters representing
   a non-zero `uint64_t`; the formatter emits that exact representation. Settings accept empty or
   any parser-valid ID independently of the currently selected profile. No Web/storage/test caller
   reimplements the length/hex/zero rule, and no new codec module or build plumbing is introduced.
2. Add one host-testable authority predicate to existing `tool_policy`: parse each non-empty
   project/chat ceiling through the shared codec and conjunctively compare it to a non-zero
   available selected ID. Invalid or mismatched input returns false; it never mutates policy and
   remains separate from settings validation.
3. Extend the existing selected-profile JIT loader to return the already verified public profile
   ID with the runtime profile. Expose `sshToolAvailableProfileId()` returning that ID only when
   the same profile is complete, otherwise zero; retain `sshToolIsAvailable()` as the Boolean
   compatibility wrapper for direct-user/non-policy consumers and clear loaded secret strings.
4. Replace the Device cached availability Boolean with the available selected profile ID. Cache
   the active chat ceiling beside its other active fields, update it on every active chat load,
   create, reload, clear and exact-owned diagnostic replacement, and refresh the selected ID at
   startup, all four provisioning returns, direct Device SSH Tools return and Web Console return.
   Device display and request-plan construction use these same caches; no render performs
   persistence I/O. Static ownership proves every non-diagnostic profile selection/profile CRUD/
   private-key installation path returns through one of these refresh sites.
5. Replace only the router's Boolean SSH availability input with the available selected profile
   ID. Use the pure ceiling predicate once and apply its Boolean result identically to `SshRead`,
   `SshMutate` and `SftpReadWrite`; leave `resolveToolPolicy` unchanged.
6. Add optional chat `ssh_profile` persistence with empty legacy default and atomic existing
   writer ownership. Preserve the existing project field and tighten only new authenticated
   writes to empty-or-canonical values; malformed legacy persisted values remain readable but
   fail closed in policy resolution.
7. Expose project/chat ceiling strings and the formatted selected available profile ID only in the
   authenticated chat-state response. Existing project/chat settings handlers accept optional
   ceiling values: missing preserves, empty clears, and any shared-codec-valid non-zero ID saves
   even when it is not currently selected; malformed input fails before save. After each successful
   project settings save, reload and assign the canonical stored project in that handler before any
   subsequent request; this preserves the saved ceiling and current revision without changing
   `saveProject()` globally. Web displayed capability state and request plans both resolve from the
   same current available ID.
8. Keep opaque IDs out of portable Shared-workspace bundles by exporting/importing an empty
   project compatibility field and not adding a chat ceiling field to bundle records.
9. Do not add selected identity to `ToolRequestPlan`: direct-Allow execution cannot interleave
   profile mutation under the existing single main-loop/WebServer scheduling described above.
   Retain a static scheduling proof for that exclusion; if a re-entrant mutation owner is later
   found, stop before implementation rather than silently relying on this design.

### Frozen proof matrix

- Pure host cases: unavailable/zero selected ID denies; both empty ceilings preserve existing
  availability; project-only, chat-only and both matching the selected ID allow; either mismatch
  denies; malformed length/uppercase/non-hex/zero IDs deny; changing selected ID proves stale
  delete/recreate authority cannot redirect.
- Codec host cases call the production parser/formatter directly: all non-zero uint64 boundaries
  round-trip as 16 lowercase hex; empty, short/long, uppercase, non-hex and zero reject. Settings
  validation accepts empty or any canonical non-zero ID without consulting current selection.
- Policy host cases execute the production ceiling predicate and prove its matching/mismatching
  result cannot elevate global/project/chat Off or change Ask/mandatory behavior; Files/Web remain
  unchanged. Applying that one result identically to `SshRead`, `SshMutate` and `SftpReadWrite` in
  `tool_router` is static wiring evidence; the retained host binary does not link or call the router.
- Storage/static cases: old project-chat metadata without `ssh_profile` loads as empty; save/load
  retains canonical chat ID; project and chat metadata revision changes invalidate a pending
  approval; all resolver callers provide the same selected available ID; profile name/index are
  absent from ceiling matching.
- Revision integration cases: two successive authenticated project ceiling updates/clear in one
  Web session each reload the canonical project and advance its stored revision; a pending call
  created before either change is stale. The saved ceiling survives each reload. Existing chat
  save/reload behavior advances chat revision without a broad persistence rewrite.
- Disposable read-only Device/static inventory: every active-chat replacement refreshes policy and ceiling together;
  unrelated saves preserve the cached ceiling; startup, four provisioning returns, direct Device
  SSH Tools return and Web Console return refresh the selected available ID; every non-diagnostic
  profile/completeness mutation reaches one of those sites; no render path loads profile or chat
  persistence.
- Resolver consistency host/runtime cases: for matching, project-mismatch and chat-mismatch inputs,
  Device and Web displayed capability resolution has the same SSH availability and included schema
  result as `resolveChatToolRequestPlan` built from the same project/chat/current-ID state.
- Authenticated API/static/Web cases: project/chat state returns only the two non-secret IDs;
  settings preserve on omission, clear on empty, reject malformed values and save any canonical
  non-zero value without requiring current selection; no Web asset/control is added in this row.
- Security/static cases: no ceiling key appears in model schema/request/result producers, audit,
  serial result or bundle output; bundle import/export clears local authority; pending authority
  still binds selected ID and trusted fingerprint.
- Disposable read-only scheduling inventory: Device plan construction through direct execution is one main-loop call
  stack; Web plan construction through direct execution is one non-reentrant `handleClient()`
  callback; cancellation, model streaming and the Web SSH worker expose no profile CRUD call.
  Therefore direct-Allow cannot switch authority, while Ask continues to revalidate current
  revisions and selected authority before execution.
- Focused runtime after review: one collision-checked exact-owned project/chat and two existing
  profile IDs prove matching access, project mismatch, chat mismatch, stale ID after selection
  change, reboot persistence, authenticated state/update behavior and exact restore/cleanup. The
  test must not change manual terminal authority or require recovery escalation.
- Resource/build: strict host/static/Web checks first, then one exact pinned 3.2.1 build/upload and
  only the changed policy/persistence boundary if normal Device readiness is available. Measure
  free heap, largest block, stack and policy-resolution latency against the retained idle/active-SSH
  baselines; no reset/freeze and no material latency regression.

### Forbidden effects

- No ceiling may select a profile, turn an unavailable capability into available, override Off/
  Ask/mandatory confirmation, or survive a current-ID mismatch by matching name or index.
- Authenticated settings validation must not require a ceiling to equal the current selection;
  only runtime authority matching compares against the current available selected ID.
- No invalid/stale metadata may fall back to the globally selected profile; it fails closed.
- No secret or key bytes/path, private-key binding, trusted fingerprint or internal authority hash
  may enter project/chat metadata, authenticated state, bundle, model context, logs or diagnostics.
- No bundle/import, failed save, SD removal, cancellation or reboot may broaden authority or leave
  a partial ceiling grant. Existing atomic metadata remains the sole recovery owner.
- No Device/Web UI redesign, manual terminal behavior change, new storage/policy abstraction,
  profile mutation, broad migration or unrelated hardening.

### Expected write set

- `.agents/P4_TRACEABILITY.md`.
- `firmware/CardputerAssistant/src/app_types.h`.
- `firmware/CardputerAssistant/src/project_chat_storage.cpp`.
- `firmware/CardputerAssistant/src/project_bundle.cpp`.
- `firmware/CardputerAssistant/src/tool_policy.h` and `tool_policy.cpp`.
- `firmware/CardputerAssistant/src/tool_router.h` and `tool_router.cpp`.
- `firmware/CardputerAssistant/src/ssh_client.h` and `ssh_client.cpp`.
- `firmware/CardputerAssistant/src/ssh_tool.h` and `ssh_tool.cpp`.
- Existing Device cache/router consumers only: `firmware/CardputerAssistant/CardputerAssistant.ino`,
  `DeviceMenus.ino`, `KeyboardNavigation.ino`, `VoiceAndSpeech.ino`, `SerialDiagnostics.ino` and
  `src/web_console.cpp`. `KeyboardNavigation.ino` owns both the four provisioning refreshes and the
  direct SSH Tools return refresh; `web_console.cpp` owns the handler-local canonical project reload.
- Existing authenticated state owner `firmware/CardputerAssistant/src/web_console_state.h` and
  `web_console_state.cpp`.
- Existing authenticated route/header owner
  `firmware/CardputerAssistant/src/web_console_routes.cpp`; this path was added only after the
  runtime A/B proved the project raw request boundary dropped the optional query argument and the
  installed WebServer 3.2.1 `hasHeader` contract proved that an empty collected header cannot encode
  clear without an explicit non-empty envelope.
- Retained proportional checks only: `tests/host_tests.cpp`, which calls the production codec and
  conjunctive authority predicate. Call-site, cache-owner and scheduling ownership use a disposable
  read-only inventory; authenticated settings/state/revision behavior uses the focused runtime path.
- Any need for another production file is a scope signal: stop and simplify or reclassify before
  expanding this list.

### Review gate

- Architect pre-edit review returned `STOP` on 2026-08-31 for three concrete omissions: the Device
  active-chat/selected-ID cache lifecycle, separation of canonical settings codec from runtime
  authority matching, and the stale direct-Allow scheduling gap. The corrected frozen design above
  now inventories every cache refresh owner (including four `KeyboardNavigation.ino` returns and
  Web Console return), uses one shared codec without selection-dependent write validation, and
  records the non-reentrant Device/Web scheduling exclusion. Production edits remain forbidden
  pending one bounded Architect re-review of this consolidated correction.
- That bounded re-review returned `STOP` on two remaining omissions: the direct Device SSH Tools
  return was missing from cache refresh ownership, and repeated project settings saves could reuse
  stale `activeProject.summary.revision`. The final correction above adds the exact SSH Tools return
  (including private-key installation), proves all non-diagnostic mutation returns, and confines
  canonical project reload/assignment to the successful Web handler. Production edits remain
  forbidden pending final bounded Architect GO on this gate.
- Architect then simplified ownership before implementation: the two pure canonical ID helpers and
  conjunctive matcher stay in the existing host-linked `tool_policy.h/.cpp`; no
  `ssh_profile_id.*` module or workflow edit is permitted. This does not change the cache or revision
  corrections. Final bounded Architect pre-edit review returned `GO` on 2026-08-31 for this exact
  frozen design and write set. Any newly discovered production path or required file reopens the
  minimality gate before expansion.
- Architect review of the initial evidence patch returned `STOP` on two concrete proof/ownership
  defects. First, the added P4-14 block in `tests/web_console_ui_test.mjs` read multiple production
  sources and asserted literal fragments, exact cache-refresh counts and source ordering instead of
  executing the changed boundary; the complete P4-14 imports/reads/assertions are absent and were
  not replaced. Second, the two `runUiSearchEndToEndTest()` active-chat replacement paths refreshed
  the SSH ceiling but not the matching cached tool policy; each now assigns policy and ceiling from
  the same created/restored chat object. The retained host cases execute the production codec and
  predicate, disposable read-only inventory owns call-site/cache/scheduling proof, and the frozen
  authenticated runtime scenario owns settings/state/revision observations. Expensive evidence is
  paused pending Architect review of this corrected mapping and exact diff.
- Architect correction review then returned `GO` for continued verification. Strict production-helper
  host tests and the unchanged WebUI smoke passed, and the current pinned 3.2.1 source compiled at
  3,436,930 flash bytes with 65,668 global bytes. The first unattended runtime attempt retained normal
  Device/Web readiness and completed exact-owned project/profile/pending cleanup, but its first
  post-save assertion combined project ceiling, chat ceiling and selected available ID and therefore
  could not assign the mismatch to production or harness state. The pre-A/B assumption that query
  arguments remained parsed for a `text/plain` POST was disproved by the narrower probe and installed
  ESP32 WebServer 3.2.1 source: the raw-handler branch does not call `_parseArguments(searchStr)`, query
  arguments are parsed only in non-raw/form branches, and `hasArg` may observe stale prior arguments.
  No production defect was assigned until A/B localized the raw request input boundary. The failed
  attempt is classified as insufficient harness observability and is not repeated unchanged. One
  narrower disposable A/B probe may distinguish project
  persistence, chat persistence and selected-ID availability before the full runtime proof resumes.
- The authorized narrower A/B probe retained normal project/profile cleanup and separately observed
  `project_saved=false`, `chat_saved=true`, and selected available profile ID unchanged/non-zero before
  and after both saves. It therefore assigns the failure to the authenticated project raw-settings
  input boundary before `saveProject`, not to profile availability, chat metadata persistence or state
  formatting. The exact-owned project/profile inventory was restored; the one temporary probe file
  left by a PowerShell finalizer spelling error was identified by its exact GUID path and removed with
  literal-path verification. Production remains frozen. The smallest proposed correction is one
  collected optional encoded SSH-profile header on the existing raw project-settings route: missing
  preserves, encoded empty clears, encoded canonical ID saves through the existing validator. This
  would add only existing route owner `web_console_routes.cpp` to the write set; no new route, codec,
  storage field, compatibility layer or framework is justified. Architect ownership approval is
  required before expanding the frozen path set or editing production.
- Architect ownership/design review returned `GO` for exactly one correction: collect
  `X-CardMind-Ssh-Profile-Encoded` in the existing header owner and consume only that header in the
  existing project raw-settings handler. Missing preserves; exact `v1:` clears; exact
  `v1:<16-lowerhex-nonzero-ID>` strips the prefix and then uses the existing production validator;
  every other non-empty value fails before save. The query argument becomes unsupported/ignored,
  with no dual path or fallback. This expands the frozen write set only by
  `web_console_routes.cpp`; no helper, codec, route, schema, storage field, UI asset or retained
  source-text test is authorized.
- Final-runtime holder observed the ordinary COM8 dispatcher answer one `PING` with `PONG`, then
  timed out because it incorrectly required exact equality with `WEB_CONSOLE result=ready` even
  though firmware emits `WEB_CONSOLE result=ready address=...`; canonical retained harnesses match
  that marker as a prefix. HTTP login, fixtures and model execution never started, the exact-owned
  disposable file was removed, and no serial/recovery chain followed. Architect therefore
  reclassified the event as a diagnostic harness defect: normal Device/Web readiness was not shown
  lost, and the earlier P4-17 backend-proof deferral is superseded. One HTTP-only authenticated
  lifecycle against the already-started Web Console now owns the remaining P4-14 backend evidence
  and must close the console through its existing authenticated CSRF-protected route after exact
  cleanup, with no HTTP or serial probe afterward. P4-17 retains only its real UI-control journey as
  the production header producer; it does not inherit this backend proof.
- That one HTTP-only lifecycle proved the established Web Console, authenticated login/session and
  baseline project/chat/workspace/SSH/pending/status reads were healthy, then stopped at its bounded
  `profiles` setup/discovery stage before creating the project fixture or exercising any ceiling
  header. The disposable report intentionally exposed no profile identity or credential and did not
  distinguish a capacity/precondition rejection from another operation inside that bounded stage;
  no second HTTP lifecycle is permitted to refine it. Its two-pass exact-owned cleanup and original
  selection/inventory verification passed, the reset/resource consistency check passed, the
  authenticated CSRF-protected console close passed, and the exact disposable file was removed.
  No HTTP or serial operation followed. The real header and match/mismatch runtime observations
  therefore remain unobserved pending Architect closure/evidence ownership decision; this failure
  is not evidence of a production defect.
- Final source with the collected encoded-header boundary passed the exact pinned M5Stack ESP32
  3.2.1 compile: 3,437,062 flash bytes and 65,668 global bytes, leaving 262,012 bytes for local
  variables. This supersedes the earlier pre-header 3,436,930-byte flash snapshot for final-source
  compatibility. No post-header Device runtime heap, largest-block, stack or latency measurement is
  claimed; the terminated HTTP lifecycle observed only a Boolean reset/resource consistency pass.
- Fresh bounded read-only code review returned `STOP` on evidence ownership only and found no
  row-owned production defect. It confirmed the installed WebServer 3.2.1 raw/header semantics,
  header-only minimality, producer/consumer paths and exact cleanup. Its resource-record blocker is
  resolved by the final-source compile evidence above. Its remaining blocker is the explicitly
  unobserved authenticated preserve/clear/canonical save+reload and match/mismatch runtime behavior,
  which requires an Architect canonical acceptance/ownership decision; no production correction or
  additional Device/Web lifecycle is indicated or permitted.
- Architect personal closure review also returned evidence-only `STOP`: it found no row-owned code,
  authority, secret-exposure, minimality, vendor-semantics, resource or cleanup defect in the actual
  22-path diff, but rejected completion because neither compilation/static router wiring nor the
  production-helper host cases execute the changed authenticated route. The approved materially
  narrower proof removes profile discovery/CRUD and model/tool execution: after one exact final-source
  pinned upload, it uses only the already-selected available profile ID to observe project/chat
  preserve, clear, canonical reload, malformed no-write and matching/mismatching capability state in
  one canonical Web Console lifecycle. First real readiness loss or unavailable selected-ID ends the
  path without recovery escalation.
- The exact final-source image was then uploaded once through the pinned 3.2.1 toolchain boundary:
  the 3,437,248-byte binary write completed and its flash hash verified. One subsequent canonical
  lifecycle answered the single readiness handshake, emitted the prefix-matched Web Console ready
  marker, kept one serial owner active throughout authenticated HTTP work and stopped normally after
  one `EXIT`; no reset, retry, recovery action, profile CRUD, model/tool call or remote mutation ran.
- That authenticated lifecycle observed the changed route before its only failed expectation:
  project canonical save/reload, missing-header preserve, exact `v1:` clear and canonical restore
  passed; raw-ID, `v2:` and uppercase malformed headers each returned 400 without changing the
  stored ceiling/revision; chat canonical save, omitted-field preserve, exact empty clear and
  canonical restore passed; the already-selected available profile remained canonical, non-zero and
  unchanged. A distinct canonical synthetic project ceiling was accepted and survived state reload.
- The lifecycle then stopped at the disposable assertion that every mismatched SSH capability must
  literally report `effective=unavailable, source=availability`: `sr` did not. Read-only ownership
  analysis classifies this as a proof expectation defect, not a production defect. A newly created
  chat's existing upper policy is SSH `Off`, and unchanged `resolveToolPolicy` intentionally preserves
  the stronger `Deny` and its policy source when availability is false; only non-denied permissions
  are rewritten to `Unavailable/Availability`. The shared production ceiling predicate and router
  wiring still make the mismatched availability input false for all three SSH capabilities, but this
  run did not observe a non-denied policy case and therefore does not claim the literal unavailable
  state for project/chat mismatch.
- Before surfacing that test-stage failure, the script completed both exact-owned cleanup passes,
  verified original project/chat/workspace inventory and selection plus the unchanged selected
  available profile and reset reason, and reported no cleanup error. The holder stopped Web Console
  normally and removed the exact script/stdout/stderr paths; literal absence of all three disposable
  paths was verified. No HTTP or serial operation followed. Failure-path output intentionally did not
  publish heap/largest-block/stack/latency values, so no new runtime resource measurement is claimed.
  Production remains frozen and the composite lifecycle is not repeated; Architect must decide
  whether the stronger inherited `Deny` plus direct production-predicate evidence is sufficient or
  explicitly authorize a different, narrower non-denied-policy observation.
- Architect personal closure review returned `GO` on 2026-08-31 for the exact current 22-path diff
  and evidence. It accepted the stronger-Deny interpretation: the shared P4-14 predicate makes SSH
  availability false for a mismatch, `tool_router` applies that same false value to `SshRead`,
  `SshMutate` and `SftpReadWrite`, and unchanged `resolveToolPolicy` correctly preserves an already
  stronger explicit `Deny`/policy source instead of replacing it with
  `Unavailable`/`Availability`. Production host evidence covers both precedence cases, so the
  disposable literal-state expectation was wrong while the required forbidden effect was proven:
  a mismatch cannot grant or redirect SSH authority. The review also accepted the final-source
  compile/upload evidence, authenticated project/chat route observations, exact cleanup and the
  truthful absence of failure-path numeric runtime metrics; no further Device/Web lifecycle or
  production/test correction is required.

**Publication:** local row checker passed for the exact 22 allowed paths. Commit
`46e62c2471dde26dd4e984525ea0444299f7d836` has the exact required Author/Committer, and
authenticated GitHub MCP resolves the phase branch to that exact SHA. The publication report was
sent to Architect before P4-15 activation.

## P4-15 design gate

**Status:** completed
**Started:** 2026-08-31 18:33:20 +03:00.

### Scope lock

- ROADMAP Phase 4 requires CardMind SSH password/passphrase bytes, private-key bytes,
  private credential storage locations and SSH authority internals to remain absent from model
  context, tool schemas/results, readable Web/API state, workspace tools, serial output, logs,
  diagnostics and Git. The model must not be able to address CardMind private-key storage through
  file tools.
- The authenticated configuration surface may expose only the already-approved public SSH profile
  summary and stable opaque non-secret profile ID. P4-14 project/chat ceiling metadata uses that
  public ID and does not expose key-record identity.
- This row verifies the logical non-addressability boundary implemented by P4-01/P4-02 and its
  existing consumers. P4-13 owns the truthful plaintext-at-rest/physical-access threat model;
  Phase 9 owns any encryption design or shipping decision.
- Real private-key authentication E2E remains P4-20 with the inseparable exact known_hosts cleanup.
  P4-21 owns the broad filename-only secret/license/full-regression closure scan.

### Inventory and ownership

- `ssh_client` owns the bounded private NVS credential/key records, selected-profile JIT loading,
  exact profile-to-key binding, authentication consumption and explicit in-memory clearing. No
  private storage key/path is part of its public profile-summary contract.
- `ssh_tool` and `sftp_tool` are the model execution consumers. They load only the selected profile,
  clear password/passphrase fields on every pre-auth failure and immediately after authentication,
  and construct results only from command/SFTP outcomes, bounded remote data and the P4-05 public
  command-log reference.
- `tool_catalog` and `api_client` produce the model schemas/prompt. SSH/SFTP calls carry command,
  fixed-action or remote-path/content/options fields only; they do not carry a profile selector,
  password, passphrase, private key, local private-storage identifier or local storage path.
- `pending_tool_call` binds confirmations to a SHA-256 digest of the selected authority. The durable
  and preview surfaces expose the digest/kind (plus the already-reviewed SFTP target name), not the
  digest inputs, profile ID or private-key record identity.
- `web_console_state` emits public profile name/host/port/username/auth mode, selected index,
  configured/key-installed booleans and the separately approved public available-profile ID. The
  write-only credential/key upload handlers have no matching read/download/export route.
- Workspace file tools validate a bounded relative workspace name and prepend the fixed workspace
  root internally. They cannot select NVS or the private-key owner. Model SFTP paths address the
  selected remote host, not CardMind local storage.
- Tool audit, serial diagnostics, error paths and bundle/export producers consume decisions,
  bounded public results and public profile metadata only. Reboot, SD-removal, migration and
  exact-owned key cleanup remain the already-verified P4-01/P4-02 owners; this row changes no
  persisted format or cleanup behavior.

### Minimal contract and non-goals

- Allowed outward data is limited to public SSH profile metadata/opaque profile ID on authenticated
  configuration surfaces, permission/availability state, confirmation authority digest, remote
  command/SFTP result data and public downloadable command-log references.
- Forbidden outward data is CardMind-owned password/passphrase/private-key bytes, private storage
  locations, opaque key-record IDs and unhashed authority internals. A failure must not add those
  values to its error/result/serial/audit path.
- Arbitrary remote command output and explicitly requested remote SFTP file content are the remote
  data feature itself; P4-15 does not add a content classifier, shell parser or remote-secret filter.
  This does not authorize disclosure of CardMind-owned authentication material.
- No production edit is expected. Do not add a secret manager, path denylist, output scrubber,
  schema, route, storage field, compatibility layer, retained diagnostic or general security
  framework. A concrete leak is classified to its existing owner before any write-set expansion.

### Frozen proof matrix and forbidden effects

- Run the existing strict host suite covering exact SSH/SFTP schemas, workspace relative-path
  validation, public Web SSH state, authority-hash pending validation and bounded tool results.
- Run one disposable no-echo static ownership check over the exact model schema/result, Web read,
  pending/preview, audit, serial/diagnostic and bundle producers. It reports only pass/fail boundary
  labels and file/line ownership; it never prints matched literals, values or private paths.
- Reuse unchanged real-runtime evidence from P4-01/P4-02 that authenticated state returned no
  password/passphrase, private-key bytes or key-record identity; from P4-05/P4-06/P4-08 that model
  results contained only the reviewed bounded output contracts; and from P4-14 that the authenticated
  public profile ID/ceiling surface contained only its approved non-secret identity.
- Inspect all selected-profile JIT exit paths and the private-key authentication consumer for
  explicit clearing before outward result construction. Prove file tools accept only workspace
  relative names and expose no selector for private NVS/key storage.
- Forbidden effects: no credential/key read or export, no private storage addressability, no model
  profile selection, no new persistence/migration/cleanup work, no remote mutation, no Device/Web
  state mutation and no secret-like value printed or retained by the proof.
- Expected write set: `.agents/P4_TRACEABILITY.md` only. Production, retained tests, firmware assets,
  build workflows and documentation outside this row remain unchanged unless a concrete row-owned
  defect is independently demonstrated before expansion.

Independent bounded pre-edit/proof review: **GO**. The fresh read-only reviewer found no concrete
CardMind credential/private-key/private-storage/authority-internal leak and accepted the frozen
verification-only, trace-only write set. This is design/proof GO, not row completion; the locked host,
no-echo ownership and JIT-clearing evidence still must run before closure review.

**Reboot-safe pause (2026-08-31):** the user announced an imminent system restart after the gate GO.
No P4-15 production/test/build/Device/Web/Git action or exact-owned mutation had started. No fixture,
external command process or open reviewer remains. On resume, first restore the complete visible plan
with P4-15 sole `in_progress`, then continue exactly with the frozen proof matrix; do not repeat inventory
or expand the write set.

### Proof execution finding and ownership hold

- After reboot, the full visible plan was restored and local state was reconfirmed at HEAD
  `46e62c2471dde26dd4e984525ea0444299f7d836` on the Phase 4 feature branch. The only tracked change
  remained this P4-15 trace; the three Architect-owned `.codex/agents/*.toml` files remained excluded.
- The no-echo ownership check passed the selected-profile JIT clearing, private-key auth clearing,
  tool-result, pending-hash, public Web-state, workspace confinement, model-selector and tracked-filename
  boundaries. Its initial serial check was correctly classified as over-broad: five auth-term occurrences
  had no direct `Serial` sink, and no value was printed by the checker.
- The narrowed no-echo literal classifier identified four credential-shaped diagnostic aggregates for
  ownership classification. It reported only file/function ownership and non-empty booleans; it never read
  or emitted literal values. Architect correctly stopped the first correction hypothesis because shape
  alone does not prove that public demo or synthetic fixture data is CardMind-owned secret material.
- A read-only in-memory comparison against the official `https://test.rebex.net/` service page proved that
  the P2 Unicode, command-output and SSH-demo profiles are the documented public Rebex test account and do
  not read persisted CardMind credentials. The profile-storage literals are synthetic exact-owned fixture
  data and never reach connect/auth. Those literals are not P4-15 violations.
- The same ownership pass found one actual P4-15 diagnostic consumer: `runSshProfileStorageTest` calls the
  legacy full-profile loader for the original inventory and compares password/passphrase fields, placing
  persisted user credential bytes inside a retained firmware diagnostic. No value is emitted to serial,
  but this contradicts the frozen prohibition on CardMind-owned secrets entering diagnostics. No
  Device/Web/remote mutation or fixture was started.

### Architect ownership and frozen minimal correction

**Status:** correction_implemented_proof_pending.

- Architect assigned genuine CardMind-owned diagnostic secret consumption to active P4-15. No predecessor
  row is reopened. Exact literal/call-site inventory, obtained without printing values:
  `SerialDiagnostics.ino::downloadP2UnicodeFixture`; `SshTools.ino::runSshProfileStorageTest`
  (owned profile creation, sixth-create rejection and cleanup comparison);
  `SshTools.ino::runSshCommandOutputRemoteTest`; and `SshTools.ino::runSshDemoTest`. Their only
  dispatcher consumers are the existing P2 Unicode setup, SSH profile-storage selector,
  SSH command-output remote selector and SSH demo selector respectively.
- Remove only `runSshProfileStorageTest`, its serial selector and its forward declaration. It is a completed
  P4-01 disposable diagnostic retained in firmware, loads every user profile including secrets, and is no
  longer an acceptable P4-15 consumer. P4-01's observed five/sixth/ID/JIT/delete/reboot evidence remains in
  the canonical trace; no replacement harness or runtime credential generator is needed.
- Preserve `downloadP2UnicodeFixture`, `runSshCommandOutputRemoteTest` and `runSshDemoTest` byte-for-byte.
  Their public Rebex account is part of the fixed public test-service contract; substituting an arbitrary
  selected profile would change remote paths/commands and risk touching a user host. Their retained P2,
  P4-05 and SSH transfer/demo consumers and existing trust/file cleanup remain unchanged.
- No persisted format, route, schema, storage field, helper/module, secret manager, compatibility layer,
  migration, recovery or background behavior is added. There is no new crash window: the selected profile
  store is no longer read by this obsolete selector and all retained remote diagnostics are unchanged.
- Exact expected write set: `.agents/P4_TRACEABILITY.md`,
  `firmware/CardputerAssistant/CardputerAssistant.ino`,
  `firmware/CardputerAssistant/SerialDiagnostics.ino`, and
  `firmware/CardputerAssistant/SshTools.ino` only.
- Smallest proof after edit: no-echo source ownership scan proves zero `runSshProfileStorageTest` declaration,
  definition or selector, zero full-profile secret-loader use by retained diagnostics, and byte-identical
  ownership for all three public Rebex consumers; filename-only tracked secret scan; `git diff --check`;
  existing strict host
  suite; one exact pinned M5Stack 3.2.1 compile-only build and options/resource report. No upload, COM8,
  Device/Web, remote SSH, fixture or trust mutation is required to prove removal of an obsolete diagnostic;
  unchanged product JIT/auth/cleanup behavior reuses P4-01/P4-02/P4-05 evidence.

Architect pre-edit review: **GO** for this exact correction package. The obsolete diagnostic definition,
selector and declaration were removed as one coherent patch; verification remains pending and the row is not completed.

### Corrected proof evidence

- No-echo ownership proof passed: `runSshProfileStorageTest` has zero remaining declarations,
  definitions or selectors; retained diagnostics contain no full-profile secret-loader call. The P2
  Unicode, P4-05 command-output and SSH-demo function segments are byte-identical to HEAD. No matched
  value or literal was emitted.
- The official Rebex test-service page was read only for ownership classification. An in-memory
  comparison proved all three unchanged remote diagnostics use that documented public fixed test account;
  the exact account values were neither printed nor retained in evidence.
- Filename-only tracked secret scan passed with no private-key/certificate filename. `git diff --check`
  passed before and after the blocker-only harness correction. The final changed-path set is exactly this
  trace, `CardputerAssistant.ino`, `SerialDiagnostics.ino`, `SshTools.ino` and
  `tools/device_regression.ps1`.
- The existing strict production-linked C++ host suite compiled with `-Wall -Wextra -Werror`, ran from
  a disposable WSL ELF and returned exit code zero; the ELF was removed by its exact cleanup trap.
- One exact pinned compile-only build passed with FQBN
  `m5stack:esp32:m5stack_cardputer:FlashSize=8M,PartitionScheme=custom` and one unique M5Stack core
  version 3.2.1. Sketch usage is 3,427,258 bytes; globals are 65,668 bytes with 262,012 bytes left for
  locals. The generated binary is 3,427,440 bytes with SHA-256
  `4E049AA6F5DF086B0CEE79D68ED0D2801B82C4FE004BD8B005BB8FDEFC44EF0B`.
- Compared with the accepted P4-14 compile (3,437,062 sketch bytes; 65,668 global bytes), the obsolete
  diagnostic removal reduces flash by 9,804 bytes and leaves global RAM unchanged. Product runtime,
  active-SSH heap/latency, persisted storage and Web/Device behavior are unchanged, so no new numeric
  runtime claim is made.
- No upload, COM8, Device/Web, remote SSH, fixture, trust, NVS or microSD mutation occurred. There is no
  exact-owned runtime cleanup obligation; generated build output remains ignored and outside row ownership.
- Residual: the three fixed public Rebex diagnostics still embed the vendor-documented public demo account
  by design. They are not CardMind-owned authority, cannot select/read CardMind persisted credentials and
  remain outside P4-15's forbidden-data set. The removed P4-01 disposable selector is no longer rerunnable;
  its completed historical hardware/reboot evidence remains canonical and is not restated as current proof.

**Closure status:** completed after mandatory personal Architect review.

### Independent code-review STOP

- Fresh read-only review found one concrete retained-harness consumer missed by the first inventory:
  `tools/device_regression.ps1` still includes the removed profile-storage selector in its offline/full
  case list. With the producer gone, that case can only wait for an impossible completion response and
  time out; no test was run after this finding.
- Failure class: P4-15-owned test/harness defect caused by removal of the obsolete diagnostic, not a
  production regression. Production and tests are frozen pending the single Architect owner verdict.
- Proposed smallest correction is to remove only that obsolete regression case, with no replacement
  selector or diagnostic. The completed P4-01 hardware/reboot/restoration evidence remains canonical.
  If authorized, the exact write set becomes this trace, the three already-reviewed firmware files and
  `tools/device_regression.ps1`; all other scripts/cases remain byte-identical.
- The earlier four-path/final-evidence wording is provisional and must be corrected after this blocker is
  resolved and rechecked. P4-15 remains sole `in_progress`; no staging, commit or push is permitted.

Architect owner verdict: **GO**. `tools/device_regression.ps1` was confirmed as the sole live caller of
the removed selector. Exactly that one `offlineCases` entry was deleted; no replacement or other harness
change was made. The final expected write set is five paths: this trace, the three firmware deletion files
and `tools/device_regression.ps1`. Existing host/build evidence remains applicable because the blocker-only
correction changes no compiled source; the same independent reviewer receives one final follow-up limited
to its stale-consumer blocker before mandatory Architect closure review.

Independent blocker-only follow-up: **GO**. The reviewer confirmed that `tools/device_regression.ps1`
equals HEAD minus exactly the obsolete case, no live producer/caller or replacement harness remains, and
the final five-path accounting is complete. No other blocker was found in the exact production deletion,
public-fixture preservation, evidence, resources or cleanup claims.

### Architect closure GO

Architect personally reviewed the actual five-path diff and raw evidence and returned **GO**. The exact
exercised boundary is firmware build/link plus serial-dispatch/regression ownership: no retained diagnostic
or runner can invoke the legacy full-profile materializer. The required behavior is absence of that consumer;
the forbidden effect shown absent is CardMind-owned password/passphrase bytes entering a retained diagnostic.
The three public Rebex diagnostics and every product JIT/auth/Web/model/workspace/audit path are unchanged.

**Completed:** 2026-08-31 20:05:57 +03:00. No next row is activated until this exact P4-15 commit is
published and its remote SHA is verified.

**Published:** commit `c6a3f78416db2dcc17db9c31497a34772c34c225`; the authenticated GitHub
Phase 4 branch resolves to that exact SHA and reports the reviewed five paths with exact required
Author/Committer. P4-16 is now the sole `in_progress` row.

## P4-16 design gate

**Status:** completed
**Started:** 2026-08-31 20:26:56 +03:00.

### Scope lock and observable acceptance

- ROADMAP Phase 4 requires the Device surface to expose the same SSH concepts and state as the
  Web Console while retaining the existing terminal/history boundaries. This row owns the one
  consolidated Device journey for profile/security plus manual command, SFTP and workspace
  transfer controls; it does not create a separate P4-18 subsystem.
- Profile inventory uses the P4-01 public summaries. Device rendering/navigation and its direct
  connect/edit consumers must not materialize every saved password/passphrase. A secret-bearing
  profile is loaded only after that exact profile is already the global default and the user starts
  an edit or connection path. The inherited bounded P4-01 CRUD writer remains an explicit
  non-evidence exception: `saveSshProfileAt()` and `deleteSshProfile()` may materialize multiple
  bounded records while rewriting the indexed store, and P4-16 does not reopen `ssh_client.*`.
- Existing project/chat capability screens gain the P4-14 ceiling control. The UI presents
  inherit/bound and available/unavailable state without displaying the opaque ID; choosing a
  named public summary stores its existing canonical ID but never selects or reconnects it.
- The existing manual terminal remains the command surface, with its current SD scrollback and
  in-session recall unchanged. Model command policy, Safe Actions and model SFTP are not Device
  authority and are not reimplemented here.
- Existing manual SFTP remains direct-user authority. Workspace upload/download must show the
  exact destination, default overwrite to denied, pass overwrite only after an explicit user
  choice, use the P4-07 controlled streaming backend, expose cooperative cancel and preserve its
  one total foreground deadline and unknown-outcome behavior.
- Existing changed-host-key failure remains an immediate block before authentication. Existing
  exact-profile host-key forget and private-key installation stay in the profile/security journey;
  real private-key authentication and real mismatch/known_hosts mutation acceptance remain P4-20
  ownership as one exact-owned lifecycle.

### Existing producers, consumers and owners

- `DeviceMenus.ino` already places one `SSH tool` entry in Utilities and the Tools carousel already
  names SSH. `KeyboardNavigation.ino` invokes `runSshTool()` and refreshes the cached available
  selected profile ID on return. No card or navigation entry is missing.
- `SshTools.ino::runSshTool()` currently calls `loadSshProfiles()` twice and therefore loads all
  stored password/passphrase values for menu rendering. It separately loads summaries only to
  obtain IDs. The top-level and per-profile SFTP paths pass full records from that materialized
  vector. P4-01 explicitly assigned migration of this Device compatibility consumer to P4-16.
- `loadSshProfileSummaries()` returns at most five public records and the selected index;
  `loadSshProfile()` JIT-loads only the selected secret pair; `selectSshProfile()` changes only the
  stored selection. Existing CRUD remains the P4-01 bounded indexed writer and is not redesigned
  or claimed as selected-only proof in this row.
- `runSshTerminal()` already JIT-loads the selected profile, blocks host-key mismatch through
  `connectTrustedSsh()`, authenticates only after trust and uses the existing bounded
  `/assistant/ssh/terminal.log` and `.old.log`. It is reused unchanged.
- `runSftpBrowser()` currently uses the legacy `uploadSftpFile()`/`downloadSftpFile()` wrappers.
  P4-07 already proved `uploadSftpFileControlled()`/`downloadSftpFileControlled()` streaming,
  total deadline, cooperative cancellation, safe temp replacement, no-overwrite and explicit
  unknown outcome. P4-16 owns only the Device confirmation and invocation boundary.
- `renderProjectToolPolicy()` and `renderChatToolPolicy()` already load their current public
  documents and display capability rows. P4-14 supplies the shared ID codec/predicate, cached
  available selected ID and persisted `sshProfile` fields. The existing capability-status view
  already resolves from the same project/chat/current-ID state as request planning.
- Project and chat storage retain their existing atomic/revision owners. A successful new ceiling
  save is canonically reloaded before replacing the active cache so repeated Device edits preserve
  revision invalidation; no storage format or writer changes.
- Vendor/storage/transport semantics are inherited unchanged from the accepted P4-01, P4-07,
  P4-11 and P4-14 evidence: M5Stack ESP32 core 3.2.1, the bounded indexed Preferences store,
  atomic workspace replacement and the pinned libssh2 controlled-transfer behavior.

### Minimal design

1. Build every Device profile list and selected-name display from `SshProfileSummary`. Keep the
   existing create/select/delete/forget/key-install actions. `Make default` remains the sole action
   that calls `selectSshProfile()`. Per-profile Connect, SFTP and Edit reject a non-selected summary
   with an instruction to make it default first; they never silently change global/model authority.
   For the already selected summary, terminal/SFTP/edit use the existing JIT loader. No
   profile-by-index secret loader or alternate CRUD API is added.
2. Extend the two existing capability lists with one `SSH host` row before `Back`. Its label uses
   only the already loaded ceiling strings, cached available selected ID and the P4-14 predicate.
   The modal returns only an empty or production-codec canonical profile ID and retains no loaded
   project/chat document. After the modal closes, the handler canonically reloads the current
   project/chat, mutates only `sshProfile`, saves once, and canonically reloads again after success
   or failure. Active caches are replaced only from that final reload. A failed save plus failed
   reload reports explicit unknown/committed-state uncertainty and is never retried automatically.
   No raw-ID input or ID text is rendered.
3. For upload/download, derive the exact destination from the current remote listing or workspace
   filename. Show it before mutation. Pass `overwrite=false` unless that exact target was observed
   and the user explicitly selected overwrite. Invoke only the controlled P4-07 method with a
   60-second total deadline and an Escape-aware cancellation callback. Report success, failure or
   unknown outcome without retry. Keep create-directory, rename and confirmed delete behavior
   otherwise unchanged.
4. Keep the existing Utilities entry, terminal, scrollback/history, trust prompt, mismatch block,
   exact-profile forget and key-install boundaries. Add no card, screen class, route, schema,
   storage field, framework, background task or retained diagnostic.

### Frozen proof matrix

- Cheap static/source: Device rendering/navigation and direct connect/edit code have no
  `loadSshProfiles()` consumer; list labels use summaries, only the already selected
  terminal/SFTP/edit path JIT-loads one profile, opaque IDs are never rendered and no non-selected
  action changes selection or loads its secret. The inherited bounded CRUD writer is excluded from
  this claim and remains source-identifiable as the P4-01 mutation owner.
- Cheap static/source: Device upload/download have no legacy wrapper call; each shows the derived
  destination, defaults overwrite false, reaches true only from its exact confirmation branch,
  passes one 60-second total deadline and one cooperative Escape callback, and never retries an
  unknown result.
- Cheap static/source: the chooser returns only empty or production-codec output and retains no
  authority document; each handler reloads before its one-field save and after every save outcome,
  updates caches only from canonical reload, reports indeterminate durable state without retry,
  never selects a profile and uses the same cached selected ID and ceiling predicate as request
  planning. Existing terminal/history and host-mismatch code are unchanged.
- Strict host/static suite and exact pinned 3.2.1 compile prove existing policy/codec/storage
  contracts and firmware compatibility. Reuse accepted P4-07 transfer and P4-11 trust backend
  evidence rather than re-running their internal scenarios.
- Existing independent final-source runtime evidence is reused only for unchanged primitives:
  P4-01 profile/ID/selected-JIT storage, P4-07 controlled transfer/overwrite/cancel/cleanup,
  P4-11 mismatch block and P4-14 ceiling persistence/predicate/request-plan consistency. None is
  relabelled as Device menu-input evidence.
- Device UI wiring is split into independent source observations: Utilities reaches `runSshTool()`;
  profile menus use summaries and selected-only consumers; project and chat capability screens own
  their separate ceiling save/reload paths; upload and download own separate destination,
  no-overwrite, overwrite and cancellation branches; terminal/history/mismatch code is unchanged.
- Final-source input audit: the only entry points that execute these menu/modal behaviors are
  physical M5Cardputer keyboard events through `KeyboardNavigation.ino`, `modalSelection()` and
  `modalTextInput()`. Existing serial `HOTFIXNAVTEST`, `E2ETEST`, `SSHSESSIONTEST`, `SFTPTEST` and
  `SFTPTRANSFERTEST` bypass those controls, and no existing serial/Web boundary injects Cardputer
  keys. Therefore no unattended existing input can produce runtime UI acceptance. This limitation
  is reported rather than hidden by a new retained/disposable diagnostic or parallel UI harness.
- If Architect accepts a bounded manual exception, separate observations cover profile/security,
  project ceiling, chat ceiling, upload, download and terminal entry/exit, with collision-checked
  exact-owned fixtures, two-pass cleanup and original profile/project/chat/workspace/remote
  inventory restoration. Otherwise those UI-runtime observations remain explicitly unproven and
  the row cannot claim them from static or backend evidence.
- The accepted runtime path, if authorized, records free heap, largest block, stack margin, reset
  reason, operation latency and expected-card readiness/ownership. Idle/general state retains the
  70-KiB floor; active SSH is compared with the existing active-SSH baseline and must not reset,
  freeze or materially regress.

### Forbidden effects

- No all-profile secret materialization in Device list/navigation; no credential, key, passphrase,
  opaque ID, private path or authority hash in display text, serial, logs, diagnostics or fixtures.
- No ceiling-based profile selection, reconnect or authority elevation; invalid/stale/mismatched
  ceilings remain unavailable and Off/Ask/mandatory confirmation remain unchanged.
- No destination mutation before the explicit confirmation result; no overwrite default true,
  final-path truncate/pre-delete, unknown-outcome retry, background transfer or unbounded RAM copy.
- No Web assets/routes, model schemas/policy/catalog/audit changes, Safe Action UI, new terminal or
  history behavior, profile/storage migration, known_hosts manager, recovery framework or Phase 5+
  preparation.
- No deletion or mutation outside collision-checked exact-owned fixtures; no changed user
  selection/inventory after success or failure.

### Expected write set and gate status

- `.agents/P4_TRACEABILITY.md`.
- `firmware/CardputerAssistant/CardputerAssistant.ino` for the existing project/chat capability
  list labels and one public-summary ceiling chooser.
- `firmware/CardputerAssistant/KeyboardNavigation.ino` for the two existing capability-screen
  save/reload actions.
- `firmware/CardputerAssistant/SshTools.ino` for summary-only Device profile navigation and the
  controlled transfer confirmation/cancel invocation.
- No other production, test, Web asset, route, storage, policy, workflow or diagnostic file.
- Architect corrected pre-edit review: **GO**. The bounded corrections narrow secret evidence to
  Device render/direct consumers, reload canonical authority around one-field ceiling saves,
  reserve selection for explicit `Make default`, and keep the production write set at exactly four
  paths. Architect accepted proportional final-source proof without a new key-injection seam or
  UI harness: source/call-path evidence covers the new menu handlers, production-linked checks
  cover summary/codec/predicate/save-reload/controlled-transfer boundaries, exact pinned compile
  covers the final source, and retained Device evidence covers unchanged runtime primitives.
  Physical key delivery through unchanged `modalSelection()`/`modalTextInput()` is not claimed as
  independently automated or manually rerun because either would add an unsafe/out-of-scope input
  path or require user participation. Production may proceed only with the frozen design.

### Implemented boundary and proportional evidence

- The coherent primary-agent patch changes exactly the frozen four tracked paths. Device SSH
  inventory and profile menus now use `SshProfileSummary`; the direct menu contains no
  `loadSshProfiles()` call. `Make default` is the sole selector, and non-selected Connect/SFTP/Edit
  returns an explicit instruction without loading a secret or changing authority. Top-level and
  selected-profile terminal/SFTP/edit paths retain `loadSshProfile()` JIT behavior. Create/delete
  retain the explicitly excluded bounded P4-01 CRUD writer.
- Existing project/chat capability screens now contain one SSH-host row. The label uses the P4-14
  predicate and cached available selected ID and renders only inherit/bound plus
  available/unavailable state. The chooser lists public profile labels, returns only empty or
  production-codec output and never renders the opaque ID. Each handler reloads canonical state
  after the modal, changes only `sshProfile`, writes once, reloads after every write outcome,
  updates active cache only from canonical reload and reports not-committed, committed-despite-
  error or unknown stored state without retry.
- Device upload/download now show the derived remote/workspace destination before transfer.
  Overwrite begins false, becomes true only when that exact destination was observed to exist and
  the user confirms it, and only `uploadSftpFileControlled()`/
  `downloadSftpFileControlled()` are called with one 60-second deadline and the existing Escape
  keyboard as cooperative cancellation. The Device browser contains no legacy transfer-wrapper
  call and does not retry failure or unknown outcome.
- Disposable strict source/call-path check: pass. Exact changed-path count is four; Device
  `runSshTool()` has zero full-profile list calls and one explicit selector call; both controlled
  transfer methods, destination confirmation and cancellation callback are present; project/chat
  save/reload owners are present. `git diff --check`: pass.
- Project dependency pins: pass for M5Cardputer, M5Unified, M5GFX and ArduinoJson `7.2.1`.
  CI-equivalent WSL C++ suite with an exact-owned `/tmp` ELF and trap cleanup: pass under
  `-std=c++17 -Wall -Wextra -Werror`; it directly exercises the production P4-14 codec/predicate
  and existing host-linked contracts.
- The first observable exact compile found one active-row integration defect: Arduino did not
  synthesize the cross-INO `modalSelection()` declaration for the helper defined earlier in the
  main sketch. One exact forward declaration of the existing signature was added inside the same
  anonymous namespace; no behavior or write set changed. The one justified rebuild then passed.
- Final exact pinned build: M5Stack ESP32 `3.2.1`, exact FQBN
  `m5stack:esp32:m5stack_cardputer:FlashSize=8M,PartitionScheme=custom`, sketch `3,435,138` bytes,
  globals `65,668` bytes, local headroom `262,012` bytes. Binary `3,435,328` bytes, SHA-256
  `82A617600D505712DA0668B4EF81D36E815D3E3EA7D2718CDFCC49268E43E45A`.
  Compared with P4-15, sketch increases by `7,880` bytes, binary by `7,888` bytes and globals are
  unchanged.
- Per the explicit Architect proportionality decision, no upload, COM8, physical key input,
  Device/Web fixture or synthetic UI-input path was used. Physical delivery through unchanged
  modal primitives is not claimed as automated or manually observed. Accepted unchanged real-
  device evidence remains owned by P4-01/P4-07/P4-11/P4-14; no new runtime latency/free-heap claim
  is made. No device, profile, project/chat, remote or microSD state was mutated, so exact-owned
  cleanup and restoration obligations are empty.
- Fresh independent code review initially returned the one timestamp blocker recorded below; its
  correction and blocker-only follow-up are now complete. P4-16 remains `in_progress`; no staging,
  commit or push is permitted before mandatory personal Architect closure GO.

### Bound-label correction

- Pre-review reconciliation found one P4-16-owned display defect: the project screen passed its own
  nonempty ceiling into a helper position that was labelled as inherited, so authority remained
  fail-closed but the project row said `Inherit project` instead of `Bound`. Production and tests
  were frozen before independent review.
- Architect returned correction **GO** for the smallest existing-file fix. The helper now accepts
  `(ownCeiling, parentProjectCeiling)`, labels any nonempty own ceiling `Bound`, labels an empty own
  ceiling with a nonempty parent `Inherit project`, and otherwise labels `Inherit selected`. The
  shared conjunctive P4-14 predicate receives parent as project and own as chat; project scope uses
  an empty parent, so authority behavior is unchanged. The project and chat callers now pass those
  exact roles. No predicate, policy, storage, schema, test or write-set change was made.
- Disposable static proof after the correction: pass. Project own nonempty maps to `Bound`;
  project empty maps to `Inherit selected`; chat own nonempty maps to `Bound`; chat empty with a
  nonempty project parent maps to `Inherit project`; available/unavailable remains only the shared
  predicate suffix. `git diff --check` and the exact pinned rebuild passed with the final resource
  values and binary hash recorded above.

### Independent code-review STOP and correction

- The fresh read-only code reviewer found one active-row defect: the new chat-ceiling branch changed
  both `sshProfile` and `summary.updatedAt`, contradicting the frozen one-field save and potentially
  changing chat recency/order. No other mandatory finding was reported.
- Architect returned correction **GO** for removal of only that new timestamp assignment. The
  ceiling branch still reloads canonical state after the modal, mutates only `sshProfile`, writes
  once, reloads after the outcome and updates active cache/status from canonical state. The adjacent
  pre-existing ordinary chat-policy branch retains its own timestamp behavior. Production write
  set and all authority/storage contracts are unchanged. The same reviewer receives one final
  follow-up limited to this blocker after diff/static and compile evidence are stable.
- Blocker-only static proof: pass. The ceiling branch contains no `summary.updatedAt`; the one
  timestamp mutation in the ChatToolPolicy handler remains in the ordinary policy branch. Exact
  pinned rebuild and options gate passed with sketch `3,435,138` bytes, globals `65,668`, local
  headroom `262,012`, binary `3,435,328` and the final SHA-256 recorded above.
- The same fresh reviewer used its single blocker-only follow-up and returned **GO**. It confirmed
  the chat-ceiling branch now mutates only `sshProfile`, the sole timestamp assignment remains in
  the ordinary policy branch, canonical reload/save/cache behavior is unchanged and the correction
  introduced no unintended production change. Reviewer lifecycle is closed; no mandatory code-
  review finding remains.

### Architect closure GO

Architect personally reviewed the exact four-path production/trace diff, ROADMAP boundary, raw
source/static/host/build evidence, producer/consumer and inherited vendor/storage owners, resources,
cleanup and residuals and returned **GO**. The accepted boundary is summary-only Device inventory,
explicit-only profile selection, selected JIT direct consumers, non-elevating canonical project/chat
ceiling controls, and destination-confirmed controlled workspace transfer. Existing terminal/history,
host mismatch, bounded CRUD, model/Web/schema/policy and storage owners remain unchanged.

The accepted proportional proof does not relabel compile/static checks as physical input behavior:
unchanged modal key delivery was not automated or manually rerun, while predecessor real-device
evidence retains ownership of the unchanged profile/transfer/mismatch/ceiling primitives. No new
latency/free-heap claim is made. No Device, profile, project/chat, remote or microSD mutation occurred,
so row-owned cleanup is empty. Accepted residuals remain the inherited bounded CRUD materialization,
P4-07 crash-temp ownership and absent new physical-input observation.

**Completed:** 2026-08-31 23:07:23 +03:00. No successor row is active until this exact P4-16 commit
is published and its remote SHA is verified.

### Publication

- Commit `14dc35b4111c7659886a5a27c30470fc603a42b4` was pushed to
  `feature/phase-4-ssh-remote-workspace`; authenticated GitHub MCP resolved that branch to the
  exact SHA and confirmed the required Author/Committer identity. The P4-16 publication report was
  sent to Architect. P4-17 is now the sole active row.

## P4-17 design gate

**Status:** completed
**Started:** 2026-08-31 23:16:04 +03:00.
**Completed:** 2026-09-01 02:40:51 +03:00.

### Scope lock and observable acceptance

- ROADMAP Phase 4 requires Device and Web Console to expose the same SSH concepts and state while
  retaining the existing single Web terminal. P4-17 owns the consolidated authenticated Web
  profile/security, manual terminal, SFTP and workspace-transfer journey; it does not recreate the
  user-removed P4-09/P4-19 subsystems.
- The profile list uses P4-01 public summaries and authenticated canonical opaque IDs. Web state
  must not load every password/passphrase; only the exact selected profile may be loaded just in
  time for selected edit/completeness or connection. IDs may be values on authenticated config
  boundaries but are never rendered as user text or returned to model/file/audit/diagnostic paths.
- One live foreground shell remains. Profile create/edit/select/delete/key-install controls are
  unavailable while a connection, trust decision, terminal or retained mismatch owns the selected
  profile. The user explicitly disconnects before changing profiles; no tab/session pool,
  reconnect scheduler or background terminal is added.
- Host-key mismatch remains blocked before authentication and is visibly distinct from ordinary
  disconnected/failed state. P4-17 may add one authenticated CSRF-protected manual forget action
  only for the exact selected profile that produced the mismatch; host and port are resolved
  server-side and no free host input, list/clear API or known-host manager is added. Reconnect is a
  separate explicit action after forget.
- The existing project/chat settings gain named SSH-profile ceiling selectors. The project control
  emits the exact P4-14 `X-CardMind-Ssh-Profile-Encoded` envelope; chat uses its existing persisted
  field. Both display saved/effective match state without rendering an ID and never select,
  reconnect or elevate a profile. If a stored canonical ceiling is absent from public summaries,
  the selector shows a selected non-ID-text `Unavailable saved profile` placeholder, preserves that
  exact stored value on an otherwise untouched save, reports the production-computed match false,
  and permits explicit clear or rebind. It never silently renders inherit or another profile.
- Existing SFTP/workspace controls show exact source and destination, default overwrite denied,
  enable overwrite only after confirmation for an observed existing target, and invoke only the
  P4-07 controlled streaming methods with one 60-second foreground deadline and cooperative HTTP
  cancellation. Failure/unknown outcome is explicit and never retried.
- P4-20 owns first real Cardputer private-key authentication together with the inseparable exact
  known_hosts mismatch/forget cleanup. P4-17 adds the existing selected-host forget UI/route/guard
  but does not connect, trust, authenticate, mutate known_hosts or execute a remote transfer in its
  runtime proof.

### Explicit non-goals

- No terminal tabs, second live SSH client, saved terminal state, new history viewer/rotation,
  background work, reconnect, retry, job/result framework or user-defined Safe Actions.
- No model schema/catalog/policy/pending/audit change and no change to manual terminal authority.
- No SSH profile/key/known_hosts storage schema, migration, transaction/recovery framework,
  pagination/rotation, diagnostics feature or encryption implementation.
- No host/port forget input, broad known-host list/clear route, new navigation shell, Phase 6 visual
  redesign, USB/Python/future-phase preparation or duplicated transfer backend.

### Existing producers, consumers, persistence and cleanup owners

- `assets/web_console.html` already has stable IDs for profile CRUD, key upload, the one ANSI/PTTY
  terminal and basic SFTP upload/download. It currently changes selection while a terminal can be
  live, has no exact-host forget control or project/chat ceiling selectors, and calls transfer
  routes without destination/overwrite/cancel state.
- `web_console.cpp::refreshSshProfiles()` currently calls `loadSshProfiles()`, materializes all five
  secret pairs, clears them and retains public fields in `consoleSshProfiles`. P4-01 assigned this
  compatibility consumer to P4-17. `handleSshSettings()` also uses the full loader only to count
  profiles on create. Selected edit/start already have a JIT loader; private-key upload already
  binds the selected public profile ID and removes its exact temporary SD file on terminal paths.
- `web_console_state.{h,cpp}` serializes only public profile fields but accepts full `SshProfile`
  records and cannot provide canonical IDs to the P4-14 controls. Chat state already returns the
  persisted project/chat ceilings and selected available canonical ID through the authenticated
  endpoint; no secret or private-key path is present.
- `web_console_routes.{h,cpp}` owns the fixed route enum/table, collected project ceiling header and
  route storage guard. Existing profile/key/terminal/SFTP mutation routes are session+CSRF guarded;
  the new exact forget route belongs here and requires SD write access.
- `handleSshStart()` loads the selected profile JIT, performs trusted-host verification before
  authentication and reuses one `webSshClient`/worker/channel. A changed key records a failed,
  blocked mismatch. The captured connection profile remains the exact server-side host/port owner.
- Existing Web SFTP handlers require that same trusted live session but call the legacy direct
  wrappers. P4-07 already proved the controlled methods' absolute deadline, bounded streaming,
  safe remote/local temporary replacement, default-deny overwrite, cancellation, unknown outcome
  and exact cleanup; P4-17 only integrates those methods.
- Project/chat metadata keeps the existing atomic/revision owners. Project raw settings already
  accept the collected P4-14 envelope; chat settings already validate and persist its canonical
  field. No new durable representation or migration exists.
- `forgetTrustedSshHost(host,port)` validates the target and atomically rewrites the existing
  bounded known_hosts file through its exact `.tmp`/`.bak` owner. P4-20, not this row, owns
  byte-for-byte unrelated-entry preservation acceptance.
- Installed M5Stack ESP32 core WebServer 3.2.1 exposes the current request as
  `NetworkClient& client()` and its public socket descriptor through `fd()`. The installed
  `NetworkClient::connected()` is not sufficient for cancellation: a nonblocking peek result of
  zero can retain stale `errno` and leave `_connected=true` after orderly browser FIN. P4-17 must
  inspect the current request socket directly, treating peek `0` and terminal socket errors as
  disconnected while preserving `EWOULDBLOCK`/`EAGAIN` and the vendor VFS `ENOENT` live states.

### Minimal reviewed design candidate

1. Replace only the Web cache/state compatibility adapter with `SshProfileSummary`. Refresh loads
   summaries plus the selected record JIT for selected completeness/key state, clears its secret
   fields immediately and never stores them in the list cache. Creation counts summaries. State
   encodes each nonzero summary ID with the production P4-14 codec and emits no raw integer or
   secret; existing index mutation APIs remain unchanged behind the authenticated UI.
2. Reuse the current single-terminal stage. At connection start capture exactly
   `{profileId, host, port}` from the selected JIT profile in existing `web_console.cpp`; transport
   close and secret zeroization do not clear that capture when a mismatch is retained. One
   server-owned predicate covers connect/trust/terminal/retained-mismatch state and rejects profile
   create/edit, select, delete and key-upload start before any SD/NVS mutation; `SshStart` also
   rejects while mismatch is retained. Matching controls are disabled and state shows `Connected`,
   `Disconnected` or `Mismatch blocked`. The one CSRF forget route requires retained mismatch,
   reloads the selected public authority immediately before mutation, requires exact ID+host+port
   equality with the capture, and calls `forgetTrustedSshHost()` only with the captured host/port.
   Drift or forget failure writes nothing and preserves mismatch/capture; only successful exact
   forget or whole Web-session release clears them. Forget never reconnects or accepts host input.
3. Add named project/chat ceiling selects to the existing details panel. Profile option values are
   authenticated canonical IDs while labels contain only public names/hosts. State supplies the
   saved ceiling values and production-computed match booleans. Project save sends exact `v1:` or
   `v1:<canonical-id>` in the existing collected header; chat save sends its existing
   `ssh_profile` argument. If a saved value is absent from summaries, a synthetic select option
   carries that value but renders only `Unavailable saved profile`; ordinary saves preserve it,
   while explicit clear/rebind changes it. Reloaded canonical state is the only displayed result.
4. Add one visible transfer-cancel control and one UI operation owner. Every upload/download first
   shows exact source/destination and whether replacement is requested. Browser abort closes only
   that request. One function-local-owner predicate in existing `web_console.cpp` peeks the public
   request-client `fd()`: result `0`, invalid descriptor or terminal socket error means cancelled;
   positive data and only `EWOULDBLOCK`/`EAGAIN`/vendor `ENOENT` remain live. Each handler strictly
   parses required `overwrite=0|1` before SFTP open or any temporary/final write, captures
   `server.client()`, calls `openSftpControlled(remaining, sameCancel)` and then the controlled
   transfer with remaining time from the same original 60-second deadline. It reports typed known/
   unknown failure, never retries, and refreshes files only after confirmed download success.
5. Retain existing key upload/authentication and terminal/SFTP backends. No test seam or key reader
   is added; real acceptance uses collision-checked exact-owned profiles, keys, remote/workspace
   paths and project/chat fixtures through existing authenticated controls.

### Changed-boundary failure and cleanup ownership

| Boundary | Required failure behavior | Cleanup/authority owner |
|---|---|---|
| Public summary/JIT refresh | Any invalid/failed selected load fails state explicitly; no all-profile secret fallback | Local selected secret strings cleared before return; existing NVS owner unchanged |
| Profile mutation during live/mismatch state | One owner predicate returns HTTP conflict before NVS/SD mutation, including key-upload start | Existing live/mismatch capture remains authoritative |
| Forget stale/nonmatching ID or same-ID host/port drift | Conflict before `forgetTrustedSshHost`; no known_hosts write | Captured exact ID/host/port and mismatch remain unchanged |
| Forget commit failure/unknown atomic state | Explicit storage error; no reconnect and no capture clear | Existing bounded known_hosts recovery owns `.tmp`/`.bak`; P4-20 verifies exact effects |
| Saved ceiling absent from summaries | Non-ID placeholder, false production match and exact preservation until explicit clear/rebind | Existing project/chat metadata remains authoritative |
| Browser transfer abort/timeout | Direct socket peek observes orderly FIN and terminal errors; controlled cancel/failure or typed outcome unknown; no retry | P4-07 exact remote/local temporary owner and existing SD recovery |
| UI/HTTP error after confirmed transfer | Never rerun mutation automatically | Canonical reload only; confirmed result remains authoritative |
| Key upload/auth failure | Existing explicit error; no secret/read response | Existing exact key-upload temp cleanup and P4-02 immutable binding owner |

### Frozen minimal proof matrix

- Cheap host/static: Web profile state/cache and create-count path contain no `loadSshProfiles()`;
  summaries expose only production-codec canonical IDs, selected JIT values are immediately cleared,
  state/asset contain no password/passphrase/key bytes/path/internal integer output, and model/file/
  audit schemas are unchanged.
- Cheap host/static: one terminal/client remains; all profile mutations and key-upload start have
  the same live/mismatch guard; forget is POST+CSRF+SD-write guarded, accepts no host/port, verifies
  captured versus current selected ID+host+port immediately before mutation, calls the existing
  exact-host primitive once with the capture and does not reconnect. The same-ID host/port-drift
  branch fails before the primitive and writes nothing. Mismatch remains fail-before-authentication.
- Cheap Web/UI: generated asset matches source; stable controls exist at desktop/tablet/narrow
  widths; project header and chat argument use canonical option values; saved/effective labels come
  from reloaded state; profile change requires disconnected state; transfer destination,
  overwrite/no-overwrite and cancel behavior are observable without tabs or a new console.
- Cheap host/static: upload/download invoke only P4-07 controlled methods, share one non-refreshing
  60-second budget across SFTP open and transfer, use request-client disconnect for cancellation,
  parse overwrite strictly, refresh only after confirmed success and contain no unknown retry or
  final-path truncate/pre-delete fallback.
- Exact pinned 3.2.1 compile after cheap checks; inspect generated asset and build options before the
  sole upload.
- The retained Web UI/asset test proves the new stable controls, responsive layout, exact route/form
  wiring and saved/effective labels. P4-14's accepted runtime evidence remains authoritative for
  ceiling persistence and conjunctive authority; P4-17 observes only that its displayed state
  refreshes after the selected profile's identity or availability changes.
- P4-07's accepted production/runtime evidence remains authoritative for transfer deadline,
  cancellation, no-commit and exact temporary cleanup. P4-17 performs only one direct visible
  SFTP/workspace control smoke proving exact destination, overwrite default deny/confirmation,
  payload transfer and no automatic retry; it does not reimplement or re-prove the backend oracle.
- P4-11 and P4-20 own the real host-key rotation, exact forget mutation and unrelated-known_hosts
  preservation scenario. P4-17 proves the selected-host forget control, no host/port input, CSRF,
  server-side exact authority guard and no reconnect through retained UI/route/static evidence.
- One narrow authenticated session owns only the remaining P4-17 integration gap without an SSH
  connection: actual page/state load, public profile summaries and canonical IDs without secrets,
  project/chat saved/effective controls rendered from authenticated state, mismatch-only forget
  hidden in ordinary state, and the existing single-terminal/SFTP destination-overwrite-cancel
  controls visibly reachable and wired. It reuses P4-14/P4-07 runtime evidence and retains no P4
  journey runner, endpoint manager, fixture protocol or comparable helper suite.
- Device bootstrap remains one initial readiness attempt, prefix-matched Web Console ready, one
  holder for the bounded session and normal `EXIT`/stopped. First readiness loss ends the path
  without probe, upload, reset, recovery or physical action. Record only safely observed absolute
  heap/block/stack/latency values and make no unsupported comparison.
- Success and failure cleanup delete only exact profile/key/workspace/remote fixtures, prove repeated
  absence, restore original profile/project/chat/workspace selections and complete inventories,
  stop/remove the disposable endpoint and close Web Console. A failed checkpoint still executes
  that bounded cleanup and reports its exact owner.

### Forbidden effects

- No credential/private-key/passphrase byte, secret path, raw internal ID or authority hash in DOM
  text, Web read response beyond allowed canonical ID/summary, model context/results, workspace,
  file tools, audit, serial, logs, diagnostics, fixtures or Git.
- No profile switch/edit/delete/key install while the live/mismatch owner exists; no mismatch trust
  bypass, free-host forget, automatic reconnect or mutation of an unrelated known-host entry.
- No overwrite default true, destination mutation before confirmation, foreground deadline reset,
  unbounded RAM copy, browser-cancel route, retry after unknown outcome or background transfer.
- No new module, route family, storage field/schema, policy/capability, manager/framework, retained
  Device diagnostic, terminal session abstraction, general recovery or future-phase work.

### Expected write set and gate status

- `.agents/P4_TRACEABILITY.md`.
- `firmware/CardputerAssistant/assets/web_console.html` and generated
  `firmware/CardputerAssistant/src/web_console_asset.h` for the existing responsive controls/state.
- `firmware/CardputerAssistant/src/web_console.cpp` for summary/JIT cache, exact handlers and the
  existing route table.
- `firmware/CardputerAssistant/src/web_console_state.h` and
  `firmware/CardputerAssistant/src/web_console_state.cpp` for authenticated public-summary IDs and
  production-computed ceiling match state.
- `firmware/CardputerAssistant/src/web_console_routes.h` and
  `firmware/CardputerAssistant/src/web_console_routes.cpp` for exactly one selected-host forget
  enum/POST route and its existing storage guard integration.
- `tests/web_console_ui_test.mjs` for stable control/responsive/asset behavior only; no production-
  source snapshot, duplicated codec, new harness or workflow file.
- `tools/hardware_web_e2e.mjs` remains byte-identical to `HEAD`; P4-17 retains no composite journey,
  dispatch plumbing, endpoint protocol, credential-path argument or comparable helper suite.
- No `ssh_client.*`, storage, project/chat, policy/catalog/router/audit, Device, workflow or new file.
- Independent bounded pre-edit review returned **STOP** on one concrete design defect: the original
  draft relied on vendor `NetworkClient::connected()`, whose pinned 3.2.1 implementation can treat
  orderly FIN as still connected when `recv(MSG_PEEK)` returns zero with stale `errno`. That could
  allow a transfer commit after the browser reports Abort and encourage an unsafe retry. The
  correction above uses only the existing request client's public `fd()` and one narrow direct-
  peek predicate in `web_console.cpp`; no cancel route, module or framework is added. The same
  reviewer used its single blocker-only follow-up and returned **GO**: the corrected direct-peek
  predicate plus required Browser commit/cleanup proof fully resolve the cancellation-semantic
  blocker. Reviewer lifecycle is closed. Production may proceed only with the frozen write set and
  design above.

### Architect pre-edit STOP and corrected package

- Architect returned **STOP** before any production/test/Device/Web edit. The coherent existing-
  owner write set is ten paths, but stale/deleted ceiling semantics, exact mismatch authority,
  pre-open overwrite validation, the fixture cardinality/order and endpoint/Device cleanup needed
  the explicit corrections now frozen above.
- The corrected package preserves absent canonical ceilings with a non-ID placeholder, binds
  mismatch to exact captured ID+host+port and one owner guard, validates overwrite before SFTP open,
  uses one profile at a time, stops at unknown B without trusting it, assigns unrelated-known_hosts
  preservation only to P4-20, and fixes one-holder/first-readiness-loss cleanup/resource ownership.
- Production, retained tests and Device/Web actions remain forbidden pending Architect re-review of
  this exact corrected design/proof package. No attempted production patch was applied.

Architect personal pre-edit re-review: **GO** for the exact corrected gate and frozen ten-path
write set. The direct request-socket peek is accepted as the necessary pinned-3.2.1 cancellation
boundary, not general socket infrastructure. Production may now proceed only with the frozen
public-summary/JIT, exact mismatch capture/guard/forget, stale-ceiling, strict-overwrite/deadline,
existing asset/state/route/test integration and proportional one-profile proof responsibilities.
No staging or commit is permitted before the later evidence-ready Architect closure review.

### Fresh implementation review STOP and active-row correction

- The fresh read-only code reviewer returned **STOP** on three P4-17-owned points after the cheap
  checks passed. First, profile save/select/delete/key-install refreshed only SSH state, leaving
  project/chat match booleans stale; the UI also conflated an existing non-selected ceiling with
  an absent saved profile. The correction reloads canonical chat state after every availability/
  identity mutation and labels absent versus present-but-nonmatching IDs separately.
- Second, a local browser `AbortError` cannot prove whether the server committed before FIN became
  observable. The correction reports that browser-side outcome as unknown and requires destination
  inspection; only an observed server result may claim cancellation. No retry is added.
- Third, the retained hardware lifecycle contained only the P4-14 header producer and did not yet
  own the frozen P4-17 checkpoints or common exact-owned cleanup. The correction adds bounded
  checkpoints to that existing owner only; no new harness, file, route or framework is introduced.
- Failure ownership is active-row implementation/proof, not a predecessor regression or environment
  failure. The diff remains inside the frozen ten paths. Expensive build/Device/Web evidence remains
  paused until the consolidated correction passes the reviewer's one allowed blocker follow-up.

### Final reviewer follow-up STOP and bounded correction

- The reviewer's single blocker-only follow-up returned **STOP** on two remaining active-row
  defects and is now closed. First, the generic saved-ceiling label claimed an existing chat profile
  was not selected when it could instead be selected but conjunctively blocked by the project
  ceiling. The corrected label states only that the saved profile is not effective; absent saved
  profiles remain visibly distinct and the server-owned match booleans remain authoritative.
- Second, the retained journey aborted after an unobserved timer, so unchanged destination state
  could not prove that the controlled server operation had started. The corrected existing harness
  arms the disposable fixture, waits for its exact `started` checkpoint with operation count one,
  aborts that request, and requires a `cancelled` checkpoint with the same count, no target commit
  and exact temporary absence before inspecting the unchanged prior destination. No retry, retained
  fixture implementation, new production seam, route, module or framework is added.
- These corrections remain within the frozen asset/generated-asset, existing hardware lifecycle
  and trace owners. Build, upload and Device/Web evidence remain pending the corrected cheap checks;
  no exact-owned mutation is active.

### First exact compile failure and active-row ownership

- The corrected cheap gate passed: generated Web asset source was 120,132 bytes and gzip was
  31,059 bytes; lifecycle syntax, retained UI/asset checks, diff hygiene, timer absence and the
  fixture-side checkpoint inventory all passed. The unchanged host C++ boundary retained its prior
  pass and was not repeated.
- The first exact pinned M5Stack 3.2.1 compile then failed before upload on two stale consumers of
  the P4-17 public-summary cache: session release attempted to swap a `vector<SshProfile>` into the
  new `vector<SshProfileSummary>`, and chat-permission enablement passed a public summary to the
  full-profile completeness function. This is an active-row cache-adapter defect, not a toolchain,
  predecessor, Device or environment failure.
- The bounded correction uses the summary vector type during release, resets the cached selected
  completeness flag with the other session state, and makes chat-permission enablement consume that
  same selected-profile JIT-derived flag. It adds no loader, secret access, route, policy or schema.

### Disposable private-key path boundary

- Runtime preparation found that the draft retained journey accepted the disposable private-key
  path as a command-line argument, making that secret-bearing path visible in process metadata.
  Failure ownership is the P4-17 test caller, not production key storage or P4-02.
- The existing journey now reads that one path only from a temporary process environment value.
  The fixture generates a random filename, the launcher never prints it and clears the environment
  value immediately after the child exits; no key bytes/path enter retained output, serial, Web,
  model, audit, Git or the production API.

### Mandatory greater-than-60-minute proof pivot

- At 2026-09-01 01:55:17 +03:00 Architect returned **STOP** after the row had exceeded its
  60-minute limit. No new production defect was established. The retained harness had accumulated
  a 594-line composite P4 journey plus roughly twenty helpers spanning profile/key CRUD, ceilings,
  SSH stages, fixture control, transfers, cancellation and cleanup, contradicting the locked
  independent-checkpoint design and the user's removal of a separate P4-19 journey subsystem.
- The materially different approach freezes the reviewed production diff, restores
  `tools/hardware_web_e2e.mjs` exactly to `HEAD`, reuses the accepted P4-07/P4-11/P4-14/P4-20 proof
  owners above, and limits runtime to one narrow disposable private-key/authentication and direct
  control smoke. No replacement retained runner, fixture framework or oracle is permitted.
- The attempted AsyncSSH 2.24.0 fixture passed its own local key-auth/shell/SFTP/control self-test,
  but no Device/Web action began. Its temporary process and environment were stopped and removed;
  the attempted exact firewall rule was denied before creation and verified absent. No exact-owned
  mutation or external process remains.
- The earlier command-line key-path correction was part of the removed composite runner and is no
  longer retained code. Any later narrow disposable caller must keep the random path in process
  memory/environment only, never argv or output, and delete it in the same bounded cleanup.

### Architect reduced-package STOP and narrowed concurrency correction

- Architect accepted the reduced proof ownership and every other reviewed P4-17 boundary, but
  returned **STOP** before upload/runtime because `handleSshStart` reread changed-host state after
  its locked composite check and `handleSshForget` composed worker state before proving that the
  worker had published its final state and cleared its task handle.
- A second independent concurrency analysis narrowed the correction. Exact authority capture is
  main-loop owned and its ID/host/port reload prevents unrelated deletion. The worker publishes its
  changed/awaiting/terminal state before clearing `webSshTask` under `webSshStateMux`, and only the
  serialized WebServer/main loop can start another worker. Therefore start retains the existing
  locked `webSshProfileStateLocked()` check but uses one generic conflict message with no later
  changed-state read; forget checks `webSshTaskIsRunning()` first and only after false reads the
  published flags. Established writes and main-owned Arduino Strings remain unchanged.
- This ordering keeps the existing exact selected-authority reload before the unchanged forget
  primitive and adds no synchronization abstraction, helper/type/module, schema, route, policy or
  proof runner. Upload, Device and Web actions remain forbidden pending ordering/static evidence,
  one exact compile and Architect re-review.

### Narrowed concurrency correction evidence

- `git diff --check` passed. A disposable first ordering assertion falsely selected an earlier
  unrelated `completeWebSshWorker()` occurrence and stopped before compilation; production was not
  changed. The corrected assertion searched only after each publication point and passed: start
  uses the locked generic conflict, forget checks task first, exact authority reload/comparison
  precedes forget, and worker changed/awaiting/terminal publication precedes task-handle clear.
- The exact pinned M5Stack ESP32 3.2.1 compile then passed. Sketch flash is 3,440,354 bytes and
  globals are 65,700 bytes. The firmware image is 3,440,544 bytes with SHA-256
  `FD1103603FF9A98EE62787BF65B26916CE91528CE32BA68A9D4CE7C48C17B7E1`; build options contain the
  exact FQBN and one unique resolved 3.2.1 core. No upload, Device, HTTP, Browser or fixture action
  followed this correction.
- The reduced write set remains nine tracked paths and `tools/hardware_web_e2e.mjs` remains exactly
  at `HEAD`. The most recent retained UI/asset check passed before this C++-only ordering correction
  and its boundary is unchanged.

### Narrow runtime preflight blocker

- Architect returned runtime-only **GO** for one upload and one bounded private-key authentication/
  direct transfer smoke, explicitly excluding real mismatch/forget mutation, which remains P4-20.
  No upload or Device/Web action has started.
- Preflight found that a fresh exact endpoint must add its host key before private-key authentication.
  Production has an ordinary manual Device profile action which can forget that exact host, while
  the authenticated P4-17 Web forget route intentionally requires a retained mismatch. There is no
  existing unattended Web/API/serial cleanup for a newly trusted non-mismatching endpoint. Using a
  real key rotation/mismatch/forget would violate the runtime GO and consume P4-20 acceptance;
  requiring manual Device input would violate unattended exact cleanup.
- Consequently the authorized scenario cannot yet satisfy both first real private-key auth and
  exact known_hosts restoration. The image was not uploaded and no endpoint/profile/key/trust/
  workspace mutation was made. This is a proof-ownership blocker, not a production defect; it is
  reported to Architect before any state-changing runtime action.

### Runtime ownership resolution

- Architect assigned the inseparable first real private-key authentication plus created known_hosts
  cleanup to P4-20, which already owns real mismatch, exact forget and unrelated-entry preservation.
  This is required Phase 4 evidence, not a waiver or deferral. P4-17 must not manufacture a cleanup
  route/diagnostic, rotate merely for cleanup, retain a trust entry, request manual Device cleanup or
  change firewall/system policy.
- P4-17 is authorized for one upload and one authenticated Web lifecycle with no SSH connection,
  trust, known_hosts, endpoint, remote-file or profile/key mutation. It observes only the actual
  public page/state and controls listed in the narrowed proof matrix, restores any Web selection it
  changes, and exits normally. First readiness loss ends the path without recovery escalation.

### Narrow authenticated Web acceptance evidence

- On 2026-09-01 after the reboot-safe resume, one direct COM8 holder performed exactly one `PING`
  and one `CONSOLE`; the device returned `PONG` and the prefix-matched Web Console ready marker on
  the first attempt. The same holder stayed open for the complete authenticated Browser observation
  and then accepted one `EXIT`, observed exact `WEB_CONSOLE result=stopped`, closed COM8 and exited.
  No readiness loss, recovery action, rebuild, upload or repeated probe occurred.
- The ignored installation credential was read only in the local automated Browser login path,
  submitted only to the CardMind Web Console and cleared from the automation variables without
  output. The authenticated page reported Cardputer online and loaded the existing project/chat
  selection without changing it.
- In the real Chat details surface, both project and chat SSH ceiling controls were visible. After
  the existing Terminal state refresh, each contained at least one public profile option; every
  non-empty option value was an exact nonzero 16-lowerhex canonical ID. Both saved/effective state
  labels were visible and non-empty, while neither those labels nor any rendered page text exposed
  a raw canonical ID, private-key material or private-key storage path.
- In the real Terminal surface, one public selected-profile option was present with a canonical ID
  held only in the control value/dataset and no raw ID in its rendered label. Password and
  passphrase fields were empty. Exactly one SSH terminal was present; state was `Disconnected`,
  Connect was available, Disconnect was disabled, and the selected-host forget control existed but
  was `hidden`, `display:none` and disabled in this ordinary non-mismatch state. No SSH connection,
  trust decision or forget mutation was attempted.
- Existing SFTP destination, download, upload, transfer-state and cancel controls were present and
  reachable in the Terminal surface. The retained UI/asset test remains the evidence for strict
  overwrite/default-deny and route wiring; this narrowed runtime observation intentionally made no
  SFTP/workspace/remote mutation and did not re-prove the P4-07 backend.
- No profile, key, project, chat, workspace, remote file, known_hosts entry or other exact-owned
  fixture was created or changed, so inventories and selections remained unchanged and no data
  deletion was required. The temporary Browser tab was closed and the sole Web Console holder
  stopped normally. The UI exposed no safe heap/block/stack/latency values, so no new numeric
  runtime resource or latency claim is made; the exact compiled-image resource evidence above
  remains authoritative.

### Architect personal closure review

- Architect returned explicit **GO** for the unchanged exact nine-path row-owned diff after personal
  review of the production path, raw authenticated Web evidence, embedded-asset equality, retained
  UI check, pinned build, cleanup and forbidden effects. The index was empty,
  `tools/hardware_web_e2e.mjs` was content-identical to `HEAD`, and the three approved untracked
  `.codex/agents` files remained excluded.
- Accepted behavior is the authenticated production public-state path into the real project/chat
  ceiling, single-terminal, SFTP and mismatch-only forget controls. Canonical IDs remained control
  values rather than visible text; exactly one terminal and the required transfer controls were
  present; forget remained hidden in ordinary disconnected state; no secret/private path/raw ID,
  second terminal, parallel framework or runtime mutation was observed.
- Accepted resources are flash 3,440,354 bytes, globals 65,700 bytes and image 3,440,544 bytes with
  SHA-256 `FD1103603FF9A98EE62787BF65B26916CE91528CE32BA68A9D4CE7C48C17B7E1`.
  No new numeric runtime resource claim is required for this UI integration row. P4-20 retains the
  mandatory real private-key mismatch/forget lifecycle; P4-07 retains backend transfer evidence.

## P4-20 design gate

**Status:** completed
**Started:** 2026-09-01 02:43:58 +03:00.
**Completed:** 2026-09-01 04:00:48 +03:00.

### Initial scope lock

- P4-20 owns changed-boundary recovery/security acceptance only: the first real Cardputer
  private-key authentication and its inseparable exact-owned known_hosts lifecycle, an explicit
  changed-key connection block, deletion of only the exact selected test-host entry through the
  P4-17 authenticated CSRF forget control, and semantic entry-for-entry preservation of every
  unrelated valid canonical record: the same host, port and fingerprint remain in the same relative
  order without deletion, addition, replacement or rebinding.
- Existing P4-02 key binding/JIT/zeroization, P4-11 unconditional mismatch block and bounded
  known_hosts primitive, and P4-17 selected ID+host+port Web guard are the production owners. The
  row begins as proof/integration work; no production edit is permitted unless concrete evidence
  assigns a defect to this row and Architect approves the corresponding minimal design.
- Explicitly excluded are broad NVS/SD recertification, known_hosts pagination/rotation or manager,
  free host/port input, list/clear API, automatic trust/reconnect/retry, profile/key/storage schema,
  recovery framework, background execution and any P5-P9 implementation.

### Producer, consumer and environment inventory

- P4-02 is the authoritative private-key owner: authenticated Device/Web installation creates an
  opaque bound record; the selected profile loads only its exact record just in time and clears key
  and passphrase buffers after the libssh2 in-memory authentication call. Its replacement,
  capacity-failure preservation, reboot, bounded orphan cleanup and non-addressability evidence is
  reused. The real Cardputer private-key authentication observation was intentionally assigned here.
- P4-11 is the authoritative host-key owner: Device, Web, model SSH and model SFTP fail before
  authentication on a stored mismatch; direct trust cannot replace a different stored fingerprint;
  the bounded atomic rewrite preserves the authoritative target and owns checked temporary cleanup.
  Pagination/rotation and a read/list API were explicitly removed. Its source/build evidence is
  reused; this row owns the real changed-key lifecycle.
- P4-17 owns the authenticated CSRF POST forget control. It accepts no host or port, requires a
  retained mismatch, reloads the exact selected profile ID/host/port immediately before calling the
  existing exact-host primitive, never reconnects and clears retained mismatch state only after a
  confirmed successful forget. The ordinary-state control is hidden and disabled.
- The existing selected user profile completed one read-only real Cardputer private-key connection
  during inventory and reached `Connected`; it then ran one no-history capability marker proving
  `sshd` and `ssh-keygen` are installed on Ubuntu 24.04 and disconnected normally. This is control
  and environment evidence only, not the exact-owned acceptance fixture. No remote file/process,
  profile/key, trust entry or selection was changed, the Browser tab closed and Web Console returned
  exact stopped.
- The local Windows host has no ready SSH service/listener and no existing inbound allow for the
  available Python/Node route. Firewall/system configuration changes are forbidden, so the earlier
  local-endpoint hypothesis is closed. The only eligible fixture host is the already trusted remote
  host, using an unprivileged exact-owned directory and high ports without package installation,
  daemon/service configuration or firewall changes.
- Ubuntu Noble/OpenSSH documentation confirms explicit `Port`, private `HostKey`,
  `AuthorizedKeysFile`, `PidFile`, public-key-only authentication and internal SFTP configuration.
  OpenSSH rejects group/world-readable host private keys. The exact remote installation remains the
  authority: its `sshd -t` validation must pass before any Device profile/trust mutation; otherwise
  exact remote/local cleanup runs and the scenario stops.

### Frozen minimal proof design

1. Capture original CardMind profile/project/chat/workspace inventories and selections. Generate one
   collision-checked local client key in an exact-owned temporary directory; keep its private bytes
   and path out of argv/output, Browser text, model context, logs and retained files. Only its public
   key enters the remote fixture. Choose one collision-checked remote directory and two currently
   unbound high ports; any collision fails before mutation.
2. Through the unchanged trusted control profile, create two unprivileged exact-owned OpenSSH
   endpoints. Each has host key A, pre-generated replacement host key B, the same exact client public
   key, public-key-only auth, an exact PID/log/config owner and a bounded supervisor. After the first
   authenticated session disconnects, or at a bounded safety deadline, that endpoint stops A and
   starts B on the same port. Validate both configurations and listeners before creating CardMind
   trust state. No root/system path, package, service, firewall or unrelated remote state changes.
3. Create one collision-checked temporary CardMind profile, install the one private key through the
   existing write-only authenticated boundary, and retain its stable public ID only inside the local
   automation state. Edit only that profile between the two ports while disconnected; P4-02 keeps the
   exact key binding. The original selected profile remains unchanged and is restored at cleanup.
4. Target port: require the first connection to be unknown, explicitly trust it and reach a real
   private-key-authenticated terminal. Disconnect so endpoint A becomes B. Sentinel port: repeat the
   same unknown/trust/private-auth observation and disconnect so sentinel A becomes B. A found or
   mismatched initial entry is a collision and stops without accepting or replacing it.
5. Reconnect the target port. It must publish mismatch blocked, never open/authenticate a terminal,
   show the selected-host forget control and prevent profile mutation. Invoke forget once; require
   success, idle state and no automatic reconnect. Reconnect the sentinel port: mismatch rather than
   unknown proves its unrelated exact-owned entry survived the target rewrite. Invoke the same exact
   forget once and require no reconnect.
6. Reconnect each replacement endpoint once. Both must now present the unknown-host decision; do not
   trust either, then disconnect. This proves both exact test entries are absent. Delete the exact
   temporary profile/key owner, stop only verified fixture PIDs, remove only the collision-owned
   remote/local directory and restore every original inventory/selection. Repeat absence/cleanup
   checks as idempotent no-ops.

`Idempotent no-op` here means that the exact owner/postcondition check observes an already absent
fixture and performs no mutation. It never means replaying an index-based profile delete after the
owned ID is absent, which could address a shifted unrelated profile, or invoking a remote cleanup
script after that exact script and directory have intentionally removed themselves.

### Failure ordering and cleanup ownership

| Boundary | Fail-closed observation | Exact cleanup owner |
|---|---|---|
| Local key or remote preflight/config/listener | No CardMind profile or trust mutation | Local exact directory and verified remote fixture directory/PIDs |
| Initial test host is found/mismatched rather than unknown | No trust/replace and no retry on that identity | Disconnect; remove profile/key and remote fixture; original trust remains untouched |
| Key install/auth failure | No trust claim beyond an explicitly accepted exact endpoint; no key bytes/path output | P4-02 profile/key delete plus endpoint A-to-B supervisor if trust was accepted |
| Disconnect or control-session failure after trust | Replacement B starts from the pre-created bounded supervisor without system changes | Retained exact mismatch enables only the selected-host Web forget |
| Mismatch path | Connection closes before authentication; no trust prompt or mutation before explicit forget | P4-11 fail-closed owner; P4-17 exact selected ID/host/port forget owner |
| Forget failure/unknown | No reconnect/retry and mismatch remains authoritative | Existing bounded atomic owner; report exact failure and preserve fixtures for the one safe cleanup attempt |
| Browser/Device readiness loss | Test path ends immediately with no reset/probe/reupload/recovery chain | Run only already-reachable exact cleanup; any unremovable exact fixture fails the row and is reported |

### Frozen proof matrix and forbidden effects

- Reuse completed P4-02 host/device evidence for key-record binding/JIT/zeroization and completed
  P4-11 static/build evidence for the bounded canonical exact-host rewrite. Runtime adds one
  exact-owned sentinel record: after target forget, sentinel A-to-B must report mismatch, proving
  that the same sentinel host/port remains bound to its original fingerprint rather than being
  deleted, added, replaced or rebound; its relative order against the target is preserved by the
  reviewed streaming rewrite. Sentinel forget and two final unknown-host observations prove exact
  absence. No firmware diagnostic, authority digest or alternate parser/store is added.
- Observe exact-owned private-key auth, target and sentinel unknown/trust, target and sentinel
  mismatch-before-auth, forget visible only during retained mismatch, exact server-side selected
  authority, no automatic reconnect and final unknown state. Do not execute a model tool or remote
  workspace mutation through the test profile.
- Record only boolean/stage/resource values. Never output a credential, key/passphrase byte or path,
  host fingerprint, internal key/profile ID, trust-store content/hash/path, session token or remote
  authority detail. Public fixture labels and port values remain only in local automation memory.
- No production/test/asset/schema/route/policy/storage change is expected. Repository write set is
  only `.agents/P4_TRACEABILITY.md`; all local/remote scripts, keys, configs, logs and markers are
  exact-owned disposable fixtures and must be absent before closure. If a production defect appears,
  stop, classify its owning row and obtain Architect authorization before any code edit.
- No firewall/system/service/package mutation, no root command, no automatic trust, no free-host
  forget, no broad known-host read/list/clear, no second live CardMind SSH session, no retry after
  unknown outcome, no recovery escalation and no retained journey/endpoint framework.

**Independent security/proof review:** initial `STOP`, followed by one blocker-only `GO` after the
Architect decision and corrections recorded below. Persistent exact-owned proof may proceed only
with the frozen canonical-semantic and transport-unknown contract.

### Independent security/proof review STOP and ownership classification

- The fresh read-only proof reviewer returned **STOP** before any exact-owned profile/key/trust or
  remote fixture was created. The private-key authentication, mismatch-before-auth ordering, exact
  selected-authority guard and no-reconnect mapping were accepted; two proof/ownership defects
  remain.
- First, the completed P4-11 rewrite primitives do not preserve unrelated input bytes as currently
  claimed: they parse trimmed lines, discard blank lines and write canonical line terminators. A
  sentinel mismatch proves semantic fingerprint preservation but cannot prove byte-for-byte
  preservation of unrelated content. This is a concrete P4-11 production-boundary/acceptance defect,
  not a P4-20 harness defect. P4-20 is paused from persistent mutation until Architect either
  authorizes a minimal P4-11 reopen for untouched-byte preservation or records an explicit ROADMAP
  reduction to canonical semantic preservation. P4-20 must not absorb that edit without ownership.
- Second, a lost HTTP response after forget is an unknown outcome: the deletion may already have
  committed and the handler may have cleared mismatch/capture before the caller observes a result.
  The corrected proof marks the call attempted before sending. If transport becomes unknown while
  the same Web session remains healthy, exactly one non-trusting connection to replacement B
  classifies the authority: retained mismatch means deletion did not commit; unknown-host state
  means it committed. The unknown-host branch is never trusted or retried. A readiness loss ends the
  path with no classification probe or recovery chain. Confirmed failures retain their exact error
  and are not repeated against an unchanged cause.
- Reviewer lifecycle is closed. Production/device/remote state remains unchanged after the earlier
  read-only capability inventory; only this trace records the STOP. The active matrix remains
  P4-20 `in_progress` while Architect resolves the cross-row ownership decision.

### Architect canonical-store scope decision

- Architect explicitly decided not to reopen P4-11. ROADMAP requires mismatch blocking and the
  bounded canonical store, not preservation of blank lines, surrounding whitespace or original
  line-ending bytes. Those formatting bytes are not product data or P4 acceptance, and preserving
  them would add parser/copy/resource complexity without security or user-visible value.
- The superseding P4-20 contract is semantic and entry-for-entry: target forget must preserve every
  unrelated valid canonical record with the same exact host, port and fingerprint, no deletion,
  addition, replacement or rebinding, and the same relative order where observable. The target plus
  sentinel A-to-B sequence above is the approved proportional runtime proof. The reviewer's
  historical byte-for-byte STOP remains recorded as the reason for this explicit scope decision but
  is no longer the active oracle.
- The transport-unknown correction is approved: mark the call attempted before send and, only while
  the same Web session is demonstrably healthy, make one non-trusting replacement-B connection to
  classify mismatch as no commit versus unknown-host as committed. Never trust or retry that branch;
  readiness/session loss terminates it without a recovery chain.
- No production/test/code reopen, alternate parser/store, retained harness or framework is
  authorized. Repository write ownership remains `ROADMAP.md` plus
  `.agents/P4_TRACEABILITY.md`; all runtime fixtures remain disposable and exact-owned.
- The same reviewer used its single blocker-only follow-up and returned **GO**: canonical semantic
  preservation now matches the existing ordered rewrite, and the attempted-call plus one healthy-
  session non-trusting B classification resolves the unknown-outcome oracle. Reviewer lifecycle is
  closed and no production/parser/diagnostic/harness expansion was introduced.

### Reboot-resumed exact-owned attempt and external transport blocker

- On 2026-09-01 the resumed run first restored the complete visible 21-row plan with only P4-20
  `in_progress`. One normal COM8 lifecycle reached exact `PONG` and
  `WEB_CONSOLE result=ready`. Its disposable holder then received EOF because it had been launched
  without a PTY; this was classified as a harness-stdin defect, not readiness loss. The already
  active Console was retained by one passive PTY holder without repeating `PING` or `CONSOLE`, and
  that holder later sent one `EXIT` and observed exact `WEB_CONSOLE result=stopped`.
- The authenticated local HTTP path captured one original selected private-key profile and no open
  terminal. The exact fixture name was collision-free. A direct create in key mode was rejected
  before mutation because the existing lifecycle requires an installed key. The observed profile
  count and selection remained unchanged. The corrected existing lifecycle created exactly one
  temporary password-mode profile, installed one in-memory-generated RSA private key through the
  write-only multipart boundary, switched the same stable opaque profile ID to key mode and cleared
  the temporary password, passphrase and harness key buffers. No private key or private-key path was
  written to local storage or emitted to output.
- Through the unchanged trusted control profile, an exact-owned remote directory was created after
  owner and path collision checks. Only the generated public key entered it. Four host keys and the
  target/sentinel A/B configurations passed the installed OpenSSH `sshd -t`; both unprivileged high-
  port A listeners had exact PID/config/log owners and were observed in `LISTEN`. No root, package,
  service, firewall or system-path mutation occurred.
- The first CardMind target connection failed before an unknown-host/trust/auth stage with exact
  firmware classification `TCP connection to SSH host failed`; no test-host known_hosts entry was
  created. A fresh control connection re-verified the exact owner and both listeners. Independent
  default and IPv4 TCP connects from the local control machine to the same exact target port also
  failed while the remote host still reported the listener. DNS exposed IPv4 only. This assigns the
  failure to external inbound high-port reachability, not P4-02 key/JIT/authentication or P4-11
  mismatch handling. Repeating the unchanged connection hypothesis is forbidden.
- Cleanup succeeded after the failure: the verified remote cleanup stopped only the two exact PID
  owners, removed the exact remote directory and then proved its absence; the temporary CardMind
  profile and bound key were deleted, repeated state reads proved absence, and the original profile
  count, order, selected opaque ID and closed-terminal state were restored. The in-memory credential,
  cookie, CSRF, key and fixture references were cleared. No Browser tab, local key/script directory,
  serial process, Web Console handler, remote listener, profile/key or test trust entry remains.
- **External proof blocker opened 2026-09-01 03:45:25 +03:00:** the approved two-high-port fixture
  cannot reach SSH protocol from CardMind on the current remote network. P4-20 remains
  `in_progress`; production and trust state are clean. The next attempt requires an Architect-owned
  proof decision or a different already-authorized reachable exact endpoint. It must not add a
  firewall/system/service mutation, third-party tunnel, production proxy/port-forwarding feature,
  system-host-key change, synthetic known_hosts mutation or acceptance waiver by implication.

### Architect external-blocker and evidence-scope decision

- Architect accepted the external ownership classification, the successful owner-checked cleanup
  and its repeated-absence/no-mutation postconditions, permanently terminated the P4-20 endpoint
  path and prohibited another server search or Device/HTTP lifecycle.
  No firewall/system/service mutation, third-party tunnel, proxy/port forwarding, system-host-key
  replacement, synthetic known_hosts edit, user action or implied waiver may be used to manufacture
  a reachable oracle.
- Mandatory product behavior is unchanged: a stored host-key mismatch blocks before
  authentication, exact selected-host forget never reconnects automatically, and every unrelated
  valid canonical record remains semantically preserved with the same host, port, fingerprint and
  relative order and without deletion, addition, replacement or rebinding.
- Phase 4 runtime evidence accepts the already observed real read-only Cardputer private-key
  connection on the existing reachable selected profile together with completed P4-02 evidence for
  exact profile-to-key binding, selected-record JIT load and zeroization; P4-11 actual code, pinned
  build and independent review evidence for mismatch-before-auth and the bounded canonical
  exact-host rewrite; and P4-17 actual authenticated UI plus exact selected ID, host and port forget
  guard with no reconnect.
- The exact-owned P4-20 attempt additionally proves collision/preflight discipline, installed
  OpenSSH configuration validation, the new-profile/key lifecycle up to the external TCP boundary,
  rejected direct key-mode create with no mutation, stable opaque identity, in-memory-only private
  key handling, failure-path cleanup and exact restoration. It does not prove target/sentinel
  unknown, trust, private-key authentication, mismatch or forget, and contributes no runtime heap,
  stack or latency measurement.
- Real rotatable-endpoint mismatch/forget runtime was not observed. Its dedicated real-endpoint
  verification is now an explicit observation in the existing Phase 9 host-key-pinning/changed-key
  security work, not a new feature, framework or Phase 4 retry. ROADMAP records this evidence-owner
  change while preserving the Phase 4 security contract.
- No production, retained test, asset, schema, route, build, Device or Web action remains in P4-20.
  The closure package is documentation/trace evidence only. P4-20 remains `in_progress` until a
  bounded independent final-evidence review and mandatory personal Architect closure verdict.

### Independent final-evidence review STOP and cleanup-oracle correction

- The fresh bounded proof reviewer returned one `STOP`: the phrase `idempotent cleanup` could be
  read as requiring destructive cleanup entry points to be invoked after their exact identity no
  longer exists, while the retained evidence described successful deletion followed by repeated
  absence reads.
- The corrected oracle is ownership-safe and matches the observed run. Remote cleanup first exited
  successfully after stopping both verified PID owners and removing the exact directory; a separate
  exact-path check then observed it absent and made no mutation. CardMind profile deletion first
  returned success for the exact captured opaque ID/index; two subsequent canonical state reads
  observed that ID/name absent, the original ordered ID inventory and selection restored, and made
  no mutation. The bound key shares that exact profile owner.
- Replaying `/api/ssh/delete` after the owned ID is absent would not be an idempotency proof: the
  persisted index can shift and such a replay could target unrelated data. Re-running the deleted
  remote script is likewise impossible by design. No endpoint, fixture, Device/Web lifecycle or
  production action is authorized or needed for this wording correction. The same reviewer receives
  one blocker-only follow-up against this exact no-op definition.
- The same fresh reviewer returned final **GO** on that single blocker-only follow-up: the corrected
  ownership-safe no-op definition and observed postcondition checks close the cleanup-idempotency
  ambiguity. The reviewer lifecycle is closed.

### Mandatory personal Architect closure review

- Architect personally reviewed the actual trace-only diff, ignored canonical ROADMAP evidence-owner
  decision, matrix, raw production boundary, cleanup semantics and accepted residual and returned
  explicit **GO**. The index was empty and no production, retained test, asset or build file changed.
- Accepted production evidence is limited exactly to the real reachable-profile private-key
  connection; P4-02 binding/JIT/zeroization; P4-11 mismatch-before-auth and canonical exact-host
  rewrite; P4-17 authenticated selected-ID/host/port forget guard; and the exact-owned P4-20
  profile/key lifecycle through the independently confirmed external TCP boundary. No unavailable
  target/sentinel runtime is claimed.
- Forbidden effects and cleanup passed: no test trust entry, secret/path/fingerprint/internal-ID or
  session exposure, root/system/network infrastructure mutation, production change or unchanged-
  hypothesis retry; only exact verified PID/directory/profile/key owners were removed, repeated
  absence checks made no mutation, original order/selection/terminal state returned and Web Console
  stopped.
- No P4-20 resource or latency result is claimed because production/build did not change and the
  endpoint failed before SSH protocol. The accepted residual remains mandatory in Phase 9: one
  reachable collision-owned rotatable endpoint must observe mismatch before authentication, exact
  forget without reconnect and semantic preservation of unrelated canonical records.

## P4-21 phase-closure gate

**Status:** completed
**Started:** 2026-09-01 04:02:50 +03:00.
**Completed:** 2026-09-01 13:49:17 +03:00.

P4-21 owns only Phase 4 closure: canonical requirement/evidence reconciliation, documentation and
license/filename-only secret checks, proportional focused changed-boundary regression for the phase
branch, exact pinned build, final Device/Web/resource evidence for changed boundaries, exact-owned
cleanup, independent final review, green CI and reviewed merge only into `develop`. Complete hardware
and Web Console regression remains the `develop`-to-`main` release-candidate gate. P4-21 adds no
product behavior, diagnostic, framework, future-phase implementation or repetition of
P4-17/P4-20-specific scenarios.

### Closure inventory, proof lock and harness ownership

- CI-equivalent local owners are the existing third-party verifier, MicroPython syntax/supervisor
  tests, strict host C++ suite and Web Console UI suite from `.github/workflows/firmware.yml`.
  The exact Windows Arduino build uses the pinned P4 build skill, M5Stack core 3.2.1, repository
  vendor pins and the custom 8 MiB FQBN. No package manager or new dependency is needed.
- Real Device regression is owned by `tools/device_regression.ps1`. Read-only inventory found a
  concrete harness defect against the current canonical Device rule: it retries `PING` up to three
  times before every case and unconditionally writes `EXIT` from `finally`, including after normal
  readiness is lost. This is a P4-21 test-harness defect, not firmware behavior or another completed
  roadmap row.
- The minimal test-only correction performs exactly one initial `PING` with one fixed total
  deadline, keeps one carried partial-input owner across readiness and every case, classifies all
  queued complete lines and the carried partial before each write, and processes each current batch
  completely before accepting `PONG` or a case completion. Reset/panic takes precedence, and reset,
  panic, serial I/O failure or command timeout marks readiness lost and permits no later serial write.
  The finalizer closes the port without a command. If Web Console was confirmed active and a
  non-readiness assertion fails while serial is still responsive, one exact `EXIT` cleanup must
  observe `WEB_CONSOLE result=stopped`; its first failure marks readiness lost and ends the path. No
  retry/recovery/reset/upload/power/card action is added.
- The retained `hardware_web_e2e` full/SSH path is not eligible closure evidence because its source
  can auto-trust an unknown SSH host. P4-21 will not edit or run it. Final authenticated Web resource
  evidence uses one disposable existing-endpoint lifecycle that fails closed on unknown/mismatch,
  performs no profile/key/trust mutation, measures idle/open/closed state and restores the selected
  profile and closed terminal. P4-17/P4-20-specific journeys are not repeated.

### Frozen closure proof matrix

| Boundary | Smallest evidence | Forbidden effect |
|---|---|---|
| Canonical Phase 4 scope | Complete ROADMAP/P3/P4 reconciliation; all prior rows completed or explicitly removed | No silent defer, duplicate feature or P5-P9 implementation |
| Documentation/licenses/secrets | Existing third-party verifier, documentation audit and tracked filename-only secret scan | No secret contents printed and no ignored credential/key fixture included |
| Host/static/Web UI | CI-equivalent Python, MicroPython, strict host C++ and retained Web UI suites | No duplicated source-snapshot harness or weakened oracle |
| Exact firmware | Vendor pin check, one pinned 3.2.1 compile, options/FQBN gate, image hash and flash/global-RAM report | No alternate core/toolchain, erase or upload-as-recovery |
| Phase-branch Device evidence | Corrected retained runner plus focused changed-boundary selectors; complete hardware/Web regression remains the `develop`-to-`main` release-candidate gate | One initial readiness attempt; first loss ends all writes and no recovery chain |
| Web/active SSH resources | Final-image configured SSH terminal/SFTP plus fragmented-sequence probe and static no-extra-reservation ownership; numeric active-session heap/largest-block/worker-stack/latency observation removed by canonical decision | No inferred numeric value or performance/no-regression claim; no unknown trust, remote mutation, profile/key edit or P4-17/P4-20 replay |
| Cleanup/state | Exact fixture/artifact absence and original profile/project/chat/workspace selections/inventories | No unrelated delete, stash/ref/main mutation or retained disposable output |
| Publication | Fresh independent final review, Architect personal GO, row commit/push, green CI, reviewed PR and merge only to `develop` | No WIP push, history rewrite, `main` change or merge before gates |

Expected and actual row write set is exactly `ROADMAP.md`, `.agents/P4_TRACEABILITY.md`, the minimal
`tools/device_regression.ps1` readiness/oracle lifecycle correction and the existing
`firmware/CardputerAssistant/src/ssh_client.cpp` contiguous-headroom correction. Any other production,
retained UI, schema, route, policy, storage, vendor/config, build-workflow or test file is out of
scope and requires an explicit ownership decision before edit.

### Pre-device harness review and cheap evidence

- The resumed fresh read-only proof reviewer returned `STOP` before Device execution: reset/panic
  recognition originally logged before setting the terminal readiness state, and serial
  read/write/flush exceptions could leave readiness apparently available. The outer failure cleanup
  also treated an open port as proof of responsiveness and could therefore write `EXIT` after a
  transport failure.
- The bounded correction sets readiness loss before fallible crash logging, classifies every serial
  I/O exception before rethrow, and permits the one Web Console failure cleanup only after a
  completed non-reset case line explicitly confirmed the serial path responsive. The initial single
  `PING`, fixed deadline, timeout/reset ownership and close/dispose-only finalizer remain unchanged.
- Observed cheap evidence: vendor-pin/static gate passed; the native CI-equivalent third-party,
  MicroPython syntax/supervisor and Web UI checks passed; the corrected WSL strict C++ command
  returned `host_tests: PASS` and removed its exact-owned temporary ELF through its trap.
- The same reviewer used its single blocker-only follow-up and returned `GO`: both reset/panic paths
  set readiness loss before logging, serial I/O failures fail closed, Web cleanup requires an exact
  completion-line responsiveness observation, and `finally` only closes/disposes. The reviewer was
  then closed and is not reused for phase-final review.
- Architect then found one healthy-path lifecycle omission before closure: the standalone
  `web-console-start` suite contained only `CONSOLE`, so removing the unconditional finalizer write
  could leave the normal successful handler active. The bounded correction adds one explicit
  stopped-pattern `EXIT` case to that suite. Normal success now closes explicitly; a responsive
  non-readiness failure uses the separately guarded one-exit cleanup; readiness loss permits neither
  retry nor write.
- The one exact M5Stack 3.2.1 compile passed with exact FQBN and only resolved core `3.2.1`:
  application flash `3,440,354` bytes, globals `65,700` bytes, app image `3,440,544` bytes,
  SHA-256 `FD1103603FF9A98EE62787BF65B26916CE91528CE32BA68A9D4CE7C48C17B7E1`. This is byte-identical
  to the already uploaded P4-17 final image, so no upload is required or permitted as recovery.
- The tracked filename-only secret scan found zero suspicious credential/key/secret filenames.
  Read-only Git evidence retained `main` at `681cc8ffa9b6d26897d4847001d5d57f17b5d340`, the sole
  stash at `6073fd15eb2836351ef2ae4323926565339a495b`, an empty index, and only the frozen two tracked
  P4-21 paths plus the three approved Architect-owned untracked agent definitions.
- The first corrected `full` Device run reached and passed status, pure functions, UI budget,
  cancellation, storage/chat/file/settings, offline/SSH/Python/audio, Web Console start/status/exit
  plus post-console responsiveness, chat/file/search API and cache boundaries. It then failed the
  inherited external-model `SEARCHTEST`: the model returned 1,688 response bytes without any tool
  call (`search_called=no`, `tool=none`, missing required Web group `0x1`). No reset/panic occurred,
  the exact Web Console cleanup was observed, and the exact-owned temporary log was removed. This is
  not a full-regression pass and is classified outside P4 product/harness behavior pending ownership
  resolution; the unchanged experiment must not be repeated without a materially different proof.
- Architect classified that failure as a P4-21 retained-runner oracle defect, not firmware or P4
  production: `tool=none` means no callback supplied a name, `search_called=no` proves exact
  `web_search` never routed, and the positive response plus exact missing-required Web group `0x1`
  error is the existing fail-closed no-effect outcome. Both retained copies now accept only the
  historical `WebSearch` alias or `none` on that exact rejected branch; arbitrary/canonical failed
  names, generic or zero-byte/preflight failures remain rejected. The failed run is not acceptance
  evidence. The then-planned complete rerun is superseded by the closure evidence ownership below.
- Post-correction static evidence found exactly one initial `PING`, one standalone `CONSOLE`, one
  standalone `EXIT`, and no finalizer writes. One focused `web-console-start` Device selector then
  passed both cases with exactly one ready and one stopped observation; normal readiness remained
  healthy and the exact-owned temporary log was deleted.
- The single Architect-authorized corrected `full` run passed the corrected fail-closed
  `SEARCHTEST` and the following UI search path. It then stopped at the inherited `SSHPROBE` before
  the remaining cases: `libssh2_session_init_ex` could not allocate its `24,576`-byte block after
  earlier session allocations, with allocator evidence of `98,920` free internal bytes and the
  serial result reporting free heap `98,816`, largest 8-bit block `32,756`, stack `2,508`. No
  reset/panic occurred, Web Console cleanup stopped, and the temporary log was removed. Because the
  final image is byte-identical to P4-17 and this may be sequential-suite fragmentation/headroom,
  production ownership is unresolved; no rerun, reorder, oracle change or production edit is allowed
  before Architect classification. This is not full-regression acceptance evidence.
- The first disposable Web/active-SSH resource attempt is classified only as a wrapper/reporting
  defect: it lost the Node result and synthesized `primary=none; cleanup=none`, so it proves neither
  product PASS nor readiness/product failure. Architect authorized one materially different attempt
  whose Node process emits one bounded non-secret structured result before exit handling, whose
  holder preserves primary and cleanup outcomes separately, and whose single shared health state
  gates every later serial/HTTP write. A first transport/readiness loss ends without EXIT/retry;
  only a healthy product/assertion failure permits one verified Console stop. No retained harness,
  trust/profile/key mutation, build/upload or replay is allowed.
- The one authorized corrected resource attempt emitted and preserved exactly one structured result.
  Serial/Web health remained `healthy`, no trust was submitted, selected-profile SSH cleanup reached
  stopped, the Console reached its exact stopped marker, and the disposable script was removed. The
  selected existing profile nevertheless reached `ssh_connection_failed` during connect before any
  active-session metric or marker; this supplies no active-SSH performance PASS and is not repeated.
  Together with the preceding public probe allocation failure it requires ownership classification,
  but does not by itself prove the same root cause or expose any host/key/credential authority detail.
- The final Architect-authorized owner-classification observation was direct and conclusive. Before
  selected-profile connect, authenticated public metrics were free heap `97,272`, largest block
  `34,804`, minimum heap `6,560`, main-loop stack `2,508`, reset reason `1`. The existing public
  worker error sanitized to `session_alloc`, matching the independent `SSHPROBE` vendor-allocation
  boundary rather than endpoint/TCP/handshake/auth. No trust was submitted; profile/key identity and
  revision remained unchanged; SSH cleanup and exact Console stop passed; the disposable script was
  removed. This is a real production resource blocker, not active-SSH acceptance evidence, and no
  further Device/HTTP action is permitted before Architect localization.

### Active-SSH contiguous-headroom correction pre-edit gate

**Status:** runtime_STOP; reviewed reorder compiled/uploaded but did not satisfy late-suite
contiguous-headroom acceptance; read-only ownership analysis required before any further Device or
production action.

- Architect localized the two independent failures to P4-21's locked active-SSH resource acceptance:
  the public probe and the selected-profile Web worker both report `session_alloc`. The startup order
  predates P4, so no completed row is reopened. Installed libssh2 1.11.1 `session.c` confirms that
  `libssh2_session_init_ex` has no socket dependency and allocates `sizeof(LIBSSH2_SESSION)`, then
  exact `24,576`- and `16,384`-byte packet buffers. The failed `24,576` allocation is therefore
  simultaneous contiguous-headroom exhaustion while TCP/banner buffers are live.
- Minimal candidate: in both `probeSshHost` and `SshClient::connectControlled`, move only the existing
  allocator setup plus `libssh2_session_init_ex` block before the existing TCP connect. Validation,
  first cancellation, timeout values, blocking mode, KEX preference, trust/host-key, handshake,
  authentication, errors and all later behavior stay in their current relative order. No allocation
  is retried and no resource threshold/fallback is added.

| Existing return boundary after reorder | Exact durable/runtime cleanup |
|---|---|
| Probe validation or `libssh2_init` failure | Unchanged: no session/client/runtime ownership exists |
| Probe session allocation failure before TCP | Unchanged error; no session exists; call existing `libssh2_exit` |
| Probe TCP connect failure | Free the already-created session, then existing `libssh2_exit`; no connected client exists |
| Probe banner timeout or non-SSH first byte | Free session, then existing `client.stop` and `libssh2_exit` |
| Probe KEX preference, handshake or host-key formatting failure | Existing session free/disconnect where applicable, client stop and runtime exit remain unchanged |
| Probe success | Existing disconnect, session free, client stop and runtime exit remain unchanged |
| Regular validation/first cancellation/runtime-init failure | Unchanged: no new owner exists |
| Regular session allocation failure before TCP | Existing `close()` sees runtime initialized, no session/network, and exits libssh2 exactly once |
| Regular TCP failure or post-connect cancellation | Existing `close()` frees the preallocated session, stops network and exits runtime exactly once |
| Regular KEX/handshake/host-key failure or later caller close | Existing `close()` remains the sole session/channel/SFTP/network/runtime cleanup owner |

- Forbidden: allocator/pool/framework, PSRAM scheme, packet/vendor/KEX/config change, retry, delay,
  recovery, worker-stack tuning, test reordering, fallback, resource guard, new helper/type/test,
  diagnostic, API/UI or adjacent SSH behavior.
- Proportional proof after a fresh pre-edit `GO`: cheap diff/static/host checks; one exact pinned build
  and upload as implementation verification, never recovery; one corrected `full` suite with no
  further oracle/order change; then one authenticated no-auto-trust selected-profile pre/open/closed
  resource, worker-stack and latency observation with read-only marker and exact cleanup. Compare
  against the failing pre-connect `97,272` free / `34,804` largest topology, retained active-SSH
  baseline and 70-KiB idle floor. First readiness loss ends the path without escalation.
- A fresh bounded read-only reviewer returned `GO` with no blocker. It verified from installed
  libssh2 source that session initialization has no socket dependency; enumerated every probe and
  regular early return as single-owned with no leak/double-free; confirmed preallocating only the
  existing session and packet buffers is the smallest coherent contiguous-headroom correction; and
  confirmed the frozen full-suite plus selected-profile proof reaches both changed paths. The
  reviewer was closed immediately and is not reused for implementation or closure review.
- Architect personally returned pre-edit `GO` for the frozen three-path design. The following
  primary patch call returned truncated tool output, so its application result is deliberately not
  inferred. At the 2026-09-01 reboot-safe boundary no validation or external action was running;
  resume must inspect the authoritative `ssh_client.cpp` and this row once, then complete only any
  missing frozen hunks rather than reapplying the patch blindly.
- After reboot, authoritative source inspection proved that the primary patch had applied the exact
  frozen reorder in both call paths with all three new probe cleanup edges present and no duplicate
  allocator/session block. `git diff --check`, the focused ordering/cleanup static check and all
  pinned vendor checks passed. The strict WSL host suite returned `host_tests: PASS`; its first
  invocation was a command-quoting failure before compilation, and the corrected collision-checked
  literal exact-owned ELF invocation passed and trap-cleaned the file.
- A fresh post-patch read-only code reviewer returned `GO`: the actual diff preserves validation,
  cancellation, timeout, KEX, trust, handshake/auth and error semantics; probe early returns and
  regular `close()` remain singly owned with no leak or double-free; and no hidden responsibility or
  adjacent behavior was added. The reviewer was closed and will not be reused for closure.
- The exact pinned M5Stack 3.2.1 build passed with exact FQBN, only resolved core `3.2.1`, flash
  `3,440,382` bytes, globals `65,700` bytes and app image `3,440,576` bytes, SHA-256
  `313CE0CEA69959384F29682617D4F4FF8C97ECB1FC38D5764D7FADBEBE09243C`. The single authorized
  implementation-verification upload completed with flash hash verified and COM8 re-enumerated.
- The one corrected `full` regression again passed all preceding boundaries through model search and
  UI search, then stopped at `SSH host probe`: `libssh2_session_init_ex` still failed its exact
  `24,576`-byte allocation with `100,916` free allocator bytes; post-failure metrics were free heap
  `101,068`, largest block `28,148`, stack `2,508`. No reset/panic or readiness loss occurred; Web
  cleanup had stopped and the collision-checked exact-owned log was deleted. The frozen reorder is
  therefore insufficient for late-suite contiguous headroom and is not closure evidence. No repeat,
  selected-profile Web proof or further production edit is permitted until ownership is reclassified.

### Second and final contiguous-headroom pivot gate

**Status:** numeric_active_observation_removed; the full observer history below is retained, its
values remain unknown, and the missing sample is a user-accepted residual rather than acceptance
evidence. P4-21 remains the only `in_progress` row pending personal Architect closure review.

- Target-object disassembly of the exact pinned ESP32-S3 libssh2 1.11.1 build proved
  `sizeof(LIBSSH2_SESSION) == 10,744`; installed source fixes the following two allocations at
  `PACKETBUFSIZE == 24,576` and `MAX_SSH_OUT_PACKET_LEN == 16,384`. The observed largest block
  `28,148` is restored only after failed init unwinds the first `10,744` bytes, so moving init before
  TCP cannot guarantee the three internal allocations on the late-suite fragmented heap.
- Architect authorized one final narrow design in existing `ssh_client.cpp`: retain the pre-TCP
  session-init order and add one shared pinned-vendor init adapter. It transiently reserves exactly
  the three already-required blocks in descending size order, using the existing selected heap
  capabilities and the private-header constants/`sizeof` rather than production numeric literals.
  Existing allocator callbacks then transfer those exact pointers to libssh2 in its pinned request
  order with no copy and no additional steady-state allocation. This is not a persistent pool,
  fallback, retry or reusable allocator framework.

| Transition | Exact ownership and result |
|---|---|
| Adapter entry | Reservation slots are empty; failed-size/index/mismatch state is reset for this one init |
| Reserve `PACKETBUFSIZE`, output packet, then session struct | Each successful pointer is adapter-owned; any reservation failure records its exact source-derived size, frees only prior reservations and never calls libssh2 |
| Expected allocate callback | Exact next pinned size and non-null slot transfer that pointer to libssh2, clear the slot and advance once; no heap allocation occurs |
| Unexpected allocate size/order/count or init-time realloc | Set mismatch/failed size and return null; libssh2 unwinds every already-transferred pointer through the existing free callback, then the adapter frees only still-non-null reservations |
| Init returns null | Disable reservation mode, free only remaining adapter-owned slots, and return null; vendor init has already unwound every transferred pointer |
| Init returns non-null but transfer count/state is incomplete or mismatched | Disable reservation mode, call `libssh2_session_free()` so the returned session releases every transferred pointer, then free only still-non-null adapter-owned slots and return null |
| Init returns complete session | All three slots are null and exactly three transfers occurred; disable reservation mode and return the session; every later alloc/realloc/free is the unchanged direct heap callback path |
| Probe/regular later failure or close | Existing probe branches and `SshClient::close()` remain the sole owners of the returned libssh2 session, network and runtime cleanup |

- Forbidden remains: vendor/config/buffer-size change, test reorder/delay, persistent pool/reserve,
  retry/fallback, generic allocator API, new file/type/framework, resource threshold, Device/UI/API
  behavior or any third implementation approach. The tracked write set stays exactly the existing
  `ssh_client.cpp`, P4-21 runner and this trace.
- Frozen proof: one bounded pre-edit review of reservation ownership and unexpected-order cleanup;
  one coherent primary patch; cheap diff/static/strict-host checks and fresh code review; one exact
  pinned build/options gate/upload; one unchanged corrected `full` suite; then one authenticated
  no-auto-trust selected-profile pre/open/closed resource, worker-stack and latency observation with
  exact cleanup. First readiness loss ends the path. If this changed boundary fails, implementation
  stops without a third approach.
- The fresh bounded design reviewer returned one `STOP` on the initially omitted owner for a
  non-null but rejected session. The corrected transition disables reservation mode, frees that
  session through `libssh2_session_free()`, then frees only untransferred slots. Its single
  blocker-only follow-up returned `GO`; the reviewer was closed and will not be reused.
- Architect personally returned pre-edit `GO` for the corrected state machine. The primary agent
  then implemented exactly one source-derived descending reservation adapter in existing
  `ssh_client.cpp`, extended only the existing allocator state/callbacks, and routed the probe and
  regular client through it while retaining their pre-TCP session-init order. No vendor, runner,
  API, config or additional production owner changed.
- Post-patch cheap evidence passed: `git diff --check`; one adapter/one pinned vendor-init/two-callsite
  ownership; source-derived sizes with no allocation-size literal in the adapter; rejected-session
  cleanup after reservation mode is disabled; and the strict WSL suite returned `host_tests: PASS`
  with its collision-checked exact-owned ELF trap-cleaned. Fresh actual-code review is pending before
  any build.
- A different fresh actual-code reviewer returned `GO`: reservations are exactly
  `24,576 -> 16,384 -> 10,744`, callback transfers exactly `10,744 -> 24,576 -> 16,384`, pointers
  change owner without copy or extra allocation, every null/non-null/mismatch path has one cleanup
  owner, allocator state outlives each session, repeated connect/close is safe, and all cancellation,
  timeout, KEX, trust, handshake/auth and error behavior remains unchanged. The reviewer was closed.
- The materially changed exact M5Stack 3.2.1 build passed with exact FQBN, sole resolved core `3.2.1`,
  flash `3,440,982` bytes, globals `65,700` bytes and app image `3,441,168` bytes, SHA-256
  `5267E73C29147DD78B58184C3296950CE041EC6ECB0E1557C3417712AC57F171`. A delayed Architect hold
  arrived after the independent reviewer had already returned `GO` and the build had completed; no
  upload had started. Architect then personally reviewed the actual adapter diff, accepted Hooke's
  real verdict and this exact build/options/hash gate, and returned `GO` for one upload plus the two
  frozen runtime proofs. The later duplicate Architect reviewer was interrupted without a verdict
  and is not evidence.
- The single approved upload of that exact image completed with every flash hash verified and COM8
  re-enumerated. The one unchanged `full` suite then passed all cases through chat API but stopped
  before the changed SSH probe at inherited `model file tool`: `TOOLTEST result=failed
  stage=tool_roundtrip api=pass write=failed file=failed link=failed cleanup=pass error=`. No
  reset/readiness-loss marker was observed and the collision-checked exact-owned log was deleted.
  This is neither full-regression nor SSH-adapter evidence; no repeat, selected-profile proof or
  production edit is allowed until the foreign/harness/provider ownership is classified.
- Architect classified that `TOOLTEST` observation as unchanged external-model/diagnostic
  nondeterminism: the prior uploaded image passed the same exact model/Web/search/UI prefix, the
  final adapter has no model/file dependency, and both outcomes remained fail-closed with exact
  cleanup. No model case was repeated. One disposable selector then reused only existing retained
  case objects/patterns for the exact 23-case post-model tail. In one healthy COM8 session it passed
  `SSHPROBE`, public SSH SFTP/PTY, STT/TTS, post-online responsiveness, configured SSH terminal and
  SFTP, OTA metadata/download, P2 schema/migration/recovery and project Device parity. It then timed
  out after 180 seconds waiting for inherited `shared project isolation`; the runner ended the path
  and the exact-owned log was removed. The changed allocator boundary therefore passed, but this is
  not a complete composed regression and the selected-profile resource proof was not attempted.
- The temporary selector was removed immediately. Its inverse hunk exposed four line terminators
  normalized by patch application; exact byte repair restored the pre-selector retained runner to
  SHA-256 `C13093F91ED9FCE0F8A64F807149B709288DFC65565D953B8D697E49F9CCE3E8`, `29,408` bytes, with
  zero temporary selector markers. No disposable mode or oracle remains in the row diff.
- Architect personally classified the final evidence. The final-image `SSHPROBE`, configured
  selected-profile SSH terminal and configured SFTP passes prove the adapter's functional
  connect/trust/auth/PTY/SFTP boundary under the fragmented closure sequence; the unchanged
  `TOOLTEST` and inherited P2 timeout reveal no demonstrated P4 production defect. They do not prove
  the separately locked active-SSH resource observable: `runSshSessionTest` closes the session before
  any retained sample, its surfaced output contains no exact serial metrics, existing samples are
  post-close/main-stack, and largest block plus elapsed active latency are absent. Static proof that
  the adapter leaves no extra reservation after init cannot substitute for active heap topology,
  worker-stack margin or latency. This is the sole missing runtime proof. The failed-readiness path
  is permanently ended: no further Device/serial/HTTP/Web action, recovery, build/upload, oracle,
  harness or production correction is authorized. P4-21 cannot be completed, staged, committed,
  pushed, reviewed for final merge or published until a separately valid proof decision exists.
- The user subsequently prompted and Architect recorded one narrow final exception for the missing
  observable. It authorizes exactly one new disposable direct observer against the already-uploaded
  final image, not recovery or reuse of the prior broad path: one COM8 open, one fixed-deadline
  `PING`, and one `CONSOLE` only after exact `PONG`. Missing `PONG` or prefix-matched ready closes and
  disposes only, removes the observer and permanently stops without `EXIT`, retry, reset, rebuild,
  upload or physical action. A healthy holder stays open through one ignored-credential authenticated
  Web lifecycle using only the selected profile, never submits trust, captures comparable pre/open/
  closed heap, minimum heap, largest block, actual SSH worker-stack and bounded connect/open latency,
  executes one established no-history read-only marker, restores selected/closed state, then sends
  one `EXIT` and requires exact stopped. Product/assertion failure permits only the same exact cleanup
  and one guarded `EXIT`. Production, retained tests/runner, profile/key/trust/known_hosts,
  project/chat/workspace/remote state, build and upload remain frozen. This decision permits one
  attempt only; no repeat or recovery chain exists.
- After the system restart, the complete visible 21-row plan was restored with only P4-21 active.
  The one attempt remained unconsumed. Its collision-checked disposable observer is limited to one
  serial holder and the authenticated read/start/input/output/stop Web boundaries. After draining
  login output, one empty carriage-return line is the no-history marker: it executes no shell
  command, creates no history entry or durable remote effect, and must produce fresh PTY output
  while the same session remains open. The observer reports only boolean identity preservation and
  non-secret metrics; it never emits profile IDs, hosts, credentials, cookies, CSRF values, terminal
  output or authority internals. Its exact local file is removed after the sole process exits.
- The sole authorized attempt is now consumed. One COM8 holder received exact `PONG` and the
  prefix-matched Web Console ready marker; ignored-credential login, authenticated session and the
  selected-profile preflight succeeded, with a valid nonzero selected public ID observed only as a
  boolean. `POST /api/ssh/start` then returned HTTP `202`, while the disposable observer incorrectly
  required `200`; it therefore stopped before polling connection state or sampling active resources.
  This is an observer status-oracle defect, not product failure evidence. The same wrong status
  assumption made its attempted SSH-stop cleanup report failure before polling; terminal closed/profile
  preservation is consequently unobserved. The healthy serial holder still sent its single guarded
  `EXIT` and observed exact `WEB_CONSOLE result=stopped`. No trust request, remote command, profile/key/
  known-host/project/chat/workspace mutation, build, upload, reset or recovery occurred. The exact
  disposable observer was removed. No active heap/largest-block/worker-stack/latency metric exists,
  no repeat is permitted, and P4-21 remains closure-evidence STOP pending Architect ownership/closure
  decision.
- Architect conclusively assigned that attempt to the disposable observer and invalidated it as
  acceptance evidence: installed `web_console.cpp` intentionally returns successful HTTP `202`
  after accepting SSH start and while accepting stop, while idle stop may return `200`; the canonical
  retained Web client accepts every successful 2xx and polls `/api/ssh/state` for the authoritative
  stage. Exact `PONG`, prefix-ready and stopped observations prove readiness was never lost, so the
  recovery prohibition was not triggered. Architect superseded only the no-repeat clause with one
  final oracle-corrected rerun. It must accept 200 through 299 for start/stop, poll connected/open and
  idle/closed states, otherwise preserve the same frozen endpoint/write/no-trust/no-mutation contract.
  A readiness loss or any second observer defect permanently ends the path without another run.
- The oracle-corrected rerun did not launch and remains unconsumed: the platform rejected the
  action-time COM8/authenticated-Web execution before creating the process because it requires a
  fresh direct user approval for this retry. No serial port, HTTP session or Device state was
  touched. The unexecuted exact-owned observer was removed. This is an external approval blocker,
  not proof or product evidence; it may not be bypassed or retried until the platform accepts the
  user's explicit authorization.
- The user then supplied the required direct action-time authorization for exactly this single
  oracle-corrected COM8/local-WebUI lifecycle, explicitly excluding build, upload, reset and
  recovery. The rerun remains unconsumed until process creation; its endpoint, state, mutation and
  cleanup constraints remain otherwise unchanged.
- The platform again rejected execution before process creation because the direct authorization
  was relayed through Architect rather than posted by the user in this task. The corrected rerun is
  still unconsumed; no COM8, HTTP or Device action occurred, and the recreated disposable observer
  was removed. The same route may not be attempted or worked around until this task receives the
  tool-enforced direct user authorization.
- The user then posted that exact action-time authorization directly in this task. The corrected
  rerun remains unconsumed until its process starts and is now eligible for the same frozen static
  gate and single execution.
- The platform nevertheless rejected the exact command again before process creation, classifying
  even the direct current-task message as an untrusted transcript delta. The required static gate
  had passed, but no COM8, HTTP or Device action occurred; the corrected rerun remains unconsumed
  and the exact observer was removed. Tool policy explicitly prohibits another invocation,
  workaround or indirect route. P4-21 therefore remains externally blocked on this exact missing
  active-resource observation pending Architect closure disposition; this is not product evidence.

### Evidence-ready closure reconciliation and accepted residual

**Status:** completed after mandatory personal Architect closure `GO`. Publication, CI, PR and merge
evidence follows through the terminal row-close/GitHub workflow and is not pre-claimed here.

- The row-owned row payload remains exactly four paths: `ROADMAP.md`, this trace, the reviewed
  `ssh_client.cpp` contiguous-headroom correction and the reviewed `device_regression.ps1`
  readiness/oracle/stream-ownership correction. The index is empty. The only untracked paths are the
  three approved Architect-owned `.codex/agents` definitions, outside P4-21 ownership.
- The earlier P4-21 requirement for one complete `full` Device suite is superseded by canonical
  release ownership: ROADMAP requires focused regression before a phase branch enters `develop`,
  while complete hardware and Web Console regression belongs to the later `develop`-to-`main`
  release-candidate gate. The incomplete retained full runs, external-model `TOOLTEST` nondeterminism
  and inherited P2 timeout remain honest compositional evidence; no further Device suite is run.
- Architect's personal closure review found one final retained-runner defect: readiness and each case
  owned separate partial buffers, discarded queued serial input before writes and could accept a
  completion before a later reset in the same batch. The bounded correction uses one carried partial
  owner, classifies queued complete/partial input before every write, processes the entire current
  batch with reset/panic precedence, and makes readiness loss terminal for every later write including
  failure cleanup. This changes only the retained runner; `git diff --check`, PowerShell parsing,
  exact write-site/stream-state/path checks and a fresh independent read-only proof red-team all
  passed with no blocker.
- The final proof red-team then identified one evidence-only `STOP`: every earlier fragmented SSH pass
  used the old reader, so a reset after a completion line in the same serial batch could have been
  hidden. Architect authorized one focused four-case sequence on the already-uploaded final image
  through the corrected runner. It completed before a later superseding cancellation arrived; that
  cancellation remains binding against every future Device/Web/build repetition.
- The single corrected-reader run passed `SSHPROBE`, public SSH SFTP/PTY, configured selected-profile
  SSH terminal and configured SFTP in one COM8 session. The full current batches contained no reset,
  panic or readiness loss. The collision-owned temporary log was deleted and verified absent, the
  tracked runner remained byte-identical, and no build, upload, reset, recovery or full suite ran.
- Architect personally returned final P4-21 closure `GO` on the exact current payload. The executed
  production paths are `probeSshHost` and regular `SshClient::connectControlled` through the
  source-derived `initializeReservedSshSession` libssh2 allocation-transfer boundary. The focused
  final-image sequence proves changed probe, public SFTP/PTY, selected terminal and configured SFTP
  behavior under the corrected reader. Forbidden effects were absent: no reset/panic/readiness loss,
  no steady adapter reservation, no profile/key/trust/known-host/project/chat/workspace/remote
  mutation, and no retained temporary log. Numeric active-session heap/largest-block/actual
  worker-stack/latency remain unknown with no optimization, performance or no-regression claim; the
  inherited P2 terminal fixture absence remains the accepted exact-owned foreign-P2 residual.
- Every prior implemented P4 row is published on the phase branch through
  `3bfb2640b85e551418e32fb2498144937de2beae`; authenticated GitHub resolves that exact SHA.
  All local phase commits report the required Alexey Bulygin Author and Committer. `develop` remains
  `99f6de9e0e5d558ba0162c714a14044111d9aab2`, `main` remains
  `681cc8ffa9b6d26897d4847001d5d57f17b5d340`, and the sole retained stash remains
  `6073fd15eb2836351ef2ae4323926565339a495b`.
- CI-equivalent third-party, MicroPython, strict host C++, static/vendor-pin and retained Web UI
  checks passed. Documentation, plaintext-at-rest threat model, licenses and filename-only secret
  checks passed without exposing secret contents. The exact final M5Stack 3.2.1/FQBN build passed at
  flash `3,440,982`, globals `65,700`, app image `3,441,168`, SHA-256
  `5267E73C29147DD78B58184C3296950CE041EC6ECB0E1557C3417712AC57F171`; the single upload verified
  every flash hash.
- On that final image, one healthy composed tail passed the changed `SSHPROBE`, public SSH SFTP/PTY,
  configured selected-profile SSH terminal, configured SFTP and all intervening non-SSH boundaries
  until an inherited P2 timeout. The allocator therefore passed functional connect/trust/auth/PTY/
  SFTP use under the fragmented closure sequence. Static and independent code review prove the
  reservation adapter transfers only the exact pinned libssh2 init allocations, leaves no adapter
  reservation after init and preserves all later allocator/cleanup behavior. No reset, panic or
  freeze was observed in the accepted changed-boundary evidence.
- The idle/general resource floor remains observed above 70 KiB and the final image retained the
  accepted globals budget. Post-correction numeric active-session heap, largest-block, actual
  worker-stack and latency were not measured; their values are unknown. The only eligible observer
  was prevented before process creation by repeated platform action-time rejection, even after
  direct user approval; no Device readiness loss or product failure occurred. ROADMAP now removes
  that numeric observation from Phase 4 acceptance and records the user's explicit acceptance of the
  measurement residual, supported only by the final-image functional SSH pass and the
  no-extra-steady-state-allocation review. This does not weaken a product behavior, infer a value,
  claim measured optimization or active-session no-regression, or authorize a future retry/recovery
  chain.
- All P4-owned disposable logs, selectors and observers named by this row are absent. No P4-owned
  profile, key, trusted-host, project, chat, workspace or remote fixture remains; selection/identity
  stayed unchanged in every successful observable lifecycle. For the inherited `P2SHAREDTEST`
  timeout, the later successful `PONG` proves the synchronous selector returned to the dispatcher and
  source inspection proves every owned-fixture path invokes cleanup before return. Its terminal
  cleanup result and final fixture absence were not observed after the timeout, so exact final absence
  remains unknown and is accepted only as an exact-owned foreign-P2 residual. The latest rejected
  observer never created a process or touched COM8/HTTP/Device state.
- The five Phase 4 product acceptance boundaries remain independently satisfied by completed rows:
  P4-05 proves cancellable long foreground output plus downloadable SD log; P4-03/P4-08 prove that
  read-only authority cannot dispatch arbitrary remote mutation; P4-06 proves visible SFTP
  destination and mandatory overwrite confirmation; P4-11/P4-17/P4-20 prove mismatch blocks before
  authentication; P4-01/P4-02/P4-15 prove model/file/API non-addressability of credential and
  private-key material. The unavailable numeric sample is closure performance evidence, not one of
  these behaviors.
- Authenticated GitHub currently reports no workflow run and no PR for the uncommitted P4-21
  payload, as expected. A green branch/PR CI run, reviewed PR and merge only to `develop` remain
  mandatory post-GO publication gates; `main` must remain unchanged.
