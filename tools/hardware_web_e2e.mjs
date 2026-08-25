import { readFile } from 'node:fs/promises';

const credentialPath = new URL('../.secrets/ui-test-credentials.json', import.meta.url);
const maximumRequestMs = 45_000;
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

async function fetchWithin(url, options, timeoutMs) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeoutMs);
  try {
    return await fetch(url, {...options, signal: controller.signal});
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

async function request(baseUrl, auth, path, options) {
  const headers = new Headers(options.headers);
  headers.set('Cookie', auth.cookie);
  headers.set('X-CardMind-CSRF', auth.csrf);
  const response = await fetchWithin(new URL(path, baseUrl), {
    ...options,
    headers,
  }, maximumRequestMs);
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
  await request(
    baseUrl,
    auth,
    `/api/file/upload?name=${encodeURIComponent(name)}`,
    {method: 'POST', body: upload},
  );
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

async function verifyProjectRoundTrip(baseUrl, auth, workspaceName) {
  const original = await activeChatState(baseUrl, auth);
  const originalProjectId = requireString(original.project_id, 'project_id');
  const suffix = String(Date.now());
  const title = `P2 E2E ${suffix}`;
  let projectId = '';
  let importedProjectId = '';
  let bundleName = '';
  let linked = false;
  try {
    const createdResponse = await request(baseUrl, auth, '/api/project/new', {
      method: 'POST',
      body: form({title}),
    });
    const created = await createdResponse.json();
    projectId = requireString(created.project_id, 'project_id');

    await request(baseUrl, auth, '/api/project/settings', {
      method: 'POST',
      body: form({
        instructions: 'P2 project instruction',
        model: '',
        context_byte_budget: '16384',
        maximum_output_tokens: '512',
        automatic_compaction: '0',
      }),
    });
    const first = await activeChatState(baseUrl, auth);
    const firstChatId = requireString(first.active_chat_id, 'active_chat_id');
    if (first.project_title !== title || first.context_byte_budget !== 16384 ||
        first.automatic_compaction !== false) {
      throw new Error('Project settings were not returned by active chat state');
    }
    await request(baseUrl, auth, '/api/chat/instructions', {
      method: 'POST',
      body: form({instructions: 'P2 first chat instruction'}),
    });
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
    const importedState = await activeChatState(baseUrl, auth);
    if (importedState.ssh_tools_enabled === true) {
      throw new Error('Imported project preserved model SSH permission');
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
    return {project_id: projectId, chats: 2, shared_link: 'pass', bundle: 'pass'};
  } finally {
    if (importedProjectId) {
      await request(baseUrl, auth, '/api/project/select', {
        method: 'POST',
        body: form({id: importedProjectId}),
      }).then(() => request(baseUrl, auth, '/api/project/delete', {
        method: 'POST',
        body: form({}),
      })).catch((error) => console.error(`Imported project cleanup failed: ${error.message}`));
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
      }).catch((error) => console.error(`P2 project cleanup failed: ${error.message}`));
    }
    await request(baseUrl, auth, '/api/project/select', {
      method: 'POST',
      body: form({id: originalProjectId}),
    }).catch((error) => console.error(`Original project restore failed: ${error.message}`));
    if (bundleName) {
      await deleteWorkspaceProbe(baseUrl, auth, bundleName).catch((error) => {
        console.error(`Project bundle cleanup failed: ${error.message}`);
      });
    }
  }
}

async function deleteWorkspaceProbe(baseUrl, auth, name) {
  await request(baseUrl, auth, '/api/file/delete', {
    method: 'POST',
    body: form({name}),
  });
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
  const response = await request(baseUrl, auth, '/api/prompt', {
    method: 'POST',
    body: form({prompt: `Reply with exactly: ${marker}`}),
  });
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

async function main() {
  const raw = JSON.parse(await readFile(credentialPath, 'utf8'));
  const baseUrl = new URL(requireString(raw.web_ui?.url, 'web_ui.url'));
  const password = requireString(
    raw.web_ui?.installation_password,
    'web_ui.installation_password',
  );
  const auth = await login(baseUrl, password);
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
