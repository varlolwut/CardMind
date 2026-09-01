import { open, readFile, rename } from 'node:fs/promises';
import { createHash } from 'node:crypto';

const credentialPath = new URL('../.secrets/ui-test-credentials.json', import.meta.url);
const maximumRequestMs = 45_000;
const maximumLargeStreamMs = 45 * 60_000;
const p2LargeStreamBytes = 335_544_320;
const formerLargeFileBoundaryBytes = 256 * 1024 * 1024;
const p2LargeStreamSha256 =
  '287c90b8a40b203b0e154463f88c39bd853b8ca6b82b4539a913f46f9684608b';
const maximumSteadyHeapLossBytes = 4096;
const p2LargeStreamMinimumHeapFloorBytes = 28 * 1024;
const p2LargeStreamListingMinimumHeapLossBytes = 8192;
const p2LargeStreamDownloadMinimumHeapLossBytes = 4 * 4096;
const p2LargeStreamJsonWindowMinimumHeapLossBytes = 3 * 12_288;
const contextHistoryOrphanTitlePattern = /^P2 context history ([0-9]{13})$/;
const historyHeapMessageBytes = 16_384;
const historyHeapSmallMessages = 2;
const historyHeapLargeMessages = 16;
const statePaths = {
  status: '/api/status',
  chats: '/api/chats',
  chat: '/api/chat',
  files: '/api/files',
  ssh: '/api/ssh/state',
  settings: '/api/settings',
};

function requireString(value, field) {
  if (typeof value !== 'string' || value.length === 0) {
    throw new Error(`Credential field '${field}' must be a non-empty string`);
  }
  return value;
}

function form(values) {
  return new URLSearchParams(values);
}

function rawTextRequest(body, headers) {
  return {
    method: 'POST',
    headers: {'Content-Type': 'text/plain; charset=utf-8', ...headers},
    body,
  };
}

function projectSettingsRequest(values) {
  return rawTextRequest(values.instructions, {
    'X-CardMind-Model-Encoded': `v1:${encodeURIComponent(values.model)}`,
    'X-CardMind-Context-Bytes': values.context_byte_budget,
    'X-CardMind-Output-Tokens': values.maximum_output_tokens,
    'X-CardMind-Auto-Compact': values.automatic_compaction,
  });
}

function chatInstructionsRequest(instructions) {
  return rawTextRequest(instructions, {});
}

function promptRequest(promptValue, maximumOutputTokens) {
  return rawTextRequest(promptValue, {
    'X-CardMind-Output-Tokens': maximumOutputTokens,
  });
}

function framedPromptRequest(promptValue, requestInstructions, maximumOutputTokens) {
  const instructionBytes = Buffer.from(requestInstructions, 'utf8');
  const promptBytes = Buffer.from(promptValue, 'utf8');
  if (instructionBytes.length > 2048) {
    throw new Error('Request instructions exceed 2048 UTF-8 bytes');
  }
  if (promptBytes.length < 1 || promptBytes.length > 16_384) {
    throw new Error('Prompt must contain 1 to 16384 UTF-8 bytes');
  }
  const prefix = Buffer.alloc(4);
  prefix.writeUInt32BE(instructionBytes.length, 0);
  return {
    method: 'POST',
    headers: {
      'Content-Type': 'application/vnd.cardmind.prompt-v1',
      'X-CardMind-Output-Tokens': maximumOutputTokens,
    },
    body: Buffer.concat([prefix, instructionBytes, promptBytes]),
  };
}

async function fetchWithin(url, options, timeoutMs) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeoutMs);
  try {
    const response = await fetch(url, {...options, signal: controller.signal});
    const body = await response.arrayBuffer();
    return new Response(body, {
      status: response.status,
      statusText: response.statusText,
      headers: response.headers,
    });
  } finally {
    clearTimeout(timer);
  }
}

async function login(baseUrl, password) {
  const response = await fetchWithin(new URL('/login', baseUrl), {
    method: 'POST',
    body: form({password}),
    redirect: 'manual',
  }, maximumRequestMs);
  if (response.status !== 303) {
    throw new Error(`CardMind login failed with HTTP ${response.status}`);
  }
  const cookie = response.headers.get('set-cookie')?.match(/cm_session=[^;]+/)?.[0];
  if (cookie === undefined) {
    throw new Error('CardMind login response did not include a session cookie');
  }
  const session = await fetchWithin(new URL('/api/session', baseUrl), {
    headers: {Cookie: cookie},
  }, maximumRequestMs);
  if (!session.ok) {
    throw new Error(`CardMind session request failed with HTTP ${session.status}`);
  }
  const document = await session.json();
  return {cookie, csrf: requireString(document.csrf, 'csrf')};
}

async function requestWithin(baseUrl, auth, path, options, timeoutMs) {
  const headers = new Headers(options.headers);
  headers.set('Cookie', auth.cookie);
  headers.set('X-CardMind-CSRF', auth.csrf);
  const response = await fetchWithin(new URL(path, baseUrl), {
    ...options,
    headers,
  }, timeoutMs);
  if (!response.ok) {
    let detail = await response.text();
    try {
      detail = JSON.parse(detail).error ?? detail;
    } catch {
      // Preserve a non-JSON error body for diagnostics.
    }
    throw new Error(`${path} failed with HTTP ${response.status}: ${detail}`);
  }
  return response;
}

async function request(baseUrl, auth, path, options) {
  return requestWithin(baseUrl, auth, path, options, maximumRequestMs);
}

async function longRequest(baseUrl, auth, path, options) {
  return requestWithin(baseUrl, auth, path, options, 180_000);
}

async function streamWorkspaceFileSha256(baseUrl, auth, name, expectedBytes) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), maximumLargeStreamMs);
  try {
    const response = await fetch(
      new URL(`/api/file/download?name=${encodeURIComponent(name)}`, baseUrl),
      {
        method: 'GET',
        headers: {
          Cookie: auth.cookie,
          'X-CardMind-CSRF': auth.csrf,
        },
        signal: controller.signal,
      },
    );
    if (!response.ok) {
      const detail = (await response.text()).slice(0, 4096);
      throw new Error(
        `/api/file/download failed with HTTP ${response.status}: ${detail}`,
      );
    }
    if (response.body === null) {
      throw new Error('Large streamed download returned no response body');
    }
    const contentLengthValue = response.headers.get('content-length');
    const contentLength = contentLengthValue === null
      ? null
      : parsePositiveSafeInteger(contentLengthValue, 'Large download Content-Length');
    if (contentLength !== null && contentLength !== expectedBytes) {
      throw new Error(
        `Large download Content-Length was ${contentLength}; expected ${expectedBytes}`,
      );
    }
    const digest = createHash('sha256');
    let bytes = 0;
    let chunks = 0;
    let maximumChunkBytes = 0;
    for await (const chunk of response.body) {
      const buffer = Buffer.from(chunk);
      bytes += buffer.length;
      chunks++;
      maximumChunkBytes = Math.max(maximumChunkBytes, buffer.length);
      if (bytes > expectedBytes) {
        throw new Error(
          `Large streamed download exceeded ${expectedBytes} bytes at chunk ${chunks}`,
        );
      }
      digest.update(buffer);
    }
    if (bytes !== expectedBytes) {
      throw new Error(
        `Large streamed download ended at ${bytes} bytes; expected ${expectedBytes}`,
      );
    }
    return {
      bytes,
      sha256: digest.digest('hex'),
      chunks,
      maximum_chunk_bytes: maximumChunkBytes,
      content_length: contentLength,
    };
  } finally {
    clearTimeout(timer);
  }
}

async function expectRequestFailure(baseUrl, auth, path, options, status) {
  try {
    await request(baseUrl, auth, path, options);
  } catch (error) {
    if (error.message.includes(`${path} failed with HTTP ${status}:`)) {
      return;
    }
    throw error;
  }
  throw new Error(`${path} accepted input that should fail with HTTP ${status}`);
}

async function expectRequestFailureDetail(baseUrl, auth, path, options, status) {
  try {
    await request(baseUrl, auth, path, options);
  } catch (error) {
    const prefix = `${path} failed with HTTP ${status}: `;
    if (error.message.startsWith(prefix)) {
      return error.message.slice(prefix.length);
    }
    throw error;
  }
  throw new Error(`${path} accepted input that should fail with HTTP ${status}`);
}

async function verifySdDegradedState(baseUrl, auth, expectedState, nonce) {
  const contracts = {
    missing: {status: 503, code: 'micro_sd_required', readable: false},
    full: {status: 507, code: 'micro_sd_full', readable: true},
    removed: {status: 503, code: 'micro_sd_removed', readable: false},
    replaced: {status: 409, code: 'micro_sd_replaced', readable: false},
  };
  const contract = contracts[expectedState];
  if (contract === undefined) {
    throw new Error(`Unsupported degraded microSD state '${expectedState}'`);
  }
  if (!/^[0-9]{8,20}$/.test(nonce)) {
    throw new Error('SD-degraded nonce must contain 8 to 20 decimal digits');
  }
  const fixturePath = `cardmind_p2_22_${nonce}/atomic.txt`;
  const fixtureContent = 'CARDMIND_P2_22_ORIGINAL\n';
  const statusResponse = await request(baseUrl, auth, '/api/status', {method: 'GET'});
  const status = await statusResponse.json();
  if (status.sd_state !== expectedState || status.sd_error_code !== contract.code ||
      status.sd_readable !== contract.readable || status.sd_writable !== false ||
      typeof status.sd_error !== 'string' || status.sd_error.length === 0) {
    throw new Error(
      `Typed microSD status mismatch for ${expectedState}: ${JSON.stringify(status)}`,
    );
  }
  let fullReadProof = null;
  const readPaths = ['/api/projects', '/api/chats', '/api/chat', '/api/files'];
  if (contract.readable) {
    const projects = await (
      await request(baseUrl, auth, '/api/projects', {method: 'GET'})
    ).json();
    const chat = await (
      await request(baseUrl, auth, '/api/chat', {method: 'GET'})
    ).json();
    if (typeof projects.active_project_id !== 'string' ||
        projects.active_project_id.length === 0 ||
        !Array.isArray(projects.projects) ||
        !projects.projects.some((project) => project.id === projects.active_project_id) ||
        chat.project_id !== projects.active_project_id ||
        typeof chat.active_chat_id !== 'string' || chat.active_chat_id.length === 0) {
      throw new Error('Full microSD startup did not preserve the committed project/chat state');
    }
    await request(baseUrl, auth, '/api/chats', {method: 'GET'});
    const matches = [];
    const seenOffsets = new Set();
    let offset = 0;
    let eof = false;
    while (!eof) {
      if (seenOffsets.has(offset)) {
        throw new Error(`Full microSD file pagination repeated offset ${offset}`);
      }
      seenOffsets.add(offset);
      const files = await (
        await request(baseUrl, auth, `/api/files?offset=${offset}`, {method: 'GET'})
      ).json();
      if (!Array.isArray(files.files) || typeof files.eof !== 'boolean' ||
          !Number.isInteger(files.next_offset) || files.next_offset < 0) {
        throw new Error('Full microSD file listing returned invalid typed state');
      }
      matches.push(...files.files.filter((file) => file.name === fixturePath));
      eof = files.eof;
      if (!eof && files.next_offset <= offset) {
        throw new Error('Full microSD file pagination offset did not advance');
      }
      offset = files.next_offset;
    }
    if (matches.length !== 1 || matches[0].size !== 24) {
      throw new Error(`Full microSD listing omitted the exact owned fixture: ${JSON.stringify(matches)}`);
    }
    const file = await (
      await request(
        baseUrl,
        auth,
        `/api/file?name=${encodeURIComponent(fixturePath)}&offset=0`,
        {method: 'GET'},
      )
    ).json();
    if (file.ok !== true || file.offset !== 0 || file.next_offset !== 24 ||
        file.total_bytes !== 24 || file.eof !== true || file.content !== fixtureContent) {
      throw new Error(`Full microSD read changed the exact owned fixture: ${JSON.stringify(file)}`);
    }
    fullReadProof = {
      project_id: projects.active_project_id,
      chat_id: chat.active_chat_id,
      fixture_path: fixturePath,
      fixture_bytes: 24,
      fixture_sha256: sha256(Buffer.from(fixtureContent, 'ascii')),
    };
  } else {
    for (const path of readPaths) {
      await expectRequestFailureDetail(
        baseUrl, auth, path, {method: 'GET'}, contract.status,
      );
    }
    await expectRequestFailureDetail(
      baseUrl,
      auth,
      `/api/file?name=${encodeURIComponent(fixturePath)}&offset=0`,
      {method: 'GET'},
      contract.status,
    );
  }
  const ownedMissingPath = `cardmind_p2_23_${nonce}_${expectedState}.txt`;
  const writeError = await expectRequestFailureDetail(
    baseUrl,
    auth,
    '/api/file/save',
    {
      method: 'POST',
      body: form({
        name: ownedMissingPath,
        offset: '0',
        original_bytes: '0',
        content: 'x',
      }),
    },
    contract.status,
  );
  if (!writeError.includes(status.sd_error)) {
    throw new Error(
      `Write rejection did not preserve the ${expectedState} storage error`,
    );
  }
  const finalStatusResponse = await request(
    baseUrl, auth, '/api/status', {method: 'GET'},
  );
  const finalStatus = await finalStatusResponse.json();
  if (finalStatus.sd_state !== expectedState ||
      finalStatus.sd_error_code !== contract.code) {
    throw new Error(`Web Console stopped reporting ${expectedState} after route failures`);
  }
  return {
    state: expectedState,
    error_code: contract.code,
    read_status: contract.readable ? 200 : contract.status,
    write_status: contract.status,
    status_responsive: true,
    confirmation_called: false,
    full_read_proof: fullReadProof,
  };
}

async function state(baseUrl, auth) {
  const statusResponse = await request(
    baseUrl,
    auth,
    statePaths.status,
    {method: 'GET'},
  );
  const status = await statusResponse.json();
  const sshResponse = await request(
    baseUrl,
    auth,
    statePaths.ssh,
    {method: 'GET'},
  );
  return {...status, ...await sshResponse.json()};
}

async function timedStateView(baseUrl, auth, view) {
  const path = statePaths[view];
  if (path === undefined) {
    throw new Error(`Unknown state view '${view}'`);
  }
  const startedAt = performance.now();
  const response = await request(
    baseUrl,
    auth,
    path,
    {method: 'GET'},
  );
  const body = await response.text();
  const elapsedMs = Math.round(performance.now() - startedAt);
  const measuredBytes = Number(response.headers.get('x-cardmind-response-bytes'));
  if (!Number.isFinite(measuredBytes) || measuredBytes !== Buffer.byteLength(body, 'utf8')) {
    throw new Error(`${path} returned an invalid response byte metric`);
  }
  if (!response.headers.has('server-timing')) {
    throw new Error(`${path} did not return Server-Timing`);
  }
  return {
    view,
    elapsed_ms: elapsedMs,
    response_bytes: Buffer.byteLength(body, 'utf8'),
    document: JSON.parse(body),
  };
}

function memorySnapshot(label, document) {
  return {
    label,
    free_heap: document.free_heap,
    largest_heap: document.largest_heap,
    minimum_heap: document.minimum_heap,
    ssh_open: document.ssh_terminal_open,
  };
}

function recordSnapshot(measurements, label, document) {
  const snapshot = memorySnapshot(label, document);
  measurements.push(snapshot);
  console.log(JSON.stringify({stage: label, ...snapshot}));
}

async function startSsh(baseUrl, auth) {
  await request(baseUrl, auth, '/api/ssh/start', {method: 'POST'});
  const deadline = performance.now() + maximumRequestMs;
  let trustSubmitted = false;
  while (performance.now() < deadline) {
    const response = await request(baseUrl, auth, statePaths.ssh, {method: 'GET'});
    const document = await response.json();
    if (document.ssh_stage === 'connected' && document.ssh_terminal_open === true) {
      return document;
    }
    if (document.ssh_stage === 'awaiting_trust' && !trustSubmitted) {
      await request(baseUrl, auth, '/api/ssh/trust', {
        method: 'POST',
        body: form({
          fingerprint: requireString(document.ssh_fingerprint, 'ssh_fingerprint'),
        }),
      });
      trustSubmitted = true;
    }
    if (document.ssh_stage === 'failed') {
      throw new Error(`SSH background connection failed: ${document.ssh_error}`);
    }
    await new Promise((resolve) => setTimeout(resolve, 100));
  }
  throw new Error('SSH background connection did not reach connected state');
}

async function stopSsh(baseUrl, auth) {
  await request(baseUrl, auth, '/api/ssh/stop', {
    method: 'POST',
    body: form({}),
  });
  const deadline = performance.now() + maximumRequestMs;
  while (performance.now() < deadline) {
    const response = await request(baseUrl, auth, statePaths.ssh, {method: 'GET'});
    const document = await response.json();
    if (document.ssh_stage === 'idle' && document.ssh_terminal_open !== true) return;
    await new Promise((resolve) => setTimeout(resolve, 100));
  }
  throw new Error('SSH background connection did not stop');
}

async function verifyInteractiveSsh(baseUrl, auth) {
  const marker = 'SSH-E2E-OK';
  const editedCommand = `printf SSH-E2E-BADX${'\u007f'.repeat(4)}OK\r`;
  await request(baseUrl, auth, '/api/ssh/input', {
    method: 'POST',
    body: form({data: editedCommand}),
  });
  const deadline = performance.now() + maximumRequestMs;
  let output = '';
  while (performance.now() < deadline) {
    const response = await request(baseUrl, auth, '/api/ssh/output', {method: 'GET'});
    const document = await response.json();
    if (typeof document.output_base64 === 'string') {
      output += Buffer.from(document.output_base64, 'base64').toString('utf8');
      if (output.includes(marker)) {
        return;
      }
    }
    if (document.open !== true) {
      throw new Error('SSH terminal closed before returning the interactive marker');
    }
    await new Promise((resolve) => setTimeout(resolve, 100));
  }
  throw new Error(`SSH terminal did not return marker '${marker}'`);
}

async function uploadWorkspaceProbe(baseUrl, auth, name, marker) {
  const upload = new FormData();
  upload.append('file', new Blob([`${marker}\n`], {type: 'text/plain'}), name);
  const response = await request(
    baseUrl,
    auth,
    `/api/file/upload?name=${encodeURIComponent(name)}`,
    {method: 'POST', body: upload},
  );
  const document = await response.json();
  if (document.ok !== true) {
    throw new Error(`Workspace upload '${name}' did not return ok=true`);
  }
}

async function verifyWorkspaceRoundTrip(baseUrl, auth, name, marker) {
  const filesResponse = await request(baseUrl, auth, statePaths.files, {method: 'GET'});
  const files = await filesResponse.json();
  if (!files.files?.some((file) => file.name === name)) {
    throw new Error(`Workspace index did not include '${name}' after upload`);
  }
  const readResponse = await request(
    baseUrl,
    auth,
    `/api/file?name=${encodeURIComponent(name)}&offset=0`,
    {method: 'GET'},
  );
  const read = await readResponse.json();
  if (read.content !== `${marker}\n` || read.eof !== true) {
    throw new Error(`Workspace round trip returned unexpected content for '${name}'`);
  }
}

async function verifyWorkspaceWindowSave(baseUrl, auth, name, marker) {
  const replacement = `${marker}-EDITED\n`;
  await request(baseUrl, auth, '/api/file/save', {
    method: 'POST',
    body: form({
      name,
      offset: '0',
      original_bytes: String(Buffer.byteLength(`${marker}\n`, 'utf8')),
      content: replacement,
    }),
  });
  const response = await request(
    baseUrl,
    auth,
    `/api/file?name=${encodeURIComponent(name)}&offset=0`,
    {method: 'GET'},
  );
  const document = await response.json();
  if (document.content !== replacement || document.eof !== true) {
    throw new Error(`Workspace window save returned unexpected content for '${name}'`);
  }
}

async function activeChatState(baseUrl, auth) {
  const response = await request(baseUrl, auth, statePaths.chat, {method: 'GET'});
  return response.json();
}

function settingsUpdateForm(settings, globalInstructions) {
  const historyQuotaBytes = settings.project_chat_history_quota_bytes;
  if (!Number.isSafeInteger(historyQuotaBytes) || historyQuotaBytes < 0 ||
      historyQuotaBytes % 1_048_576 !== 0) {
    throw new Error('Settings returned an invalid project chat history quota');
  }
  return form({
    wifi_ssid: settings.wifi_ssid,
    wifi_password: '',
    api_base_url: settings.api_base_url,
    api_key: '',
    model: settings.model,
    global_instructions: globalInstructions,
    project_chat_history_quota_mib: String(historyQuotaBytes / 1_048_576),
    stt_base_url: settings.stt_base_url,
    stt_model: settings.stt_model,
    stt_api_key: '',
    clear_stt_key: '0',
    search_base_url: settings.search_base_url,
    search_api_key: '',
    clear_search_key: '0',
    tts_base_url: settings.tts_base_url,
    tts_model: settings.tts_model,
    tts_voice: settings.tts_voice,
    tts_api_key: '',
    clear_tts_key: '0',
    tts_auto_play: settings.tts_auto_play ? '1' : '0',
    tts_volume: String(settings.tts_volume),
    display_brightness: String(settings.display_brightness),
    screen_sleep_minutes: String(settings.screen_sleep_minutes),
    keyboard_repeat_ms: String(settings.keyboard_repeat_ms),
    power_profile: String(settings.power_profile),
  });
}

async function settingsState(baseUrl, auth) {
  const response = await request(baseUrl, auth, statePaths.settings, {method: 'GET'});
  return response.json();
}

async function saveGlobalInstructions(baseUrl, auth, settings, instructions) {
  await request(baseUrl, auth, '/api/settings', {
    method: 'POST',
    body: settingsUpdateForm(settings, instructions),
  });
}

async function expectFailingProviderSse(baseUrl, auth, promptOptions) {
  const response = await request(baseUrl, auth, '/api/prompt/raw', promptOptions);
  const events = parseSse(await response.text());
  if (!events.some((event) => event.type === 'error') ||
      events.some((event) => event.type === 'done')) {
    throw new Error('Prompt did not reach the configured failing provider path');
  }
}

async function expectFailingRetrySse(baseUrl, auth, maximumOutputTokens) {
  const response = await request(baseUrl, auth, '/api/prompt/retry', {
    method: 'POST',
    body: form({maximum_output_tokens: maximumOutputTokens}),
  });
  const events = parseSse(await response.text());
  if (!events.some((event) => event.type === 'error') ||
      events.some((event) => event.type === 'done')) {
    throw new Error('Retry did not reach the configured failing provider path');
  }
}

async function verifyInstructionPrecedenceWeb(baseUrl, auth) {
  const nonce = String(Date.now());
  const originalChat = await activeChatState(baseUrl, auth);
  const originalProjectId = requireString(originalChat.project_id, 'project_id');
  const originalSettings = await settingsState(baseUrl, auth);
  const originalGlobalInstructions = originalSettings.global_instructions;
  let projectId = '';
  let globalChanged = false;
  try {
    const createdResponse = await request(baseUrl, auth, '/api/project/new', {
      method: 'POST',
      body: form({title: `P2 instruction precedence ${nonce}`}),
    });
    projectId = requireString((await createdResponse.json()).project_id, 'project_id');
    globalChanged = true;
    const globalMarker = `global-scope-${nonce}`;
    await saveGlobalInstructions(
      baseUrl, auth, originalSettings, globalMarker);
    const updatedSettings = await settingsState(baseUrl, auth);
    if (updatedSettings.global_instructions !== globalMarker) {
      throw new Error('Global instruction marker was not persisted exactly');
    }
    await request(baseUrl, auth, '/api/project/settings/raw',
      projectSettingsRequest({
        instructions: `project-scope-${nonce}`,
        model: `cardmind-p2-24-invalid-${nonce}`,
        context_byte_budget: '8192',
        maximum_output_tokens: '128',
        automatic_compaction: '0',
      }));
    await request(baseUrl, auth, '/api/chat/instructions/raw',
      chatInstructionsRequest(`chat-scope-${nonce}`));
    const before = await activeChatState(baseUrl, auth);
    if (before.project_instructions !== `project-scope-${nonce}` ||
        before.instructions !== `chat-scope-${nonce}` ||
        before.project_model !== `cardmind-p2-24-invalid-${nonce}`) {
      throw new Error('Project/chat instruction routes did not persist exact scope markers');
    }
    await expectFailingProviderSse(
      baseUrl,
      auth,
      framedPromptRequest(
        `instruction precedence prompt ${nonce}`,
        `request-scope-${nonce}`,
        '256',
      ),
    );
    const afterFramed = await activeChatState(baseUrl, auth);
    if (afterFramed.total_messages !== before.total_messages + 1) {
      throw new Error('Framed prompt was not appended exactly once');
    }
    await expectFailingRetrySse(baseUrl, auth, '');
    const afterRetry = await activeChatState(baseUrl, auth);
    if (afterRetry.total_messages !== afterFramed.total_messages) {
      throw new Error('Instruction retry duplicated the stored user prompt');
    }

    const oversizedInstructions = Buffer.alloc(2049, 0x78);
    const oversizedPrefix = Buffer.alloc(4);
    oversizedPrefix.writeUInt32BE(oversizedInstructions.length, 0);
    await expectRequestFailure(baseUrl, auth, '/api/prompt/raw', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/vnd.cardmind.prompt-v1',
        'X-CardMind-Output-Tokens': '0',
      },
      body: Buffer.concat([
        oversizedPrefix,
        oversizedInstructions,
        Buffer.from('rejected', 'utf8'),
      ]),
    }, 400);
    const afterRejection = await activeChatState(baseUrl, auth);
    if (afterRejection.total_messages !== afterFramed.total_messages) {
      throw new Error('Rejected request instructions changed chat history');
    }

    await expectFailingProviderSse(
      baseUrl,
      auth,
      promptRequest(`legacy prompt ${nonce}`, '0'),
    );
    const afterLegacy = await activeChatState(baseUrl, auth);
    if (afterLegacy.total_messages !== afterFramed.total_messages + 1) {
      throw new Error('Legacy text prompt compatibility path did not append exactly once');
    }
    await request(baseUrl, auth, '/api/chat/clear', {
      method: 'POST',
      body: form({}),
    });
    const afterClear = await activeChatState(baseUrl, auth);
    if (afterClear.total_messages !== 0 || afterClear.messages.length !== 0) {
      throw new Error('Instruction suite chat cleanup left stored messages present');
    }
    return {
      framed_prompt: 'pass',
      retry_preserved_output: 'pass',
      request_instruction_bytes: Buffer.byteLength(`request-scope-${nonce}`, 'utf8'),
      oversized_rejection: 'pass',
      legacy_prompt: 'pass',
      chat_clear: 'pass',
      provider_result: 'explicit_error',
    };
  } finally {
    const cleanupErrors = [];
    if (globalChanged) {
      try {
        await saveGlobalInstructions(
          baseUrl, auth, originalSettings, originalGlobalInstructions);
        const restoredSettings = await settingsState(baseUrl, auth);
        if (restoredSettings.global_instructions !== originalGlobalInstructions) {
          cleanupErrors.push('global restoration did not persist the original value');
        }
      } catch (error) {
        cleanupErrors.push(`global restoration: ${error.message}`);
      }
    }
    if (projectId) {
      await request(baseUrl, auth, '/api/project/select', {
        method: 'POST',
        body: form({id: projectId}),
      }).then(() => request(baseUrl, auth, '/api/project/delete', {
        method: 'POST',
        body: form({}),
      })).catch((error) => cleanupErrors.push(`project cleanup: ${error.message}`));
      try {
        if ((await listAllProjectIds(baseUrl, auth)).includes(projectId)) {
          cleanupErrors.push('project cleanup left the instruction-suite project present');
        }
      } catch (error) {
        cleanupErrors.push(`project cleanup verification: ${error.message}`);
      }
    }
    await request(baseUrl, auth, '/api/project/select', {
      method: 'POST',
      body: form({id: originalProjectId}),
    }).catch((error) => cleanupErrors.push(`project restoration: ${error.message}`));
    try {
      const restoredChat = await activeChatState(baseUrl, auth);
      if (restoredChat.project_id !== originalProjectId) {
        cleanupErrors.push('project restoration selected a different project');
      }
    } catch (error) {
      cleanupErrors.push(`project restoration verification: ${error.message}`);
    }
    if (cleanupErrors.length > 0) {
      throw new Error(`Instruction suite cleanup failed (${cleanupErrors.join('; ')})`);
    }
  }
}

async function verifyRequestSettingsWeb(baseUrl, auth) {
  const nonce = String(Date.now());
  const original = await activeChatState(baseUrl, auth);
  const originalProjectId = requireString(original.project_id, 'project_id');
  let projectId = '';
  try {
    const createdResponse = await request(baseUrl, auth, '/api/project/new', {
      method: 'POST',
      body: form({title: `P2 request settings ${nonce}`}),
    });
    projectId = requireString((await createdResponse.json()).project_id, 'project_id');
    const expected = {
      model: `cardmind-p2-25-invalid-${nonce}`,
      context: 8192,
      output: 128,
      automaticCompaction: false,
    };
    await request(baseUrl, auth, '/api/project/settings/raw',
      projectSettingsRequest({
        instructions: '',
        model: expected.model,
        context_byte_budget: String(expected.context),
        maximum_output_tokens: String(expected.output),
        automatic_compaction: expected.automaticCompaction ? '1' : '0',
      }));
    const persisted = await activeChatState(baseUrl, auth);
    if (persisted.project_model !== expected.model ||
        persisted.context_byte_budget !== expected.context ||
        persisted.maximum_output_tokens !== expected.output ||
        persisted.automatic_compaction !== expected.automaticCompaction) {
      throw new Error('Project request settings were not persisted exactly');
    }
    await expectFailingProviderSse(
      baseUrl,
      auth,
      framedPromptRequest(`request settings prompt ${nonce}`, '', '512'),
    );
    const after = await activeChatState(baseUrl, auth);
    if (after.total_messages !== persisted.total_messages + 1) {
      throw new Error('Request-output override prompt was not appended exactly once');
    }
    await expectFailingRetrySse(baseUrl, auth, '');
    const afterPreservedRetry = await activeChatState(baseUrl, auth);
    if (afterPreservedRetry.total_messages !== after.total_messages) {
      throw new Error('Blank request-settings retry duplicated the stored user prompt');
    }
    await expectFailingRetrySse(baseUrl, auth, '1024');
    const afterExplicitRetry = await activeChatState(baseUrl, auth);
    if (afterExplicitRetry.total_messages !== after.total_messages) {
      throw new Error('Explicit request-settings retry duplicated the stored user prompt');
    }
    return {
      model: 'pass',
      context_bytes: expected.context,
      project_output_tokens: expected.output,
      request_output_tokens: 512,
      retry_preserved_output: 'pass',
      retry_explicit_override: 1024,
      automatic_compaction: expected.automaticCompaction,
      provider_result: 'explicit_error',
    };
  } finally {
    const cleanupErrors = [];
    if (projectId) {
      await request(baseUrl, auth, '/api/project/select', {
        method: 'POST',
        body: form({id: projectId}),
      }).then(() => request(baseUrl, auth, '/api/project/delete', {
        method: 'POST',
        body: form({}),
      })).catch((error) => cleanupErrors.push(`project cleanup: ${error.message}`));
      try {
        if ((await listAllProjectIds(baseUrl, auth)).includes(projectId)) {
          cleanupErrors.push('project cleanup left the request-settings project present');
        }
      } catch (error) {
        cleanupErrors.push(`project cleanup verification: ${error.message}`);
      }
    }
    await request(baseUrl, auth, '/api/project/select', {
      method: 'POST',
      body: form({id: originalProjectId}),
    }).catch((error) => cleanupErrors.push(`project restoration: ${error.message}`));
    try {
      const restoredChat = await activeChatState(baseUrl, auth);
      if (restoredChat.project_id !== originalProjectId) {
        cleanupErrors.push('project restoration selected a different project');
      }
    } catch (error) {
      cleanupErrors.push(`project restoration verification: ${error.message}`);
    }
    if (cleanupErrors.length > 0) {
      throw new Error(`Request-settings suite cleanup failed (${cleanupErrors.join('; ')})`);
    }
  }
}

async function verifyProjectRoundTrip(baseUrl, auth, workspaceName) {
  const original = await activeChatState(baseUrl, auth);
  const originalProjectId = requireString(original.project_id, 'project_id');
  const baselineProjectIds = (await listAllProjectIds(baseUrl, auth)).sort();
  const baselineWorkspaceNames = (await listAllWorkspaceNames(baseUrl, auth)).sort();
  const suffix = String(Date.now());
  const title = `P2 E2E ${suffix}`;
  const renamedTitle = `P2 renamed ${suffix}`;
  const duplicateTitle = `P2 duplicate ${suffix}`;
  let projectId = '';
  let duplicatedProjectId = '';
  let importedProjectId = '';
  let bundleName = '';
  let linked = false;
  let evidence = null;
  let testError = null;
  const cleanupErrors = [];
  try {
    const createdResponse = await request(baseUrl, auth, '/api/project/new', {
      method: 'POST',
      body: form({title}),
    });
    const created = await createdResponse.json();
    projectId = requireString(created.project_id, 'project_id');

    await request(baseUrl, auth, '/api/project/settings/raw',
      projectSettingsRequest({
        instructions: 'P2 project instruction',
        model: '',
        context_byte_budget: '16384',
        maximum_output_tokens: '512',
        automatic_compaction: '0',
      }));
    const first = await activeChatState(baseUrl, auth);
    const firstChatId = requireString(first.active_chat_id, 'active_chat_id');
    if (first.project_title !== title || first.context_byte_budget !== 16384 ||
        first.automatic_compaction !== false) {
      throw new Error('Project settings were not returned by active chat state');
    }
    await request(baseUrl, auth, '/api/chat/instructions/raw',
      chatInstructionsRequest('P2 first chat instruction'));
    await request(baseUrl, auth, '/api/chat/new', {method: 'POST', body: form({})});
    const second = await activeChatState(baseUrl, auth);
    if (second.active_chat_id === firstChatId || second.instructions !== '') {
      throw new Error('New project chat did not start with independent instructions');
    }
    await request(baseUrl, auth, '/api/chat/select', {
      method: 'POST',
      body: form({id: firstChatId}),
    });
    const selected = await activeChatState(baseUrl, auth);
    if (selected.instructions !== 'P2 first chat instruction') {
      throw new Error('Selecting the first project chat did not restore its instructions');
    }

    await request(baseUrl, auth, '/api/project/link', {
      method: 'POST',
      body: form({path: workspaceName, linked: '1'}),
    });
    linked = true;
    const linksResponse = await request(
      baseUrl,
      auth,
      '/api/project/links?offset=0',
      {method: 'GET'},
    );
    const links = await linksResponse.json();
    if (!links.links?.includes(workspaceName)) {
      throw new Error('Shared workspace link was not persisted for the active project');
    }

    await request(baseUrl, auth, '/api/project/rename', {
      method: 'POST',
      body: form({title: renamedTitle}),
    });
    const renamed = await activeChatState(baseUrl, auth);
    if (renamed.project_title !== renamedTitle) {
      throw new Error('Renamed project title was not returned by active chat state');
    }

    const duplicatedResponse = await request(baseUrl, auth, '/api/project/duplicate', {
      method: 'POST',
      body: form({title: duplicateTitle}),
    });
    const duplicated = await duplicatedResponse.json();
    duplicatedProjectId = requireString(duplicated.project_id, 'project_id');
    if (duplicatedProjectId === projectId) {
      throw new Error('Duplicated project reused the source project id');
    }
    const duplicateState = await activeChatState(baseUrl, auth);
    const duplicateChatsResponse = await request(
      baseUrl,
      auth,
      statePaths.chats,
      {method: 'GET'},
    );
    const duplicateChats = await duplicateChatsResponse.json();
    const duplicateLinksResponse = await request(
      baseUrl,
      auth,
      '/api/project/links?offset=0',
      {method: 'GET'},
    );
    const duplicateLinks = await duplicateLinksResponse.json();
    if (duplicateState.project_title !== duplicateTitle ||
        duplicateState.project_instructions !== 'P2 project instruction' ||
        duplicateState.context_byte_budget !== 16384 ||
        duplicateState.automatic_compaction !== false ||
        duplicateState.active_chat_id !== firstChatId ||
        duplicateState.instructions !== 'P2 first chat instruction' ||
        duplicateChats.chats?.length !== 2 ||
        !duplicateLinks.links?.includes(workspaceName)) {
      throw new Error('Duplicated project did not preserve chats, settings and Shared links');
    }
    await request(baseUrl, auth, '/api/project/archive', {
      method: 'POST',
      body: form({archived: '1'}),
    });
    if ((await activeChatState(baseUrl, auth)).project_archived !== true) {
      throw new Error('Project archive state was not persisted');
    }
    await request(baseUrl, auth, '/api/project/archive', {
      method: 'POST',
      body: form({archived: '0'}),
    });
    if ((await activeChatState(baseUrl, auth)).project_archived !== false) {
      throw new Error('Project restore state was not persisted');
    }
    await request(baseUrl, auth, '/api/project/delete', {method: 'POST', body: form({})});
    duplicatedProjectId = '';
    await request(baseUrl, auth, '/api/project/select', {
      method: 'POST',
      body: form({id: projectId}),
    });

    const exportedResponse = await request(baseUrl, auth, '/api/chat/export-bundle', {
      method: 'POST',
      body: form({}),
    });
    const exported = await exportedResponse.json();
    bundleName = requireString(exported.filename, 'filename');
    const importedResponse = await request(baseUrl, auth, '/api/chat/import', {
      method: 'POST',
      body: form({name: bundleName}),
    });
    const imported = await importedResponse.json();
    importedProjectId = requireString(imported.project_id, 'project_id');
    if (importedProjectId === projectId) {
      throw new Error('Imported project reused the source project id');
    }
    await request(baseUrl, auth, '/api/project/select', {
      method: 'POST', body: form({id: projectId}),
    });
    await request(baseUrl, auth, '/api/project/select', {
      method: 'POST', body: form({id: importedProjectId}),
    });
    const importedState = await activeChatState(baseUrl, auth);
    const importedChats = await (await request(
      baseUrl, auth, '/api/chats?offset=0', {method: 'GET'})).json();
    const importedLinks = await (await request(
      baseUrl, auth, '/api/project/links?offset=0', {method: 'GET'})).json();
    if (importedState.project_title !== renamedTitle ||
        importedState.project_instructions !== 'P2 project instruction' ||
        importedState.context_byte_budget !== 16384 ||
        importedState.maximum_output_tokens !== 512 ||
        importedState.automatic_compaction !== false ||
        importedState.active_chat_id !== firstChatId ||
        importedState.instructions !== 'P2 first chat instruction' ||
        importedState.ssh_tools_enabled === true ||
        importedChats.chats?.length !== 2 ||
        !importedLinks.links?.includes(workspaceName)) {
      throw new Error(
        'Imported project did not preserve its title, settings, chats and Shared links',
      );
    }
    await request(baseUrl, auth, '/api/project/delete', {method: 'POST', body: form({})});
    importedProjectId = '';
    await request(baseUrl, auth, '/api/project/select', {
      method: 'POST',
      body: form({id: projectId}),
    });
    await request(baseUrl, auth, '/api/project/link', {
      method: 'POST',
      body: form({path: workspaceName, linked: '0'}),
    });
    linked = false;
    evidence = {
      project_id: projectId,
      chats: 2,
      rename: 'pass',
      duplicate: 'pass',
      archive_restore: 'pass',
      shared_link: 'pass',
      bundle: 'pass',
    };
  } catch (error) {
    testError = error;
  } finally {
    if (duplicatedProjectId) {
      await request(baseUrl, auth, '/api/project/select', {
        method: 'POST',
        body: form({id: duplicatedProjectId}),
      }).then(() => request(baseUrl, auth, '/api/project/delete', {
        method: 'POST',
        body: form({}),
      })).catch((error) => cleanupErrors.push(
        `duplicated project cleanup: ${error.message}`));
    }
    if (importedProjectId) {
      await request(baseUrl, auth, '/api/project/select', {
        method: 'POST',
        body: form({id: importedProjectId}),
      }).then(() => request(baseUrl, auth, '/api/project/delete', {
        method: 'POST',
        body: form({}),
      })).catch((error) => cleanupErrors.push(
        `imported project cleanup: ${error.message}`));
    }
    if (projectId) {
      await request(baseUrl, auth, '/api/project/select', {
        method: 'POST',
        body: form({id: projectId}),
      }).then(async () => {
        if (linked) {
          await request(baseUrl, auth, '/api/project/link', {
            method: 'POST',
            body: form({path: workspaceName, linked: '0'}),
          });
        }
        await request(baseUrl, auth, '/api/project/delete', {
          method: 'POST',
          body: form({}),
        });
      }).catch((error) => cleanupErrors.push(`source project cleanup: ${error.message}`));
    }
    await request(baseUrl, auth, '/api/project/select', {
      method: 'POST',
      body: form({id: originalProjectId}),
    }).catch((error) => cleanupErrors.push(`original project restore: ${error.message}`));
    if (bundleName) {
      await deleteWorkspaceProbe(baseUrl, auth, bundleName).catch((error) => {
        cleanupErrors.push(`project bundle cleanup: ${error.message}`);
      });
    }
    try {
      const finalProjectIds = (await listAllProjectIds(baseUrl, auth)).sort();
      if (JSON.stringify(finalProjectIds) !== JSON.stringify(baselineProjectIds)) {
        throw new Error('project inventory differs from baseline');
      }
      const finalWorkspaceNames = (await listAllWorkspaceNames(baseUrl, auth)).sort();
      if (JSON.stringify(finalWorkspaceNames) !== JSON.stringify(baselineWorkspaceNames)) {
        throw new Error('workspace inventory differs from baseline');
      }
      if ((await activeChatState(baseUrl, auth)).project_id !== originalProjectId) {
        throw new Error('original active project was not restored');
      }
    } catch (error) {
      cleanupErrors.push(`inventory verification: ${error.message}`);
    }
  }
  if (testError || cleanupErrors.length > 0) {
    throw new Error(
      `Project round trip failed; test=${testError?.message ?? 'none'}; ` +
      `cleanup=${cleanupErrors.length === 0 ? 'none' : cleanupErrors.join('; ')}`,
    );
  }
  return evidence;
}

async function deleteWorkspaceProbe(baseUrl, auth, name) {
  const response = await request(baseUrl, auth, '/api/file/delete', {
    method: 'POST',
    body: form({name}),
  });
  const document = await response.json();
  if (document.ok !== true) {
    throw new Error(`Workspace delete '${name}' did not return ok=true`);
  }
}

async function exerciseStatePolling(baseUrl, auth, iterations) {
  for (let index = 0; index < iterations; index += 1) {
    await Promise.all([
      request(baseUrl, auth, statePaths.status, {method: 'GET'}),
      request(baseUrl, auth, statePaths.ssh, {method: 'GET'}),
    ]);
  }
}

function parseSse(text) {
  const events = [];
  for (const frame of text.replace(/\r\n?/g, '\n').split('\n\n')) {
    if (!frame.startsWith('data:')) continue;
    events.push(JSON.parse(frame.slice(5)));
  }
  return events;
}

async function prompt(baseUrl, auth, marker) {
  const startedAt = performance.now();
  const response = await request(
    baseUrl,
    auth,
    '/api/prompt/raw',
    promptRequest(`Reply with exactly: ${marker}`, '0'),
  );
  const body = await response.text();
  const events = parseSse(body);
  const error = events.find((event) => event.type === 'error');
  if (error !== undefined) {
    throw new Error(`Prompt stream returned an error: ${error.error}`);
  }
  if (!events.some((event) => event.type === 'done')) {
    throw new Error('Prompt stream ended without a done event');
  }
  const responseText = events.map((event) => event.delta ?? '').join('');
  if (!responseText.includes(marker)) {
    throw new Error(`Prompt response did not contain marker '${marker}'`);
  }
  return Math.round(performance.now() - startedAt);
}

async function statusState(baseUrl, auth) {
  const response = await request(baseUrl, auth, statePaths.status, {method: 'GET'});
  return response.json();
}

function validatedHeapSnapshot(label, document) {
  for (const field of ['free_heap', 'largest_heap', 'minimum_heap']) {
    if (!Number.isInteger(document[field]) || document[field] <= 0) {
      throw new Error(`Status field '${field}' is not a positive integer at ${label}`);
    }
  }
  const snapshot = memorySnapshot(label, document);
  console.log(JSON.stringify({stage: label, ...snapshot}));
  return snapshot;
}

function requireBoundedHeapLoss(before, after, label) {
  const freeLoss = before.free_heap - after.free_heap;
  const largestLoss = before.largest_heap - after.largest_heap;
  if (after.free_heap < 85 * 1024 || after.largest_heap < 28 * 1024 ||
      freeLoss > maximumSteadyHeapLossBytes ||
      largestLoss > maximumSteadyHeapLossBytes) {
    throw new Error(
      `${label} exceeded bounded heap loss: ` +
      `free_loss=${freeLoss}, largest_loss=${largestLoss}, ` +
      `free=${after.free_heap}, largest=${after.largest_heap}`,
    );
  }
}

function requireLargeStreamHeapBounds(
  before, after, label, minimumHeapLossAllowanceBytes) {
  requireBoundedHeapLoss(before, after, label);
  const minimumLoss = before.minimum_heap - after.minimum_heap;
  if (after.minimum_heap < p2LargeStreamMinimumHeapFloorBytes ||
      minimumLoss > minimumHeapLossAllowanceBytes) {
    throw new Error(
      `${label} exceeded the transient minimum-heap budget: ` +
      `minimum_before=${before.minimum_heap}, minimum_after=${after.minimum_heap}, ` +
      `minimum_loss=${minimumLoss}, ` +
      `minimum_loss_allowance=${minimumHeapLossAllowanceBytes}`,
    );
  }
}

async function readChatScalePages(baseUrl, auth, projectId, passLabel) {
  const ids = [];
  const offsets = [];
  const pageSizes = [];
  const heap = [];
  let offset = 0;
  let eof = false;
  let passBaseline = null;
  while (!eof) {
    if (offsets.includes(offset)) {
      throw new Error(`Chat pagination repeated offset ${offset}`);
    }
    offsets.push(offset);
    const pageNumber = offsets.length;
    const before = validatedHeapSnapshot(
      `${passLabel}_page_${pageNumber}_before`, await statusState(baseUrl, auth));
    if (passBaseline === null) passBaseline = before;
    const response = await request(
      baseUrl, auth, `/api/chats?offset=${offset}`, {method: 'GET'});
    const document = await response.json();
    const after = validatedHeapSnapshot(
      `${passLabel}_page_${pageNumber}_after`, await statusState(baseUrl, auth));
    requireBoundedHeapLoss(before, after, `${passLabel} page ${pageNumber}`);
    requireBoundedHeapLoss(
      passBaseline, after, `${passLabel} cumulative through page ${pageNumber}`);
    if (document.project_id !== projectId || !Array.isArray(document.chats) ||
        typeof document.eof !== 'boolean' || !Number.isInteger(document.next_offset) ||
        document.next_offset < 0) {
      throw new Error(`Chat page ${pageNumber} returned invalid typed state`);
    }
    for (const chat of document.chats) {
      ids.push(requireString(chat.id, `chat page ${pageNumber} id`));
    }
    pageSizes.push(document.chats.length);
    eof = document.eof;
    if (!eof && document.next_offset <= offset) {
      throw new Error(`Chat page ${pageNumber} offset did not advance`);
    }
    offset = document.next_offset;
    heap.push({before, after});
    if (pageNumber > 4) {
      throw new Error('100-chat pagination exceeded four bounded pages');
    }
  }
  if (pageSizes.join(',') !== '32,32,32,4' || ids.length !== 100 ||
      new Set(ids).size !== 100) {
    throw new Error(
      `100-chat pagination mismatch: pages=${pageSizes.join(',')} ` +
      `total=${ids.length} unique=${new Set(ids).size}`,
    );
  }
  return {ids, offsets, page_sizes: pageSizes, heap};
}

async function listAllProjectIds(baseUrl, auth) {
  const ids = [];
  const seenOffsets = new Set();
  let offset = 0;
  let eof = false;
  while (!eof) {
    if (seenOffsets.has(offset)) throw new Error(`Project pagination repeated offset ${offset}`);
    seenOffsets.add(offset);
    const response = await request(
      baseUrl, auth, `/api/projects?offset=${offset}`, {method: 'GET'});
    const document = await response.json();
    if (!Array.isArray(document.projects) || typeof document.eof !== 'boolean' ||
        !Number.isInteger(document.next_offset)) {
      throw new Error('Project cleanup verification returned invalid typed state');
    }
    for (const project of document.projects) {
      ids.push(requireString(project.id, 'project cleanup id'));
    }
    eof = document.eof;
    if (!eof && document.next_offset <= offset) {
      throw new Error('Project cleanup pagination offset did not advance');
    }
    offset = document.next_offset;
  }
  return ids;
}

async function verifyChatScale(baseUrl, auth) {
  const original = await activeChatState(baseUrl, auth);
  const originalProjectId = requireString(original.project_id, 'project_id');
  let projectId = '';
  let testError = null;
  let evidence = null;
  try {
    const createdResponse = await request(baseUrl, auth, '/api/project/new', {
      method: 'POST',
      body: form({title: `P2 chat scale ${Date.now()}`}),
    });
    const created = await createdResponse.json();
    projectId = requireString(created.project_id, 'project_id');
    const growthSnapshots = [{
      count: 1,
      heap: validatedHeapSnapshot(
        'chat_scale_count_1', await statusState(baseUrl, auth)),
    }];
    for (let count = 2; count <= 100; count += 1) {
      const newChatResponse = await request(
        baseUrl, auth, '/api/chat/new', {method: 'POST', body: form({})});
      const newChat = await newChatResponse.json();
      if (newChat.ok !== true) {
        throw new Error(`Chat creation ${count} did not return ok=true`);
      }
      if (count % 10 === 0 || count === 32 || count === 64) {
        growthSnapshots.push({
          count,
          heap: validatedHeapSnapshot(
            `chat_scale_count_${count}`, await statusState(baseUrl, auth)),
        });
      }
      if (count % 10 === 0) {
        console.log(JSON.stringify({stage: 'chat_scale_progress', chats: count}));
      }
    }
    const growthHeap = new Map(growthSnapshots.map((entry) => [entry.count, entry.heap]));
    for (const count of [50, 60, 64, 70, 80, 90, 100]) {
      requireBoundedHeapLoss(
        growthHeap.get(40), growthHeap.get(count), `chat growth 40 to ${count}`);
    }

    const warmResponse = await request(
      baseUrl, auth, '/api/chats?offset=0', {method: 'GET'});
    const warmPage = await warmResponse.json();
    if (!Array.isArray(warmPage.chats) || warmPage.chats.length !== 32) {
      throw new Error('Warm 100-chat page did not contain 32 bounded summaries');
    }
    const firstPass = await readChatScalePages(baseUrl, auth, projectId, 'chat_scale_first');
    await expectRequestFailure(
      baseUrl, auth, '/api/chats?offset=1', {method: 'GET'}, 500);
    const activeBeforeReload = await activeChatState(baseUrl, auth);
    const activeChatId = requireString(
      activeBeforeReload.active_chat_id, 'active_chat_id before scale reload');
    const beforeReload = validatedHeapSnapshot(
      'chat_scale_reload_before', await statusState(baseUrl, auth));
    await request(baseUrl, auth, '/api/project/select', {
      method: 'POST',
      body: form({id: projectId}),
    });
    const afterReload = validatedHeapSnapshot(
      'chat_scale_reload_after', await statusState(baseUrl, auth));
    requireBoundedHeapLoss(beforeReload, afterReload, '100-chat project reload');
    const activeAfterReload = await activeChatState(baseUrl, auth);
    if (activeAfterReload.project_id !== projectId ||
        activeAfterReload.active_chat_id !== activeChatId) {
      throw new Error('100-chat project reload changed the active chat selection');
    }
    const secondPass = await readChatScalePages(
      baseUrl, auth, projectId, 'chat_scale_second');
    if (firstPass.ids.join(',') !== secondPass.ids.join(',')) {
      throw new Error('100-chat pagination changed after project reload');
    }
    if (firstPass.offsets.join(',') !== secondPass.offsets.join(',')) {
      throw new Error('100-chat pagination offsets changed after project reload');
    }
    evidence = {
      chats: 100,
      page_sizes: firstPass.page_sizes,
      first_offsets: firstPass.offsets,
      second_offsets: secondPass.offsets,
      active_chat_preserved: true,
      invalid_offset_rejected: true,
      growth_heap: growthSnapshots,
      first_page_heap: firstPass.heap,
      reload_heap: {before: beforeReload, after: afterReload},
      second_page_heap: secondPass.heap,
    };
  } catch (error) {
    testError = error;
  }

  const cleanupErrors = [];
  if (projectId) {
    try {
      await request(baseUrl, auth, '/api/project/select', {
        method: 'POST', body: form({id: projectId}),
      });
      await longRequest(baseUrl, auth, '/api/project/delete', {
        method: 'POST', body: form({}),
      });
    } catch (error) {
      cleanupErrors.push(`test project delete: ${error.message}`);
    }
  }
  try {
    await request(baseUrl, auth, '/api/project/select', {
      method: 'POST', body: form({id: originalProjectId}),
    });
    const restored = await activeChatState(baseUrl, auth);
    if (restored.project_id !== originalProjectId) {
      throw new Error(`restored project is ${restored.project_id}`);
    }
  } catch (error) {
    cleanupErrors.push(`original project restore: ${error.message}`);
  }
  try {
    if (projectId && (await listAllProjectIds(baseUrl, auth)).includes(projectId)) {
      throw new Error(`test project ${projectId} still exists`);
    }
  } catch (error) {
    cleanupErrors.push(`test project absence: ${error.message}`);
  }
  if (testError || cleanupErrors.length > 0) {
    const messages = [];
    if (testError) messages.push(`test: ${testError.message}`);
    messages.push(...cleanupErrors);
    throw new Error(`100-chat scale failed; ${messages.join('; ')}`);
  }
  return evidence;
}

async function readWorkspaceScalePages(baseUrl, auth, passLabel) {
  const names = [];
  const offsets = [];
  const pageSizes = [];
  const heap = [];
  const latencyMs = [];
  const seenOffsets = new Set();
  let offset = 0;
  let eof = false;
  let passBaseline = null;
  while (!eof) {
    if (seenOffsets.has(offset)) {
      throw new Error(`Workspace pagination repeated offset ${offset}`);
    }
    seenOffsets.add(offset);
    offsets.push(offset);
    const pageNumber = offsets.length;
    const before = validatedHeapSnapshot(
      `${passLabel}_page_${pageNumber}_before`, await statusState(baseUrl, auth));
    if (passBaseline === null) passBaseline = before;
    const startedAt = performance.now();
    const response = await request(
      baseUrl, auth, `/api/files?offset=${offset}`, {method: 'GET'});
    const document = await response.json();
    latencyMs.push(Math.round(performance.now() - startedAt));
    const after = validatedHeapSnapshot(
      `${passLabel}_page_${pageNumber}_after`, await statusState(baseUrl, auth));
    requireBoundedHeapLoss(before, after, `${passLabel} page ${pageNumber}`);
    requireBoundedHeapLoss(
      passBaseline, after, `${passLabel} cumulative through page ${pageNumber}`);
    if (!Array.isArray(document.files) || typeof document.eof !== 'boolean' ||
        !Number.isInteger(document.next_offset) || document.next_offset < 0) {
      throw new Error(`Workspace page ${pageNumber} returned invalid typed state`);
    }
    if (document.files.length > 64) {
      throw new Error(`Workspace page ${pageNumber} exceeded 64 bounded entries`);
    }
    for (const file of document.files) {
      names.push(requireString(file.name, `workspace page ${pageNumber} name`));
    }
    pageSizes.push(document.files.length);
    eof = document.eof;
    const expectedNextOffset = offset + document.files.length;
    if (document.next_offset !== expectedNextOffset ||
        (!eof && document.next_offset <= offset)) {
      throw new Error(
        `Workspace page ${pageNumber} returned invalid next offset ` +
        `${document.next_offset}; expected ${expectedNextOffset}`,
      );
    }
    offset = document.next_offset;
    heap.push({before, after});
  }
  if (names.length !== new Set(names).size) {
    throw new Error('Workspace pagination returned duplicate names');
  }
  return {names, offsets, page_sizes: pageSizes, latency_ms: latencyMs, heap};
}

function requireSameStringSet(expected, actual, label) {
  const expectedSet = new Set(expected);
  const actualSet = new Set(actual);
  const missing = [...expectedSet].filter((value) => !actualSet.has(value));
  const unexpected = [...actualSet].filter((value) => !expectedSet.has(value));
  if (missing.length > 0 || unexpected.length > 0) {
    throw new Error(
      `${label} set mismatch: missing=${JSON.stringify(missing.slice(0, 5))} ` +
      `unexpected=${JSON.stringify(unexpected.slice(0, 5))}`,
    );
  }
}

async function verifyWorkspaceScale(baseUrl, auth, nonce) {
  if (!/^[0-9]{8,20}$/.test(nonce)) {
    throw new Error('Workspace scale nonce must contain 8 to 20 decimal digits');
  }
  const prefix = `cardmind_p2_17_${nonce}_`;
  const marker = 'P2-WORKSPACE-SCALE';
  const expectedNames = Array.from(
    {length: 500},
    (_, index) => `${prefix}${String(index).padStart(3, '0')}.txt`,
  );
  const firstPass = await readWorkspaceScalePages(
    baseUrl, auth, 'workspace_scale_first');
  const firstTestNames = firstPass.names.filter((name) => name.startsWith(prefix));
  requireSameStringSet(expectedNames, firstTestNames, 'first 500-file pass');

  const duplicateUpload = new FormData();
  duplicateUpload.append(
    'file', new Blob(['SHOULD-NOT-REPLACE\n'], {type: 'text/plain'}), expectedNames[0]);
  await expectRequestFailure(
    baseUrl,
    auth,
    `/api/file/upload?name=${encodeURIComponent(expectedNames[0])}`,
    {method: 'POST', body: duplicateUpload},
    400,
  );
  await verifyWorkspaceRoundTrip(baseUrl, auth, expectedNames[0], marker);
  await expectRequestFailure(
    baseUrl, auth, '/api/files?offset=invalid', {method: 'GET'}, 400);
  const afterFailures = validatedHeapSnapshot(
    'workspace_scale_after_failures', await statusState(baseUrl, auth));
  requireBoundedHeapLoss(
    firstPass.heap[firstPass.heap.length - 1].after,
    afterFailures,
    'workspace failure paths',
  );

  const secondPass = await readWorkspaceScalePages(
    baseUrl, auth, 'workspace_scale_second');
  const secondTestNames = secondPass.names.filter((name) => name.startsWith(prefix));
  requireSameStringSet(expectedNames, secondTestNames, 'second 500-file pass');
  if (firstPass.names.join('\n') !== secondPass.names.join('\n') ||
      firstPass.offsets.join(',') !== secondPass.offsets.join(',')) {
    throw new Error('Workspace pagination changed between identical reload passes');
  }
  return {
    files: expectedNames.length,
    pre_existing_files: firstPass.names.length - expectedNames.length,
    page_sizes: firstPass.page_sizes,
    page_latency_ms: firstPass.latency_ms,
    first_offsets: firstPass.offsets,
    second_offsets: secondPass.offsets,
    invalid_offset_rejected: true,
    duplicate_rejected_and_original_preserved: true,
    first_page_heap: firstPass.heap,
    failure_heap: afterFailures,
    second_page_heap: secondPass.heap,
  };
}

function sha256(value) {
  return createHash('sha256').update(value).digest('hex');
}

function fnv1a32(value) {
  let hash = 0x811c9dc5;
  for (const byte of value) {
    hash ^= byte;
    hash = Math.imul(hash, 0x01000193) >>> 0;
  }
  return hash.toString(16).padStart(8, '0');
}

function requiredCommandArgument(name) {
  const index = process.argv.indexOf(name);
  if (index < 0 || process.argv[index + 1] === undefined) {
    throw new Error(`Missing required command argument '${name}'`);
  }
  return process.argv[index + 1];
}

function parsePositiveSafeInteger(value, label) {
  if (!/^[0-9]+$/.test(value)) {
    throw new Error(`${label} must contain only decimal digits`);
  }
  const parsed = Number(value);
  if (!Number.isSafeInteger(parsed) || parsed <= 0) {
    throw new Error(`${label} must be a positive safe integer`);
  }
  return parsed;
}

function unicodeWorkspacePath(nonce) {
  return `cardmind_p2_19_${nonce}/Проекты-世界/` +
    'глубокая-папка-مرحبا/заметки-データ-🌍.txt';
}

function unicodeProjectTitle(nonce) {
  return `P2 Unicode path ${nonce}`;
}

function sharedIsolationPath(nonce) {
  return `cardmind_p2_20_${nonce}_shared.txt`;
}

function sharedIsolationTitle(nonce, suffix) {
  return `P2 Shared isolation ${nonce} ${suffix}`;
}

function largeStreamPath(nonce) {
  return `cardmind_p2_21_${nonce}/large-320mib.txt`;
}

function atomicFailurePath(nonce) {
  return `cardmind_p2_22_${nonce}/atomic.txt`;
}

function contextHistoryTitle(nonce) {
  return `P2 context history ${nonce}`;
}

function validateContextHistoryLedger(document, expectedNonce) {
  if (document === null || typeof document !== 'object' || Array.isArray(document) ||
      document.version !== 1 || !/^[0-9]{8,20}$/.test(document.nonce) ||
      (expectedNonce !== '' && document.nonce !== expectedNonce) ||
      document.title !== contextHistoryTitle(document.nonce) ||
      typeof document.baseline_ready !== 'boolean' ||
      !Array.isArray(document.baseline_project_ids) ||
      typeof document.original_project_id !== 'string' ||
      typeof document.project_create_pending !== 'boolean' ||
      typeof document.owned_project_id !== 'string' ||
      typeof document.web_cleanup_complete !== 'boolean') {
    throw new Error('P2-27 ledger has an invalid or unsafe typed shape');
  }
  const identifiers = [
    ...document.baseline_project_ids,
    document.original_project_id,
    document.owned_project_id,
  ].filter((value) => value !== '');
  if (identifiers.some((value) =>
    typeof value !== 'string' || value.length > 180 || !/^[A-Za-z0-9._-]+$/.test(value))) {
    throw new Error('P2-27 ledger contains an unsafe project identifier');
  }
  if (new Set(document.baseline_project_ids).size !==
      document.baseline_project_ids.length) {
    throw new Error('P2-27 ledger contains duplicate baseline project identifiers');
  }
  if (!document.baseline_ready &&
      (document.baseline_project_ids.length > 0 || document.original_project_id !== '' ||
       document.project_create_pending || document.owned_project_id !== '')) {
    throw new Error('P2-27 ledger has owned state without a persisted baseline');
  }
  if (document.baseline_ready &&
      !document.baseline_project_ids.includes(document.original_project_id)) {
    throw new Error('P2-27 ledger original project is absent from its baseline');
  }
  if (document.owned_project_id !== '' && !document.project_create_pending) {
    throw new Error('P2-27 ledger contains a project id without a pending create marker');
  }
  if (document.baseline_project_ids.includes(document.owned_project_id)) {
    throw new Error('P2-27 ledger-owned project collides with its baseline');
  }
  if (document.web_cleanup_complete &&
      (document.project_create_pending || document.owned_project_id !== '')) {
    throw new Error('P2-27 ledger cleanup state is inconsistent');
  }
  return {...document, baseline_project_ids: [...document.baseline_project_ids]};
}

async function readContextHistoryLedger(path, expectedNonce) {
  if (!/[\\/]artifacts[\\/]p2-27-context-history-ledger\.json$/.test(path)) {
    throw new Error('P2-27 ledger path is outside the exact artifacts target');
  }
  return validateContextHistoryLedger(
    JSON.parse(await readFile(path, 'utf8')), expectedNonce);
}

async function writeContextHistoryLedger(path, current, fields) {
  const next = validateContextHistoryLedger({...current, ...fields}, current.nonce);
  const temporaryPath = `${path}.node.tmp`;
  const handle = await open(temporaryPath, 'w');
  try {
    await handle.writeFile(`${JSON.stringify(next)}\n`, 'utf8');
    await handle.sync();
  } finally {
    await handle.close();
  }
  await rename(temporaryPath, path);
  return next;
}

function validateAtomicFailureLedger(document, expectedNonce) {
  if (document === null || typeof document !== 'object' || Array.isArray(document) ||
      document.version !== 1 || !/^[0-9]{8,20}$/.test(document.nonce) ||
      document.nonce !== expectedNonce ||
      document.path !== atomicFailurePath(document.nonce) ||
      document.expected_bytes !== 24 || document.expected_fnv32 !== '9e5f863b' ||
      document.setup_pending !== true) {
    throw new Error('P2-22 ledger has an invalid or unsafe typed shape');
  }
  return {...document};
}

async function readAtomicFailureLedger(path, expectedNonce) {
  if (!/[\\/]artifacts[\\/]p2-22-atomic-failure-ledger\.json$/.test(path)) {
    throw new Error('P2-22 ledger path is outside the exact artifacts target');
  }
  return validateAtomicFailureLedger(
    JSON.parse(await readFile(path, 'utf8')), expectedNonce);
}

function validateLargeStreamLedger(document, expectedNonce) {
  if (document === null || typeof document !== 'object' || Array.isArray(document) ||
      document.version !== 1 || !/^[0-9]{8,20}$/.test(document.nonce) ||
      (expectedNonce !== '' && document.nonce !== expectedNonce) ||
      document.path !== largeStreamPath(document.nonce) ||
      document.expected_bytes !== p2LargeStreamBytes ||
      document.expected_fnv32 !== '09529dc5' ||
      document.setup_pending !== true ||
      typeof document.setup_complete !== 'boolean' ||
      typeof document.expected_sha256 !== 'string' ||
      typeof document.web_verification_complete !== 'boolean' ||
      typeof document.device_verification_complete !== 'boolean') {
    throw new Error('P2-21 ledger has an invalid or unsafe typed shape');
  }
  if (document.expected_sha256 !== p2LargeStreamSha256) {
    throw new Error('P2-21 ledger contains an unexpected SHA-256 digest');
  }
  if (document.setup_complete && document.expected_sha256 === '') {
    throw new Error('P2-21 ledger marks setup complete without an expected SHA-256');
  }
  if (document.web_verification_complete && !document.setup_complete) {
    throw new Error('P2-21 ledger marks Web verification complete before setup');
  }
  if (document.device_verification_complete && !document.web_verification_complete) {
    throw new Error('P2-21 ledger marks device verification complete before Web verification');
  }
  return {...document};
}

function expectedLargeStreamBytes(offset, length) {
  const marker = Buffer.from('CARDMIND_P2_21_BLOCK', 'ascii');
  const result = Buffer.alloc(length, 0x78);
  const blockBytes = 4096;
  let position = 0;
  while (position < length) {
    const blockOffset = (offset + position) % blockBytes;
    const bytesInBlock = Math.min(blockBytes - blockOffset, length - position);
    if (blockOffset < marker.length) {
      const markerBytes = Math.min(marker.length - blockOffset, bytesInBlock);
      marker.copy(result, position, blockOffset, blockOffset + markerBytes);
    }
    position += bytesInBlock;
  }
  return result;
}

async function readLargeStreamLedger(path, expectedNonce) {
  if (!/[\\/]artifacts[\\/]p2-21-large-stream-ledger\.json$/.test(path)) {
    throw new Error('P2-21 ledger path is outside the exact artifacts target');
  }
  return validateLargeStreamLedger(
    JSON.parse(await readFile(path, 'utf8')), expectedNonce);
}

async function writeLargeStreamLedger(path, current, fields) {
  const next = validateLargeStreamLedger({...current, ...fields}, current.nonce);
  const temporaryPath = `${path}.node.tmp`;
  const handle = await open(temporaryPath, 'w');
  try {
    await handle.writeFile(`${JSON.stringify(next)}\n`, 'utf8');
    await handle.sync();
  } finally {
    await handle.close();
  }
  await rename(temporaryPath, path);
  return next;
}

function validateSharedIsolationLedger(document, expectedNonce) {
  if (document === null || typeof document !== 'object' || Array.isArray(document) ||
      document.version !== 1 || !/^[0-9]{8,20}$/.test(document.nonce) ||
      (expectedNonce !== '' && document.nonce !== expectedNonce) ||
      document.path !== sharedIsolationPath(document.nonce) ||
      document.title_a !== sharedIsolationTitle(document.nonce, 'A') ||
      document.title_b !== sharedIsolationTitle(document.nonce, 'B') ||
      document.title_c !== sharedIsolationTitle(document.nonce, 'C') ||
      typeof document.baseline_ready !== 'boolean' ||
      !Array.isArray(document.baseline_project_ids) ||
      typeof document.original_project_id !== 'string' ||
      typeof document.project_a_id !== 'string' ||
      typeof document.project_b_id !== 'string' ||
      typeof document.project_c_id !== 'string' ||
      typeof document.project_a_pending !== 'boolean' ||
      typeof document.project_b_pending !== 'boolean' ||
      typeof document.project_c_pending !== 'boolean' ||
      typeof document.file_baseline_checked !== 'boolean' ||
      typeof document.file_preexisting !== 'boolean' ||
      typeof document.file_upload_pending !== 'boolean' ||
      typeof document.web_cleanup_complete !== 'boolean') {
    throw new Error('P2-20 ledger has an invalid or unsafe typed shape');
  }
  const identifiers = [
    ...document.baseline_project_ids,
    document.original_project_id,
    document.project_a_id,
    document.project_b_id,
    document.project_c_id,
  ].filter((value) => value !== '');
  if (identifiers.some((value) =>
    typeof value !== 'string' || value.length > 180 || !/^[A-Za-z0-9._-]+$/.test(value))) {
    throw new Error('P2-20 ledger contains an unsafe project identifier');
  }
  if (new Set(document.baseline_project_ids).size !==
      document.baseline_project_ids.length) {
    throw new Error('P2-20 ledger contains duplicate baseline project identifiers');
  }
  if (!document.baseline_ready &&
      (document.baseline_project_ids.length > 0 || document.original_project_id !== '' ||
       document.project_a_id !== '' || document.project_b_id !== '' ||
       document.project_c_id !== '' || document.project_a_pending ||
       document.project_b_pending || document.project_c_pending ||
       document.file_baseline_checked || document.file_preexisting ||
       document.file_upload_pending || document.web_cleanup_complete)) {
    throw new Error('P2-20 ledger contains owned state without a persisted baseline');
  }
  if (document.file_upload_pending &&
      (!document.file_baseline_checked || document.file_preexisting)) {
    throw new Error('P2-20 ledger has unsafe file ownership state');
  }
  return {...document, baseline_project_ids: [...document.baseline_project_ids]};
}

async function readSharedIsolationLedger(path, expectedNonce) {
  if (!/[\\/]artifacts[\\/]p2-20-shared-isolation-ledger\.json$/.test(path)) {
    throw new Error('P2-20 ledger path is outside the exact artifacts target');
  }
  return validateSharedIsolationLedger(
    JSON.parse(await readFile(path, 'utf8')), expectedNonce);
}

async function writeSharedIsolationLedger(path, current, fields) {
  const next = validateSharedIsolationLedger(
    {...current, ...fields}, current.nonce);
  const temporaryPath = `${path}.node.tmp`;
  const handle = await open(temporaryPath, 'w');
  try {
    await handle.writeFile(`${JSON.stringify(next)}\n`, 'utf8');
    await handle.sync();
  } finally {
    await handle.close();
  }
  await rename(temporaryPath, path);
  return next;
}

function validateUnicodeLedger(document, expectedNonce) {
  if (document === null || typeof document !== 'object' || Array.isArray(document) ||
      document.version !== 2 || !/^[0-9]{8,20}$/.test(document.nonce) ||
      (expectedNonce !== '' && document.nonce !== expectedNonce) ||
      document.path !== unicodeWorkspacePath(document.nonce) ||
      document.web_title !== unicodeProjectTitle(document.nonce) ||
      typeof document.baseline_ready !== 'boolean' ||
      !Array.isArray(document.baseline_project_ids) ||
      typeof document.original_project_id !== 'string' ||
      typeof document.source_project_id !== 'string' ||
      typeof document.imported_project_id !== 'string' ||
      typeof document.bundle_name !== 'string' ||
      typeof document.bundle_baseline_checked !== 'boolean' ||
      typeof document.bundle_preexisting !== 'boolean' ||
      typeof document.bundle_export_pending !== 'boolean' ||
      typeof document.import_pending !== 'boolean' ||
      typeof document.web_cleanup_complete !== 'boolean') {
    throw new Error('P2-19 ledger has an invalid or unsafe typed shape');
  }
  const identifiers = [
    ...document.baseline_project_ids,
    document.original_project_id,
    document.source_project_id,
    document.imported_project_id,
  ].filter((value) => value !== '');
  if (identifiers.some((value) =>
    typeof value !== 'string' || value.length > 180 || !/^[A-Za-z0-9._-]+$/.test(value))) {
    throw new Error('P2-19 ledger contains an unsafe project identifier');
  }
  if (new Set(document.baseline_project_ids).size !==
      document.baseline_project_ids.length) {
    throw new Error('P2-19 ledger contains duplicate baseline project identifiers');
  }
  if (!document.baseline_ready &&
      (document.baseline_project_ids.length > 0 || document.original_project_id !== '' ||
       document.source_project_id !== '' || document.imported_project_id !== '' ||
       document.bundle_name !== '' || document.bundle_baseline_checked ||
       document.bundle_preexisting || document.bundle_export_pending ||
       document.import_pending)) {
    throw new Error('P2-19 ledger contains owned state without a persisted baseline');
  }
  if (document.imported_project_id !== '' && document.source_project_id === '') {
    throw new Error('P2-19 ledger contains an imported project without a source project');
  }
  if (document.bundle_name !== '') {
    const expectedBundle = document.source_project_id === ''
      ? ''
      : `project_${document.source_project_id}.cardmind-project.jsonl`;
    if (document.bundle_name !== expectedBundle) {
      throw new Error('P2-19 ledger bundle name does not match its source project');
    }
  }
  if ((document.bundle_baseline_checked || document.bundle_export_pending) &&
      document.bundle_name === '') {
    throw new Error('P2-19 ledger has bundle state without an exact bundle name');
  }
  if (document.import_pending && !document.bundle_export_pending) {
    throw new Error('P2-19 ledger has import state without an owned bundle export');
  }
  return {...document, baseline_project_ids: [...document.baseline_project_ids]};
}

async function readUnicodeLedger(path, expectedNonce) {
  if (!/[\\/]artifacts[\\/]p2-19-unicode-path-ledger\.json$/.test(path)) {
    throw new Error('P2-19 ledger path is outside the exact artifacts target');
  }
  const document = JSON.parse(await readFile(path, 'utf8'));
  return validateUnicodeLedger(document, expectedNonce);
}

async function writeUnicodeLedger(path, current, fields) {
  const next = validateUnicodeLedger({...current, ...fields}, current.nonce);
  const temporaryPath = `${path}.node.tmp`;
  const handle = await open(temporaryPath, 'w');
  try {
    await handle.writeFile(`${JSON.stringify(next)}\n`, 'utf8');
    await handle.sync();
  } finally {
    await handle.close();
  }
  await rename(temporaryPath, path);
  return next;
}

function createLargeTextFixture(totalBytes, markerOffset, marker) {
  const pattern = Buffer.from(
    'CARDMIND-LARGE-FILE-0123456789-abcdefghijklmnopqrstuvwxyz\n', 'ascii');
  const fixture = Buffer.alloc(totalBytes, pattern);
  Buffer.from(marker, 'ascii').copy(fixture, markerOffset);
  fixture[fixture.length - 1] = 0x0a;
  return fixture;
}

async function downloadWorkspaceFile(baseUrl, auth, name) {
  const response = await longRequest(
    baseUrl,
    auth,
    `/api/file/download?name=${encodeURIComponent(name)}`,
    {method: 'GET'},
  );
  return Buffer.from(await response.arrayBuffer());
}

async function listAllWorkspaceNames(baseUrl, auth) {
  const names = [];
  const seenOffsets = new Set();
  let offset = 0;
  let eof = false;
  while (!eof) {
    if (seenOffsets.has(offset)) {
      throw new Error(`Workspace cleanup pagination repeated offset ${offset}`);
    }
    seenOffsets.add(offset);
    const response = await request(
      baseUrl, auth, `/api/files?offset=${offset}`, {method: 'GET'});
    const document = await response.json();
    if (!Array.isArray(document.files) || typeof document.eof !== 'boolean' ||
        !Number.isInteger(document.next_offset) || document.next_offset < 0) {
      throw new Error('Workspace cleanup verification returned invalid typed state');
    }
    for (const file of document.files) {
      names.push(requireString(file.name, 'workspace cleanup file name'));
    }
    eof = document.eof;
    if (!eof && document.next_offset <= offset) {
      throw new Error('Workspace cleanup pagination offset did not advance');
    }
    offset = document.next_offset;
  }
  return names;
}

async function readUnicodeWorkspacePass(baseUrl, auth, path, expectedBytes, label) {
  const entries = [];
  const offsets = [];
  const pageSizes = [];
  const seenOffsets = new Set();
  let offset = 0;
  let eof = false;
  const before = validatedHeapSnapshot(
    `${label}_before`, await statusState(baseUrl, auth));
  while (!eof) {
    if (seenOffsets.has(offset)) {
      throw new Error(`${label} repeated workspace offset ${offset}`);
    }
    seenOffsets.add(offset);
    offsets.push(offset);
    const response = await request(
      baseUrl, auth, `/api/files?offset=${offset}`, {method: 'GET'});
    const document = await response.json();
    if (!Array.isArray(document.files) || typeof document.eof !== 'boolean' ||
        !Number.isInteger(document.next_offset) || document.next_offset < 0) {
      throw new Error(`${label} returned invalid typed workspace pagination state`);
    }
    for (const file of document.files) {
      const name = requireString(file.name, `${label} workspace file name`);
      if (!Number.isSafeInteger(file.size) || file.size < 0) {
        throw new Error(`${label} returned invalid size for '${name}'`);
      }
      entries.push({name, size: file.size});
    }
    pageSizes.push(document.files.length);
    eof = document.eof;
    if (!eof && document.next_offset <= offset) {
      throw new Error(`${label} workspace pagination offset did not advance`);
    }
    offset = document.next_offset;
  }
  const matches = entries.filter((entry) => entry.name === path);
  if (matches.length !== 1) {
    throw new Error(`${label} found exact Unicode path ${matches.length} times`);
  }
  if (matches[0].size !== expectedBytes) {
    throw new Error(
      `${label} reported ${matches[0].size} bytes for the Unicode path; ` +
      `serial setup reported ${expectedBytes}`,
    );
  }
  const after = validatedHeapSnapshot(
    `${label}_after`, await statusState(baseUrl, auth));
  requireBoundedHeapLoss(before, after, label);
  return {entries, offsets, page_sizes: pageSizes, heap: {before, after}};
}

async function listAllProjects(baseUrl, auth) {
  const projects = [];
  const seenOffsets = new Set();
  let offset = 0;
  let eof = false;
  while (!eof) {
    if (seenOffsets.has(offset)) {
      throw new Error(`Project pagination repeated offset ${offset}`);
    }
    seenOffsets.add(offset);
    const response = await request(
      baseUrl, auth, `/api/projects?offset=${offset}`, {method: 'GET'});
    const document = await response.json();
    if (!Array.isArray(document.projects) || typeof document.eof !== 'boolean' ||
        !Number.isInteger(document.next_offset) || document.next_offset < 0) {
      throw new Error('Project discovery returned invalid typed pagination state');
    }
    for (const project of document.projects) {
      projects.push({
        id: requireString(project.id, 'project discovery id'),
        title: requireString(project.title, 'project discovery title'),
      });
    }
    eof = document.eof;
    if (!eof && document.next_offset <= offset) {
      throw new Error('Project discovery pagination offset did not advance');
    }
    offset = document.next_offset;
  }
  return projects;
}

async function listAllProjectLinks(baseUrl, auth) {
  const links = [];
  const seenOffsets = new Set();
  let offset = 0;
  let eof = false;
  while (!eof) {
    if (seenOffsets.has(offset)) {
      throw new Error(`Shared-link pagination repeated offset ${offset}`);
    }
    seenOffsets.add(offset);
    const response = await request(
      baseUrl, auth, `/api/project/links?offset=${offset}`, {method: 'GET'});
    const document = await response.json();
    if (!Array.isArray(document.links) || typeof document.eof !== 'boolean' ||
        !Number.isInteger(document.next_offset) || document.next_offset < 0) {
      throw new Error('Shared-link discovery returned invalid typed pagination state');
    }
    for (const link of document.links) {
      links.push(requireString(link, 'shared link'));
    }
    eof = document.eof;
    if (!eof && document.next_offset <= offset) {
      throw new Error('Shared-link pagination offset did not advance');
    }
    offset = document.next_offset;
  }
  return links;
}

function verifyExactProjectLink(links, path, label) {
  const matches = links.filter((link) => link === path);
  if (matches.length !== 1 || links.length !== 1) {
    throw new Error(
      `${label} must contain exactly one shared link equal to the Unicode path; ` +
      `links=${JSON.stringify(links)}`,
    );
  }
}

function parseProjectBundle(bundle, path) {
  const decoded = new TextDecoder('utf-8', {fatal: true}).decode(bundle);
  const lines = decoded.split(/\r?\n/).filter((line) => line.length > 0);
  if (lines.length < 2) {
    throw new Error('Exported project bundle did not contain header and link records');
  }
  const records = lines.map((line, index) => {
    try {
      return JSON.parse(line);
    } catch (error) {
      throw new Error(`Exported project bundle JSONL line ${index + 1} is invalid: ${error.message}`);
    }
  });
  const header = records[0];
  if (header.record !== 'project' || header.format !== 1 ||
      header.shared_link_count !== 1) {
    throw new Error('Exported project bundle header did not declare one shared link');
  }
  const links = records.filter((record) => record.record === 'shared_link');
  if (links.length !== 1 || links[0].path !== path) {
    throw new Error('Exported project bundle did not preserve the exact Unicode link path');
  }
  return {records: records.length, shared_links: links.length};
}

async function deleteProjectById(baseUrl, auth, projectId) {
  await request(baseUrl, auth, '/api/project/select', {
    method: 'POST', body: form({id: projectId}),
  });
  await longRequest(baseUrl, auth, '/api/project/delete', {
    method: 'POST', body: form({}),
  });
}

async function cleanupUnicodeWebOwnership(baseUrl, auth, ledgerPath, inputLedger) {
  let ledger = validateUnicodeLedger(inputLedger, inputLedger.nonce);
  const cleanupErrors = [];
  let currentProjects = [];
  try {
    currentProjects = await listAllProjects(baseUrl, auth);
  } catch (error) {
    cleanupErrors.push(`project discovery: ${error.message}`);
  }
  const baselineIds = new Set(ledger.baseline_project_ids);
  const titleMatches = currentProjects.filter(
    (project) => project.title === ledger.web_title);
  const baselineCollisions = titleMatches.filter((project) => baselineIds.has(project.id));
  if (baselineCollisions.length > 0) {
    cleanupErrors.push(
      `pre-existing project title collision: ${baselineCollisions.map((item) => item.id).join(',')}`,
    );
  }
  const ownedByTitle = titleMatches.filter((project) => !baselineIds.has(project.id));
  if (!ledger.baseline_ready && ownedByTitle.length > 0) {
    cleanupErrors.push('project ownership is ambiguous because the baseline was not persisted');
  }
  if (ownedByTitle.length > 2) {
    cleanupErrors.push(`more than two nonce-owned projects exist: ${ownedByTitle.length}`);
  }
  const projectsById = new Map(currentProjects.map((project) => [project.id, project]));
  const knownOwnedIds = [ledger.source_project_id, ledger.imported_project_id]
    .filter((projectId) => projectId !== '');
  for (const projectId of knownOwnedIds) {
    if (baselineIds.has(projectId)) {
      cleanupErrors.push(`ledger-owned project id '${projectId}' collides with the baseline`);
      continue;
    }
    const project = projectsById.get(projectId);
    if (project !== undefined && project.title !== ledger.web_title) {
      cleanupErrors.push(
        `ledger-owned project id '${projectId}' has unexpected title '${project.title}'`,
      );
    }
  }
  const ownedProjectIds = new Set([
    ...ownedByTitle.map((project) => project.id),
    ...knownOwnedIds.filter((projectId) => !baselineIds.has(projectId)),
  ]);
  const ownershipValidated = cleanupErrors.length === 0;
  if (ownershipValidated) {
    for (const projectId of ownedProjectIds) {
      try {
        if ((await listAllProjectIds(baseUrl, auth)).includes(projectId)) {
          await deleteProjectById(baseUrl, auth, projectId);
        }
      } catch (error) {
        cleanupErrors.push(`project '${projectId}' delete: ${error.message}`);
      }
    }
  }

  if (ownershipValidated) {
    try {
      const workspaceNames = await listAllWorkspaceNames(baseUrl, auth);
      if (ledger.bundle_name !== '' && workspaceNames.includes(ledger.bundle_name)) {
        if (!ledger.bundle_baseline_checked) {
          throw new Error(
            `bundle '${ledger.bundle_name}' exists without a persisted ownership baseline`,
          );
        }
        if (ledger.bundle_preexisting) {
          throw new Error(`bundle '${ledger.bundle_name}' collided with a pre-existing file`);
        }
        if (!ledger.bundle_export_pending) {
          throw new Error(`bundle '${ledger.bundle_name}' exists without a pending export marker`);
        }
        await deleteWorkspaceProbe(baseUrl, auth, ledger.bundle_name);
      }
      if (ledger.bundle_name !== '' && !ledger.bundle_preexisting &&
          (await listAllWorkspaceNames(baseUrl, auth)).includes(ledger.bundle_name)) {
        throw new Error(`bundle '${ledger.bundle_name}' still exists`);
      }
    } catch (error) {
      cleanupErrors.push(`bundle cleanup: ${error.message}`);
    }
  }

  try {
    const remainingProjects = await listAllProjects(baseUrl, auth);
    const remainingOwned = remainingProjects.filter(
      (project) => project.title === ledger.web_title && !baselineIds.has(project.id));
    const remainingKnown = remainingProjects.filter(
      (project) => knownOwnedIds.includes(project.id));
    if (remainingOwned.length > 0 || remainingKnown.length > 0) {
      throw new Error(
        `owned project ids remain: ${[...remainingOwned, ...remainingKnown]
          .map((project) => project.id).join(',')}`,
      );
    }
    const remainingIds = new Set(remainingProjects.map((project) => project.id));
    if (ledger.original_project_id !== '' &&
        remainingIds.has(ledger.original_project_id)) {
      await request(baseUrl, auth, '/api/project/select', {
        method: 'POST', body: form({id: ledger.original_project_id}),
      });
    }
    const active = await activeChatState(baseUrl, auth);
    const activeProjectId = requireString(active.project_id, 'post-cleanup active project_id');
    if (ownedProjectIds.has(activeProjectId) || !remainingIds.has(activeProjectId)) {
      throw new Error(`active project '${activeProjectId}' is not a retained non-owned project`);
    }
  } catch (error) {
    cleanupErrors.push(`project restore/absence: ${error.message}`);
  }
  if (cleanupErrors.length > 0) {
    throw new Error(`P2-19 Web ownership cleanup failed; ${cleanupErrors.join('; ')}`);
  }
  ledger = await writeUnicodeLedger(
    ledgerPath, ledger, {web_cleanup_complete: true});
  return {
    ledger,
    deleted_projects: ownedProjectIds.size,
    bundle_removed: ledger.bundle_name !== '' && ledger.bundle_export_pending,
  };
}

async function verifyUnicodePathRoundTrip(
  baseUrl, auth, nonce, expectedBytes, expectedFnv32, ledgerPath) {
  let ledger = await readUnicodeLedger(ledgerPath, nonce);
  if (ledger.web_cleanup_complete || ledger.baseline_ready) {
    throw new Error('P2-19 normal suite requires a fresh, unclaimed ledger');
  }
  const path = ledger.path;
  if (path.split('/').length !== 4) {
    throw new Error(`Unicode workspace fixture depth changed for '${path}'`);
  }
  const original = await activeChatState(baseUrl, auth);
  const originalProjectId = requireString(original.project_id, 'original project_id');
  const title = ledger.web_title;
  const baselineProjects = await listAllProjects(baseUrl, auth);
  const baselineProjectIds = new Set(baselineProjects.map((project) => project.id));
  ledger = await writeUnicodeLedger(ledgerPath, ledger, {
    baseline_ready: true,
    baseline_project_ids: [...baselineProjectIds],
    original_project_id: originalProjectId,
  });
  if (baselineProjects.some((project) => project.title === title)) {
    throw new Error(`Unicode-path test project title already exists: '${title}'`);
  }
  const initialStatus = validatedHeapSnapshot(
    'unicode_path_initial', await statusState(baseUrl, auth));
  let sourceProjectId = '';
  let importedProjectId = '';
  let bundleName = '';
  let initialDownload = null;
  let evidence = null;
  let testError = null;
  try {
    const firstPass = await readUnicodeWorkspacePass(
      baseUrl, auth, path, expectedBytes, 'unicode_path_first_list');
    const secondPass = await readUnicodeWorkspacePass(
      baseUrl, auth, path, expectedBytes, 'unicode_path_second_list');
    const firstListing = firstPass.entries.map(
      (entry) => `${entry.name}\0${entry.size}`).join('\n');
    const secondListing = secondPass.entries.map(
      (entry) => `${entry.name}\0${entry.size}`).join('\n');
    if (firstListing !== secondListing ||
        firstPass.offsets.join(',') !== secondPass.offsets.join(',')) {
      throw new Error('Unicode workspace listing changed between full paginated passes');
    }

    const readResponse = await request(
      baseUrl,
      auth,
      `/api/file?name=${encodeURIComponent(path)}&offset=0`,
      {method: 'GET'},
    );
    const read = await readResponse.json();
    if (read.ok !== true || read.offset !== 0 || read.total_bytes !== expectedBytes ||
        typeof read.content !== 'string' || read.content.length === 0 ||
        typeof read.eof !== 'boolean') {
      throw new Error('Bounded Unicode file read returned invalid typed state');
    }

    initialDownload = await downloadWorkspaceFile(baseUrl, auth, path);
    const initialFnv32 = fnv1a32(initialDownload);
    const initialSha256 = sha256(initialDownload);
    if (initialDownload.length !== expectedBytes || initialFnv32 !== expectedFnv32) {
      throw new Error(
        `Unicode download differs from serial setup: bytes=${initialDownload.length}/` +
        `${expectedBytes} fnv32=${initialFnv32}/${expectedFnv32}`,
      );
    }
    const boundedBytes = Buffer.from(read.content, 'utf8');
    if (boundedBytes.length > initialDownload.length ||
        !boundedBytes.equals(initialDownload.subarray(0, boundedBytes.length))) {
      throw new Error('Bounded Unicode file view differs from the full download prefix');
    }

    const createdResponse = await request(baseUrl, auth, '/api/project/new', {
      method: 'POST', body: form({title}),
    });
    const created = await createdResponse.json();
    sourceProjectId = requireString(created.project_id, 'source project_id');
    if (baselineProjectIds.has(sourceProjectId)) {
      throw new Error(`Unicode source project reused existing id '${sourceProjectId}'`);
    }
    bundleName = `project_${sourceProjectId}.cardmind-project.jsonl`;
    ledger = await writeUnicodeLedger(ledgerPath, ledger, {
      source_project_id: sourceProjectId,
      bundle_name: bundleName,
    });
    const bundlePreexisting = (await listAllWorkspaceNames(baseUrl, auth))
      .includes(bundleName);
    ledger = await writeUnicodeLedger(ledgerPath, ledger, {
      bundle_baseline_checked: true,
      bundle_preexisting: bundlePreexisting,
    });
    if (bundlePreexisting) {
      throw new Error(`Unicode bundle cleanup target already exists: '${bundleName}'`);
    }

    await request(baseUrl, auth, '/api/project/link', {
      method: 'POST', body: form({path, linked: '1'}),
    });
    verifyExactProjectLink(
      await listAllProjectLinks(baseUrl, auth), path, 'source project');

    ledger = await writeUnicodeLedger(
      ledgerPath, ledger, {bundle_export_pending: true});
    const exportedResponse = await longRequest(
      baseUrl, auth, '/api/chat/export-bundle', {method: 'POST', body: form({})});
    const exported = await exportedResponse.json();
    if (exported.ok !== true ||
        requireString(exported.filename, 'exported filename') !== bundleName) {
      throw new Error('Project export returned an unexpected Unicode bundle filename');
    }
    const bundle = await downloadWorkspaceFile(baseUrl, auth, bundleName);
    const bundleEvidence = parseProjectBundle(bundle, path);

    ledger = await writeUnicodeLedger(ledgerPath, ledger, {import_pending: true});
    const importedResponse = await longRequest(baseUrl, auth, '/api/chat/import', {
      method: 'POST', body: form({name: bundleName}),
    });
    const imported = await importedResponse.json();
    importedProjectId = requireString(imported.project_id, 'imported project_id');
    if (importedProjectId === sourceProjectId || baselineProjectIds.has(importedProjectId)) {
      throw new Error(`Imported Unicode project returned unsafe id '${importedProjectId}'`);
    }
    ledger = await writeUnicodeLedger(
      ledgerPath, ledger, {imported_project_id: importedProjectId});
    verifyExactProjectLink(
      await listAllProjectLinks(baseUrl, auth), path, 'imported project');

    const finalDownload = await downloadWorkspaceFile(baseUrl, auth, path);
    const finalFnv32 = fnv1a32(finalDownload);
    const finalSha256 = sha256(finalDownload);
    if (!finalDownload.equals(initialDownload) || finalFnv32 !== initialFnv32 ||
        finalSha256 !== initialSha256) {
      throw new Error('Unicode workspace bytes changed across project export/import');
    }
    evidence = {
      path,
      path_bytes: Buffer.byteLength(path, 'utf8'),
      depth: 4,
      bytes: initialDownload.length,
      fnv32: initialFnv32,
      sha256: initialSha256,
      list_passes: [
        {offsets: firstPass.offsets, page_sizes: firstPass.page_sizes, heap: firstPass.heap},
        {offsets: secondPass.offsets, page_sizes: secondPass.page_sizes, heap: secondPass.heap},
      ],
      bounded_read_bytes: boundedBytes.length,
      bundle_bytes: bundle.length,
      bundle_sha256: sha256(bundle),
      bundle_records: bundleEvidence.records,
      shared_link_records: bundleEvidence.shared_links,
      source_project_id: sourceProjectId,
      imported_project_id: importedProjectId,
      post_import_download_equal: true,
      initial_status: initialStatus,
    };
  } catch (error) {
    testError = error;
  }

  const cleanupErrors = [];
  let webCleanupEvidence = null;
  try {
    ledger = await readUnicodeLedger(ledgerPath, nonce);
    webCleanupEvidence = await cleanupUnicodeWebOwnership(
      baseUrl, auth, ledgerPath, ledger);
    ledger = webCleanupEvidence.ledger;
  } catch (error) {
    cleanupErrors.push(`Web ownership cleanup: ${error.message}`);
  }
  try {
    const finalPass = await readUnicodeWorkspacePass(
      baseUrl, auth, path, expectedBytes, 'unicode_path_cleanup_list');
    if (initialDownload !== null) {
      const finalDownload = await downloadWorkspaceFile(baseUrl, auth, path);
      if (!finalDownload.equals(initialDownload) ||
          fnv1a32(finalDownload) !== expectedFnv32) {
        throw new Error('Unicode fixture changed during Web cleanup');
      }
    }
    const finalStatus = validatedHeapSnapshot(
      'unicode_path_final', await statusState(baseUrl, auth));
    if (evidence !== null) {
      evidence.cleanup_list = {
        offsets: finalPass.offsets,
        page_sizes: finalPass.page_sizes,
        heap: finalPass.heap,
      };
      evidence.final_status = finalStatus;
      if (webCleanupEvidence !== null) {
        evidence.cleanup = 'pass';
        evidence.web_cleanup = {
          deleted_projects: webCleanupEvidence.deleted_projects,
          bundle_removed: webCleanupEvidence.bundle_removed,
        };
      }
    }
  } catch (error) {
    cleanupErrors.push(`Unicode fixture final verification: ${error.message}`);
  }
  if (testError || cleanupErrors.length > 0) {
    const messages = [];
    if (testError) messages.push(`test: ${testError.message}`);
    messages.push(...cleanupErrors);
    throw new Error(`Unicode-path round trip failed; ${messages.join('; ')}`);
  }
  return evidence;
}

async function recoverUnicodePathOwnership(baseUrl, auth, ledgerPath) {
  const ledger = await readUnicodeLedger(ledgerPath, '');
  const recovered = await cleanupUnicodeWebOwnership(
    baseUrl, auth, ledgerPath, ledger);
  return {
    nonce: ledger.nonce,
    deleted_projects: recovered.deleted_projects,
    bundle_removed: recovered.bundle_removed,
    web_cleanup_complete: recovered.ledger.web_cleanup_complete,
    final_status: validatedHeapSnapshot(
      'unicode_path_recovery_final', await statusState(baseUrl, auth)),
  };
}

async function verifySharedFileBytes(baseUrl, auth, path, expected, label) {
  const links = await listAllProjectLinks(baseUrl, auth);
  const matches = links.filter((link) => link === path);
  if (matches.length !== 1 || links.length !== 1) {
    throw new Error(
      `${label} did not expose exactly one expected Shared link; ` +
      `links=${JSON.stringify(links)}`,
    );
  }
  const downloaded = await downloadWorkspaceFile(baseUrl, auth, path);
  if (!downloaded.equals(expected)) {
    throw new Error(`${label} downloaded bytes differ from the owned Shared file`);
  }
  return {bytes: downloaded.length, sha256: sha256(downloaded)};
}

async function cleanupSharedIsolationOwnership(
  baseUrl, auth, ledgerPath, inputLedger) {
  let ledger = validateSharedIsolationLedger(inputLedger, inputLedger.nonce);
  const cleanupErrors = [];
  let projects = [];
  try {
    projects = await listAllProjects(baseUrl, auth);
  } catch (error) {
    cleanupErrors.push(`project discovery: ${error.message}`);
  }
  const baselineIds = new Set(ledger.baseline_project_ids);
  let deletedProjectCount = 0;
  let fileRemoved = false;
  const specifications = [
    {title: ledger.title_a, id: ledger.project_a_id, pending: ledger.project_a_pending},
    {title: ledger.title_b, id: ledger.project_b_id, pending: ledger.project_b_pending},
    {title: ledger.title_c, id: ledger.project_c_id, pending: ledger.project_c_pending},
  ];
  const projectsById = new Map(projects.map((project) => [project.id, project]));
  const ownedIds = new Set();
  for (const specification of specifications) {
    const titleMatches = projects.filter(
      (project) => project.title === specification.title);
    if (titleMatches.some((project) => baselineIds.has(project.id))) {
      cleanupErrors.push(`pre-existing title collision for '${specification.title}'`);
    }
    const nonBaselineMatches = titleMatches.filter(
      (project) => !baselineIds.has(project.id));
    if (nonBaselineMatches.length > 1) {
      cleanupErrors.push(
        `ambiguous duplicate nonce projects for '${specification.title}'`,
      );
    }
    if (nonBaselineMatches.length > 0 &&
        (!ledger.baseline_ready || !specification.pending)) {
      cleanupErrors.push(
        `project '${specification.title}' exists without persisted ownership`,
      );
    }
    for (const project of nonBaselineMatches) ownedIds.add(project.id);
    if (specification.id !== '') {
      if (baselineIds.has(specification.id)) {
        cleanupErrors.push(
          `ledger-owned project id '${specification.id}' collides with the baseline`,
        );
      }
      const known = projectsById.get(specification.id);
      if (known !== undefined && known.title !== specification.title) {
        cleanupErrors.push(
          `ledger-owned project id '${specification.id}' has title '${known.title}'`,
        );
      }
      if (!baselineIds.has(specification.id)) ownedIds.add(specification.id);
    }
  }
  const ownershipValidated = cleanupErrors.length === 0;
  if (ownershipValidated) {
    for (const projectId of ownedIds) {
      try {
        if ((await listAllProjects(baseUrl, auth))
          .some((project) => project.id === projectId)) {
          await deleteProjectById(baseUrl, auth, projectId);
          deletedProjectCount += 1;
        }
      } catch (error) {
        cleanupErrors.push(`project '${projectId}' delete: ${error.message}`);
      }
    }
  }

  if (ownershipValidated) {
    try {
      const names = await listAllWorkspaceNames(baseUrl, auth);
      const pathMatches = names.filter((name) => name === ledger.path);
      if (pathMatches.length > 1) {
        throw new Error(`owned Shared path appears ${pathMatches.length} times`);
      }
      if (pathMatches.length === 1) {
        if (!ledger.baseline_ready || !ledger.file_baseline_checked ||
            ledger.file_preexisting || !ledger.file_upload_pending) {
          throw new Error('Shared file exists without exact persisted ownership');
        }
        await deleteWorkspaceProbe(baseUrl, auth, ledger.path);
        fileRemoved = true;
      }
      if (!ledger.file_preexisting &&
          (await listAllWorkspaceNames(baseUrl, auth)).includes(ledger.path)) {
        throw new Error(`owned Shared file '${ledger.path}' still exists`);
      }
    } catch (error) {
      cleanupErrors.push(`Shared file cleanup: ${error.message}`);
    }
  }

  try {
    const remainingProjects = await listAllProjects(baseUrl, auth);
    const remainingIds = new Set(remainingProjects.map((project) => project.id));
    const remainingOwned = remainingProjects.filter((project) =>
      ownedIds.has(project.id) || specifications.some(
        (specification) => project.title === specification.title &&
          !baselineIds.has(project.id)));
    if (remainingOwned.length > 0) {
      throw new Error(
        `owned projects remain: ${remainingOwned.map((project) => project.id).join(',')}`,
      );
    }
    if (ledger.original_project_id !== '' &&
        remainingIds.has(ledger.original_project_id)) {
      await request(baseUrl, auth, '/api/project/select', {
        method: 'POST', body: form({id: ledger.original_project_id}),
      });
    }
    const activeProjectId = requireString(
      (await activeChatState(baseUrl, auth)).project_id,
      'post-cleanup active project_id',
    );
    if (!remainingIds.has(activeProjectId) || ownedIds.has(activeProjectId)) {
      throw new Error(`active project '${activeProjectId}' is not retained baseline state`);
    }
  } catch (error) {
    cleanupErrors.push(`project restore/absence: ${error.message}`);
  }
  if (cleanupErrors.length > 0) {
    throw new Error(
      `P2-20 Web ownership cleanup failed; ${cleanupErrors.join('; ')}`,
    );
  }
  ledger = await writeSharedIsolationLedger(
    ledgerPath, ledger, {web_cleanup_complete: true});
  return {ledger, deleted_projects: deletedProjectCount, file_removed: fileRemoved};
}

async function createSharedIsolationProject(
  baseUrl, auth, ledgerPath, ledger, suffix) {
  const pendingField = `project_${suffix.toLowerCase()}_pending`;
  const idField = `project_${suffix.toLowerCase()}_id`;
  ledger = await writeSharedIsolationLedger(
    ledgerPath, ledger, {[pendingField]: true});
  const response = await request(baseUrl, auth, '/api/project/new', {
    method: 'POST', body: form({title: ledger[`title_${suffix.toLowerCase()}`]}),
  });
  const projectId = requireString(
    (await response.json()).project_id, `project ${suffix} id`);
  if (ledger.baseline_project_ids.includes(projectId)) {
    throw new Error(`Project ${suffix} reused baseline id '${projectId}'`);
  }
  ledger = await writeSharedIsolationLedger(
    ledgerPath, ledger, {[idField]: projectId});
  return {ledger, projectId};
}

async function verifySharedIsolationRoundTrip(baseUrl, auth, nonce, ledgerPath) {
  let ledger = await readSharedIsolationLedger(ledgerPath, nonce);
  if (ledger.baseline_ready || ledger.web_cleanup_complete) {
    throw new Error('P2-20 normal suite requires a fresh, unclaimed ledger');
  }
  const fixture = Buffer.from(
    `P2-20-SHARED-ISOLATION:${nonce}\n` +
    'one-storage-object-two-project-links-no-implicit-copy\n',
    'utf8',
  );
  const expectedHash = sha256(fixture);
  const baselineProjects = await listAllProjects(baseUrl, auth);
  const originalProjectId = requireString(
    (await activeChatState(baseUrl, auth)).project_id, 'original project_id');
  ledger = await writeSharedIsolationLedger(ledgerPath, ledger, {
    baseline_ready: true,
    baseline_project_ids: baselineProjects.map((project) => project.id),
    original_project_id: originalProjectId,
  });
  const reservedTitles = new Set([ledger.title_a, ledger.title_b, ledger.title_c]);
  if (baselineProjects.some((project) => reservedTitles.has(project.title))) {
    throw new Error('P2-20 reserved project title collides with baseline state');
  }
  const baselineNames = await listAllWorkspaceNames(baseUrl, auth);
  const pathMatches = baselineNames.filter((name) => name === ledger.path);
  ledger = await writeSharedIsolationLedger(ledgerPath, ledger, {
    file_baseline_checked: true,
    file_preexisting: pathMatches.length > 0,
  });
  if (pathMatches.length > 0) {
    throw new Error(`P2-20 reserved Shared path already exists: '${ledger.path}'`);
  }

  const initialStatus = validatedHeapSnapshot(
    'shared_isolation_initial', await statusState(baseUrl, auth));
  let testError = null;
  let evidence = null;
  try {
    ledger = await writeSharedIsolationLedger(
      ledgerPath, ledger, {file_upload_pending: true});
    const upload = new FormData();
    upload.append('file', new Blob([fixture], {type: 'text/plain'}), ledger.path);
    const uploadedResponse = await request(
      baseUrl,
      auth,
      `/api/file/upload?name=${encodeURIComponent(ledger.path)}`,
      {method: 'POST', body: upload},
    );
    const uploaded = await uploadedResponse.json();
    if (uploaded.ok !== true || uploaded.name !== ledger.path ||
        uploaded.bytes !== fixture.length) {
      throw new Error('P2-20 Shared upload returned unexpected typed state');
    }
    const exactMatches = (await listAllWorkspaceNames(baseUrl, auth))
      .filter((name) => name === ledger.path);
    if (exactMatches.length !== 1) {
      throw new Error(`P2-20 Shared path appears ${exactMatches.length} times`);
    }
    const initialDownload = await downloadWorkspaceFile(
      baseUrl, auth, ledger.path);
    if (!initialDownload.equals(fixture) || sha256(initialDownload) !== expectedHash) {
      throw new Error('P2-20 initial Shared download differs from uploaded bytes');
    }

    let created = await createSharedIsolationProject(
      baseUrl, auth, ledgerPath, ledger, 'A');
    ledger = created.ledger;
    const projectAId = created.projectId;
    created = await createSharedIsolationProject(
      baseUrl, auth, ledgerPath, ledger, 'B');
    ledger = created.ledger;
    const projectBId = created.projectId;
    created = await createSharedIsolationProject(
      baseUrl, auth, ledgerPath, ledger, 'C');
    ledger = created.ledger;
    const projectCId = created.projectId;
    if (new Set([projectAId, projectBId, projectCId]).size !== 3) {
      throw new Error('P2-20 projects did not receive distinct identifiers');
    }

    for (const projectId of [projectAId, projectBId]) {
      await request(baseUrl, auth, '/api/project/select', {
        method: 'POST', body: form({id: projectId}),
      });
      await request(baseUrl, auth, '/api/project/link', {
        method: 'POST', body: form({path: ledger.path, linked: '1'}),
      });
    }
    await request(baseUrl, auth, '/api/project/select', {
      method: 'POST', body: form({id: projectAId}),
    });
    const throughA = await verifySharedFileBytes(
      baseUrl, auth, ledger.path, fixture, 'project A');
    await request(baseUrl, auth, '/api/project/select', {
      method: 'POST', body: form({id: projectBId}),
    });
    const throughB = await verifySharedFileBytes(
      baseUrl, auth, ledger.path, fixture, 'project B');
    await request(baseUrl, auth, '/api/project/select', {
      method: 'POST', body: form({id: projectCId}),
    });
    const projectCLinks = await listAllProjectLinks(baseUrl, auth);
    if (projectCLinks.includes(ledger.path) || projectCLinks.length !== 0) {
      throw new Error(`Project C unexpectedly exposes Shared links: ${JSON.stringify(projectCLinks)}`);
    }

    await expectRequestFailure(
      baseUrl,
      auth,
      '/api/file/delete',
      {method: 'POST', body: form({name: ledger.path})},
      400,
    );
    const afterRejectedDelete = await downloadWorkspaceFile(
      baseUrl, auth, ledger.path);
    if (!afterRejectedDelete.equals(fixture) ||
        sha256(afterRejectedDelete) !== expectedHash) {
      throw new Error('Rejected linked-file deletion changed Shared bytes');
    }

    await deleteProjectById(baseUrl, auth, projectAId);
    await request(baseUrl, auth, '/api/project/select', {
      method: 'POST', body: form({id: projectBId}),
    });
    const survivingB = await verifySharedFileBytes(
      baseUrl, auth, ledger.path, fixture, 'project B after deleting A');
    await request(baseUrl, auth, '/api/project/link', {
      method: 'POST', body: form({path: ledger.path, linked: '0'}),
    });
    if ((await listAllProjectLinks(baseUrl, auth)).length !== 0) {
      throw new Error('Project B Shared unlink did not persist');
    }
    await deleteProjectById(baseUrl, auth, projectBId);
    await deleteProjectById(baseUrl, auth, projectCId);
    await deleteWorkspaceProbe(baseUrl, auth, ledger.path);
    const finalNames = await listAllWorkspaceNames(baseUrl, auth);
    const finalProjects = await listAllProjects(baseUrl, auth);
    if (finalNames.includes(ledger.path) || finalProjects.some(
      (project) => reservedTitles.has(project.title) ||
        [projectAId, projectBId, projectCId].includes(project.id))) {
      throw new Error('P2-20 explicit test cleanup left owned file or projects');
    }
    await request(baseUrl, auth, '/api/project/select', {
      method: 'POST', body: form({id: originalProjectId}),
    });
    evidence = {
      path: ledger.path,
      bytes: fixture.length,
      sha256: expectedHash,
      physical_file_count: 1,
      project_ids_distinct: true,
      project_a: throughA,
      project_b: throughB,
      project_c_link_absent: true,
      linked_delete_rejected_and_unchanged: true,
      project_b_survived_project_a_delete: survivingB.sha256 === expectedHash,
      explicit_unlink_delete: 'pass',
      initial_status: initialStatus,
    };
  } catch (error) {
    testError = error;
  }

  let cleanupEvidence = null;
  const cleanupErrors = [];
  try {
    ledger = await readSharedIsolationLedger(ledgerPath, nonce);
    cleanupEvidence = await cleanupSharedIsolationOwnership(
      baseUrl, auth, ledgerPath, ledger);
  } catch (error) {
    cleanupErrors.push(error.message);
  }
  try {
    const finalStatus = validatedHeapSnapshot(
      'shared_isolation_final', await statusState(baseUrl, auth));
    if (evidence !== null) {
      evidence.final_status = finalStatus;
      evidence.cleanup = cleanupEvidence === null ? 'failed' : 'pass';
      if (cleanupEvidence !== null) {
        evidence.cleanup_deleted_projects = cleanupEvidence.deleted_projects;
        evidence.cleanup_file_removed = cleanupEvidence.file_removed;
      }
    }
  } catch (error) {
    cleanupErrors.push(`final status: ${error.message}`);
  }
  if (testError !== null || cleanupErrors.length > 0) {
    const messages = [];
    if (testError !== null) messages.push(`test: ${testError.message}`);
    messages.push(...cleanupErrors.map((message) => `cleanup: ${message}`));
    throw new Error(`Shared-isolation round trip failed; ${messages.join('; ')}`);
  }
  return evidence;
}

async function recoverSharedIsolationOwnership(baseUrl, auth, ledgerPath) {
  const ledger = await readSharedIsolationLedger(ledgerPath, '');
  const recovered = await cleanupSharedIsolationOwnership(
    baseUrl, auth, ledgerPath, ledger);
  return {
    nonce: ledger.nonce,
    deleted_projects: recovered.deleted_projects,
    file_removed: recovered.file_removed,
    web_cleanup_complete: recovered.ledger.web_cleanup_complete,
    final_status: validatedHeapSnapshot(
      'shared_isolation_recovery_final', await statusState(baseUrl, auth)),
  };
}

async function verifyLargeStreamRoundTrip(
  baseUrl, auth, nonce, ledgerPath, runFullStreamSha) {
  let ledger = await readLargeStreamLedger(ledgerPath, nonce);
  if (!ledger.setup_pending || !ledger.setup_complete ||
      ledger.web_verification_complete || ledger.device_verification_complete) {
    throw new Error('P2-21 Web suite requires a fresh completed device setup ledger');
  }
  const initialStatus = validatedHeapSnapshot(
    'large_stream_initial', await statusState(baseUrl, auth));

  const listingBefore = validatedHeapSnapshot(
    'large_stream_listing_before', await statusState(baseUrl, auth));
  const entries = [];
  const offsets = [];
  const seenOffsets = new Set();
  let offset = 0;
  let eof = false;
  while (!eof) {
    if (seenOffsets.has(offset)) {
      throw new Error(`P2-21 workspace pagination repeated offset ${offset}`);
    }
    seenOffsets.add(offset);
    offsets.push(offset);
    const response = await request(
      baseUrl, auth, `/api/files?offset=${offset}`, {method: 'GET'});
    const document = await response.json();
    if (!Array.isArray(document.files) || typeof document.eof !== 'boolean' ||
        !Number.isInteger(document.next_offset) || document.next_offset < 0) {
      throw new Error('P2-21 workspace listing returned invalid typed state');
    }
    for (const file of document.files) {
      const name = requireString(file.name, 'P2-21 workspace file name');
      if (!Number.isSafeInteger(file.size) || file.size < 0) {
        throw new Error(`P2-21 workspace listing returned invalid size for '${name}'`);
      }
      entries.push({name, size: file.size});
    }
    eof = document.eof;
    if (!eof && document.next_offset <= offset) {
      throw new Error('P2-21 workspace pagination offset did not advance');
    }
    offset = document.next_offset;
  }
  const matches = entries.filter((entry) => entry.name === ledger.path);
  if (matches.length !== 1 || matches[0].size !== p2LargeStreamBytes) {
    throw new Error(
      `P2-21 expected one ${p2LargeStreamBytes}-byte fixture; ` +
      `found ${JSON.stringify(matches)}`,
    );
  }
  const listingAfter = validatedHeapSnapshot(
    'large_stream_listing_after', await statusState(baseUrl, auth));
  requireLargeStreamHeapBounds(
    listingBefore,
    listingAfter,
    'P2-21 workspace listing',
    p2LargeStreamListingMinimumHeapLossBytes,
  );

  let streamedDownload = null;
  if (runFullStreamSha) {
    const downloadBefore = validatedHeapSnapshot(
      'large_stream_download_before', await statusState(baseUrl, auth));
    const downloadStarted = performance.now();
    const streamed = await streamWorkspaceFileSha256(
      baseUrl, auth, ledger.path, p2LargeStreamBytes);
    const downloadMs = Math.round(performance.now() - downloadStarted);
    if (streamed.sha256 !== ledger.expected_sha256) {
      throw new Error(
        `P2-21 streamed SHA-256 was ${streamed.sha256}; expected ${ledger.expected_sha256}`,
      );
    }
    const downloadAfter = validatedHeapSnapshot(
      'large_stream_download_after', await statusState(baseUrl, auth));
    requireLargeStreamHeapBounds(
      downloadBefore,
      downloadAfter,
      'P2-21 streamed download',
      p2LargeStreamDownloadMinimumHeapLossBytes,
    );
    streamedDownload = {
      bytes: streamed.bytes,
      sha256: streamed.sha256,
      chunks: streamed.chunks,
      maximum_chunk_bytes: streamed.maximum_chunk_bytes,
      content_length: streamed.content_length,
      elapsed_ms: downloadMs,
      minimum_heap_loss_allowance_bytes:
        p2LargeStreamDownloadMinimumHeapLossBytes,
      heap: {before: downloadBefore, after: downloadAfter},
    };
  }

  const windowOffsets = [
    formerLargeFileBoundaryBytes - 4096,
    formerLargeFileBoundaryBytes + 64,
    p2LargeStreamBytes - 4096,
  ];
  const windows = [];
  for (const windowOffset of windowOffsets) {
    const before = validatedHeapSnapshot(
      `large_stream_read_${windowOffset}_before`, await statusState(baseUrl, auth));
    const response = await request(
      baseUrl,
      auth,
      `/api/file?name=${encodeURIComponent(ledger.path)}&offset=${windowOffset}`,
      {method: 'GET'},
    );
    const document = await response.json();
    const content = Buffer.from(requireString(document.content, 'P2-21 window content'), 'utf8');
    const expectedWindowBytes = Math.min(
      12_288, p2LargeStreamBytes - windowOffset);
    if (document.ok !== true || document.offset !== windowOffset ||
        document.total_bytes !== p2LargeStreamBytes ||
        typeof document.eof !== 'boolean' ||
        !Number.isSafeInteger(document.next_offset) ||
        content.length !== expectedWindowBytes ||
        document.next_offset !== windowOffset + content.length ||
        !content.equals(expectedLargeStreamBytes(windowOffset, content.length))) {
      throw new Error(`P2-21 bounded Web read mismatch at offset ${windowOffset}`);
    }
    if ((windowOffset === p2LargeStreamBytes - 4096) !== (document.eof === true)) {
      throw new Error(`P2-21 bounded Web EOF state mismatch at offset ${windowOffset}`);
    }
    if (windowOffset === p2LargeStreamBytes - 4096 &&
        document.next_offset !== p2LargeStreamBytes) {
      throw new Error('P2-21 final Web window did not end at the exact file boundary');
    }
    const after = validatedHeapSnapshot(
      `large_stream_read_${windowOffset}_after`, await statusState(baseUrl, auth));
    requireLargeStreamHeapBounds(
      before,
      after,
      `P2-21 bounded read at ${windowOffset}`,
      p2LargeStreamJsonWindowMinimumHeapLossBytes,
    );
    windows.push({
      offset: windowOffset,
      bytes: content.length,
      sha256: sha256(content),
      eof: document.eof,
      minimum_heap_loss_allowance_bytes:
        p2LargeStreamJsonWindowMinimumHeapLossBytes,
      heap: {before, after},
    });
  }
  ledger = await writeLargeStreamLedger(
    ledgerPath, ledger, {web_verification_complete: true});
  return {
    path: ledger.path,
    bytes: p2LargeStreamBytes,
    expected_fnv32: ledger.expected_fnv32,
    listing: {
      matches: matches.length,
      offsets,
      minimum_heap_loss_allowance_bytes:
        p2LargeStreamListingMinimumHeapLossBytes,
      heap: {before: listingBefore, after: listingAfter},
    },
    windows,
    full_stream_sha_soak: streamedDownload,
    initial_status: initialStatus,
    web_verification_complete: ledger.web_verification_complete,
  };
}

async function verifyAtomicFailureRoundTrip(baseUrl, auth, nonce, ledgerPath) {
  const ledger = await readAtomicFailureLedger(ledgerPath, nonce);
  const expected = Buffer.from('CARDMIND_P2_22_ORIGINAL\n', 'ascii');
  const expectedSha256 = sha256(expected);
  const readExact = async (label) => {
    const response = await request(
      baseUrl,
      auth,
      `/api/file?name=${encodeURIComponent(ledger.path)}&offset=0`,
      {method: 'GET'},
    );
    const document = await response.json();
    const content = Buffer.from(
      requireString(document.content, `P2-22 ${label} content`), 'utf8');
    if (document.ok !== true || document.offset !== 0 || document.next_offset !== 24 ||
        document.total_bytes !== 24 || document.eof !== true ||
        !content.equals(expected) || sha256(content) !== expectedSha256) {
      throw new Error(`P2-22 ${label} bounded read did not match the exact original`);
    }
    const downloaded = await downloadWorkspaceFile(baseUrl, auth, ledger.path);
    if (downloaded.length !== 24 || !downloaded.equals(expected) ||
        sha256(downloaded) !== expectedSha256) {
      throw new Error(`P2-22 ${label} download did not match the exact original`);
    }
    return {
      bytes: downloaded.length,
      sha256: sha256(downloaded),
      bounded_read_eof: document.eof,
    };
  };

  const initial = await readExact('initial');
  const errorDetail = await expectRequestFailureDetail(
    baseUrl,
    auth,
    '/api/file/save',
    {
      method: 'POST',
      body: form({
        name: ledger.path,
        offset: '0',
        original_bytes: '0',
        content: 'X',
      }),
    },
    400,
  );
  if (!errorDetail.includes('Failed to remove stale storage file') ||
      !errorDetail.includes(`${ledger.path}.bak`)) {
    throw new Error(
      `P2-22 Web edit returned an unexpected replacement error: ${errorDetail}`,
    );
  }
  const afterFailure = await readExact('post-failure');
  return {
    path: ledger.path,
    expected_bytes: ledger.expected_bytes,
    expected_fnv32: ledger.expected_fnv32,
    initial,
    rejected_edit: {
      route: '/api/file/save',
      status: 400,
      explicit_filesystem_error: true,
    },
    after_failure: afterFailure,
    original_unchanged: true,
  };
}

async function verifyVersionHistoryPolicy(baseUrl, auth, nonce) {
  const prefix = `cardmind_p2_30_${nonce}`;
  const name = `${prefix}.txt`;
  const initial = Buffer.from('P2-30 initial\n', 'utf8');
  const rangeReplacement = Buffer.from('P2-30 range replacement\n', 'utf8');
  const wholeReplacement = Buffer.from('P2-30 whole-file replacement\n', 'utf8');
  let ownsFixture = false;
  let evidence = null;
  let testError = null;
  const matchingNames = async () =>
    (await listAllWorkspaceNames(baseUrl, auth)).filter(
      (candidate) => candidate.startsWith(prefix));
  const requireCurrentOnly = async (label) => {
    const matches = await matchingNames();
    if (matches.length !== 1 || matches[0] !== name) {
      throw new Error(
        `P2-30 ${label} left unexpected file versions: ${JSON.stringify(matches)}`,
      );
    }
    return matches;
  };
  const replaceWholeFile = async (bytes, expectedStatus) => {
    const upload = new FormData();
    upload.append('file', new Blob([bytes], {type: 'application/octet-stream'}), name);
    const path = `/api/file/upload?replace=1&name=${encodeURIComponent(name)}`;
    if (expectedStatus === 200) {
      await request(baseUrl, auth, path, {method: 'POST', body: upload});
      return '';
    }
    return expectRequestFailureDetail(
      baseUrl, auth, path, {method: 'POST', body: upload}, expectedStatus);
  };

  try {
    const collisions = await matchingNames();
    if (collisions.length !== 0) {
      throw new Error(
        `P2-30 fixture namespace is not empty: ${JSON.stringify(collisions)}`,
      );
    }
    await uploadWorkspaceBytes(baseUrl, auth, name, initial);
    ownsFixture = true;
    await request(baseUrl, auth, '/api/file/save', {
      method: 'POST',
      body: form({
        name,
        offset: '0',
        original_bytes: String(initial.length),
        content: rangeReplacement.toString('utf8'),
      }),
    });
    const afterRange = await downloadWorkspaceFile(baseUrl, auth, name);
    if (!afterRange.equals(rangeReplacement)) {
      throw new Error('P2-30 range replacement bytes do not match');
    }
    const rangeArtifacts = await requireCurrentOnly('range replacement');

    await replaceWholeFile(wholeReplacement, 200);
    const afterWhole = await downloadWorkspaceFile(baseUrl, auth, name);
    if (!afterWhole.equals(wholeReplacement)) {
      throw new Error('P2-30 whole-file replacement bytes do not match');
    }
    const wholeArtifacts = await requireCurrentOnly('whole-file replacement');

    const invalidDetail = await replaceWholeFile(Buffer.from([0xc3, 0x28]), 400);
    if (!/UTF-8/i.test(invalidDetail)) {
      throw new Error(`P2-30 invalid UTF-8 returned an unexpected error: ${invalidDetail}`);
    }
    const afterFailure = await downloadWorkspaceFile(baseUrl, auth, name);
    if (!afterFailure.equals(wholeReplacement)) {
      throw new Error('P2-30 failed replacement changed the current file');
    }
    const failureArtifacts = await requireCurrentOnly('failed replacement');
    evidence = {
      path: name,
      range_sha256: sha256(afterRange),
      whole_sha256: sha256(afterWhole),
      failed_replacement_sha256: sha256(afterFailure),
      range_artifacts: rangeArtifacts,
      whole_file_artifacts: wholeArtifacts,
      failure_artifacts: failureArtifacts,
      no_automatic_versions: 'pass',
      successful_commit_cleanup: 'pass',
      explicit_failure_cleanup: 'pass',
      nonmutation: 'pass',
    };
  } catch (error) {
    testError = error;
  }

  let cleanupError = null;
  if (ownsFixture) {
    try {
      await deleteWorkspaceProbe(baseUrl, auth, name);
      const remaining = await matchingNames();
      if (remaining.length !== 0) {
        throw new Error(`P2-30 cleanup left files: ${JSON.stringify(remaining)}`);
      }
    } catch (error) {
      cleanupError = error;
    }
  }
  if (testError || cleanupError) {
    throw new Error(
      `P2-30 version-history policy failed; test=${testError?.message ?? 'none'}; ` +
      `cleanup=${cleanupError?.message ?? 'none'}`,
    );
  }
  return {...evidence, cleanup: 'pass'};
}

async function verifyLargeFileRoundTrip(baseUrl, auth) {
  const name = `cardmind_p2_18_${Date.now()}.txt`;
  const deviceDiagnosticNames = [
    'firmware_editor_test.txt',
    'firmware_editor_copy.txt',
    'firmware_editor_renamed.txt',
    'firmware_editor_upload.txt',
  ];
  const totalBytes = 4 * 1024 * 1024;
  const marker = 'P2-18-SEARCH-MARKER-NEAR-END';
  const markerOffset = totalBytes - 4096;
  const fixture = createLargeTextFixture(totalBytes, markerOffset, marker);
  const expected = Buffer.from(fixture);
  const originalHash = sha256(fixture);
  let testError = null;
  let evidence = null;
  try {
    const beforeUpload = validatedHeapSnapshot(
      'large_file_upload_before', await statusState(baseUrl, auth));
    const upload = new FormData();
    upload.append('file', new Blob([fixture], {type: 'text/plain'}), name);
    const uploadResponse = await longRequest(
      baseUrl,
      auth,
      `/api/file/upload?name=${encodeURIComponent(name)}`,
      {method: 'POST', body: upload},
    );
    const uploaded = await uploadResponse.json();
    if (uploaded.ok !== true || uploaded.bytes !== totalBytes || uploaded.name !== name) {
      throw new Error(`Large-file upload returned invalid state: ${JSON.stringify(uploaded)}`);
    }
    const afterUpload = validatedHeapSnapshot(
      'large_file_upload_after', await statusState(baseUrl, auth));
    requireBoundedHeapLoss(beforeUpload, afterUpload, '4 MiB upload');

    for (const offset of [0, markerOffset, totalBytes - 64]) {
      const beforeRead = validatedHeapSnapshot(
        `large_file_read_${offset}_before`, await statusState(baseUrl, auth));
      const response = await request(
        baseUrl,
        auth,
        `/api/file?name=${encodeURIComponent(name)}&offset=${offset}`,
        {method: 'GET'},
      );
      const window = await response.json();
      const expectedWindow = fixture.subarray(offset, offset + Buffer.byteLength(window.content));
      if (window.ok !== true || window.offset !== offset ||
          window.total_bytes !== totalBytes ||
          !Buffer.from(window.content, 'utf8').equals(expectedWindow)) {
        throw new Error(`Large-file window mismatch at offset ${offset}`);
      }
      if (offset === markerOffset && !window.content.startsWith(marker)) {
        throw new Error('Large-file near-end search marker was not visible in its bounded window');
      }
      const afterRead = validatedHeapSnapshot(
        `large_file_read_${offset}_after`, await statusState(baseUrl, auth));
      requireBoundedHeapLoss(beforeRead, afterRead, `4 MiB read at ${offset}`);
    }

    const initialDownload = await downloadWorkspaceFile(baseUrl, auth, name);
    if (initialDownload.length !== totalBytes || sha256(initialDownload) !== originalHash ||
        !initialDownload.equals(fixture)) {
      throw new Error('Initial 4 MiB download failed byte equality');
    }
    await expectRequestFailure(
      baseUrl,
      auth,
      '/api/file/save',
      {
        method: 'POST',
        body: form({
          name,
          offset: String(totalBytes - 8),
          original_bytes: '64',
          content: 'REJECTED',
        }),
      },
      400,
    );
    const afterRejectedEdit = await downloadWorkspaceFile(baseUrl, auth, name);
    if (afterRejectedEdit.length !== totalBytes ||
        sha256(afterRejectedEdit) !== originalHash ||
        !afterRejectedEdit.equals(fixture)) {
      throw new Error('Rejected out-of-range edit changed the 4 MiB source bytes');
    }

    const editOffset = 2 * 1024 * 1024 + 123;
    const replacement = Buffer.from('P2-18-EDITED-WINDOW-BYTE-EQUALITY', 'ascii');
    replacement.copy(expected, editOffset);
    const beforeEdit = validatedHeapSnapshot(
      'large_file_edit_before', await statusState(baseUrl, auth));
    const editResponse = await longRequest(baseUrl, auth, '/api/file/save', {
      method: 'POST',
      body: form({
        name,
        offset: String(editOffset),
        original_bytes: String(replacement.length),
        content: replacement.toString('ascii'),
      }),
    });
    if ((await editResponse.json()).ok !== true) {
      throw new Error('4 MiB bounded edit did not return ok=true');
    }
    const afterEdit = validatedHeapSnapshot(
      'large_file_edit_after', await statusState(baseUrl, auth));
    requireBoundedHeapLoss(beforeEdit, afterEdit, '4 MiB bounded edit');
    const finalDownload = await downloadWorkspaceFile(baseUrl, auth, name);
    const expectedHash = sha256(expected);
    if (finalDownload.length !== expected.length || sha256(finalDownload) !== expectedHash ||
        !finalDownload.equals(expected)) {
      throw new Error('Edited 4 MiB download failed exact byte equality');
    }
    evidence = {
      bytes: totalBytes,
      original_sha256: originalHash,
      edited_sha256: expectedHash,
      bounded_views: 3,
      near_end_marker_offset: markerOffset,
      edit_offset: editOffset,
      rejected_edit_preserved_hash: true,
      heap: {upload: {before: beforeUpload, after: afterUpload}, edit: {before: beforeEdit, after: afterEdit}},
    };
  } catch (error) {
    testError = error;
  }
  let cleanupError = null;
  try {
    const namesBeforeCleanup = await listAllWorkspaceNames(baseUrl, auth);
    if (namesBeforeCleanup.includes(name)) {
      await deleteWorkspaceProbe(baseUrl, auth, name);
    }
    const namesAfterCleanup = await listAllWorkspaceNames(baseUrl, auth);
    if (namesAfterCleanup.includes(name)) {
      throw new Error(`Large-file cleanup did not remove exact file '${name}'`);
    }
    const conflictingNames = deviceDiagnosticNames.filter(
      (diagnosticName) => namesAfterCleanup.includes(diagnosticName));
    if (conflictingNames.length > 0) {
      throw new Error(
        `Device FILETEST is unsafe because user files already exist: ` +
        `${JSON.stringify(conflictingNames)}`,
      );
    }
    const finalStatus = validatedHeapSnapshot(
      'large_file_cleanup_final', await statusState(baseUrl, auth));
    if (evidence !== null) {
      evidence.final_status = finalStatus;
      evidence.device_filetest_names_absent = true;
    }
  } catch (error) {
    cleanupError = error;
  }
  if (testError || cleanupError) {
    throw new Error(
      `4 MiB file round trip failed; test=${testError?.message ?? 'none'}; ` +
      `cleanup=${cleanupError?.message ?? 'none'}`,
    );
  }
  return evidence;
}

async function verifyRetryRoundTrip(baseUrl, auth) {
  const original = await activeChatState(baseUrl, auth);
  const originalProjectId = requireString(original.project_id, 'project_id');
  const marker = `WEB-RETRY-${Date.now()}`;
  let projectId = '';
  try {
    const createdResponse = await request(baseUrl, auth, '/api/project/new', {
      method: 'POST',
      body: form({title: `P2 retry ${Date.now()}`}),
    });
    projectId = requireString((await createdResponse.json()).project_id, 'project_id');
    await request(baseUrl, auth, '/api/project/settings/raw',
      projectSettingsRequest({
        instructions: '',
        model: 'cardmind-nonexistent-retry-model',
        context_byte_budget: '16384',
        maximum_output_tokens: '128',
        automatic_compaction: '0',
      }));
    const failedResponse = await request(
      baseUrl,
      auth,
      '/api/prompt/raw',
      promptRequest(`Reply with exactly: ${marker}`, '128'),
    );
    const failedEvents = parseSse(await failedResponse.text());
    if (!failedEvents.some((event) => event.type === 'error') ||
        failedEvents.some((event) => event.type === 'done')) {
      throw new Error('Deliberately invalid model did not create a retryable failed turn');
    }
    const pending = await activeChatState(baseUrl, auth);
    const pendingUsers = pending.messages.filter((message) => message.role === 'user');
    if (pending.total_messages !== 1 || pendingUsers.length !== 1) {
      throw new Error(`Failed request was not stored exactly once: ${JSON.stringify(pending.messages)}`);
    }
    await request(baseUrl, auth, '/api/project/settings/raw',
      projectSettingsRequest({
        instructions: '',
        model: '',
        context_byte_budget: '16384',
        maximum_output_tokens: '128',
        automatic_compaction: '0',
      }));
    const retriedResponse = await request(baseUrl, auth, '/api/prompt/retry', {
      method: 'POST',
      body: form({maximum_output_tokens: '128'}),
    });
    const retriedEvents = parseSse(await retriedResponse.text());
    const retryError = retriedEvents.find((event) => event.type === 'error');
    if (retryError !== undefined || !retriedEvents.some((event) => event.type === 'done')) {
      throw new Error(`Retry stream failed: ${retryError?.error ?? 'missing done event'}`);
    }
    const responseText = retriedEvents.map((event) => event.delta ?? '').join('');
    if (!responseText.includes(marker)) {
      throw new Error(`Retry response did not contain marker '${marker}'`);
    }
    const completed = await activeChatState(baseUrl, auth);
    const completedUsers = completed.messages.filter((message) => message.role === 'user');
    if (completed.total_messages !== 2 || completedUsers.length !== 1) {
      throw new Error(`Retry duplicated the stored request: ${JSON.stringify(completed.messages)}`);
    }
    return {messages: completed.total_messages, user_copies: completedUsers.length};
  } finally {
    if (projectId) {
      await request(baseUrl, auth, '/api/project/select', {
        method: 'POST',
        body: form({id: projectId}),
      }).then(() => request(baseUrl, auth, '/api/project/delete', {
        method: 'POST',
        body: form({}),
      })).catch((error) => console.error(`Retry project cleanup failed: ${error.message}`));
    }
    await request(baseUrl, auth, '/api/project/select', {
      method: 'POST',
      body: form({id: originalProjectId}),
    }).catch((error) => console.error(`Original project restore failed: ${error.message}`));
  }
}

async function verifyWorkspaceToolRefresh(baseUrl, auth) {
  const original = await activeChatState(baseUrl, auth);
  const originalProjectId = requireString(original.project_id, 'project_id');
  const nonce = String(Date.now());
  const projectTitle = `Workspace tool refresh ${nonce}`;
  const fileName = `_cardmind_workspace_tool_${nonce}.py`;
  const marker = `CARDMIND_WORKSPACE_TOOL_${nonce}`;
  let projectId = '';
  let fileCreated = false;
  let evidence = null;
  let testError = null;
  const cleanupErrors = [];
  try {
    const createdResponse = await request(baseUrl, auth, '/api/project/new', {
      method: 'POST',
      body: form({title: projectTitle}),
    });
    projectId = requireString((await createdResponse.json()).project_id, 'project_id');
    const beforeFiles = await (
      await request(baseUrl, auth, '/api/files?offset=0', {method: 'GET'})
    ).json();
    if (!Number.isSafeInteger(beforeFiles.files_revision)) {
      throw new Error('Workspace endpoint omitted its pre-write revision');
    }
    const writeResponse = await longRequest(
      baseUrl,
      auth,
      '/api/prompt/raw',
      promptRequest(
        `/file Create ${fileName} with exactly this Python source: print("${marker}")`,
        '512',
      ),
    );
    const writeEvents = parseSse(await writeResponse.text());
    const writeError = writeEvents.find((event) => event.type === 'error');
    if (writeError !== undefined || !writeEvents.some((event) => event.type === 'done')) {
      throw new Error(
        `Workspace write stream failed: ${writeError?.error ?? 'missing done event'}`,
      );
    }
    const afterFiles = await (
      await request(baseUrl, auth, '/api/files?offset=0', {method: 'GET'})
    ).json();
    if (!Number.isSafeInteger(afterFiles.files_revision) ||
        afterFiles.files_revision <= beforeFiles.files_revision) {
      throw new Error(
        `Workspace revision did not advance after tool write: ` +
        `${beforeFiles.files_revision} -> ${afterFiles.files_revision}`,
      );
    }
    fileCreated = (await listAllWorkspaceNames(baseUrl, auth)).includes(fileName);
    if (!fileCreated) {
      throw new Error(`Workspace listing omitted model-created file '${fileName}'`);
    }
    const file = await (
      await request(
        baseUrl,
        auth,
        `/api/file?name=${encodeURIComponent(fileName)}&offset=0`,
        {method: 'GET'},
      )
    ).json();
    if (file.ok !== true || file.eof !== true ||
        typeof file.content !== 'string' || !file.content.includes(marker)) {
      throw new Error(`Model-created file content is invalid: ${JSON.stringify(file)}`);
    }
    const listResponse = await longRequest(
      baseUrl,
      auth,
      '/api/prompt/raw',
      promptRequest(`/file List workspace files and confirm whether ${fileName} exists.`, '512'),
    );
    const listEvents = parseSse(await listResponse.text());
    const listError = listEvents.find((event) => event.type === 'error');
    if (listError !== undefined || !listEvents.some((event) => event.type === 'done')) {
      throw new Error(
        `Workspace list stream failed: ${listError?.error ?? 'missing done event'}`,
      );
    }
    evidence = {
      file: fileName,
      revision_before: beforeFiles.files_revision,
      revision_after: afterFiles.files_revision,
      write_stream: 'pass',
      list_stream: 'pass',
    };
  } catch (error) {
    testError = error;
  } finally {
    try {
      const ownedProjectIds = new Set();
      if (projectId) ownedProjectIds.add(projectId);
      for (const project of await listAllProjects(baseUrl, auth)) {
        if (project.title === projectTitle) ownedProjectIds.add(project.id);
      }
      for (const ownedProjectId of ownedProjectIds) {
        await request(baseUrl, auth, '/api/project/select', {
          method: 'POST',
          body: form({id: ownedProjectId}),
        });
        await request(baseUrl, auth, '/api/project/delete', {
          method: 'POST',
          body: form({}),
        });
      }
    } catch (error) {
      cleanupErrors.push(`project cleanup: ${error.message}`);
    }
    try {
      if (fileCreated || (await listAllWorkspaceNames(baseUrl, auth)).includes(fileName)) {
        await deleteWorkspaceProbe(baseUrl, auth, fileName);
      }
    } catch (error) {
      cleanupErrors.push(`file cleanup: ${error.message}`);
    }
    try {
      await request(baseUrl, auth, '/api/project/select', {
        method: 'POST',
        body: form({id: originalProjectId}),
      });
      const remainingProjects = await listAllProjects(baseUrl, auth);
      const remainingFiles = await listAllWorkspaceNames(baseUrl, auth);
      const restored = await activeChatState(baseUrl, auth);
      if (remainingProjects.some((project) => project.title === projectTitle) ||
          remainingFiles.includes(fileName) || restored.project_id !== originalProjectId) {
        throw new Error('temporary data remained or the original project was not restored');
      }
    } catch (error) {
      cleanupErrors.push(`state restoration: ${error.message}`);
    }
  }
  if (testError !== null || cleanupErrors.length > 0) {
    throw new Error(
      `Workspace tool round trip failed; test=${testError?.message ?? 'none'}; ` +
      `cleanup=${cleanupErrors.length === 0 ? 'none' : cleanupErrors.join('; ')}`,
    );
  }
  return evidence;
}

async function verifyCompactionRoundTrip(baseUrl, auth) {
  const original = await activeChatState(baseUrl, auth);
  const originalProjectId = requireString(original.project_id, 'project_id');
  let projectId = '';
  try {
    const createdResponse = await request(baseUrl, auth, '/api/project/new', {
      method: 'POST',
      body: form({title: `P2 compact ${Date.now()}`}),
    });
    projectId = requireString((await createdResponse.json()).project_id, 'project_id');
    await request(baseUrl, auth, '/api/project/settings/raw',
      projectSettingsRequest({
        instructions: '',
        model: 'cardmind-nonexistent-compaction-model',
        context_byte_budget: '8192',
        maximum_output_tokens: '128',
        automatic_compaction: '0',
      }));
    for (let index = 0; index < 10; index += 1) {
      const failed = await request(
        baseUrl,
        auth,
        '/api/prompt/raw',
        promptRequest(`compaction-raw-${index}`, '128'),
      );
      const events = parseSse(await failed.text());
      if (!events.some((event) => event.type === 'error')) {
        throw new Error(`Compaction seed request ${index} did not fail as expected`);
      }
    }
    const before = await activeChatState(baseUrl, auth);
    if (before.total_messages !== 10 || before.summarized_messages !== 0) {
      throw new Error(`Unexpected pre-compaction state: ${JSON.stringify(before.messages)}`);
    }
    await request(baseUrl, auth, '/api/project/settings/raw',
      projectSettingsRequest({
        instructions: '',
        model: '',
        context_byte_budget: '8192',
        maximum_output_tokens: '128',
        automatic_compaction: '0',
      }));
    const compactResponse = await request(baseUrl, auth, '/api/chat/compact', {
      method: 'POST',
      body: form({}),
    });
    const compacted = await compactResponse.json();
    const after = await activeChatState(baseUrl, auth);
    const rawMarkers = after.messages.filter(
      (message) => message.role === 'user' && message.content.startsWith('compaction-raw-'),
    );
    if (compacted.summarized_messages !== 2 || after.summarized_messages !== 2 ||
        after.total_messages !== 10 || rawMarkers.length !== 10 ||
        typeof after.context_summary !== 'string' || after.context_summary.length === 0) {
      throw new Error(`Compaction changed raw history or omitted summary metadata: ${JSON.stringify(after)}`);
    }
    return {
      raw_messages: after.total_messages,
      summarized_messages: after.summarized_messages,
      raw_markers: rawMarkers.length,
    };
  } finally {
    if (projectId) {
      await request(baseUrl, auth, '/api/project/select', {
        method: 'POST',
        body: form({id: projectId}),
      }).then(() => request(baseUrl, auth, '/api/project/delete', {
        method: 'POST',
        body: form({}),
      })).catch((error) => console.error(`Compaction project cleanup failed: ${error.message}`));
    }
    await request(baseUrl, auth, '/api/project/select', {
      method: 'POST',
      body: form({id: originalProjectId}),
    }).catch((error) => console.error(`Original project restore failed: ${error.message}`));
  }
}

function expectedContextUsage(messages, maximumBytes) {
  let retainedBytes = 0;
  let retainedMessages = 0;
  for (let index = messages.length - 1; index >= 0; index -= 1) {
    const message = messages[index];
    const candidateBytes = Buffer.byteLength(message.role, 'utf8') +
      Buffer.byteLength(message.content, 'utf8') + 16;
    if (retainedBytes + candidateBytes > maximumBytes && retainedMessages > 0) break;
    retainedBytes += candidateBytes;
    retainedMessages++;
  }
  return {
    retained_bytes: retainedBytes,
    retained_messages: retainedMessages,
    dropped_messages: messages.length - retainedMessages,
  };
}

async function seedFailedUserMessages(baseUrl, auth, prompts, outputTokens) {
  for (const prompt of prompts) {
    const response = await request(
      baseUrl, auth, '/api/prompt/raw', promptRequest(prompt, outputTokens));
    const events = parseSse(await response.text());
    if (!events.some((event) => event.type === 'error') ||
        events.some((event) => event.type === 'done')) {
      throw new Error('Owned history seed did not reach the configured failing provider path');
    }
  }
}

async function cleanupOwnedProjectAndRestore(baseUrl, auth, projectId, originalProjectId) {
  const errors = [];
  if (projectId) {
    try {
      const ids = await listAllProjectIds(baseUrl, auth);
      if (ids.includes(projectId)) {
        await request(baseUrl, auth, '/api/project/select', {
          method: 'POST', body: form({id: projectId}),
        });
        await request(baseUrl, auth, '/api/project/delete', {
          method: 'POST', body: form({}),
        });
      }
      if ((await listAllProjectIds(baseUrl, auth)).includes(projectId)) {
        errors.push('owned project is still present');
      }
    } catch (error) {
      errors.push(`owned project cleanup: ${error.message}`);
    }
  }
  try {
    await request(baseUrl, auth, '/api/project/select', {
      method: 'POST', body: form({id: originalProjectId}),
    });
    const restored = await activeChatState(baseUrl, auth);
    if (restored.project_id !== originalProjectId) {
      errors.push('original project restoration selected a different project');
    }
  } catch (error) {
    errors.push(`original project restoration: ${error.message}`);
  }
  if (errors.length > 0) {
    throw new Error(`Owned summary/history cleanup failed (${errors.join('; ')})`);
  }
}

async function cleanupContextHistoryOwnership(baseUrl, auth, ledgerPath, inputLedger) {
  let ledger = validateContextHistoryLedger(inputLedger, inputLedger.nonce);
  if (ledger.web_cleanup_complete) {
    return {ledger, deleted_projects: 0, restored: ledger.baseline_ready};
  }
  if (!ledger.baseline_ready) {
    ledger = await writeContextHistoryLedger(
      ledgerPath, ledger, {web_cleanup_complete: true});
    return {ledger, deleted_projects: 0, restored: false};
  }

  const projects = await listAllProjects(baseUrl, auth);
  const baselineIds = new Set(ledger.baseline_project_ids);
  const projectsById = new Map(projects.map((project) => [project.id, project]));
  const titleMatches = projects.filter((project) => project.title === ledger.title);
  if (titleMatches.some((project) => baselineIds.has(project.id))) {
    throw new Error('P2-27 cleanup found an owned-title collision in the baseline');
  }
  const nonBaselineTitleMatches = titleMatches.filter(
    (project) => !baselineIds.has(project.id));
  if (nonBaselineTitleMatches.length > 1) {
    throw new Error('P2-27 cleanup found ambiguous duplicate owned-title projects');
  }
  if (nonBaselineTitleMatches.length > 0 && !ledger.project_create_pending) {
    throw new Error('P2-27 cleanup found a project without persisted create ownership');
  }

  const ownedIds = new Set(nonBaselineTitleMatches.map((project) => project.id));
  if (ledger.owned_project_id !== '') {
    const known = projectsById.get(ledger.owned_project_id);
    if (known !== undefined && known.title !== ledger.title) {
      throw new Error('P2-27 ledger-owned project id has an unexpected title');
    }
    ownedIds.add(ledger.owned_project_id);
  }
  if ([...ownedIds].some((projectId) => baselineIds.has(projectId))) {
    throw new Error('P2-27 cleanup refused to delete a baseline project');
  }

  let deletedProjects = 0;
  for (const projectId of ownedIds) {
    if (projectsById.has(projectId)) {
      await deleteProjectById(baseUrl, auth, projectId);
      deletedProjects++;
    }
  }
  const remaining = await listAllProjects(baseUrl, auth);
  if (remaining.some((project) =>
    ownedIds.has(project.id) ||
    (!baselineIds.has(project.id) && project.title === ledger.title))) {
    throw new Error('P2-27 ledger-owned project is still present after cleanup');
  }
  if (!remaining.some((project) => project.id === ledger.original_project_id)) {
    throw new Error('P2-27 original project is absent after owned cleanup');
  }
  await request(baseUrl, auth, '/api/project/select', {
    method: 'POST', body: form({id: ledger.original_project_id}),
  });
  if ((await activeChatState(baseUrl, auth)).project_id !== ledger.original_project_id) {
    throw new Error('P2-27 cleanup selected a different project than the ledger original');
  }
  ledger = await writeContextHistoryLedger(ledgerPath, ledger, {
    project_create_pending: false,
    owned_project_id: '',
    web_cleanup_complete: true,
  });
  return {ledger, deleted_projects: deletedProjects, restored: true};
}

async function recoverContextHistoryOwnership(baseUrl, auth, ledgerPath) {
  const ledger = await readContextHistoryLedger(ledgerPath, '');
  const recovered = await cleanupContextHistoryOwnership(
    baseUrl, auth, ledgerPath, ledger);
  return {
    nonce: ledger.nonce,
    deleted_projects: recovered.deleted_projects,
    original_restored: recovered.restored,
    web_cleanup_complete: recovered.ledger.web_cleanup_complete,
  };
}

async function recoverPreLedgerContextHistoryOrphan(baseUrl, auth) {
  const projects = await listAllProjects(baseUrl, auth);
  const candidates = projects.map((project) => ({
    project,
    match: contextHistoryOrphanTitlePattern.exec(project.title),
  })).filter((candidate) => candidate.match !== null);
  if (candidates.length !== 1) {
    throw new Error(
      `P2-27 orphan recovery requires exactly one strict title match; found ${candidates.length}`,
    );
  }
  const candidate = candidates[0];
  const nonce = candidate.match[1];
  if (candidate.project.id.length > 180 ||
      !/^[A-Za-z0-9._-]+$/.test(candidate.project.id) ||
      projects.filter((project) => project.id === candidate.project.id).length !== 1) {
    throw new Error('P2-27 orphan candidate has an unsafe or ambiguous project id');
  }
  await request(baseUrl, auth, '/api/project/select', {
    method: 'POST', body: form({id: candidate.project.id}),
  });
  const selected = await activeChatState(baseUrl, auth);
  if (selected.project_id !== candidate.project.id ||
      !Number.isSafeInteger(selected.total_messages) ||
      selected.total_messages < 1 || selected.total_messages > 15) {
    throw new Error('P2-27 orphan did not load as a bounded fixture prefix');
  }
  const expectedMessages = selected.total_messages;
  requireString(selected.active_chat_id, 'P2-27 orphan active chat id');

  const seenOffsets = new Set();
  let offset = 0;
  let eof = false;
  let verifiedMessages = 0;
  let pages = 0;
  while (!eof) {
    if (seenOffsets.has(offset)) {
      throw new Error('P2-27 orphan history pagination repeated its cursor');
    }
    seenOffsets.add(offset);
    const response = await request(
      baseUrl, auth, `/api/chat/archived?offset=${offset}`, {method: 'GET'});
    const page = await response.json();
    if (page.ok !== true || !Array.isArray(page.messages) ||
        !Number.isSafeInteger(page.next_offset) || page.next_offset < 0 ||
        typeof page.eof !== 'boolean' ||
        (!page.eof && page.next_offset <= offset) ||
        (!page.eof && page.messages.length === 0)) {
      throw new Error('P2-27 orphan history returned invalid typed pagination state');
    }
    for (const message of page.messages) {
      const marker = `P2HISTORY-${nonce}-${String(verifiedMessages).padStart(2, '0')}|`;
      if (message?.role !== 'user' || typeof message.content !== 'string' ||
          !message.content.startsWith(marker)) {
        throw new Error('P2-27 orphan history does not match the exact owned marker sequence');
      }
      verifiedMessages++;
      if (verifiedMessages > expectedMessages) {
        throw new Error('P2-27 orphan history exceeds its active message count');
      }
    }
    pages++;
    offset = page.next_offset;
    eof = page.eof;
    if (pages > 15) {
      throw new Error('P2-27 orphan history exceeded its bounded page count');
    }
  }
  if (verifiedMessages !== expectedMessages) {
    throw new Error('P2-27 orphan history does not match its active message count');
  }

  await deleteProjectById(baseUrl, auth, candidate.project.id);
  const remaining = await listAllProjects(baseUrl, auth);
  if (remaining.some((project) =>
    project.id === candidate.project.id ||
    contextHistoryOrphanTitlePattern.test(project.title))) {
    throw new Error('P2-27 verified orphan is still present after deletion');
  }
  return {
    matched_projects: 1,
    verified_messages: verifiedMessages,
    pages,
    deleted: true,
    original_selection: 'unrecoverable',
  };
}

async function verifyManualSummaryRegeneration(baseUrl, auth) {
  const original = await activeChatState(baseUrl, auth);
  const originalProjectId = requireString(original.project_id, 'project_id');
  const nonce = String(Date.now());
  const failingModel = `cardmind-p2-26-invalid-${nonce}`;
  const prompts = Array.from(
    {length: 10}, (_, index) => `P2SUMMARY-${nonce}-${String(index).padStart(2, '0')}`);
  let projectId = '';
  try {
    const created = await request(baseUrl, auth, '/api/project/new', {
      method: 'POST', body: form({title: `P2 manual summary ${nonce}`}),
    });
    projectId = requireString((await created.json()).project_id, 'project_id');
    const projectSettings = (model) => projectSettingsRequest({
      instructions: '',
      model,
      context_byte_budget: '8192',
      maximum_output_tokens: '128',
      automatic_compaction: '0',
    });
    await request(
      baseUrl, auth, '/api/project/settings/raw', projectSettings(failingModel));
    await seedFailedUserMessages(baseUrl, auth, prompts, '128');
    const beforeFailure = await activeChatState(baseUrl, auth);
    if (beforeFailure.total_messages !== prompts.length ||
        beforeFailure.summarized_messages !== 0 || beforeFailure.context_summary !== '') {
      throw new Error('Manual summary fixture did not start from exact empty metadata');
    }
    await expectRequestFailure(
      baseUrl, auth, '/api/chat/compact', {method: 'POST', body: form({})}, 500);
    const afterFailure = await activeChatState(baseUrl, auth);
    if (afterFailure.total_messages !== beforeFailure.total_messages ||
        afterFailure.summarized_messages !== beforeFailure.summarized_messages ||
        afterFailure.context_summary !== beforeFailure.context_summary) {
      throw new Error('Failed manual summary mutated persisted summary metadata or raw count');
    }

    await request(baseUrl, auth, '/api/project/settings/raw', projectSettings(''));
    const compactResponse = await longRequest(
      baseUrl, auth, '/api/chat/compact', {method: 'POST', body: form({})});
    const compacted = await compactResponse.json();
    if (compacted.ok !== true || compacted.summary_event !== 'manual_regenerated' ||
        compacted.summarized_messages !== 2) {
      throw new Error('Manual summary response did not expose its exact event/count contract');
    }
    const afterSuccess = await activeChatState(baseUrl, auth);
    if (afterSuccess.total_messages !== prompts.length ||
        afterSuccess.summarized_messages !== 2 ||
        typeof afterSuccess.context_summary !== 'string' ||
        afterSuccess.context_summary.length === 0 ||
        afterSuccess.messages.length !== prompts.length ||
        !afterSuccess.messages.every(
          (message, index) => message.role === 'user' && message.content === prompts[index])) {
      throw new Error('Successful manual summary changed raw history or omitted metadata');
    }

    const persistedSummary = afterSuccess.context_summary;
    await request(
      baseUrl, auth, '/api/project/settings/raw', projectSettings(failingModel));
    await expectRequestFailure(
      baseUrl, auth, '/api/chat/compact', {method: 'POST', body: form({})}, 500);
    const afterReplacementFailure = await activeChatState(baseUrl, auth);
    if (afterReplacementFailure.total_messages !== prompts.length ||
        afterReplacementFailure.summarized_messages !== 2 ||
        afterReplacementFailure.context_summary !== persistedSummary) {
      throw new Error('Failed already-current replacement mutated the persisted summary');
    }
    return {
      raw_messages: prompts.length,
      summarized_messages: 2,
      event: 'manual_regenerated',
      initial_failure_non_mutation: 'pass',
      replacement_attempt_non_mutation: 'pass',
      raw_history_preserved: 'pass',
    };
  } finally {
    await cleanupOwnedProjectAndRestore(
      baseUrl, auth, projectId, originalProjectId);
  }
}

async function verifyContextHistoryParity(baseUrl, auth, nonce, ledgerPath) {
  let ledger = await readContextHistoryLedger(ledgerPath, nonce);
  if (ledger.baseline_ready || ledger.web_cleanup_complete) {
    throw new Error('P2-27 normal suite requires a fresh, unclaimed ledger');
  }
  const original = await activeChatState(baseUrl, auth);
  const originalProjectId = requireString(original.project_id, 'project_id');
  const projects = await listAllProjects(baseUrl, auth);
  const baselineProjectIds = projects.map((project) => project.id);
  if (!baselineProjectIds.includes(originalProjectId)) {
    throw new Error('P2-27 original project is absent from the persisted baseline');
  }
  if (projects.some((project) => project.title === ledger.title)) {
    throw new Error('P2-27 owned project title collides with baseline state');
  }
  ledger = await writeContextHistoryLedger(ledgerPath, ledger, {
    baseline_ready: true,
    baseline_project_ids: baselineProjectIds,
    original_project_id: originalProjectId,
  });
  const promptMarker = (index) =>
    `P2HISTORY-${nonce}-${String(index).padStart(2, '0')}|`;
  const prompts = Array.from({length: 15}, (_, index) => {
    const fillerBytes = index < 8 ? 0 : index === 8 ? 6000 : 1000;
    return `${promptMarker(index)}${'x'.repeat(fillerBytes)}`;
  });
  const fixtureBytes = prompts.reduce(
    (total, prompt) => total + Buffer.byteLength(prompt, 'utf8'), 0);
  const visibleTailBytes = prompts.slice(9).reduce(
    (total, prompt) => total + Buffer.byteLength(prompt, 'utf8'), 0);
  const barrierBytes = Buffer.byteLength(prompts[8], 'utf8');
  const expectedUsage = expectedContextUsage(
    prompts.map((content) => ({role: 'user', content})), 8192);
  if (fixtureBytes >= 13_000 || visibleTailBytes > 12_000 ||
      visibleTailBytes + barrierBytes <= 12_000 ||
      expectedUsage.retained_messages !== 6 || expectedUsage.dropped_messages !== 9) {
    throw new Error('P2-27 fixture does not preserve its bounded tail/context proof');
  }
  let projectId = '';
  try {
    ledger = await writeContextHistoryLedger(
      ledgerPath, ledger, {project_create_pending: true});
    const created = await request(baseUrl, auth, '/api/project/new', {
      method: 'POST', body: form({title: ledger.title}),
    });
    projectId = requireString((await created.json()).project_id, 'project_id');
    if (ledger.baseline_project_ids.includes(projectId)) {
      throw new Error('P2-27 created project id collides with baseline state');
    }
    ledger = await writeContextHistoryLedger(
      ledgerPath, ledger, {owned_project_id: projectId});
    await request(baseUrl, auth, '/api/project/settings/raw', projectSettingsRequest({
      instructions: '',
      model: `cardmind-p2-27-invalid-${nonce}`,
      context_byte_budget: '8192',
      maximum_output_tokens: '128',
      automatic_compaction: '0',
    }));
    await seedFailedUserMessages(baseUrl, auth, prompts, '128');
    const state = await activeChatState(baseUrl, auth);
    if (state.total_messages !== prompts.length || state.summarized_messages !== 0 ||
        state.active_context_messages !== expectedUsage.retained_messages ||
        state.active_context_bytes !== expectedUsage.retained_bytes ||
        state.dropped_context_messages !== expectedUsage.dropped_messages ||
        state.history_before_messages !== 9) {
      throw new Error('Context/history state did not match production usage semantics');
    }
    const visiblePrompts = state.messages.map((message) => message.content);
    const expectedHidden = prompts.slice(0, state.history_before_messages);
    if (visiblePrompts.length + expectedHidden.length !== prompts.length ||
        !visiblePrompts.every(
          (content, index) => content === prompts[state.history_before_messages + index])) {
      throw new Error('Visible chat tail did not match the hidden-message boundary');
    }

    await expectRequestFailure(
      baseUrl, auth, '/api/chat/archived?offset=1', {method: 'GET'}, 500);
    const afterInvalidOffset = await activeChatState(baseUrl, auth);
    if (afterInvalidOffset.total_messages !== state.total_messages ||
        afterInvalidOffset.active_context_bytes !== state.active_context_bytes ||
        afterInvalidOffset.history_before_messages !== state.history_before_messages) {
      throw new Error('Invalid archived cursor mutated chat state');
    }

    const archivedPrompts = [];
    const cursors = [];
    let cursor = 0;
    while (archivedPrompts.length < state.history_before_messages) {
      cursors.push(cursor);
      const response = await request(
        baseUrl, auth, `/api/chat/archived?offset=${cursor}`, {method: 'GET'});
      const page = await response.json();
      if (page.ok !== true || !Array.isArray(page.messages) ||
          !Number.isSafeInteger(page.next_offset) || page.next_offset < 0 ||
          typeof page.eof !== 'boolean' ||
          (!page.eof && page.next_offset <= cursor) ||
          !page.messages.every((message) =>
            message?.role === 'user' && typeof message.content === 'string')) {
        throw new Error('Archived history returned invalid typed paging state');
      }
      const remaining = state.history_before_messages - archivedPrompts.length;
      if (page.eof && page.messages.length < remaining) {
        throw new Error('Archived history reached EOF before the hidden boundary');
      }
      archivedPrompts.push(...page.messages.slice(0, remaining).map(
        (message) => message.content));
      cursor = page.next_offset;
      if (cursors.length > prompts.length) {
        throw new Error('Archived history exceeded its bounded page count');
      }
    }
    if (cursors.length < 2 || archivedPrompts.length !== expectedHidden.length ||
        !archivedPrompts.every((content, index) => content === expectedHidden[index]) ||
        new Set([...archivedPrompts, ...visiblePrompts]).size !== prompts.length) {
      throw new Error('Archived history paging duplicated, skipped, or reordered messages');
    }
    return {
      total_messages: prompts.length,
      fixture_bytes: fixtureBytes,
      hidden_messages: state.history_before_messages,
      visible_messages: visiblePrompts.length,
      pages: cursors.length,
      retained_messages: expectedUsage.retained_messages,
      retained_bytes: expectedUsage.retained_bytes,
      dropped_messages: expectedUsage.dropped_messages,
      invalid_cursor_non_mutation: 'pass',
      chronological_no_duplicates: 'pass',
    };
  } finally {
    ledger = await readContextHistoryLedger(ledgerPath, nonce);
    await cleanupContextHistoryOwnership(baseUrl, auth, ledgerPath, ledger);
  }
}

async function verifyPhaseTwoBoundaries(baseUrl, auth) {
  const original = await activeChatState(baseUrl, auth);
  const originalProjectId = requireString(original.project_id, 'project_id');
  let projectId = '';
  try {
    const createdResponse = await request(baseUrl, auth, '/api/project/new', {
      method: 'POST',
      body: form({title: `P2 boundaries ${Date.now()}`}),
    });
    projectId = requireString((await createdResponse.json()).project_id, 'project_id');
    const exactInstructions = `${'界'.repeat(5_461)}x`;
    const oversizedInstructions = `${exactInstructions}y`;
    if (Buffer.byteLength(exactInstructions, 'utf8') !== 16_384 ||
        Buffer.byteLength(oversizedInstructions, 'utf8') !== 16_385) {
      throw new Error('Boundary fixture does not have the declared UTF-8 byte length');
    }
    const exactSettings = {
        instructions: exactInstructions,
        model: 'cardmind-nonexistent-boundary-model',
        context_byte_budget: '262144',
        maximum_output_tokens: '8192',
        automatic_compaction: '0',
    };
    await request(baseUrl, auth, '/api/project/settings/raw',
      projectSettingsRequest(exactSettings));
    const exactProject = await activeChatState(baseUrl, auth);
    if (exactProject.project_instructions !== exactInstructions ||
        Buffer.byteLength(exactProject.project_instructions, 'utf8') !== 16_384 ||
        exactProject.project_model !== exactSettings.model ||
        exactProject.context_byte_budget !== 262_144 ||
        exactProject.maximum_output_tokens !== 8192 ||
        exactProject.automatic_compaction !== false) {
      throw new Error(
        `Exact project boundaries were not persisted: instructions=${Buffer.byteLength(exactProject.project_instructions, 'utf8')}, ` +
        `context=${exactProject.context_byte_budget}, output=${exactProject.maximum_output_tokens}`,
      );
    }
    const assertProjectUnchanged = async (failure) => {
      const observed = await activeChatState(baseUrl, auth);
      if (observed.project_instructions !== exactInstructions ||
          observed.project_model !== exactSettings.model ||
          observed.context_byte_budget !== 262_144 ||
          observed.maximum_output_tokens !== 8192 ||
          observed.automatic_compaction !== false) {
        throw new Error(`${failure} changed previously persisted project settings`);
      }
    };
    const invalidProjectSettings = [
      {...exactSettings, instructions: oversizedInstructions},
      {...exactSettings, instructions: '', context_byte_budget: '8191'},
      {...exactSettings, instructions: '', context_byte_budget: '262145'},
      {...exactSettings, instructions: '', maximum_output_tokens: '127'},
      {...exactSettings, instructions: '', maximum_output_tokens: '8193'},
    ];
    for (const invalid of invalidProjectSettings) {
      await expectRequestFailure(
        baseUrl,
        auth,
        '/api/project/settings/raw',
        projectSettingsRequest(invalid),
        400,
      );
      await assertProjectUnchanged('Invalid raw project request');
    }
    await expectRequestFailure(
      baseUrl,
      auth,
      '/api/project/settings',
      {method: 'POST', body: form(exactSettings)},
      415,
    );
    await assertProjectUnchanged('Legacy form project request');
    await request(baseUrl, auth, '/api/project/settings/raw',
      projectSettingsRequest({...exactSettings, instructions: ''}));
    const clearedProject = await activeChatState(baseUrl, auth);
    if (clearedProject.project_instructions !== '' ||
        clearedProject.project_model !== exactSettings.model ||
        clearedProject.context_byte_budget !== 262_144 ||
        clearedProject.maximum_output_tokens !== 8192) {
      throw new Error('Project boundary cleanup changed unrelated settings');
    }

    const exactChatInstructions = `${'語'.repeat(5_461)}x`;
    const oversizedChatInstructions = `${exactChatInstructions}y`;
    await request(baseUrl, auth, '/api/chat/instructions/raw',
      chatInstructionsRequest(exactChatInstructions));
    const exactChat = await activeChatState(baseUrl, auth);
    if (exactChat.instructions !== exactChatInstructions ||
        Buffer.byteLength(exactChat.instructions, 'utf8') !== 16_384) {
      throw new Error('Exact chat instruction boundary was not persisted');
    }
    await expectRequestFailure(
      baseUrl,
      auth,
      '/api/chat/instructions/raw',
      chatInstructionsRequest(oversizedChatInstructions),
      400,
    );
    const chatAfterRejection = await activeChatState(baseUrl, auth);
    if (chatAfterRejection.instructions !== exactChatInstructions) {
      throw new Error('Oversized chat instructions replaced the previous value');
    }
    await request(baseUrl, auth, '/api/chat/instructions/raw',
      chatInstructionsRequest(''));
    const clearedChat = await activeChatState(baseUrl, auth);
    if (clearedChat.instructions !== '') {
      throw new Error('Chat boundary cleanup did not clear its persisted value');
    }

    const exactPrompt = `${'用'.repeat(5_461)}x`;
    const oversizedPrompt = `${exactPrompt}y`;
    const beforePrompt = await activeChatState(baseUrl, auth);
    await expectRequestFailure(
      baseUrl,
      auth,
      '/api/prompt/raw',
      promptRequest(oversizedPrompt, '0'),
      400,
    );
    const afterPromptRejection = await activeChatState(baseUrl, auth);
    if (afterPromptRejection.total_messages !== beforePrompt.total_messages ||
        afterPromptRejection.active_context_bytes !== beforePrompt.active_context_bytes) {
      throw new Error('Oversized prompt changed the persisted chat history');
    }
    const promptResponse = await request(
      baseUrl,
      auth,
      '/api/prompt/raw',
      promptRequest(exactPrompt, '16384'),
    );
    const promptEvents = parseSse(await promptResponse.text());
    if (!promptEvents.some((event) => event.type === 'error') ||
        promptEvents.some((event) => event.type === 'done')) {
      throw new Error('Boundary prompt did not reach the configured failing provider path');
    }
    const afterPrompt = await activeChatState(baseUrl, auth);
    if (afterPrompt.total_messages !== beforePrompt.total_messages + 1 ||
        afterPrompt.active_context_bytes !== beforePrompt.active_context_bytes + 16_384 ||
        afterPrompt.history_before_offset !== beforePrompt.history_before_offset + 1) {
      throw new Error('Exact boundary prompt was truncated or stored more than once');
    }
    await request(baseUrl, auth, '/api/chat/clear', {method: 'POST', body: form({})});
    const afterPromptClear = await activeChatState(baseUrl, auth);
    if (afterPromptClear.total_messages !== 0 || afterPromptClear.active_context_bytes !== 0 ||
        afterPromptClear.messages.length !== 0) {
      throw new Error('Boundary prompt cleanup did not clear persisted chat history');
    }
    return {
      prompt_bytes: 16_384,
      project_instruction_bytes: 16_384,
      chat_instruction_bytes: 16_384,
      context_bytes: 262_144,
      project_output_tokens: 8192,
      request_output_tokens: 16_384,
    };
  } finally {
    if (projectId) {
      await request(baseUrl, auth, '/api/project/select', {
        method: 'POST',
        body: form({id: projectId}),
      }).then(() => request(baseUrl, auth, '/api/project/delete', {
        method: 'POST',
        body: form({}),
      })).catch((error) => console.error(`Boundary project cleanup failed: ${error.message}`));
    }
    await request(baseUrl, auth, '/api/project/select', {
      method: 'POST',
      body: form({id: originalProjectId}),
    }).catch((error) => console.error(`Original project restore failed: ${error.message}`));
  }
}

function archiveQuotaTitle(nonce) {
  return `P2 archive quota ${nonce}`;
}

function binaryTextNames(nonce) {
  return [
    `p2_29_${nonce}.bin`, `p2_29_${nonce}_moved.BIN`, `p2_29_${nonce}.TXT`,
    `p2_29_${nonce}.Md`, `p2_29_${nonce}.JSONL`, `p2_29_${nonce}.exe`,
    `p2_29_${nonce}.zip`, `p2_29_${nonce}`,
  ];
}

function validateBinaryTextLedger(document, expectedNonce) {
  if (document === null || typeof document !== 'object' || Array.isArray(document) ||
      document.version !== 1 || !/^[0-9]{8,20}$/.test(document.nonce) ||
      (expectedNonce !== '' && document.nonce !== expectedNonce) ||
      typeof document.original_project_id !== 'string' ||
      !Array.isArray(document.names) ||
      JSON.stringify(document.names) !== JSON.stringify(binaryTextNames(document.nonce)) ||
      typeof document.cleanup_complete !== 'boolean') {
    throw new Error('P2-29 ledger has an invalid or unsafe typed shape');
  }
  if (!/^[A-Za-z0-9._-]{1,180}$/.test(document.original_project_id)) {
    throw new Error('P2-29 ledger original project id is unsafe');
  }
  return {...document, names: [...document.names]};
}

async function readBinaryTextLedger(path, expectedNonce) {
  if (!/[\\/]artifacts[\\/]p2-29-binary-text-ledger\.json$/.test(path)) {
    throw new Error('P2-29 ledger path is outside the exact artifacts target');
  }
  return validateBinaryTextLedger(
    JSON.parse(await readFile(path, 'utf8')), expectedNonce);
}

async function writeBinaryTextLedger(path, current, fields) {
  const next = validateBinaryTextLedger({...current, ...fields}, current.nonce);
  const temporaryPath = `${path}.node.tmp`;
  const handle = await open(temporaryPath, 'w');
  try {
    await handle.writeFile(`${JSON.stringify(next)}\n`, 'utf8');
    await handle.sync();
  } finally {
    await handle.close();
  }
  await rename(temporaryPath, path);
  return next;
}

async function listWorkspacePolicy(baseUrl, auth) {
  const entries = [];
  let offset = 0;
  const seen = new Set();
  while (true) {
    if (seen.has(offset)) throw new Error('P2-29 file pagination repeated its offset');
    seen.add(offset);
    const response = await request(
      baseUrl, auth, `/api/files?offset=${offset}`, {method: 'GET'});
    const page = await response.json();
    if (!Array.isArray(page.files) || typeof page.eof !== 'boolean' ||
        !Number.isSafeInteger(page.next_offset) ||
        page.files.some((file) => typeof file?.name !== 'string' ||
          typeof file.editable !== 'boolean')) {
      throw new Error('P2-29 file state omitted typed editable policy');
    }
    entries.push(...page.files.map((file) => ({
      name: file.name, editable: file.editable, size: file.size,
    })));
    if (page.eof) return entries;
    if (page.next_offset <= offset) throw new Error('P2-29 file cursor did not advance');
    offset = page.next_offset;
  }
}

async function uploadWorkspaceBytes(baseUrl, auth, name, bytes) {
  const upload = new FormData();
  upload.append('file', new Blob([bytes]), name);
  await request(
    baseUrl, auth, `/api/file/upload?name=${encodeURIComponent(name)}`,
    {method: 'POST', body: upload});
}

async function cleanupBinaryTextOwnership(baseUrl, auth, ledgerPath, inputLedger) {
  let ledger = validateBinaryTextLedger(inputLedger, inputLedger.nonce);
  const errors = [];
  try {
    await request(baseUrl, auth, '/api/project/select', {
      method: 'POST', body: form({id: ledger.original_project_id}),
    });
    const links = await listAllProjectLinks(baseUrl, auth);
    for (const name of ledger.names) {
      if (links.includes(name)) {
        await request(baseUrl, auth, '/api/project/link', {
          method: 'POST', body: form({path: name, linked: '0'}),
        });
      }
    }
  } catch (error) {
    errors.push(`link/selection cleanup: ${error.message}`);
  }
  try {
    const existing = new Set(await listAllWorkspaceNames(baseUrl, auth));
    for (const name of ledger.names) {
      if (existing.has(name)) await deleteWorkspaceProbe(baseUrl, auth, name);
    }
    const remaining = new Set(await listAllWorkspaceNames(baseUrl, auth));
    if (ledger.names.some((name) => remaining.has(name))) {
      throw new Error('owned files remain after cleanup');
    }
  } catch (error) {
    errors.push(`file cleanup: ${error.message}`);
  }
  if (errors.length > 0) throw new Error(`P2-29 cleanup failed; ${errors.join('; ')}`);
  ledger = await writeBinaryTextLedger(
    ledgerPath, ledger, {cleanup_complete: true});
  return ledger;
}

async function recoverBinaryTextOwnership(baseUrl, auth, ledgerPath) {
  const ledger = await readBinaryTextLedger(ledgerPath, '');
  await cleanupBinaryTextOwnership(baseUrl, auth, ledgerPath, ledger);
  return {cleanup: 'pass'};
}

async function verifyBinaryTextPolicy(baseUrl, auth, nonce, ledgerPath) {
  const names = binaryTextNames(nonce);
  const initialState = await activeChatState(baseUrl, auth);
  const existing = new Set(await listAllWorkspaceNames(baseUrl, auth));
  if (names.some((name) => existing.has(name))) {
    throw new Error('P2-29 reserved file already exists');
  }
  let ledger = validateBinaryTextLedger({
    version: 1, nonce, original_project_id: initialState.project_id,
    names, cleanup_complete: false,
  }, nonce);
  ledger = await writeBinaryTextLedger(ledgerPath, ledger, {});
  const binary = Buffer.from([0xff, 0xfe, 0x00, 0x80, 0x50, 0x32, 0x32, 0x39]);
  let evidence = null;
  let testError = null;
  try {
    await uploadWorkspaceBytes(baseUrl, auth, names[0], binary);
    await request(baseUrl, auth, '/api/file/rename', {
      method: 'POST', body: form({name: names[0], new_name: names[1]}),
    });
    await request(baseUrl, auth, '/api/project/link', {
      method: 'POST', body: form({path: names[1], linked: '1'}),
    });
    if (!(await downloadWorkspaceFile(baseUrl, auth, names[1])).equals(binary)) {
      throw new Error('P2-29 binary download changed uploaded bytes');
    }
    for (const path of [
      `/api/file?name=${encodeURIComponent(names[1])}&offset=0`,
      `/api/qr/file?name=${encodeURIComponent(names[1])}`,
    ]) {
      await expectRequestFailure(baseUrl, auth, path, {method: 'GET'}, 400);
    }
    await expectRequestFailure(baseUrl, auth, '/api/file/save', {
      method: 'POST', body: form({
        name: names[1], offset: '0', original_bytes: String(binary.length), content: 'changed',
      }),
    }, 400);
    if (sha256(await downloadWorkspaceFile(baseUrl, auth, names[1])) !== sha256(binary)) {
      throw new Error('P2-29 rejected binary text operation mutated bytes');
    }
    for (const name of names.slice(2, 5)) {
      await uploadWorkspaceBytes(baseUrl, auth, name, Buffer.from('P2-29 text'));
      await request(
        baseUrl, auth, `/api/file?name=${encodeURIComponent(name)}&offset=0`, {method: 'GET'});
      await request(baseUrl, auth, '/api/file/save', {
        method: 'POST', body: form({
          name, offset: '0', original_bytes: '10', content: 'P2-29 text',
        }),
      });
      await request(
        baseUrl, auth, `/api/qr/file?name=${encodeURIComponent(name)}`, {method: 'GET'});
    }
    for (const name of names.slice(5)) {
      await uploadWorkspaceBytes(baseUrl, auth, name, binary);
      await expectRequestFailure(
        baseUrl, auth, `/api/file?name=${encodeURIComponent(name)}&offset=0`,
        {method: 'GET'}, 400);
    }
    const policy = new Map((await listWorkspacePolicy(baseUrl, auth))
      .map((entry) => [entry.name, entry.editable]));
    if (names.slice(2, 5).some((name) => policy.get(name) !== true) ||
        [names[1], ...names.slice(5)].some((name) => policy.get(name) !== false)) {
      throw new Error('P2-29 Web file state differs from the production text policy');
    }
    evidence = {
      binary_bytes: binary.length, binary_sha256: sha256(binary),
      binary_round_trip: 'pass', manage_rename_link: 'pass',
      text_allowlist: 'pass', binary_text_rejection: 'pass', nonmutation: 'pass',
    };
  } catch (error) {
    testError = error;
  }
  let cleanupError = null;
  try {
    await cleanupBinaryTextOwnership(baseUrl, auth, ledgerPath, ledger);
  } catch (error) {
    cleanupError = error;
  }
  if (testError || cleanupError) {
    throw new Error(`P2-29 binary/text failed; test=${testError?.message ?? 'none'}; ` +
      `cleanup=${cleanupError?.message ?? 'none'}`);
  }
  return {...evidence, cleanup: 'pass'};
}

function archiveQuotaBundleName(nonce) {
  return `cardmind_p2_28_${nonce}.cardmind-project.jsonl`;
}

function validateArchiveQuotaLedger(document, expectedNonce) {
  if (document === null || typeof document !== 'object' || Array.isArray(document) ||
      document.version !== 1 || !/^[0-9]{8,20}$/.test(document.nonce) ||
      (expectedNonce !== '' && document.nonce !== expectedNonce) ||
      document.title !== archiveQuotaTitle(document.nonce) ||
      document.bundle_name !== archiveQuotaBundleName(document.nonce) ||
      !Array.isArray(document.baseline_project_ids) ||
      typeof document.original_project_id !== 'string' ||
      typeof document.original_quota_bytes !== 'number' ||
      typeof document.bundle_preexisting !== 'boolean' ||
      typeof document.bundle_upload_pending !== 'boolean' ||
      typeof document.import_pending !== 'boolean' ||
      typeof document.imported_project_id !== 'string' ||
      typeof document.quota_changed !== 'boolean' ||
      typeof document.web_cleanup_complete !== 'boolean') {
    throw new Error('P2-28 ledger has an invalid or unsafe typed shape');
  }
  const ids = [
    ...document.baseline_project_ids,
    document.original_project_id,
    document.imported_project_id,
  ].filter((value) => value !== '');
  if (ids.some((value) =>
    typeof value !== 'string' || !/^[A-Za-z0-9._-]{1,180}$/.test(value)) ||
      new Set(document.baseline_project_ids).size !== document.baseline_project_ids.length ||
      !Number.isSafeInteger(document.original_quota_bytes) ||
      document.original_quota_bytes < 0 || document.original_quota_bytes > 0xffffffff ||
      document.original_quota_bytes % 1_048_576 !== 0 ||
      document.baseline_project_ids.includes(document.imported_project_id) ||
      document.bundle_preexisting && document.bundle_upload_pending) {
    throw new Error('P2-28 ledger ownership fields are unsafe');
  }
  return {...document, baseline_project_ids: [...document.baseline_project_ids]};
}

async function readArchiveQuotaLedger(path, expectedNonce) {
  if (!/[\\/]artifacts[\\/]p2-28-archive-quota-ledger\.json$/.test(path)) {
    throw new Error('P2-28 ledger path is outside the exact artifacts target');
  }
  return validateArchiveQuotaLedger(
    JSON.parse(await readFile(path, 'utf8')), expectedNonce);
}

async function writeArchiveQuotaLedger(path, current, fields) {
  const next = validateArchiveQuotaLedger({...current, ...fields}, current.nonce);
  const temporaryPath = `${path}.node.tmp`;
  const handle = await open(temporaryPath, 'w');
  try {
    await handle.writeFile(`${JSON.stringify(next)}\n`, 'utf8');
    await handle.sync();
  } finally {
    await handle.close();
  }
  await rename(temporaryPath, path);
  return next;
}

const archiveQuotaMessageBytes = 16_384;
const archiveQuotaMessageCount = 129;

function archiveQuotaContent(nonce, index) {
  const prefix = `P2ARCHIVE-${nonce}-${String(index).padStart(3, '0')}|`;
  return prefix + 'x'.repeat(archiveQuotaMessageBytes - Buffer.byteLength(prefix));
}

function buildArchiveQuotaBundle(nonce) {
  const chatId = createHash('sha256').update(`p2-28-${nonce}`).digest('hex').slice(0, 16);
  const messageCount = archiveQuotaMessageCount;
  const records = [{
    record: 'project', format: 1, title: archiveQuotaTitle(nonce),
    pinned: false, archived: false, instructions: '', active_chat_id: chatId,
    model: `cardmind-p2-28-invalid-${nonce}`, api_profile: '', tool_policy: '',
    ssh_profile: '', context_byte_budget: 8192, maximum_output_tokens: 128,
    automatic_compaction: false, chat_count: 1, shared_link_count: 0,
  }, {
    record: 'chat', id: chatId, title: 'Archive quota', updated_at: Date.now(),
    message_count: messageCount, pinned: false, archived: false, instructions: '',
    draft: '', ssh_tools_enabled: false, context_summary: 'P2-28 fixture summarized',
    summarized_message_count: messageCount,
  }];
  for (let index = 1; index <= messageCount; ++index) {
    records.push({
      record: 'message', chat_id: chatId, sequence: index, role: 'user',
      content: archiveQuotaContent(nonce, index),
    });
  }
  return Buffer.from(`${records.map((record) => JSON.stringify(record)).join('\n')}\n`);
}

async function readCompleteArchiveEvidence(baseUrl, auth, nonce, expectedMessages) {
  const digest = createHash('sha256');
  let cursor = 0;
  let messages = 0;
  let contentBytes = 0;
  let pages = 0;
  const seen = new Set();
  while (true) {
    if (seen.has(cursor) || pages > expectedMessages + 2) {
      throw new Error('P2-28 archived pagination did not make bounded progress');
    }
    seen.add(cursor);
    const response = await request(
      baseUrl, auth, `/api/chat/archived?offset=${cursor}`, {method: 'GET'});
    const page = await response.json();
    if (page.ok !== true || !Array.isArray(page.messages) ||
        !Number.isSafeInteger(page.next_offset) || page.next_offset < 0 ||
        typeof page.eof !== 'boolean' || (!page.eof && page.next_offset <= cursor)) {
      throw new Error('P2-28 archived pagination returned invalid typed state');
    }
    for (const message of page.messages) {
      ++messages;
      const expectedContent = messages <= archiveQuotaMessageCount
        ? archiveQuotaContent(nonce, messages)
        : `P2ARCHIVE-APPEND-${nonce}`;
      if (message?.role !== 'user' || typeof message.content !== 'string' ||
          message.content !== expectedContent) {
        throw new Error(`P2-28 archived sequence ${messages} is invalid`);
      }
      const bytes = Buffer.from(message.content);
      contentBytes += bytes.length;
      digest.update(bytes);
    }
    ++pages;
    cursor = page.next_offset;
    if (page.eof) break;
  }
  if (messages !== expectedMessages) {
    throw new Error(`P2-28 archived count ${messages} differs from ${expectedMessages}`);
  }
  return {
    messages, content_bytes: contentBytes, raw_bytes: cursor, pages,
    sha256: digest.digest('hex'),
  };
}

async function restoreArchiveQuotaOwnership(baseUrl, auth, ledgerPath, inputLedger) {
  let ledger = validateArchiveQuotaLedger(inputLedger, inputLedger.nonce);
  const errors = [];
  try {
    const settings = await settingsState(baseUrl, auth);
    if (settings.project_chat_history_quota_bytes !== ledger.original_quota_bytes) {
      const restored = {...settings,
        project_chat_history_quota_bytes: ledger.original_quota_bytes};
      await request(baseUrl, auth, '/api/settings', {
        method: 'POST', body: settingsUpdateForm(restored, settings.global_instructions),
      });
    }
  } catch (error) {
    errors.push(`quota restore: ${error.message}`);
  }
  try {
    const projects = await listAllProjects(baseUrl, auth);
    const baseline = new Set(ledger.baseline_project_ids);
    const owned = new Set();
    if (ledger.imported_project_id !== '') owned.add(ledger.imported_project_id);
    if (ledger.import_pending) {
      for (const project of projects) {
        if (!baseline.has(project.id) && project.title === ledger.title) owned.add(project.id);
      }
    }
    if (owned.size > 1) throw new Error('owned project discovery is ambiguous');
    for (const id of owned) await deleteProjectById(baseUrl, auth, id);
  } catch (error) {
    errors.push(`project cleanup: ${error.message}`);
  }
  try {
    const names = await listAllWorkspaceNames(baseUrl, auth);
    if (names.includes(ledger.bundle_name)) {
      if (ledger.bundle_preexisting || !ledger.bundle_upload_pending) {
        throw new Error('bundle ownership is not proven');
      }
      await deleteWorkspaceProbe(baseUrl, auth, ledger.bundle_name);
    }
  } catch (error) {
    errors.push(`bundle cleanup: ${error.message}`);
  }
  try {
    if ((await listAllProjects(baseUrl, auth)).some(
      (project) => project.id === ledger.original_project_id)) {
      await request(baseUrl, auth, '/api/project/select', {
        method: 'POST', body: form({id: ledger.original_project_id}),
      });
    }
  } catch (error) {
    errors.push(`selection restore: ${error.message}`);
  }
  if (errors.length > 0) throw new Error(`P2-28 cleanup failed; ${errors.join('; ')}`);
  ledger = await writeArchiveQuotaLedger(ledgerPath, ledger, {
    bundle_upload_pending: false, import_pending: false, imported_project_id: '',
    quota_changed: false, web_cleanup_complete: true,
  });
  return ledger;
}

async function recoverArchiveQuotaOwnership(baseUrl, auth, ledgerPath) {
  const ledger = await readArchiveQuotaLedger(ledgerPath, '');
  const restored = await restoreArchiveQuotaOwnership(baseUrl, auth, ledgerPath, ledger);
  return {cleanup: restored.web_cleanup_complete ? 'pass' : 'fail'};
}

async function verifyArchiveQuota(baseUrl, auth, nonce, ledgerPath) {
  const initialState = await activeChatState(baseUrl, auth);
  const initialSettings = await settingsState(baseUrl, auth);
  const baselineProjects = await listAllProjects(baseUrl, auth);
  const baselineNames = await listAllWorkspaceNames(baseUrl, auth);
  let ledger = validateArchiveQuotaLedger({
    version: 1, nonce, title: archiveQuotaTitle(nonce),
    bundle_name: archiveQuotaBundleName(nonce),
    baseline_project_ids: baselineProjects.map((project) => project.id),
    original_project_id: initialState.project_id,
    original_quota_bytes: initialSettings.project_chat_history_quota_bytes,
    bundle_preexisting: baselineNames.includes(archiveQuotaBundleName(nonce)),
    bundle_upload_pending: false, import_pending: false, imported_project_id: '',
    quota_changed: false, web_cleanup_complete: false,
  }, nonce);
  if (ledger.bundle_preexisting) throw new Error('P2-28 reserved bundle already exists');
  ledger = await writeArchiveQuotaLedger(ledgerPath, ledger, {});
  let evidence = null;
  let testError = null;
  try {
    const bundle = buildArchiveQuotaBundle(nonce);
    ledger = await writeArchiveQuotaLedger(
      ledgerPath, ledger, {bundle_upload_pending: true});
    const upload = new FormData();
    upload.append('file', new Blob([bundle], {type: 'application/jsonl'}), ledger.bundle_name);
    await longRequest(
      baseUrl, auth, `/api/file/upload?name=${encodeURIComponent(ledger.bundle_name)}`,
      {method: 'POST', body: upload});
    ledger = await writeArchiveQuotaLedger(ledgerPath, ledger, {import_pending: true});
    const importedResponse = await longRequest(baseUrl, auth, '/api/chat/import', {
      method: 'POST', body: form({name: ledger.bundle_name}),
    });
    const imported = await importedResponse.json();
    const importedProjectId = requireString(imported.project_id, 'P2-28 imported project id');
    if (ledger.baseline_project_ids.includes(importedProjectId)) {
      throw new Error('P2-28 imported project collided with baseline');
    }
    ledger = await writeArchiveQuotaLedger(
      ledgerPath, ledger, {imported_project_id: importedProjectId});
    const before = await readCompleteArchiveEvidence(
      baseUrl, auth, nonce, archiveQuotaMessageCount);
    if (before.content_bytes !== archiveQuotaMessageCount * archiveQuotaMessageBytes ||
        before.raw_bytes <= 2_097_152) {
      throw new Error('P2-28 fixture did not cross the former 2 MiB boundary');
    }
    const quotaSettings = {...initialSettings, project_chat_history_quota_bytes: 2_097_152};
    ledger = await writeArchiveQuotaLedger(ledgerPath, ledger, {quota_changed: true});
    await request(baseUrl, auth, '/api/settings', {
      method: 'POST',
      body: settingsUpdateForm(quotaSettings, initialSettings.global_instructions),
    });
    const rejectionDetail = await expectRequestFailureDetail(
      baseUrl, auth, '/api/prompt/raw', promptRequest(`P2ARCHIVE-REJECT-${nonce}`, '128'), 500);
    if (!/quota/i.test(rejectionDetail)) {
      throw new Error('P2-28 quota rejection did not identify the quota boundary');
    }
    const after = await readCompleteArchiveEvidence(
      baseUrl, auth, nonce, archiveQuotaMessageCount);
    if (after.raw_bytes !== before.raw_bytes || after.sha256 !== before.sha256) {
      throw new Error('P2-28 quota rejection mutated archived history');
    }
    const unlimitedSettings = {
      ...initialSettings, project_chat_history_quota_bytes: 0,
    };
    await request(baseUrl, auth, '/api/settings', {
      method: 'POST',
      body: settingsUpdateForm(unlimitedSettings, initialSettings.global_instructions),
    });
    await expectFailingProviderSse(
      baseUrl, auth, promptRequest(`P2ARCHIVE-APPEND-${nonce}`, '128'));
    const appendedResponse = await request(
      baseUrl, auth, `/api/chat/archived?offset=${before.raw_bytes}`, {method: 'GET'});
    const appended = await appendedResponse.json();
    const appendedContent = `P2ARCHIVE-APPEND-${nonce}`;
    if (appended.ok !== true || appended.eof !== true ||
        !Number.isSafeInteger(appended.next_offset) ||
        appended.next_offset <= before.raw_bytes || appended.messages?.length !== 1 ||
        appended.messages[0]?.role !== 'user' ||
        appended.messages[0]?.content !== appendedContent) {
      throw new Error('P2-28 append above the former boundary was not persisted exactly once');
    }
    evidence = {
      bundle_bytes: bundle.length, messages: before.messages,
      content_bytes: before.content_bytes, raw_bytes: before.raw_bytes,
      pages: before.pages, sha256: before.sha256,
      final_messages: before.messages + 1,
      former_2mib_boundary: 'pass', append_above_boundary: 'pass',
      quota_rejection: 'pass', nonmutation: 'pass',
    };
  } catch (error) {
    testError = error;
  }
  let cleanupError = null;
  try {
    await restoreArchiveQuotaOwnership(baseUrl, auth, ledgerPath, ledger);
  } catch (error) {
    cleanupError = error;
  }
  if (testError || cleanupError) {
    throw new Error(`P2-28 archive/quota failed; test=${testError?.message ?? 'none'}; ` +
      `cleanup=${cleanupError?.message ?? 'none'}`);
  }
  return {...evidence, cleanup: 'pass'};
}

function historyHeapFixture(nonce, label, messageCount) {
  const title = label === 'small'
    ? `P2 история ${nonce} α`
    : `P2 большая история ${nonce} Ω`;
  const chatId = createHash('sha256')
    .update(`p2-32-history-heap-${nonce}-${label}`).digest('hex').slice(0, 16);
  const contents = Array.from({length: messageCount}, (_, index) => {
    const prefix = `P2HEAP-${nonce}-${label}-${String(index).padStart(3, '0')}|`;
    return prefix + 'x'.repeat(historyHeapMessageBytes - Buffer.byteLength(prefix));
  });
  const records = [{
    record: 'project', format: 1, title, pinned: false, archived: false,
    instructions: '', active_chat_id: chatId,
    model: `cardmind-p2-32-invalid-${nonce}`, api_profile: '', tool_policy: '',
    ssh_profile: '', context_byte_budget: 8192, maximum_output_tokens: 128,
    automatic_compaction: false, chat_count: 1, shared_link_count: 0,
  }, {
    record: 'chat', id: chatId, title: `P2 ${label} chat`, updated_at: Date.now(),
    message_count: messageCount, pinned: false, archived: false, instructions: '',
    draft: '', ssh_tools_enabled: false, context_summary: '',
    summarized_message_count: 0,
  }];
  contents.forEach((content, index) => records.push({
    record: 'message', chat_id: chatId, sequence: index + 1,
    role: 'user', content,
  }));
  const rawRecords = contents.map((content, index) =>
    `${JSON.stringify({sequence: index + 1, role: 'user', content})}\n`);
  return {
    title, chatId, contents,
    bundle: Buffer.from(`${records.map((record) => JSON.stringify(record)).join('\n')}\n`),
    rawHistoryBytes: rawRecords.reduce(
      (total, record) => total + Buffer.byteLength(record), 0),
    lastRecordOffset: rawRecords.slice(0, -1).reduce(
      (total, record) => total + Buffer.byteLength(record), 0),
  };
}

function historyHeapSnapshot(document, label) {
  const snapshot = {};
  for (const field of ['free_heap', 'largest_heap']) {
    if (!Number.isSafeInteger(document[field]) || document[field] <= 0) {
      throw new Error(`P2-32 ${label} status has invalid ${field}`);
    }
    snapshot[field] = document[field];
  }
  return snapshot;
}

function requireHistoryHeapSettled(before, after, label) {
  const freeLoss = before.free_heap - after.free_heap;
  const largestLoss = before.largest_heap - after.largest_heap;
  if (after.free_heap < 70_000 || after.largest_heap < 28 * 1024 ||
      freeLoss > maximumSteadyHeapLossBytes ||
      largestLoss > maximumSteadyHeapLossBytes) {
    throw new Error(
      `P2-32 ${label} request did not return to bounded heap: ` +
      `free_loss=${freeLoss}, largest_loss=${largestLoss}, ` +
      `free=${after.free_heap}, largest=${after.largest_heap}`,
    );
  }
}

async function importHistoryHeapFixture(baseUrl, auth, nonce, label, messageCount,
  baselineProjectIds, baselineNames, ownership) {
  const fixture = historyHeapFixture(nonce, label, messageCount);
  const bundleName = `cardmind_p2_32_${nonce}_${label}.cardmind-project.jsonl`;
  if (baselineNames.includes(bundleName)) {
    throw new Error(`P2-32 reserved bundle '${bundleName}' already exists`);
  }
  ownership.bundleNames.add(bundleName);
  ownership.titles.add(fixture.title);
  const upload = new FormData();
  upload.append('file', new Blob([fixture.bundle], {type: 'application/jsonl'}), bundleName);
  await longRequest(
    baseUrl, auth, `/api/file/upload?name=${encodeURIComponent(bundleName)}`,
    {method: 'POST', body: upload});
  const importedResponse = await longRequest(baseUrl, auth, '/api/chat/import', {
    method: 'POST', body: form({name: bundleName}),
  });
  const projectId = requireString(
    (await importedResponse.json()).project_id, `P2-32 ${label} project id`);
  if (baselineProjectIds.includes(projectId)) {
    throw new Error(`P2-32 ${label} import reused a baseline project id`);
  }
  ownership.projectIds.add(projectId);
  return {...fixture, bundleName, projectId};
}

async function verifyHistoryHeapPhase(baseUrl, auth, nonce, label, fixture) {
  const beforeState = await activeChatState(baseUrl, auth);
  if (beforeState.project_id !== fixture.projectId ||
      beforeState.active_chat_id !== fixture.chatId ||
      beforeState.total_messages !== fixture.contents.length ||
      beforeState.context_byte_budget !== 8192) {
    throw new Error(`P2-32 ${label} imported state is not exact`);
  }
  const tailResponse = await request(
    baseUrl, auth,
    `/api/chat/archived?offset=${fixture.lastRecordOffset}`, {method: 'GET'});
  const tail = await tailResponse.json();
  if (tail.ok !== true || tail.eof !== true ||
      tail.next_offset !== fixture.rawHistoryBytes || tail.messages?.length !== 1 ||
      tail.messages[0]?.role !== 'user' ||
      tail.messages[0]?.content !== fixture.contents.at(-1)) {
    throw new Error(`P2-32 ${label} exact raw-history byte boundary is invalid`);
  }
  const beforeHeap = historyHeapSnapshot(
    await statusState(baseUrl, auth), `${label} before`);
  const prompt = `P2HEAP-REQUEST-${nonce}-${label.toUpperCase()}`;
  await expectFailingProviderSse(baseUrl, auth, promptRequest(prompt, '128'));
  const afterState = await activeChatState(baseUrl, auth);
  const expectedContextBytes = Buffer.byteLength('user') +
    Buffer.byteLength(prompt) + 16;
  if (afterState.total_messages !== fixture.contents.length + 1 ||
      afterState.maximum_context_bytes !== 8192 ||
      afterState.active_context_messages !== 1 ||
      afterState.active_context_bytes !== expectedContextBytes ||
      afterState.dropped_context_messages !== fixture.contents.length ||
      afterState.messages?.filter(
        (message) => message.role === 'user' && message.content === prompt).length !== 1 ||
      afterState.messages.at(-1)?.content !== prompt) {
    throw new Error(`P2-32 ${label} retained request context is not exact and bounded`);
  }
  const afterHeap = historyHeapSnapshot(
    await statusState(baseUrl, auth), `${label} after`);
  requireHistoryHeapSettled(beforeHeap, afterHeap, label);
  return {
    raw_messages: fixture.contents.length,
    raw_content_bytes: fixture.contents.length * historyHeapMessageBytes,
    raw_history_bytes: fixture.rawHistoryBytes,
    context_budget_bytes: 8192,
    retained_messages: afterState.active_context_messages,
    retained_bytes: afterState.active_context_bytes,
    dropped_messages: afterState.dropped_context_messages,
    heap_before: beforeHeap,
    heap_after: afterHeap,
    provider_result: 'explicit_error',
    exact_non_duplication: 'pass',
  };
}

async function cleanupHistoryHeapOwnership(baseUrl, auth, ownership,
  baselineProjectIds, baselineNames, originalProjectId) {
  const errors = [];
  try {
    const projects = await listAllProjects(baseUrl, auth);
    const baseline = new Set(baselineProjectIds);
    const owned = new Set(ownership.projectIds);
    for (const project of projects) {
      if (!baseline.has(project.id) && ownership.titles.has(project.title)) {
        owned.add(project.id);
      }
    }
    for (const projectId of owned) {
      if ((await listAllProjectIds(baseUrl, auth)).includes(projectId)) {
        await deleteProjectById(baseUrl, auth, projectId);
      }
    }
    const remaining = await listAllProjects(baseUrl, auth);
    if (remaining.some((project) =>
      owned.has(project.id) || (!baseline.has(project.id) && ownership.titles.has(project.title)))) {
      throw new Error('owned project remains');
    }
  } catch (error) {
    errors.push(`project cleanup: ${error.message}`);
  }
  try {
    const names = await listAllWorkspaceNames(baseUrl, auth);
    for (const name of ownership.bundleNames) {
      if (names.includes(name)) {
        if (baselineNames.includes(name)) throw new Error(`baseline owns '${name}'`);
        await deleteWorkspaceProbe(baseUrl, auth, name);
      }
    }
    const remaining = await listAllWorkspaceNames(baseUrl, auth);
    if ([...ownership.bundleNames].some((name) => remaining.includes(name))) {
      throw new Error('owned bundle remains');
    }
  } catch (error) {
    errors.push(`bundle cleanup: ${error.message}`);
  }
  try {
    if ((await listAllProjectIds(baseUrl, auth)).includes(originalProjectId)) {
      await request(baseUrl, auth, '/api/project/select', {
        method: 'POST', body: form({id: originalProjectId}),
      });
    }
  } catch (error) {
    errors.push(`selection restore: ${error.message}`);
  }
  if (errors.length > 0) {
    throw new Error(`P2-32 cleanup failed; ${errors.join('; ')}`);
  }
}

async function verifyHistoryHeapAndUtf8Identity(baseUrl, auth, nonce) {
  const original = await activeChatState(baseUrl, auth);
  const originalProjectId = requireString(original.project_id, 'P2-32 original project id');
  const baselineProjects = await listAllProjects(baseUrl, auth);
  const baselineProjectIds = baselineProjects.map((project) => project.id);
  const baselineNames = await listAllWorkspaceNames(baseUrl, auth);
  const ownership = {projectIds: new Set(), bundleNames: new Set(), titles: new Set()};
  let evidence = null;
  let testError = null;
  try {
    const small = await importHistoryHeapFixture(
      baseUrl, auth, nonce, 'small', historyHeapSmallMessages,
      baselineProjectIds, baselineNames, ownership);
    if (!/^[0-9a-f]{16}$/.test(small.projectId)) {
      throw new Error('P2-32 generated project id is not fixed lowercase 16-hex');
    }
    const renamedProjectTitle = `P2 память ${nonce} Ж`;
    const renamedChatTitle = `Ж-${nonce}`;
    ownership.titles.add(renamedProjectTitle);
    await request(baseUrl, auth, '/api/chat/new', {method: 'POST', body: form({})});
    const generatedChat = await activeChatState(baseUrl, auth);
    const generatedChatId = requireString(
      generatedChat.active_chat_id, 'P2-32 generated chat id');
    if (!/^[0-9a-f]{16}$/.test(generatedChatId) || generatedChatId === small.chatId) {
      throw new Error('P2-32 generated chat id is not a distinct lowercase 16-hex value');
    }
    await request(baseUrl, auth, '/api/project/rename', {
      method: 'POST', body: form({title: renamedProjectTitle}),
    });
    await request(baseUrl, auth, '/api/chat/rename', {
      method: 'POST', body: form({title: renamedChatTitle}),
    });
    await request(baseUrl, auth, '/api/project/select', {
      method: 'POST', body: form({id: originalProjectId}),
    });
    await request(baseUrl, auth, '/api/project/select', {
      method: 'POST', body: form({id: small.projectId}),
    });
    const reloaded = await activeChatState(baseUrl, auth);
    const chats = await (await request(
      baseUrl, auth, '/api/chats?offset=0', {method: 'GET'})).json();
    if (reloaded.project_id !== small.projectId ||
        reloaded.project_title !== renamedProjectTitle) {
      throw new Error(
        `P2-32 Unicode project reload changed identity: ` +
        `id_match=${reloaded.project_id === small.projectId}, ` +
        `title_match=${reloaded.project_title === renamedProjectTitle}`,
      );
    }
    if (reloaded.active_chat_id !== generatedChatId ||
        reloaded.active_chat_title !== renamedChatTitle) {
      throw new Error(
        `P2-32 Unicode active chat reload changed identity: ` +
        `id_match=${reloaded.active_chat_id === generatedChatId}, ` +
        `title_match=${reloaded.active_chat_title === renamedChatTitle}`,
      );
    }
    if (!chats.chats?.some((chat) =>
      chat.id === generatedChatId && chat.title === renamedChatTitle)) {
      throw new Error('P2-32 Unicode chat index reload changed stable identity');
    }
    await request(baseUrl, auth, '/api/chat/select', {
      method: 'POST', body: form({id: small.chatId}),
    });
    const smallEvidence = await verifyHistoryHeapPhase(
      baseUrl, auth, nonce, 'small', small);
    await deleteProjectById(baseUrl, auth, small.projectId);
    await deleteWorkspaceProbe(baseUrl, auth, small.bundleName);

    const large = await importHistoryHeapFixture(
      baseUrl, auth, nonce, 'large', historyHeapLargeMessages,
      baselineProjectIds, baselineNames, ownership);
    const largeEvidence = await verifyHistoryHeapPhase(
      baseUrl, auth, nonce, 'large', large);
    if (largeEvidence.raw_content_bytes <= largeEvidence.heap_before.free_heap ||
        largeEvidence.raw_content_bytes <= smallEvidence.raw_content_bytes * 4 ||
        largeEvidence.retained_messages !== smallEvidence.retained_messages ||
        largeEvidence.retained_bytes !== smallEvidence.retained_bytes ||
        smallEvidence.heap_after.free_heap - largeEvidence.heap_after.free_heap >
          maximumSteadyHeapLossBytes ||
        smallEvidence.heap_after.largest_heap - largeEvidence.heap_after.largest_heap >
          maximumSteadyHeapLossBytes) {
      throw new Error('P2-32 complete raw-history size scaled retained context or settled heap');
    }
    evidence = {
      small: smallEvidence,
      large: largeEvidence,
      id_format: 'pass', utf8_identity: 'pass',
      complete_raw_exceeds_heap: 'pass', bounded_context: 'pass',
      settled_heap_non_scaling: 'pass',
    };
  } catch (error) {
    testError = error;
  }
  let cleanupError = null;
  try {
    await cleanupHistoryHeapOwnership(
      baseUrl, auth, ownership, baselineProjectIds, baselineNames, originalProjectId);
  } catch (error) {
    cleanupError = error;
  }
  if (testError || cleanupError) {
    throw new Error(`P2-32 history/heap failed; test=${testError?.message ?? 'none'}; ` +
      `cleanup=${cleanupError?.message ?? 'none'}`);
  }
  return {...evidence, cleanup: 'pass'};
}

async function verifyP4SshOutputDownload(baseUrl, auth, name, expectedBytesValue) {
  if (!/^ssh-command-[0-9a-f]{16}\.log$/.test(name)) {
    throw new Error('P4-05 output filename is not an exact collision-owned log name');
  }
  if (!/^[1-9][0-9]*$/.test(expectedBytesValue)) {
    throw new Error('P4-05 expected output byte count must be a positive integer');
  }
  const expectedBytes = Number(expectedBytesValue);
  if (!Number.isSafeInteger(expectedBytes) || expectedBytes > 0xFFFF_FFFF) {
    throw new Error('P4-05 expected output byte count exceeds the download contract');
  }
  const response = await request(
    baseUrl,
    auth,
    '/api/file/download?name=' + encodeURIComponent(name),
    {method: 'GET'},
  );
  const downloaded = new Uint8Array(await response.arrayBuffer());
  if (downloaded.byteLength !== expectedBytes) {
    throw new Error(
      'P4-05 downloaded ' + downloaded.byteLength +
      ' bytes; expected ' + expectedBytes,
    );
  }
  return {
    download: 'pass',
    output_bytes: expectedBytes,
    filename_pattern: 'pass',
  };
}

async function main() {
  const raw = JSON.parse(await readFile(credentialPath, 'utf8'));
  const baseUrl = new URL(requireString(raw.web_ui?.url, 'web_ui.url'));
  const password = requireString(
    raw.web_ui?.installation_password,
    'web_ui.installation_password',
  );
  const auth = await login(baseUrl, password);
  const suiteIndex = process.argv.indexOf('--suite');
  const suite = suiteIndex >= 0 ? process.argv[suiteIndex + 1] : 'full';
  if (![
    'full', 'projects', 'retry', 'compaction', 'limits', 'chat-scale',
    'workspace-scale', 'file-scale', 'unicode-path', 'unicode-path-recover',
    'shared-isolation', 'shared-isolation-recover', 'diagnostics', 'ssh',
    'p4-ssh-output', 'workspace-tool',
    'large-stream', 'atomic-failure', 'sd-degraded', 'instructions',
    'version-history',
    'request-settings', 'summary-regeneration', 'context-history',
    'context-history-recover', 'context-history-orphan-recover',
    'archive-quota', 'archive-quota-recover',
    'binary-text', 'binary-text-recover',
    'history-heap',
  ].includes(suite)) {
    throw new Error(`Unknown hardware Web E2E suite '${suite}'`);
  }
  if (suite === 'diagnostics') {
    const response = await request(baseUrl, auth, '/api/diagnostics', {method: 'GET'});
    const report = await response.text();
    const relevant = report.split(/\r?\n/).filter(
      (line) => /reset_reason|previous_operation|panic|abort|heap/i.test(line),
    );
    console.log(JSON.stringify({result: 'pass', suite, relevant}));
    return;
  }
  if (suite === 'p4-ssh-output') {
    const output = await verifyP4SshOutputDownload(
      baseUrl,
      auth,
      requiredCommandArgument('--p4-ssh-output-name'),
      requiredCommandArgument('--p4-ssh-output-bytes'),
    );
    console.log(JSON.stringify({result: 'pass', suite, ssh_output: output}));
    return;
  }
  if (suite === 'ssh') {
    const measurements = [];
    recordSnapshot(measurements, 'baseline', await state(baseUrl, auth));
    let connected = false;
    try {
      const startedAt = performance.now();
      const connection = await startSsh(baseUrl, auth);
      connected = true;
      recordSnapshot(measurements, 'ssh_open', await state(baseUrl, auth));
      await verifyInteractiveSsh(baseUrl, auth);
      await stopSsh(baseUrl, auth);
      connected = false;
      recordSnapshot(measurements, 'ssh_closed', await state(baseUrl, auth));
      console.log(JSON.stringify({
        result: 'pass',
        suite,
        ssh_connect_ms: Math.round(performance.now() - startedAt),
        ssh_device_connect_ms: connection.ssh_connect_ms,
        ssh_device_authenticate_ms: connection.ssh_authenticate_ms,
        ssh_device_open_ms: connection.ssh_open_ms,
        ssh_worker_stack_free: connection.ssh_worker_stack_free,
        interactive_ssh: 'pass',
        measurements,
      }));
    } finally {
      if (connected) {
        await stopSsh(baseUrl, auth).catch((error) => {
          console.error(`SSH cleanup failed: ${error.message}`);
        });
      }
    }
    return;
  }
  if (suite === 'workspace-tool') {
    const workspaceTool = await verifyWorkspaceToolRefresh(baseUrl, auth);
    console.log(JSON.stringify({result: 'pass', suite, workspace_tool: workspaceTool}));
    return;
  }
  if (suite === 'compaction') {
    const compaction = await verifyCompactionRoundTrip(baseUrl, auth);
    console.log(JSON.stringify({result: 'pass', suite, compaction}));
    return;
  }
  if (suite === 'summary-regeneration') {
    const summaryRegeneration = await verifyManualSummaryRegeneration(baseUrl, auth);
    console.log(JSON.stringify({
      result: 'pass', suite, summary_regeneration: summaryRegeneration,
    }));
    return;
  }
  if (suite === 'context-history-recover') {
    const recovered = await recoverContextHistoryOwnership(
      baseUrl, auth, requiredCommandArgument('--context-history-ledger'));
    console.log(JSON.stringify({result: 'pass', suite, recovery: recovered}));
    return;
  }
  if (suite === 'context-history-orphan-recover') {
    const recovered = await recoverPreLedgerContextHistoryOrphan(baseUrl, auth);
    console.log(JSON.stringify({
      result: 'pass', suite, orphan_recovery: recovered,
    }));
    return;
  }
  if (suite === 'context-history') {
    const nonce = requiredCommandArgument('--context-history-nonce');
    if (!/^[0-9]{8,20}$/.test(nonce)) {
      throw new Error('Context-history nonce must contain 8 to 20 decimal digits');
    }
    const contextHistory = await verifyContextHistoryParity(
      baseUrl,
      auth,
      nonce,
      requiredCommandArgument('--context-history-ledger'),
    );
    console.log(JSON.stringify({
      result: 'pass', suite, context_history: contextHistory,
    }));
    return;
  }
  if (suite === 'archive-quota-recover') {
    const recovery = await recoverArchiveQuotaOwnership(
      baseUrl, auth, requiredCommandArgument('--archive-quota-ledger'));
    console.log(JSON.stringify({result: 'pass', suite, recovery}));
    return;
  }
  if (suite === 'archive-quota') {
    const nonce = requiredCommandArgument('--archive-quota-nonce');
    if (!/^[0-9]{8,20}$/.test(nonce)) {
      throw new Error('Archive-quota nonce must contain 8 to 20 decimal digits');
    }
    const archiveQuota = await verifyArchiveQuota(
      baseUrl, auth, nonce, requiredCommandArgument('--archive-quota-ledger'));
    console.log(JSON.stringify({result: 'pass', suite, archive_quota: archiveQuota}));
    return;
  }
  if (suite === 'binary-text-recover') {
    const recovery = await recoverBinaryTextOwnership(
      baseUrl, auth, requiredCommandArgument('--binary-text-ledger'));
    console.log(JSON.stringify({result: 'pass', suite, recovery}));
    return;
  }
  if (suite === 'binary-text') {
    const nonce = requiredCommandArgument('--binary-text-nonce');
    if (!/^[0-9]{8,20}$/.test(nonce)) {
      throw new Error('Binary-text nonce must contain 8 to 20 decimal digits');
    }
    const binaryText = await verifyBinaryTextPolicy(
      baseUrl, auth, nonce, requiredCommandArgument('--binary-text-ledger'));
    console.log(JSON.stringify({result: 'pass', suite, binary_text: binaryText}));
    return;
  }
  if (suite === 'history-heap') {
    const nonce = requiredCommandArgument('--history-heap-nonce');
    if (!/^[0-9]{8,20}$/.test(nonce)) {
      throw new Error('History-heap nonce must contain 8 to 20 decimal digits');
    }
    const historyHeap = await verifyHistoryHeapAndUtf8Identity(baseUrl, auth, nonce);
    console.log(JSON.stringify({result: 'pass', suite, history_heap: historyHeap}));
    return;
  }
  if (suite === 'instructions') {
    const instructions = await verifyInstructionPrecedenceWeb(baseUrl, auth);
    console.log(JSON.stringify({result: 'pass', suite, instructions}));
    return;
  }
  if (suite === 'request-settings') {
    const requestSettings = await verifyRequestSettingsWeb(baseUrl, auth);
    console.log(JSON.stringify({result: 'pass', suite, request_settings: requestSettings}));
    return;
  }
  if (suite === 'limits') {
    const boundaries = await verifyPhaseTwoBoundaries(baseUrl, auth);
    console.log(JSON.stringify({result: 'pass', suite, boundaries}));
    return;
  }
  if (suite === 'chat-scale') {
    const chatScale = await verifyChatScale(baseUrl, auth);
    console.log(JSON.stringify({result: 'pass', suite, chat_scale: chatScale}));
    return;
  }
  if (suite === 'workspace-scale') {
    const nonceIndex = process.argv.indexOf('--workspace-scale-nonce');
    if (nonceIndex < 0 || process.argv[nonceIndex + 1] === undefined) {
      throw new Error('workspace-scale requires --workspace-scale-nonce');
    }
    const workspaceScale = await verifyWorkspaceScale(
      baseUrl, auth, process.argv[nonceIndex + 1]);
    console.log(JSON.stringify({result: 'pass', suite, workspace_scale: workspaceScale}));
    return;
  }
  if (suite === 'file-scale') {
    const largeFile = await verifyLargeFileRoundTrip(baseUrl, auth);
    console.log(JSON.stringify({result: 'pass', suite, large_file: largeFile}));
    return;
  }
  if (suite === 'large-stream') {
    const nonce = requiredCommandArgument('--large-stream-nonce');
    if (!/^[0-9]{8,20}$/.test(nonce)) {
      throw new Error('Large-stream nonce must contain 8 to 20 decimal digits');
    }
    const largeStream = await verifyLargeStreamRoundTrip(
      baseUrl,
      auth,
      nonce,
      requiredCommandArgument('--large-stream-ledger'),
      process.argv.includes('--large-stream-soak'),
    );
    console.log(JSON.stringify({result: 'pass', suite, large_stream: largeStream}));
    return;
  }
  if (suite === 'atomic-failure') {
    const nonce = requiredCommandArgument('--atomic-failure-nonce');
    if (!/^[0-9]{8,20}$/.test(nonce)) {
      throw new Error('Atomic-failure nonce must contain 8 to 20 decimal digits');
    }
    const atomicFailure = await verifyAtomicFailureRoundTrip(
      baseUrl,
      auth,
      nonce,
      requiredCommandArgument('--atomic-failure-ledger'),
    );
    console.log(JSON.stringify({result: 'pass', suite, atomic_failure: atomicFailure}));
    return;
  }
  if (suite === 'version-history') {
    const nonce = requiredCommandArgument('--version-history-nonce');
    if (!/^[0-9]{8,20}$/.test(nonce)) {
      throw new Error('Version-history nonce must contain 8 to 20 decimal digits');
    }
    const versionHistory = await verifyVersionHistoryPolicy(baseUrl, auth, nonce);
    console.log(JSON.stringify({result: 'pass', suite, version_history: versionHistory}));
    return;
  }
  if (suite === 'sd-degraded') {
    const degraded = await verifySdDegradedState(
      baseUrl,
      auth,
      requiredCommandArgument('--sd-degraded-state'),
      requiredCommandArgument('--sd-degraded-nonce'),
    );
    console.log(JSON.stringify({result: 'pass', suite, sd_degraded: degraded}));
    return;
  }
  if (suite === 'unicode-path-recover') {
    const recovered = await recoverUnicodePathOwnership(
      baseUrl, auth, requiredCommandArgument('--unicode-path-ledger'));
    console.log(JSON.stringify({result: 'pass', suite, recovery: recovered}));
    return;
  }
  if (suite === 'unicode-path') {
    const nonce = requiredCommandArgument('--unicode-path-nonce');
    if (!/^[0-9]{8,20}$/.test(nonce)) {
      throw new Error('Unicode path nonce must contain 8 to 20 decimal digits');
    }
    const expectedBytes = parsePositiveSafeInteger(
      requiredCommandArgument('--unicode-path-bytes'), 'Unicode path byte count');
    const expectedFnv32 = requiredCommandArgument('--unicode-path-fnv32').toLowerCase();
    if (!/^[0-9a-f]{8}$/.test(expectedFnv32)) {
      throw new Error('Unicode path FNV-1a checksum must contain exactly 8 hex digits');
    }
    const unicodePath = await verifyUnicodePathRoundTrip(
      baseUrl,
      auth,
      nonce,
      expectedBytes,
      expectedFnv32,
      requiredCommandArgument('--unicode-path-ledger'),
    );
    console.log(JSON.stringify({result: 'pass', suite, unicode_path: unicodePath}));
    return;
  }
  if (suite === 'shared-isolation-recover') {
    const recovered = await recoverSharedIsolationOwnership(
      baseUrl, auth, requiredCommandArgument('--shared-isolation-ledger'));
    console.log(JSON.stringify({result: 'pass', suite, recovery: recovered}));
    return;
  }
  if (suite === 'shared-isolation') {
    const nonce = requiredCommandArgument('--shared-isolation-nonce');
    if (!/^[0-9]{8,20}$/.test(nonce)) {
      throw new Error('Shared-isolation nonce must contain 8 to 20 decimal digits');
    }
    const sharedIsolation = await verifySharedIsolationRoundTrip(
      baseUrl,
      auth,
      nonce,
      requiredCommandArgument('--shared-isolation-ledger'),
    );
    console.log(JSON.stringify({
      result: 'pass', suite, shared_isolation: sharedIsolation,
    }));
    return;
  }
  if (suite === 'retry') {
    const retry = await verifyRetryRoundTrip(baseUrl, auth);
    console.log(JSON.stringify({result: 'pass', suite, retry}));
    return;
  }
  if (suite === 'projects') {
    const workspaceProbe = `cardmind_project_e2e_${Date.now()}.txt`;
    let workspaceProbeCreated = false;
    try {
      await uploadWorkspaceProbe(baseUrl, auth, workspaceProbe, 'PROJECT-WEB-E2E');
      workspaceProbeCreated = true;
      const projectRoundTrip = await verifyProjectRoundTrip(
        baseUrl,
        auth,
        workspaceProbe,
      );
      console.log(JSON.stringify({result: 'pass', suite, project_round_trip: projectRoundTrip}));
    } finally {
      if (workspaceProbeCreated) {
        await deleteWorkspaceProbe(baseUrl, auth, workspaceProbe).catch((error) => {
          console.error(`Workspace cleanup failed: ${error.message}`);
        });
      }
    }
    return;
  }
  const measurements = [];
  const stateTimings = [];
  for (const view of ['status', 'chats', 'chat', 'files', 'ssh', 'settings']) {
    const timing = await timedStateView(baseUrl, auth, view);
    stateTimings.push({
      view: timing.view,
      elapsed_ms: timing.elapsed_ms,
      response_bytes: timing.response_bytes,
    });
  }
  const baseline = await state(baseUrl, auth);
  console.log(JSON.stringify({stage: 'state_views', samples: stateTimings}));
  recordSnapshot(measurements, 'baseline', baseline);
  let sshStarted = false;
  const workspaceProbe = `cardmind_web_e2e_${Date.now()}.txt`;
  let workspaceProbeCreated = false;
  try {
    const sshStartedAt = performance.now();
    const connected = await startSsh(baseUrl, auth);
    const sshConnectMs = Math.round(performance.now() - sshStartedAt);
    sshStarted = true;
    recordSnapshot(measurements, 'ssh_open', await state(baseUrl, auth));
    await verifyInteractiveSsh(baseUrl, auth);
    const workspaceMarker = `SD-E2E-OK-${Date.now()}`;
    await uploadWorkspaceProbe(baseUrl, auth, workspaceProbe, workspaceMarker);
    workspaceProbeCreated = true;
    await verifyWorkspaceRoundTrip(baseUrl, auth, workspaceProbe, workspaceMarker);
    await verifyWorkspaceWindowSave(baseUrl, auth, workspaceProbe, workspaceMarker);
    const projectRoundTrip = await verifyProjectRoundTrip(
      baseUrl,
      auth,
      workspaceProbe,
    );
    await exerciseStatePolling(baseUrl, auth, 20);
    const activeSshPromptMs = await prompt(baseUrl, auth, 'WEB-E2E-SSH-OK');
    await deleteWorkspaceProbe(baseUrl, auth, workspaceProbe);
    workspaceProbeCreated = false;
    await stopSsh(baseUrl, auth);
    sshStarted = false;
    recordSnapshot(measurements, 'ssh_closed', await state(baseUrl, auth));
    const closedSshPromptMs = await prompt(baseUrl, auth, 'WEB-E2E-CLOSED-OK');
    const recovered = measurements.at(-1);
    if (recovered.free_heap < 85_000 || recovered.largest_heap < 28_000) {
      throw new Error(`Heap did not recover after SSH: ${JSON.stringify(recovered)}`);
    }
    console.log(JSON.stringify({
      result: 'pass',
      ssh_connect_ms: sshConnectMs,
      ssh_device_connect_ms: connected.ssh_connect_ms,
      ssh_device_authenticate_ms: connected.ssh_authenticate_ms,
      ssh_device_open_ms: connected.ssh_open_ms,
      interactive_ssh: 'pass',
      workspace_sd: 'pass',
      workspace_window_save: 'pass',
      project_round_trip: projectRoundTrip,
      state_poll_iterations: 20,
      active_ssh_prompt_ms: activeSshPromptMs,
      closed_ssh_prompt_ms: closedSshPromptMs,
      measurements,
    }));
  } finally {
    if (workspaceProbeCreated) {
      await deleteWorkspaceProbe(baseUrl, auth, workspaceProbe).catch((error) => {
        console.error(`Workspace cleanup failed: ${error.message}`);
      });
    }
    if (sshStarted) {
      await stopSsh(baseUrl, auth).catch((error) => {
        console.error(`SSH cleanup failed: ${error.message}`);
      });
    }
  }
}

await main();
